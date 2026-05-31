# libcimbar Windows 移植版

[![License](https://img.shields.io/badge/license-MPL--2.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.9.0-orange.svg)](https://github.com/sz3/libcimbar)

基于 [sz3/libcimbar](https://github.com/sz3/libcimbar) 的 Windows 平台移植与二次开发。将原项目编译为 **DLL 动态链接库**，并提供 **CLI 命令行工具** 和 **aardio GUI 图形界面**，方便在 Windows 环境下集成和使用。

## 功能特性

- **libcimbar.dll** — C 接口动态库，可从 C/C++/C#/Python/aardio 等任意语言调用
- **libcimbar_cli.exe** — 命令行工具，一键编码/解码 cimbar 帧图像
- **aardio GUI** — 图形界面（编码 + 摄像头扫码解码），开箱即用
- **绿色部署** — 无需安装任何依赖，拷贝目录即可运行

## 快速开始

从 [Releases](https://github.com/sz3/libcimbar/releases) 下载 `libcimbar-v0.9.0-win64.zip`，解压后即可使用。

### 编码示例

```bash
# 将文件编码为一组 cimbar 帧图像
libcimbar_cli --encode -i README.txt -o cimbar_frame

# 输出: cimbar_frame_0.png, cimbar_frame_1.png, ...

# 高压缩级别
libcimbar_cli -e -i data.bin -o frame -z 22

# 紧凑模式（适合屏幕显示）
libcimbar_cli -e -i small.txt -o frame -m Bm
```

### 解码示例

```bash
# 从帧图像批量还原文件
libcimbar_cli --decode -i "cimbar_frame_*.png" -o output_dir

# 解码拍照的帧图像（开启去畸变）
libcimbar_cli -d -i "photo_*.png" -o ./result --undistort
```

### CLI 参数列表

| 参数 | 说明 |
|------|------|
| `-e, --encode` | 编码模式（文件 → 帧图像） |
| `-d, --decode` | 解码模式（帧图像 → 文件） |
| `-i, --in` | 输入文件路径 / 图像通配符 |
| `-o, --out` | 输出前缀 / 输出目录 |
| `-m, --mode` | cimbar 模式：`B`(默认) / `Bm` / `Bu` / `4C` |
| `-z, --comp` | zstd 压缩级别 0~22（默认 16） |
| `--id` | 编码会话 ID [0-127] |
| `--undistort` | 解码时尝试去畸变 |
| `-h, --help` | 显示帮助信息 |

## 模式参考

| 模式 | 尺寸 | 帧容量(有效) | 适用场景 |
|------|------|-------------|----------|
| **B** | 1024×1024 | ~7500 字节 | 标准 / 高可靠 |
| **Bm** | 1024×720 | ~5200 字节 | 屏幕传输 |
| **Bu** | 736×637 | ~4000 字节 | 小体积 / 快速 |
| **4C** | 1024×1024 | ~5600 字节 | 兼容旧版 |

## Fountain 码冗余机制

libcimbar 使用 Fountain 码（无率纠删码）作为编码核心：

- **不需要全部帧** — 只需收集到"足够多"的 fountain 块即可恢复数据
- **顺序无关** — 可以按任意顺序扫描帧图像
- **自带冗余** — 编码器自动生成额外冗余帧，提供传输容错

> 对于小文件（<7KB），1 帧通常就包含全部所需的 fountain 块，其余帧为冗余备份。

## libcimbar.dll API

所有导出函数使用 C 调用约定（`extern "C"`），头文件参见 [libcimbar_export.h](src/dll/libcimbar_export.h)。

### 编码流程（伪代码）

```c
// 1. 配置
cimbar_encode_configure(68, 16);    // Mode B, zstd 16

// 2. 初始化编码会话
cimbar_encode_init("myfile.txt", 10, -1);

// 3. 分块喂入数据
int chunk = cimbar_encode_chunk_size();
char buf[chunk];
while (read_from_file(buf, chunk, &actual)) {
    cimbar_encode_feed((unsigned char*)buf, actual);
}

// 4. 生成帧图像
int w = cimbar_encode_image_width();
int h = cimbar_encode_image_height();
unsigned char* frame = malloc(w * h * 3);
while (cimbar_encode_next_frame(frame, w*h*3) > 0) {
    save_as_png(frame, w, h);  // BGR 格式像素
}
free(frame);
```

### 解码流程（伪代码）

```c
// 1. 配置
cimbar_decode_configure(68);

// 2. 分配 scan 缓冲区
int scanBufSize = cimbar_decode_bufsize();
unsigned char* scanBuf = malloc(scanBufSize);

// 3. 逐帧处理
for each frame image {
    unsigned char* pixels = load_image("frame_X.png", &w, &h);
    
    int bytes = cimbar_decode_scan(pixels, w, h, 3, scanBuf, scanBufSize);
    if (bytes <= 0) continue;
    
    int64_t fid = cimbar_decode_fountain(scanBuf, bytes);
    if (fid > 0) {  // 解码完成
        char fname[256];
        cimbar_decode_filename((uint32_t)fid, fname, sizeof(fname));
        
        FILE* out = fopen(fname, "wb");
        unsigned char readBuf[65536];
        int n;
        while ((n = cimbar_decode_read((uint32_t)fid, readBuf, sizeof(readBuf))) > 0)
            fwrite(readBuf, 1, n, out);
        fclose(out);
        break;
    }
}
free(scanBuf);
```

## 目录结构

```
libcimbar_cli/
├── CMakeLists.txt          # 顶层构建文件
├── LICENSE                 # MPL-2.0 协议
├── README.md
├── .gitignore
├── src/
│   ├── dll/                # libcimbar.dll 源码（C API 封装）
│   │   ├── CMakeLists.txt
│   │   ├── libcimbar_export.h
│   │   ├── libcimbar_encode.cpp
│   │   └── libcimbar_decode.cpp
│   └── cli/                # libcimbar_cli.exe 源码
│       ├── CMakeLists.txt
│       └── libcimbar_cli.cpp
└── gui/                    # aardio 图形界面
    ├── main.aardio         # 主窗口
    ├── encoder.aardio      # 编码逻辑
    ├── decoder.aardio      # 解码逻辑
    ├── camera.aardio       # 摄像头采集
    └── libcimbar_dll.aardio # DLL 接口封装
```

## 构建指南

### 依赖

- **MinGW-w64** (GCC 14.2+, MSVCRT, POSIX threads)
- **CMake** 3.10+
- **OpenCV** 4.5.5

### 编译步骤

```bash
# 配置
cmake -G "MinGW Makefiles" -S . -B build \
      -D CMAKE_BUILD_TYPE=Release \
      -D OpenCV_DIR=<opencv_install_path> \
      -D libcimbar_ref_SOURCE_DIR=<libcimbar_ref_path>

# 编译
mingw32-make -C build -j8
```

产物将输出到 `dist/` 目录。

## 许可

本项目基于原 [sz3/libcimbar](https://github.com/sz3/libcimbar) 进行移植和二次开发，遵循原项目的 **[Mozilla Public License 2.0](LICENSE)** 协议。

```
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
```

## 相关链接

- 原始项目: [sz3/libcimbar](https://github.com/sz3/libcimbar) — Color Icon Matrix Barcode
- OpenCV: [opencv/opencv](https://github.com/opencv/opencv)
- aardio: [aardio](https://www.aardio.com/) — 国产桌面开发工具