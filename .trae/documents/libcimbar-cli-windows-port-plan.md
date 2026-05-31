# libcimbar 移植 Windows 平台计划

## 一、项目目标

将 `libcimbar` 开源项目移植到 Windows 平台，产出两件：
1. **libcimbar.dll** — 纯 C 接口的动态链接库，供其他 Windows GUI 程序（如 CFC）调用
2. **libcimbar_cli.exe** — 命令行工具，支持编码/解码模式，通过参数控制 I/O

---

## 二、项目结构设计

```
D:\Projects\aardio\libcimbar_cli/
├── CMakeLists.txt                  # 顶层 CMake 构建脚本
├── src/
│   ├── dll/                        # DLL 导出层 — 纯 C 接口封装
│   │   ├── CMakeLists.txt
│   │   ├── libcimbar_export.h      # DLL 导出宏 + 公共 C API 头文件
│   │   ├── libcimbar_encode.cpp    # 编码接口实现
│   │   └── libcimbar_decode.cpp    # 解码接口实现
│   ├── cli/                        # CLI 可执行程序层
│   │   ├── CMakeLists.txt
│   │   └── libcimbar_cli.cpp       # CLI 入口（编码/解码/参数解析）
│   └── lib/                        # 来自 libcimbar_ref 的库符号链接/拷贝
│       └── (所有 src/lib/* 子目录)
├── third_party/                    # 来自 libcimbar_ref 的第三方库
│   └── (所有 src/third_party_lib/* 子目录)
├── bitmap/                         # 来自 libcimbar_ref 的 bitmap 资源
└── cmake/                          # CMake 辅助模块（FindOpenCV 等）
```

### 文件说明

| 文件 | 职责 |
|---|---|
| `libcimbar_export.h` | 定义 `LIB_CIMBAR_API` 导出宏；声明所有 C API 函数 |
| `libcimbar_encode.cpp` | 实现编码链路：压缩 → fountain 编码 → 生成 cimbar 帧 |
| `libcimbar_decode.cpp` | 实现解码链路：图像提取 → 符号解码 → fountain 解码 → 解压 |
| `libcimbar_cli.cpp` | CLI 主入口：cxxopts 解析参数，调度 DLL 函数完成编/解码 |

---

## 三、DLL C API 接口设计

参考 `cimbar_js.h`（编码）和 `cimbar_recv_js.h`（解码），移除 GLFW/窗口依赖，设计纯数据处理的 C API。

### 3.1 编码接口

```c
// 配置编码参数
// mode_val: 4=4C, 66=Bu, 67=Bm, 68=B（默认）
// compression_level: 0=无压缩, 默认 16（zstd）
// 返回: 0=成功, 负数=失败
int cimbar_encode_configure(int mode_val, int compression_level);

// 初始化编码器，准备压缩待编码文件
// filename: 原始文件名（用于解压时恢复文件名）, 可为 NULL
// fnsize: 文件名长度
// encode_id: 编码会话 ID [0-127], <0 则自动递增
// 返回: 0=成功, 负数=失败
int cimbar_encode_init(const char* filename, unsigned fnsize, int encode_id);

// 获取每次编码块的推荐大小（字节）
// 返回: CHUNK_SIZE
int cimbar_encode_chunk_size();

// 喂入数据块进行压缩编码
// buffer: 数据指针, size: 数据大小（字节）
// 当 size < chunk_size 时表示最后一块，触发 fountain 编码流创建
// 返回: 1=继续喂入, 0=压缩完成 fountain 流已创建, 负数=失败
int cimbar_encode_feed(const unsigned char* buffer, unsigned size);

// 生成下一帧 cimbar 图像
// img_buffer: 输出缓冲区（大小 = img_width * img_height * 3）
// img_size: 输出缓冲区大小
// 返回: 实际写入字节数, 0=编码循环已重新开始, 负数=失败
int cimbar_encode_next_frame(unsigned char* img_buffer, unsigned img_size);

// 获取当前配置下的图像尺寸
int cimbar_encode_image_width();
int cimbar_encode_image_height();
```

### 3.2 解码接口

```c
// 配置解码参数
// mode_val: 同编码
// 返回: 0=成功
int cimbar_decode_configure(int mode_val);

// 获取解码所需的中间缓冲区大小
int cimbar_decode_bufsize();

// 从图像扫描并提取 cimbar 数据
// imgdata: RGB/RGBA 原始图像数据
// imgw, imgh: 图像宽高
// format: 3=RGB, 4=RGBA
// bufspace: 解码输出缓冲区（大小 >= cimbar_decode_bufsize()）
// bufsize: 缓冲区大小
// 返回: 解码出的有效字节数, 负数=失败
int cimbar_decode_scan(const unsigned char* imgdata, unsigned imgw, unsigned imgh,
                       int format, unsigned char* bufspace, unsigned bufsize);

// fountain 解码（累积多帧数据直到完成）
// buffer: 上一级 decode_scan 的输出
// size: 字节数
// 返回: >0=解码完成的文件 ID, 0=还需更多帧, 负数=失败
int64_t cimbar_decode_fountain(const unsigned char* buffer, unsigned size);

// 获取已完成文件的（压缩后）大小
unsigned cimbar_decode_filesize(uint32_t file_id);

// 获取原始文件名
// 返回: 文件名长度, 0=无文件名, 负数=错误
int cimbar_decode_filename(uint32_t file_id, char* filename, unsigned fnsize);

// 获取解压缓冲区推荐大小
int cimbar_decode_decompress_bufsize();

// 分块读取解压后的文件内容
// 返回: 本次读取字节数, 0=读取完毕, 负数=错误
int cimbar_decode_read(uint32_t file_id, unsigned char* buffer, unsigned size);
```

---

## 四、libcimbar_cli.exe 命令行接口设计

### 4.1 编码模式

```
libcimbar_cli.exe encode -i <input_file> -o <output_prefix> [选项]

选项:
  -i, --in <file>          输入文件路径（必填）
  -o, --out <prefix>       输出 PNG 前缀（默认: ./output）
  -m, --mode <mode>        cimbar 模式: B(默认), Bm, Bu, 4C
  -z, --compression <n>    压缩级别: 0=无压缩, 默认 16
  --encode-id <id>         编码会话 ID [0-127], 默认自动递增
  -h, --help               显示帮助
```

### 4.2 解码模式

```
libcimbar_cli.exe decode -i <input_files...> -o <output_dir> [选项]

选项:
  -i, --in <files...>      输入 PNG 文件列表（必填，支持通配符）
  -o, --out <dir>          输出目录（必填）
  -m, --mode <mode>        cimbar 模式: B(默认), Bm, Bu, 4C
  --no-deskew              跳过纠偏步骤（图像已预提取）
  --undistort              尝试去畸变
  -h, --help               显示帮助
```

### 4.3 编码工作流

```
1. 解析参数，配置 mode + compression
2. 读取输入文件二进制内容
3. cimbar_encode_configure()
4. cimbar_encode_init(filename, fnsize, encode_id)
5. 分块调用 cimbar_encode_feed(chunk, chunk_size)
6. 循环:
     cimbar_encode_next_frame(img_buf, img_size)
     将输出保存为 PNG (output_prefix_N.png)
     当返回值为 0 时表示一轮循环结束，退出循环
```

### 4.4 解码工作流

```
1. 解析参数，配置 mode
2. cimbar_decode_configure()
3. 分配解码缓冲区 (cimbar_decode_bufsize())
4. 遍历所有输入 PNG:
     cv::imread → RGB 格式
     [可选] undistort, deskew
     cimbar_decode_scan() 提取 cimbar 数据
     cimbar_decode_fountain() 累积 fountain 解码
5. 当 fountain 解码完成（返回 >0 的 file_id）:
     cimbar_decode_filename() 获取文件名
     cimbar_decode_filesize() 获取大小
     cimbar_decode_read() 分块读取并写入输出文件
```

---

## 五、构建系统改造（CMake）

### 5.1 顶层 CMakeLists.txt

- 同原项目的库模块结构 (`src/lib/*`)
- 移除 `USE_WASM` 分支，全部走本地 MSVC 编译
- 移除 `src/exe/*`（原项目的 cimbar, cimbar_send, cimbar_recv 等）
- 移除 `src/lib/gui`（GLFW/OpenGL 窗口模块，CLI/DLL 不需要）
- 移除 `src/lib/image_hash`（aHash 用于帧去重，CLI 不需要）
- 新增 `src/dll` 子项目 → 输出 `libcimbar.dll`
- 新增 `src/cli` 子项目 → 输出 `libcimbar_cli.exe`

### 5.2 src/dll/CMakeLists.txt

```cmake
add_library(libcimbar SHARED
    libcimbar_encode.cpp
    libcimbar_decode.cpp
)

target_link_libraries(libcimbar
    cimb_translator
    extractor
    correct_static
    wirehair
    zstd
    ${OPENCV_LIBS}
)
```

### 5.3 src/cli/CMakeLists.txt

```cmake
add_executable(libcimbar_cli
    libcimbar_cli.cpp
)

target_link_libraries(libcimbar_cli
    libcimbar
    ${OPENCV_LIBS}
)
```

### 5.4 需要适配的 MSVC 细节

- 原项目已有 MSVC 兼容处理（`iso646.h` include，`stdc++fs` 跳过）
- `pkg_check_modules` 仅在 `BUILD_PORTABLE_LINUX` 生效，Windows 跳过
- OpenCV 需通过 `FindOpenCV` 找到 Windows 预编译版本
- 静态链接 CRT：建议 `/MT`（Release）/ `/MTd`（Debug）

---

## 六、依赖准备

| 依赖 | 版本 | Windows 获取方式 |
|---|---|---|
| OpenCV | 4.x | 预编译 Windows 包或 vcpkg |
| CMake | 3.10+ | 官网下载 |
| Visual Studio | 2019/2022 | VS Installer |
| MSVC 工具链 | 14.2+ | 随 VS 安装 |

推荐方式：
```powershell
# 使用 vcpkg 安装 OpenCV
vcpkg install opencv4:x64-windows-static
# 或下载预编译包设置 OpenCV_DIR 环境变量
```

---

## 七、实施步骤

### 第 1 步：搭建构建环境

1. 在 `D:\Projects\aardio\libcimbar_cli` 创建项目目录结构
2. 编写顶层 `CMakeLists.txt`，配置 MSVC 编译器选项
3. 配置 OpenCV 依赖查找（FindOpenCV 或 vcpkg）
4. 验证：确保 CMake 配置通过

### 第 2 步：创建 DLL 接口层

1. 创建 `src/dll/libcimbar_export.h`
   - 定义 `LIB_CIMBAR_API` 宏（`__declspec(dllexport)` / `__declspec(dllimport)`）
   - 声明所有 C API 函数（`extern "C"`）
2. 创建 `src/dll/libcimbar_encode.cpp`
   - 实现编码链路（参考 `cimbar_js.cpp` 但不依赖 GLFW）
   - 内部使用 `Encoder`, `zstd_compressor`, `fountain_encoder_stream`
3. 创建 `src/dll/libcimbar_decode.cpp`
   - 实现解码链路（参考 `cimbar_recv_js.cpp` 但不依赖 GLFW）
   - 内部使用 `Extractor`, `Decoder`, `fountain_decoder_sink`, `zstd_decompressor`
4. 编译验证：确保产出 `libcimbar.dll`

### 第 3 步：创建 CLI 工具

1. 创建 `src/cli/libcimbar_cli.cpp`
   - 使用 `cxxopts.hpp` 解析命令行参数
   - **编码模式**：调用 `cimbar_encode_configure` → `cimbar_encode_init` → `cimbar_encode_feed` → `cimbar_encode_next_frame`（循环生成 PNG）
   - **解码模式**：调用 `cimbar_decode_configure` → 读入 PNG → `cimbar_decode_scan` → `cimbar_decode_fountain` → `cimbar_decode_read`（输出文件）
2. 编译验证：确保产出 `libcimbar_cli.exe`

### 第 4 步：集成测试

1. 准备测试文件（小文本/图片）
2. 编码测试：
   ```
   libcimbar_cli.exe encode -i test.txt -o test_frame
   ```
   验证产出 PNG 帧序列
3. 解码测试：
   ```
   libcimbar_cli.exe decode -i test_frame_*.png -o decoded/
   ```
   验证解码文件与原始文件一致

### 第 5 步：文档与验收

1. 提供 DLL 接口文档（函数签名、参数说明、调用顺序）
2. 提供 CLI 使用示例
3. 最终验收：编码 → 解码 闭环通过

---

## 八、关键技术点

### 8.1 DLL 内部状态管理

DLL 使用文件级静态变量维护编码/解码会话状态（类似 WASM 版本的 `namespace {}` 匿名命名空间）：

- `_encoder`, `_fes`（fountain_encoder_stream）, `_comp`（zstd_compressor）, `_encodeId`
- `_sink`（fountain_decoder_sink）, `_decId`, `_reassembled`, `_dec`（zstd_decompressor）
- 线程安全：暂不保证，单会话单线程使用

### 8.2 编码帧循环终止条件

原 WASM 代码的循环逻辑：
- 当 `cimbar_encode_next_frame()` 的 frameCount 回绕时，表示一轮编码完成
- 在 CLI 中采用固定冗余倍数（如 4x）来控制帧数，生成足够帧后停止

### 8.3 图像格式约定

- DLL 内部使用 `cv::Mat` RGB 格式（3 通道）
- CLI 输出 PNG 时将 RGB 转为 BGR 再写入（OpenCV imwrite 约定）
- 解码输入统一转为 RGB 后再传给 DLL

### 8.4 文件分块拼接

编码流程中：
1. `cimbar_encode_init` 设置文件名，创建压缩器
2. `cimbar_encode_feed` 分块喂入数据；最后一块触发 fountain 流创建
3. `cimbar_encode_next_frame` 逐帧生成 cimbar 图像

解码流程中：
1. `cimbar_decode_scan` 从每张图片提取 fountain 块
2. `cimbar_decode_fountain` 累积块直到完成
3. 完成后通过 `cimbar_decode_read` 分块读出解压数据

---

## 九、注意事项

1. **不引入 GLFW/OpenGL 依赖**：原 `cimbar_js.cpp` 依赖 `window_glfw.h` 来获取 canvas 尺寸和渲染。DLL 版本直接使用 `cimbar::Config::image_size_x/y()` 获取尺寸。
2. **跳过 GUI 相关库**：编译时排除 `src/lib/gui`、`src/lib/image_hash`。
3. **MSVC 兼容**：原项目已有 `iso646.h` 的 MSVC 处理，保持兼容。
4. **告警级别**：WASM 构建使用 `-Os`，Windows 构建建议 `/O2` 或 `/Ox`。
5. **bitmap 资源路径**：编译时需正确设置 `LIBCIMBAR_PROJECT_ROOT` 宏指向 bitmap 目录。