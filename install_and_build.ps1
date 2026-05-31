# install_and_build.ps1 — libcimbar_cli 一键下载依赖 + 编译 + 绿色打包
# 用法: powershell -ExecutionPolicy Bypass -File install_and_build.ps1
# 所有依赖均为便携版，无需管理员权限

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

$toolsDir = "$scriptDir\.tools"
$outputDir = "$scriptDir\dist_green"          # 绿色部署目录
$buildDir = "$scriptDir\build"

Write-Host "========================================"  -ForegroundColor Cyan
Write-Host " libcimbar_cli 一键构建脚本"             -ForegroundColor Cyan
Write-Host "========================================"  -ForegroundColor Cyan
Write-Host ""

# ============================================================
# 第 1 步: 检查/下载 CMake 便携版
# ============================================================
$cmakeDir = "$toolsDir\cmake"
$cmakeExe = "$cmakeDir\bin\cmake.exe"

if (-not (Test-Path $cmakeExe)) {
    Write-Host "[1/5] 下载 CMake 便携版..." -ForegroundColor Yellow
    $cmakeUrl = "https://cmake.org/files/v4.0/cmake-4.0.5-windows-x86_64.zip"
    $cmakeZip = "$toolsDir\cmake.zip"
    New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $cmakeUrl -OutFile $cmakeZip -UseBasicParsing
    Expand-Archive -Path $cmakeZip -DestinationPath $toolsDir -Force
    # CMake zip 内层目录名通常为 cmake-4.0.5-windows-x86_64
    $inner = Get-ChildItem "$toolsDir" -Directory | Where-Object { $_.Name -like "cmake-*" } | Select-Object -First 1
    if ($inner -and $inner.FullName -ne $cmakeDir) {
        if (Test-Path $cmakeDir) { Remove-Item $cmakeDir -Recurse -Force }
        Rename-Item $inner.FullName $cmakeDir
    }
    Remove-Item $cmakeZip -Force -ErrorAction SilentlyContinue
    Write-Host "  CMake 就绪: $cmakeExe" -ForegroundColor Green
} else {
    Write-Host "[1/5] CMake 已存在，跳过下载" -ForegroundColor Green
}

# ============================================================
# 第 2 步: 检查/下载 MinGW-w64 便携版 (winlibs UCRT)
# ============================================================
$mingwDir = "$toolsDir\mingw64"
$gccExe = "$mingwDir\bin\g++.exe"

if (-not (Test-Path $gccExe)) {
    Write-Host "[2/5] 下载 MinGW-w64 (GCC 14.2 UCRT) 便携版..." -ForegroundColor Yellow
    # winlibs 提供 GCC + MinGW-w64 完整工具链
    $mingwUrl = "https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-18.1.8-12.0.0-ucrt-r1/winlibs-x86_64-posix-seh-gcc-14.2.0-llvm-18.1.8-mingw-w64ucrt-12.0.0-r1.zip"
    $mingwZip = "$toolsDir\mingw64.zip"
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $mingwUrl -OutFile $mingwZip -UseBasicParsing
    Expand-Archive -Path $mingwZip -DestinationPath $mingwDir -Force
    Remove-Item $mingwZip -Force -ErrorAction SilentlyContinue
    Write-Host "  MinGW-w64 就绪: $gccExe" -ForegroundColor Green
} else {
    Write-Host "[2/5] MinGW-w64 已存在，跳过下载" -ForegroundColor Green
}

# ============================================================
# 第 3 步: 检查/下载 OpenCV MinGW 预编译包
# ============================================================
$opencvDir = "$toolsDir\opencv"
$opencvCmake = "$opencvDir\OpenCVConfig.cmake"

if (-not (Test-Path $opencvCmake)) {
    Write-Host "[3/5] 下载 OpenCV MinGW 预编译包..." -ForegroundColor Yellow
    # huihut/OpenCV-MinGW-Build 提供 MinGW 兼容的预编译 OpenCV
    $opencvUrl = "https://github.com/huihut/OpenCV-MinGW-Build/releases/download/OpenCV-4.10.0/OpenCV-MinGW-Build-4.10.0-x64.zip"
    $opencvZip = "$toolsDir\opencv.zip"
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $opencvUrl -OutFile $opencvZip -UseBasicParsing
    Expand-Archive -Path $opencvZip -DestinationPath $opencvDir -Force
    Remove-Item $opencvZip -Force -ErrorAction SilentlyContinue
    Write-Host "  OpenCV 就绪: $opencvDir" -ForegroundColor Green
} else {
    Write-Host "[3/5] OpenCV 已存在，跳过下载" -ForegroundColor Green
}

# ============================================================
# 第 4 步: CMake 配置
# ============================================================
Write-Host "[4/5] CMake 配置项目..." -ForegroundColor Yellow

if (Test-Path $buildDir) { Remove-Item $buildDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$env:PATH = "$cmakeDir\bin;$mingwDir\bin;$env:PATH"

# 设置 OpenCV_DIR 指向包含 OpenCVConfig.cmake 的目录
# huihut 的 OpenCV MinGW Build 的典型结构:
#  OpenCVConfig.cmake 可能在根目录或 x64/mingw/lib 下
$opencvConfigPath = $opencvDir
if (-not (Test-Path "$opencvConfigPath\OpenCVConfig.cmake")) {
    $altPath = Get-ChildItem $opencvDir -Recurse -Filter "OpenCVConfig.cmake" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($altPath) {
        $opencvConfigPath = $altPath.DirectoryName
    }
}

Write-Host "  OpenCV_DIR = $opencvConfigPath" -ForegroundColor Gray

& $cmakeExe -G "MinGW Makefiles" `
    -S $scriptDir `
    -B $buildDir `
    -D CMAKE_BUILD_TYPE=Release `
    -D OpenCV_DIR="$opencvConfigPath" `
    -D libcimbar_ref_SOURCE_DIR="$scriptDir\..\libcimbar_ref"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake 配置失败!" -ForegroundColor Red
    exit 1
}
Write-Host "  CMake 配置成功" -ForegroundColor Green

# ============================================================
# 第 5 步: 编译
# ============================================================
Write-Host "[5/5] 编译项目..." -ForegroundColor Yellow

$mingwMake = "$mingwDir\bin\mingw32-make.exe"
& $mingwMake -C $buildDir -j $env:NUMBER_OF_PROCESSORS

if ($LASTEXITCODE -ne 0) {
    Write-Host "编译失败! 请检查错误信息" -ForegroundColor Red
    exit 1
}
Write-Host "  编译成功!" -ForegroundColor Green

# ============================================================
# 第 6 步: 绿色打包
# ============================================================
Write-Host ""
Write-Host "========================================"  -ForegroundColor Cyan
Write-Host " 绿色部署打包"                             -ForegroundColor Cyan
Write-Host "========================================"  -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

# 收集编译产物
$dllPath = Get-ChildItem $buildDir -Recurse -Filter "libcimbar.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
$exePath = Get-ChildItem $buildDir -Recurse -Filter "libcimbar_cli.exe" -ErrorAction SilentlyContinue | Select-Object -First 1

if ($dllPath) {
    Copy-Item $dllPath.FullName "$outputDir\" -Force
    Write-Host "  libcimbar.dll   -> $outputDir\" -ForegroundColor Green
}
if ($exePath) {
    Copy-Item $exePath.FullName "$outputDir\" -Force
    Write-Host "  libcimbar_cli.exe -> $outputDir\" -ForegroundColor Green
}

# 收集运行时依赖 DLL（OpenCV + MinGW 运行时）
$opencvBin = Get-ChildItem $opencvDir -Recurse -Include "*.dll" -ErrorAction SilentlyContinue
$opencvBinDir = $null
if ($opencvBin.Count -gt 0) {
    $opencvBinDir = $opencvBin[0].DirectoryName
}

# 只复制必要的 OpenCV DLL
$neededDlls = @(
    "libopencv_core*.dll",
    "libopencv_imgproc*.dll",
    "libopencv_imgcodecs*.dll",
    "libopencv_calib3d*.dll",
    "libopencv_features2d*.dll",
    "libopencv_flann*.dll",
    "libopencv_photo*.dll"
)

foreach ($pattern in $neededDlls) {
    if ($opencvBinDir) {
        $found = Get-ChildItem $opencvBinDir -Filter $pattern -ErrorAction SilentlyContinue
        foreach ($f in $found) {
            Copy-Item $f.FullName "$outputDir\" -Force
            Write-Host "  $($f.Name) -> $outputDir\" -ForegroundColor DarkGray
        }
    }
}

# MinGW 运行时 DLL
$mingwRuntimeDlls = @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
foreach ($dllName in $mingwRuntimeDlls) {
    $dll = "$mingwDir\bin\$dllName"
    if (Test-Path $dll) {
        Copy-Item $dll "$outputDir\" -Force
        Write-Host "  $dllName -> $outputDir\" -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host "========================================"  -ForegroundColor Cyan
Write-Host " 构建完成！绿色部署目录: $outputDir"        -ForegroundColor Cyan
Write-Host "========================================"  -ForegroundColor Cyan
Write-Host ""
Write-Host " 内容:" -ForegroundColor White
Get-ChildItem $outputDir | ForEach-Object { Write-Host "   $($_.Name) ($([math]::Round($_.Length/1KB,1)) KB)" }
Write-Host ""
Write-Host " 使用示例:" -ForegroundColor White
Write-Host "   编码: .\libcimbar_cli.exe --encode -i test.txt -o test" -ForegroundColor Gray
Write-Host "   解码: .\libcimbar_cli.exe --decode -i test_*.png -o decoded\" -ForegroundColor Gray
Write-Host "   帮助: .\libcimbar_cli.exe --help" -ForegroundColor Gray