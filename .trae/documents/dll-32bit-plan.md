# 编译 32位 libcimbar.dll — 实施方案（方案 B 确认执行）

## 背景

- `libcimbar.dll` 当前为 **64位**（PE Machine 0x8664，x86_64-w64-mingw32 编译）
- aardio **无 64位运行时**，只有 32位        ─→ 方案 A 不可行
- CLI 管道方案实时性差、大量读写文件        ─→ 方案 C 不可行
- ➡️ **方案 B：重新编译为 32位 DLL**，输出到 `dist32_green/`

## 输出目录

产物放到 **`dist32_green/`**（新建目录，与已有的 64位 `dist_green/` 并存，互不干扰）。

## 实施步骤

### 步骤 1：下载 32位 MinGW-w64 编译器

- 来源：winlibs 或 niXman/mingw-builds-binaries
- 目标文件：`i686-posix-dwarf-msvcrt-gcc-14.2.0-mingw-w64msvcrt-*.7z`
- 解压到：`.tools/mingw32/`
- 核心可执行：`.tools/mingw32/bin/gcc.exe`（i686-w64-mingw32 目标）

### 步骤 2：编译 32位 OpenCV

**必须自己编译**，因为 huihut/OpenCV-MinGW-Build 只提供 x64 预编译包。

编译范围（仅 CFC 需要的模块）：
- `opencv_core`, `opencv_imgproc`, `opencv_imgcodecs`
- `opencv_calib3d`, `opencv_features2d`, `opencv_flann`
- `opencv_photo`, `opencv_stitching`

**不需要的模块**：dnn, highgui, videoio, video, gapi, ml, objdetect

### 步骤 3：修改 CMakeLists.txt

将 `x64/mingw/lib` 链接路径改为 `x86/mingw/lib`。

### 步骤 4：清理并重新编译

- 删除旧的 `build/` 目录
- 用 32位 `gcc.exe` 和 `g++.exe` 重新 cmake + mingw32-make
- 确保 `OpenCV_DIR` 指向含有 32位库的路径

### 步骤 5：收集产物到 dist32_green/

- 主产物：`libcimbar.dll`、`libcimbar_cli.exe`
- 32位 OpenCV DLL：`libopencv_*.dll`
- 32位 MinGW 运行时：`libgcc_s_dw2-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`

### 步骤 6：验证

- PE 头检查：Machine 应为 0x014C（32位）
- aardio 中 `raw.loadDll()` 加载验证
- 端到端编码-解码测试