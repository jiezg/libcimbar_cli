#include "libcimbar_export.h"

#include "extractor/Extractor.h"
#include "extractor/SimpleCameraCalibration.h"
#include "extractor/Undistort.h"
#include "serialize/format.h"

#include <opencv2/opencv.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using std::string;
using std::vector;

// ============================================================
// 编码模式
// ============================================================
static int do_encode(const string& infile, const string& out_prefix,
                     int mode_val, int compression, int encode_id)
{
    // 1. 配置 DLL
    int ret = cimbar_encode_configure(mode_val, compression);
    if (ret < 0)
    {
        std::cerr << "编码配置失败 (错误码 " << ret << ")" << std::endl;
        return ret;
    }

    // 2. 读取输入文件
    std::ifstream f(infile, std::ios::binary);
    if (!f)
    {
        std::cerr << "无法打开输入文件: " << infile << std::endl;
        return -1;
    }
    f.seekg(0, std::ios::end);
    size_t fileSize = f.tellg();
    f.seekg(0, std::ios::beg);

    std::cerr << "输入文件: " << infile << " (" << fileSize << " 字节)" << std::endl;

    // 3. 初始化编码会话
    string basename = std::filesystem::path(infile).filename().string();
    ret = cimbar_encode_init(basename.c_str(), (unsigned)basename.size(), encode_id);
    if (ret < 0)
    {
        std::cerr << "编码初始化失败 (错误码 " << ret << ")" << std::endl;
        return ret;
    }

    // 4. 分块喂入数据
    int chunkSize = cimbar_encode_chunk_size();
    std::vector<char> chunk(chunkSize);
    bool is_last = false;

    while (!is_last)
    {
        f.read(chunk.data(), chunkSize);
        size_t bytes = f.gcount();

        if (bytes < (size_t)chunkSize || f.eof())
        {
            is_last = true;
            // 最后一块可能不满 chunkSize，这会触发 fountain 流创建
        }

        ret = cimbar_encode_feed(
            reinterpret_cast<const unsigned char*>(chunk.data()),
            (unsigned)bytes
        );
        if (ret < 0)
        {
            std::cerr << "数据喂入失败 (错误码 " << ret << ")" << std::endl;
            return ret;
        }

        if (!is_last)
        {
            std::cerr << "." << std::flush;
        }
    }
    std::cerr << std::endl;

    // 如果文件为空或数据全部喂完后 fountain 流可能还未触发
    // 再发一个空块确保触发
    if (ret == 1)
    {
        ret = cimbar_encode_feed(nullptr, 0);
        if (ret < 0)
        {
            std::cerr << "编码完成触发失败 (错误码 " << ret << ")" << std::endl;
            return ret;
        }
    }

    // 5. 生成帧并保存
    int imgW = cimbar_encode_image_width();
    int imgH = cimbar_encode_image_height();
    unsigned frameSize = imgW * imgH * 3;
    std::vector<unsigned char> frameBuf(frameSize);

    unsigned frameIndex = 0;
    while (true)
    {
        ret = cimbar_encode_next_frame(frameBuf.data(), frameSize);
        if (ret == 0)
        {
            // 本轮编码循环完成
            break;
        }
        if (ret < 0)
        {
            std::cerr << "帧生成失败 (错误码 " << ret << ")" << std::endl;
            return ret;
        }

        // cimbar_encode_next_frame 返回的是 cv::Mat 内部数据（BGR 顺序），
        // 直接用 cv::imwrite 保存即可，不需要颜色转换
        cv::Mat bgr(imgH, imgW, CV_8UC3, (void*)frameBuf.data());

        string outpath = fmt::format("{}_{}.png", out_prefix, frameIndex);
        cv::imwrite(outpath, bgr);
        std::cerr << "  生成: " << outpath << std::endl;

        ++frameIndex;
    }

    std::cerr << "编码完成，共生成 " << frameIndex << " 帧" << std::endl;
    return 0;
}

// ============================================================
// 解码模式
// ============================================================
static int do_decode(const vector<string>& infiles, const string& out_dir,
                     int mode_val, bool undistort)
{
    // 1. 配置 DLL
    int ret = cimbar_decode_configure(mode_val);
    if (ret < 0)
    {
        std::cerr << "解码配置失败 (错误码 " << ret << ")" << std::endl;
        return ret;
    }

    // 2. 分配解码缓冲区
    unsigned bufSize = (unsigned)cimbar_decode_bufsize();
    std::vector<unsigned char> scanBuf(bufSize);

    // 3. 创建输出目录
    std::filesystem::create_directories(out_dir);

    // 4. 遍历输入图像
    for (const string& inf : infiles)
    {
        if (inf.empty())
            continue;

        std::cerr << "处理: " << inf << std::endl;

        // imread 读取的是 BGR 格式，DLL 内部也是当 BGR 处理，不需要转换
        cv::Mat img = cv::imread(inf);
        if (img.empty())

        // 可选: 尝试去畸变
        if (undistort)
        {
            Undistort<SimpleCameraCalibration> und;
            cv::UMat umat = img.getUMat(cv::ACCESS_RW);
            und.undistort(umat, umat);
            img = umat.getMat(cv::ACCESS_RW);
        }

        // 5. 扫描并提取 cimbar 数据（传入 BGR 格式数据）
        int bytes = cimbar_decode_scan(
            img.data,
            img.cols, img.rows,
            3,
            scanBuf.data(), bufSize
        );
        if (bytes <= 0)
        {
            std::cerr << "  未检测到 cimbar 码 (错误码 " << bytes << ")" << std::endl;
            continue;
        }

        // 6. fountain 解码
        int64_t fileId = cimbar_decode_fountain(scanBuf.data(), (unsigned)bytes);
        if (fileId <= 0)
        {
            if (fileId == 0)
                std::cerr << "  收集到 fountain 数据块，等待更多帧..." << std::endl;
            else
                std::cerr << "  fountain 解码错误 (错误码 " << fileId << ")" << std::endl;
            continue;
        }

        // 7. 解码完成，恢复文件名并保存
        uint32_t fid = (uint32_t)fileId;

        // 获取文件名
        char fname[256] = {0};
        int fnsize = cimbar_decode_filename(fid, fname, sizeof(fname));
        string outFilename;
        if (fnsize > 0)
            outFilename = string(fname, fnsize);
        else
            outFilename = fmt::format("decoded_{}.bin", fid);

        string outPath = out_dir + "/" + outFilename;
        std::cerr << "  解码完成! 保存: " << outPath << std::endl;

        // 读取解压数据并写入文件
        std::ofstream outf(outPath, std::ios::binary);
        if (!outf)
        {
            std::cerr << "  无法创建输出文件: " << outPath << std::endl;
            continue;
        }

        unsigned readBufSize = (unsigned)cimbar_decode_decompress_bufsize();
        std::vector<unsigned char> readBuf(readBufSize);

        int readBytes;
        size_t totalWritten = 0;
        while ((readBytes = cimbar_decode_read(fid, readBuf.data(), readBufSize)) > 0)
        {
            outf.write(reinterpret_cast<const char*>(readBuf.data()), readBytes);
            totalWritten += readBytes;
        }

        if (readBytes < 0)
            std::cerr << "  解压读取警告 (错误码 " << readBytes << ")" << std::endl;

        outf.close();
        std::cerr << "  写入 " << totalWritten << " 字节" << std::endl;
        return 0;
    }

    std::cerr << "解码完成" << std::endl;
    return 0;
}

static void show_help()
{
    std::cout << "libcimbar Windows CLI - Cimbar 码编码/解码" << std::endl;
    std::cout << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  编码: libcimbar_cli --encode -i <input_file> -o <output_prefix>" << std::endl;
    std::cout << "  解码: libcimbar_cli --decode -i <frame_pattern> -o <output_dir>" << std::endl;
    std::cout << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -e, --encode       编码模式" << std::endl;
    std::cout << "  -d, --decode       解码模式" << std::endl;
    std::cout << "  -i, --in <val>     输入文件(编码) 或 输入图像(解码, 支持通配符)" << std::endl;
    std::cout << "  -o, --out <val>    输出前缀(编码) 或 输出目录(解码)" << std::endl;
    std::cout << "  -m, --mode <val>   cimbar 模式: B(默认), Bm, Bu, 4C" << std::endl;
    std::cout << "  -z, --comp <val>   zstd 压缩级别 0-22 (默认 16)" << std::endl;
    std::cout << "  --id <val>         编码会话 ID [0-127] (默认自动递增)" << std::endl;
    std::cout << "  --undistort        解码时尝试去畸变" << std::endl;
    std::cout << "  -h, --help         显示帮助" << std::endl;
}

// ============================================================
// 主入口 - 手写参数解析器
// ============================================================
int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2)
    {
        show_help();
        return 0;
    }

    bool encodeFlag = false;
    bool decodeFlag = false;
    string infile;
    string outval;
    string modeStr = "B";
    int compression = 16;
    int encodeId = -1;
    bool undistort = false;

    for (int i = 1; i < argc; ++i)
    {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            show_help();
            return 0;
        }
        else if (arg == "-e" || arg == "--encode")
        {
            encodeFlag = true;
        }
        else if (arg == "-d" || arg == "--decode")
        {
            decodeFlag = true;
        }
        else if (arg == "-i" || arg == "--in")
        {
            if (i + 1 < argc)
                infile = argv[++i];
        }
        else if (arg == "-o" || arg == "--out")
        {
            if (i + 1 < argc)
                outval = argv[++i];
        }
        else if (arg == "-m" || arg == "--mode")
        {
            if (i + 1 < argc)
                modeStr = argv[++i];
        }
        else if (arg == "-z" || arg == "--comp")
        {
            if (i + 1 < argc)
                compression = std::atoi(argv[++i]);
        }
        else if (arg == "--id")
        {
            if (i + 1 < argc)
                encodeId = std::atoi(argv[++i]);
        }
        else if (arg == "--undistort")
        {
            undistort = true;
        }
    }

    // 校验
    if (!encodeFlag && !decodeFlag)
    {
        std::cerr << "请指定 --encode 或 --decode" << std::endl;
        show_help();
        return 1;
    }
    if (encodeFlag && decodeFlag)
    {
        std::cerr << "不能同时指定 --encode 和 --decode" << std::endl;
        return 1;
    }

    // 解析模式
    int config_mode = 68; // 默认 B
    if (modeStr == "4" || modeStr == "4c" || modeStr == "4C")
        config_mode = 4;
    else if (modeStr == "Bu" || modeStr == "BU")
        config_mode = 66;
    else if (modeStr == "Bm" || modeStr == "BM")
        config_mode = 67;

    if (encodeFlag)
    {
        if (infile.empty())
        {
            std::cerr << "编码模式需要 -i/--in 指定输入文件" << std::endl;
            return 1;
        }
        string outPrefix = outval.empty() ? "output" : outval;
        return do_encode(infile, outPrefix, config_mode, compression, encodeId);
    }
    else
    {
        // 解码模式
        if (infile.empty())
        {
            std::cerr << "解码模式需要 -i/--in 指定输入图像" << std::endl;
            return 1;
        }

        // 支持通配符展开
        vector<string> expanded;
        std::vector<cv::String> matches;
        cv::glob(infile, matches, false);
        if (matches.empty())
        {
            expanded.push_back(infile);
        }
        else
        {
            for (const auto& m : matches)
                expanded.push_back(m);
        }

        string outDir = outval.empty() ? "." : outval;
        return do_decode(expanded, outDir, config_mode, undistort);
    }
}