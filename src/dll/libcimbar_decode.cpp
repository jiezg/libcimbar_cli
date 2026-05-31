#include "libcimbar_export.h"

#include "cimb_translator/Config.h"
#include "compression/zstd_decompressor.h"
#include "compression/zstd_header_check.h"
#include "encoder/Decoder.h"
#include "encoder/escrow_buffer_writer.h"
#include "extractor/Extractor.h"
#include "fountain/fountain_decoder_sink.h"
#include "fountain/FountainMetadata.h"
#include "util/File.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {
    // 解码器状态
    std::shared_ptr<fountain_decoder_sink> _sink;

    // 解压状态 — 同时只支持一个解压会话
    uint32_t _decId = 0;
    std::vector<unsigned char> _reassembled;
    std::unique_ptr<cimbar::zstd_decompressor<std::stringstream>> _dec;

    int _modeVal = 68;

    // fountain 参数快捷访问
    unsigned fountain_chunks_per_frame()
    {
        return cimbar::Config::fountain_chunks_per_frame(
            cimbar::Config::bits_per_cell()
        );
    }

    unsigned fountain_chunk_size()
    {
        return cimbar::Config::fountain_chunk_size();
    }

    // 将原始像素数据转为 RGB 格式的 cv::UMat
    cv::UMat get_rgb(const unsigned char* imgdata, int width, int height, int format)
    {
        cv::UMat img;
        int cvtype = (format == 4) ? CV_8UC4 : CV_8UC3;
        // 使用临时 Mat 包裹外部数据，UMat 会做深拷贝
        img = cv::Mat(height, width, cvtype, (void*)imgdata).getUMat(cv::ACCESS_RW).clone();
        if (format == 4)
            cv::cvtColor(img, img, cv::COLOR_RGBA2RGB);
        return img;
    }

    // 初始化解压器
    int init_decompress(uint32_t id)
    {
        if (id != _decId)
            return -11;
        if (_dec)
            _dec.reset();
        _dec = std::make_unique<cimbar::zstd_decompressor<std::stringstream>>();
        if (!_dec)
            return -12;
        _dec->init_decompress(reinterpret_cast<char*>(_reassembled.data()), _reassembled.size());
        return 0;
    }

    // 恢复文件内容到 _reassembled，设置 _decId
    int recover_contents(uint32_t id)
    {
        if (id != _decId)
        {
            if (!_sink)
                return -1;
            if (_sink->is_done(id))
                return -2; // 已经完成并清理了

            _reassembled.resize(cimbar_decode_filesize(id));
            if (!_sink->recover(id, _reassembled.data(), _reassembled.size()))
                return -3;
            _decId = id;

            int res = init_decompress(id);
            if (res < 0)
                return res;
        }
        if (_reassembled.empty())
            return -5;

        return 0;
    }
}

int cimbar_decode_configure(int mode_val)
{
    if (mode_val <= 0)
        mode_val = 68;

    bool refresh = (mode_val != _modeVal);
    if (refresh)
    {
        _modeVal = mode_val;
        cimbar::Config::update(mode_val);
        _sink.reset();
    }

    return 0;
}

int cimbar_decode_bufsize(void)
{
    return (int)(fountain_chunks_per_frame() * fountain_chunk_size());
}

int cimbar_decode_scan(const unsigned char* imgdata, unsigned imgw, unsigned imgh,
                       int format, unsigned char* bufspace, unsigned bufsize)
{
    if (format <= 0)
        format = 3;
    if (imgw == 0 || imgh == 0)
        return -1;

    unsigned chunksPerFrame = fountain_chunks_per_frame();
    unsigned chunkSize = fountain_chunk_size();

    // 缓冲区大小必须至少能容纳一个完整帧的解码结果
    if (bufsize < chunkSize * chunksPerFrame)
        return -2;

    // escrow_buffer_writer 将解码输出写入对齐的缓冲区
    escrow_buffer_writer ebw(bufspace, chunksPerFrame, chunkSize);
    Extractor ext;
    Decoder dec;

    cv::UMat img = get_rgb(imgdata, imgw, imgh, format);

    // 图像提取 — 定位 cimbar 码区域并纠偏
    bool shouldPreprocess = false;
    int res = ext.extract(img, img);
    if (!res)
        return -3;
    else if (res == Extractor::NEEDS_SHARPEN)
        shouldPreprocess = true;

    // 符号解码 — 用 CimbReader 解码 → fountain 块
    dec.decode_fountain(img, ebw, shouldPreprocess);

    return (int)(ebw.buffers_in_use() * chunkSize);
}

int64_t cimbar_decode_fountain(const unsigned char* buffer, unsigned size)
{
    unsigned chunkSize = fountain_chunk_size();

    if (!_sink)
        _sink = std::make_shared<fountain_decoder_sink>(chunkSize);

    if (size == 0 || size % chunkSize != 0)
        return -5;

    int64_t res = 0;
    for (unsigned i = 0; i < size && res == 0; i += chunkSize)
    {
        res = _sink->decode_frame(
            reinterpret_cast<const char*>(buffer + i), chunkSize
        );
    }

    // res > 0 → 文件 ID（解码完成）
    // res == 0 → 还需更多帧
    return res;
}

unsigned cimbar_decode_filesize(uint32_t file_id)
{
    FountainMetadata md(file_id);
    return md.file_size();
}

int cimbar_decode_filename(uint32_t file_id, char* filename, unsigned fnsize)
{
    int res = recover_contents(file_id);
    if (res < 0)
        return res;

    const unsigned char* finbuffer = _reassembled.data();
    unsigned size = _reassembled.size();

    std::string fn = cimbar::zstd_header_check::get_filename(finbuffer, size);
    if (!fn.empty())
        fn = File::basename(fn);
    if (fn.empty())
        return 0;

    if (fnsize < fn.size())
        fn.resize(fnsize);
    std::copy(fn.begin(), fn.end(), filename);
    return (int)fn.size();
}

int cimbar_decode_decompress_bufsize(void)
{
    return (int)ZSTD_DStreamOutSize();
}

int cimbar_decode_read(uint32_t file_id, unsigned char* buffer, unsigned size)
{
    int res = recover_contents(file_id);
    if (res < 0)
        return res;

    if (!_dec)
        return -13;
    if (!_dec->good())
        return -14;

    _dec->str(std::string());
    _dec->write_once();
    std::string temp = _dec->str();
    if (size > temp.size())
        size = (unsigned)temp.size();
    std::copy(temp.data(), temp.data() + size, buffer);
    return (int)size;
}