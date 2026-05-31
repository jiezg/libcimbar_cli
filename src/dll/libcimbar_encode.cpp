#include "libcimbar_export.h"

#include "cimb_translator/Config.h"
#include "compression/zstd_compressor.h"
#include "encoder/Encoder.h"
#include "fountain/fountain_encoder_stream.h"
#include "fountain/FountainInit.h"
#include "util/vec_xy.h"

#include <memory>
#include <optional>
#include <sstream>
#include <iostream>

namespace {
    // 编码器实例 — 无窗口/GL 依赖
    std::shared_ptr<fountain_encoder_stream> _fes;
    std::unique_ptr<cimbar::zstd_compressor<std::stringstream>> _comp;
    std::optional<cv::Mat> _cached_frame;

    int _frameCount = 0;
    uint8_t _encodeId = 109;

    int _modeVal = 68;
    int _compressionLevel = cimbar::Config::compression_level();
}

int cimbar_encode_configure(int mode_val, int compression_level)
{
    if (compression_level < 0 || compression_level > 22)
        compression_level = cimbar::Config::compression_level();

    _modeVal = mode_val;
    _compressionLevel = compression_level;
    cimbar::Config::update(_modeVal);

    return 0;
}

int cimbar_encode_init(const char* filename, unsigned fnsize, int encode_id)
{
    _frameCount = 0;

    if (!FountainInit::init())
    {
        std::cerr << "cimbar: FountainInit failed" << std::endl;
        return -5;
    }

    if (encode_id < 0)
        ++_encodeId;
    else
        _encodeId = (uint8_t)(encode_id & 0x7F);

    _comp = std::make_unique<cimbar::zstd_compressor<std::stringstream>>();
    if (!_comp)
        return -1;

    _comp->set_compression_level(_compressionLevel);

    // 将原始文件名写入压缩流头部，解码时可恢复
    if (fnsize > 0 && filename != nullptr)
        _comp->write_header(filename, fnsize);

    _fes.reset();
    _cached_frame.reset();
    return 0;
}

int cimbar_encode_chunk_size(void)
{
    return (int)cimbar::zstd_compressor<std::stringstream>::CHUNK_SIZE;
}

int cimbar_encode_feed(const unsigned char* buffer, unsigned size)
{
    if (!_comp)
        return -1;

    if (size > 0)
    {
        if (!_comp->write(reinterpret_cast<const char*>(buffer), size))
            return -2;
    }

    // 当喂入块小于 CHUNK_SIZE 时，表示数据喂完，触发 fountain 流创建
    if (size < (unsigned)cimbar_encode_chunk_size())
    {
        unsigned fountainChunkSize = cimbar::Config::fountain_chunk_size();
        size_t compressedSize = _comp->size();

        // 如果压缩后数据小于一个 fountain chunk，填充到足够大
        if (compressedSize < fountainChunkSize)
            _comp->pad(fountainChunkSize - compressedSize + 1);

        // 创建 fountain 编码流
        _fes = fountain_encoder_stream::create(*_comp, fountainChunkSize, _encodeId);
        _comp.reset();

        if (!_fes)
            return -3;

        _cached_frame.reset();
        return 0;
    }

    return 1; // 还有数据要喂入
}

int cimbar_encode_next_frame(unsigned char* img_buffer, unsigned img_size)
{
    if (!_fes)
        return -1;

    unsigned img_width = cimbar::Config::image_size_x();
    unsigned img_height = cimbar::Config::image_size_y();
    unsigned expected_size = img_width * img_height * 3;

    if (img_size < expected_size)
        return -2;

    // 检查 fountain 流是否已经循环完一轮
    // 当已生成块数超过所需块数的 8 倍时，重新开始
    unsigned required = _fes->blocks_required() * 8;
    if (_fes->block_count() > required)
    {
        _fes->restart();
        _cached_frame.reset();
        _frameCount = 0;
        return 0; // 通知调用方本轮编码完成
    }

    // 生成一帧 cimbar 图像
    Encoder enc;
    enc.set_encode_id(_encodeId);

    // canvas_size 为空 → CimbWriter 使用 Config 默认尺寸
    cimbar::vec_xy canvas_size{};
    _cached_frame = enc.encode_next(*_fes, canvas_size);

    if (!_cached_frame || !_cached_frame->data)
        return -3;

    // 将 RGB 数据拷贝到调用方缓冲区
    unsigned total_bytes = _cached_frame->cols * _cached_frame->rows * _cached_frame->channels();
    if (total_bytes > img_size)
        total_bytes = img_size;

    std::copy(_cached_frame->data, _cached_frame->data + total_bytes, img_buffer);
    ++_frameCount;
    return (int)total_bytes;
}

int cimbar_encode_image_width(void)
{
    return (int)cimbar::Config::image_size_x();
}

int cimbar_encode_image_height(void)
{
    return (int)cimbar::Config::image_size_y();
}