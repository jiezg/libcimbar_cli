# GitHub 仓库发布实施方案 (v0.9.0)

## 项目现状分析

| 项目 | 状态 |
|------|------|
| Git 仓库 | **未初始化** |
| README.md | **不存在**（仅有 dist_green/README.txt） |
| .gitignore | **不存在** |
| LICENSE | **不存在**（原项目 libcimbar_ref 使用 MPL-2.0） |
| 源代码 | `src/`（C++ DLL + CLI）、`gui/`（aardio）、`CMakeLists.txt` |
| 构建工具 | `.tools/` 目录巨大（编译器、7zip、cmake 等，数百 MB） |
| 64位产物 | `dist_green/`（libcimbar.dll + CLI + OpenCV DLLs） |

## 实施步骤

### 步骤 1：创建 `.gitignore`

排除以下内容，仅保留源代码和文档：

```
# 构建工具链（数百 MB，不应入库）
.tools/

# 构建产物目录
build/
dist_green/
dist32_green/
dist/

# 编译中间文件
*.o
*.obj
*.exe
*.dll
*.a
*.lib
*.so
*.dylib

# CMake 生成文件
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile

# IDE / 编辑器
.vs/
.vscode/
.idea/
*.swp
*.swo
*~

# 系统文件
Thumbs.db
Desktop.ini
.DS_Store

# 压缩包
*.zip
*.7z
*.rar
*.tar.gz

# Python 缓存
__pycache__/
*.pyc
```

### 步骤 2：复制 LICENSE

从 `D:\Projects\aardio\libcimbar_ref\LICENSE` 复制 MPL-2.0 协议到项目根目录。

### 步骤 3：编写 `README.md`

基于 `dist_green/README.txt` 内容改写为 GitHub 风格的 Markdown，包含：

1. **项目标题与简介** — 说明这是对 [sz3/libcimbar](https://github.com/sz3/libcimbar) 的 Windows 移植和二次开发
2. **功能特性** — DLL 动态库 + CLI 命令行工具 + aardio GUI 三合一
3. **快速开始** — 从 Release 下载预编译包，直接运行
4. **CLI 命令行用法** — 编码/解码示例
5. **DLL API 文档** — C API 接口说明
6. **Fountain 码原理** — 冗余机制简要说明
7. **构建指南** — 开发者自行编译的方法
8. **许可声明** — MPL-2.0，注明原始项目
9. **目录结构** — 项目文件组织

### 步骤 4：初始化 Git 仓库并提交

```bash
cd D:\Projects\aardio\libcimbar_cli
git init
git add -A
git commit -m "初始提交: libcimbar Windows 移植版 v0.9.0"
```

### 步骤 5：创建 GitHub Release v0.9.0

将 `dist_green/` 目录打包为 `libcimbar-v0.9.0-win64.zip`，上传到 Release。

Release 描述内容：
- 版本号 v0.9.0
- 包含的二进制文件列表
- 使用方法（编码/解码命令示例）
- 系统要求（Windows 10+ x64）
- 已知限制
- 与原项目的关系说明

### 步骤 6：（用户自行操作）推送至 GitHub

用户在 GitHub 创建仓库后，设置 remote 并推送：

```bash
git remote add origin <用户仓库URL>
git push -u origin main
```

然后在 GitHub 网页上创建 Release v0.9.0，上传打包好的 zip 文件。

## 注意事项

- `.tools/` 目录**不入库**（包含 mingw32/mingw64 编译器、cmake、7zip 等，约 1GB+）
- `dist_green/` 和 `dist32_green/` **不入库**（二进制产物通过 GitHub Release 分发）
- 仅源代码、构建脚本（CMakeLists.txt）、GUI 代码（aardio）入库
- 遵循原项目 MPL-2.0 协议，在 LICENSE 和 README 中明确标注