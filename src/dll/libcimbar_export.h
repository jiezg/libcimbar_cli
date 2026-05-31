#ifndef LIBCIMBAR_EXPORT_H
#define LIBCIMBAR_EXPORT_H

#include <stdint.h>

#ifdef _WIN32
    #ifdef LIBCIMBAR_DLL_EXPORTS
        #define LIB_CIMBAR_API __declspec(dllexport)
    #else
        #define LIB_CIMBAR_API __declspec(dllimport)
    #endif
#else
    #define LIB_CIMBAR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 编码接口 — 将文件数据编码为 cimbar 图像帧
// ============================================================

// 配置编码参数
// mode_val: 4=4C, 66=Bu, 67=Bm, 68=B（默认）
// compression_level: zstd 压缩级别, 0=无压缩, 默认 16
// 返回: 0=成功, 负数=失败
LIB_CIMBAR_API int cimbar_encode_configure(int mode_val, int compression_level);

// 初始化编码会话
// filename: 原始文件名（解码时用于恢复）, 可为 NULL
// fnsize: 文件名长度, 若 filename 为 NULL 则传 0
// encode_id: 编码会话 ID [0-127], <0 则自动递增
// 返回: 0=成功, 负数=失败
LIB_CIMBAR_API int cimbar_encode_init(const char* filename, unsigned fnsize, int encode_id);

// 获取每次喂入数据的推荐块大小（字节）
LIB_CIMBAR_API int cimbar_encode_chunk_size(void);

// 向编码器喂入数据块
// buffer: 数据指针
// size: 数据大小（字节）
// 当 size < chunk_size 时表示最后一块，将触发 fountain 编码流创建
// 返回: 1=继续喂入下一块, 0=压缩完成/fountain 流已创建, 负数=失败
LIB_CIMBAR_API int cimbar_encode_feed(const unsigned char* buffer, unsigned size);

// 生成下一帧 cimbar 图像（RGB 格式）
// img_buffer: 输出缓冲区, 大小需 >= img_width * img_height * 3
// img_size: 缓冲区字节大小
// 返回: 实际写入的字节数（img_width * img_height * 3）, 0=本轮编码循环结束, 负数=失败
LIB_CIMBAR_API int cimbar_encode_next_frame(unsigned char* img_buffer, unsigned img_size);

// 获取当前配置下的图像宽度（像素）
LIB_CIMBAR_API int cimbar_encode_image_width(void);

// 获取当前配置下的图像高度（像素）
LIB_CIMBAR_API int cimbar_encode_image_height(void);

// ============================================================
// 解码接口 — 从 cimbar 图像中提取并解码数据
// ============================================================

// 配置解码参数
// mode_val: 同编码
// 返回: 0=成功
LIB_CIMBAR_API int cimbar_decode_configure(int mode_val);

// 获取解码中间缓冲区所需大小（字节）
LIB_CIMBAR_API int cimbar_decode_bufsize(void);

// 从 RGB/RGBA 图像扫描并提取 cimbar 数据（提取 + 符号解码一步完成）
// imgdata: 原始像素数据指针
// imgw, imgh: 图像宽高
// format: 3=RGB, 4=RGBA
// bufspace: 输出缓冲区（大小 >= cimbar_decode_bufsize()）
// bufsize: 缓冲区大小
// 返回: 成功解码出的有效字节数, 负数=失败（无 cimbar 码或提取失败）
LIB_CIMBAR_API int cimbar_decode_scan(const unsigned char* imgdata, unsigned imgw, unsigned imgh,
                                      int format, unsigned char* bufspace, unsigned bufsize);

// fountain 解码 — 将 scan 输出的数据块累积直到完成
// buffer: cimbar_decode_scan 的输出数据
// size: 字节数（必须是 fountain_chunk_size 的整数倍）
// 返回: >0=解码完成并返回文件 ID, 0=还需更多帧, 负数=错误
LIB_CIMBAR_API int64_t cimbar_decode_fountain(const unsigned char* buffer, unsigned size);

// 获取已完成文件的压缩后大小（字节）
// file_id: fountain_decode 返回的文件 ID
LIB_CIMBAR_API unsigned cimbar_decode_filesize(uint32_t file_id);

// 获取原始文件名
// file_id: fountain_decode 返回的文件 ID
// filename: 输出缓冲区
// fnsize: 缓冲区大小
// 返回: 实际文件名长度, 0=无文件名, 负数=错误
LIB_CIMBAR_API int cimbar_decode_filename(uint32_t file_id, char* filename, unsigned fnsize);

// 获取解压读取缓冲区推荐大小
LIB_CIMBAR_API int cimbar_decode_decompress_bufsize(void);

// 分块读取解压后的文件内容
// file_id: fountain_decode 返回的文件 ID
// buffer: 输出缓冲区
// size: 缓冲区大小
// 返回: 本次读取字节数, 0=全部读取完毕, 负数=错误
LIB_CIMBAR_API int cimbar_decode_read(uint32_t file_id, unsigned char* buffer, unsigned size);

#ifdef __cplusplus
}
#endif

#endif // LIBCIMBAR_EXPORT_H