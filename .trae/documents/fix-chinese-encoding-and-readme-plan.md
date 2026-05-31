# 修复中文乱码 + 编写 README.txt 实施计划

## 问题分析

### 1. CMD 中文乱码原因

- 源文件（`.cpp`）编码为 UTF-8，编译后字符串字面量以 UTF-8 字节嵌入二进制
- Windows CMD 默认代码页为 GBK（936），将 UTF-8 字节序列按 GBK 解码，导致乱码
- 受影响位置：`libcimbar_cli.cpp` 中 `show_help()`、所有 `std::cerr`/`std::cout` 输出的中文，以及 `libcimbar_encode.cpp` 中的一条中文错误提示

### 2. 帧容量与冗余机制

- **单帧容量（Mode B 默认）**: 1024×1024 图像，约 12400 个数据单元，每单元 6 bits，原始吞吐约 9300 字节/帧。去除 Reed-Solomon ECC 开销（30/155），实际有效数据约 **7500 字节/帧**（12 个 fountain chunk × ~625 字节）
- **为何 5KB 文件生成 2 帧**: Fountain 码是一种"无率纠删码"，编码器会持续生成冗余块。1 帧已包含全部原始 fountain 块（压缩后数据＜7500 字节时，12 个块远多于实际需要的 3-5 个），足够解码。第 2 帧是**额外冗余**——Fountain 码的特性就是"不需要全部帧就能恢复"，任意足够多的帧都能解码。这在扫码场景中很有用：扫到任意 1 帧即可恢复文件，损坏的帧可丢弃
- **终止条件**: `cimbar_encode_next_frame()` 内部在生成 `blocks_required × 8` 个 fountain 块后自动结束本轮循环并返回 0

---

## 实施步骤

### 步骤 1：修复 CMD 中文乱码

**文件**: `D:\Projects\aardio\libcimbar_cli\src\cli\libcimbar_cli.cpp`

在文件头部已有 `#include` 区域追加条件编译头文件，在 `main()` 开头设置 UTF-8 控制台：

```cpp
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    // ...
}
```

> `CP_UTF8` (65001) 在 Windows 10 1903+ 原生支持。对旧版 Windows，可在 cmd 中先执行 `chcp 65001` 再运行。

**文件**: `D:\Projects\aardio\libcimbar_cli\src\dll\libcimbar_encode.cpp`

将唯一的 `std::cerr` 中文错误提示改为英文，避免影响调用方控制台编码。

重新编译 → 在 CMD 默认 GBK 环境测试 `--help` 输出中文正常显示。

### 步骤 2：编写 README.txt

**文件**: `D:\Projects\aardio\libcimbar_cli\dist_green\README.txt`

内容大纲：
1. **概述** — 项目说明、Windows 移植信息
2. **绿色部署** — 拷贝 dist_green 即可运行，无需安装
3. **libcimbar_cli.exe 命令行工具**
   - 编码/解码参数完整列表（含中文说明）
   - 模式说明（B/Bm/Bu/4C 的区别）
   - 帧容量说明（单帧约 7500 字节有效数据，含 fountain 冗余）
   - 命令行示例
4. **libcimbar.dll 动态链接库 API**
   - 13 个导出函数，分编码组(7)和解码组(6)
   - 每个函数参数类型、返回值含义
   - 编码调用流程伪代码
   - 解码调用流程伪代码
5. **Fountain 码冗余说明**
   - 为什么 1 帧就够解码却生成多帧
   - 在扫码场景中 redundancy 的意义（任意 1 帧即可恢复）
6. **常见问题**
   - CMD 乱码解决方案
   - 文件路径含空格的处理
   - 模式选择建议

### 步骤 3：验证

- CMD 中验证 `libcimbar_cli.exe --help` 中文正常
- 编码→解码闭环测试确认功能完整
- DLL 导出符号检查

---

## 涉及文件清单

| 文件 | 操作 |
|------|------|
| `src/cli/libcimbar_cli.cpp` | 添加 `SetConsoleOutputCP(CP_UTF8)` |
| `src/dll/libcimbar_encode.cpp` | 中文错误提示改为英文 |
| `dist_green/README.txt` | 新建文档 |