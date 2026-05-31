# aardio 桌面 GUI 程序实施计划

## 概述

使用 aardio 语言编写 GUI 程序，调用 `libcimbar.dll` 实现编码（文件→帧图像显示）和解码（摄像头→实时扫描→保存文件）两大功能。参考原项目 `index.html`（编码）和 `recv.html`（解码）的交互效果。

---

## 1. 项目结构

```
libcimbar_cli/
├── dist_green/
│   ├── libcimbar.dll          ← 核心 DLL
│   ├── libcimbar_cli.exe
│   ├── libopencv_*.dll
│   ├── libgcc_s_seh-1.dll
│   ├── libstdc++-6.dll
│   ├── libwinpthread-1.dll
│   └── README.txt
└── gui/
    ├── main.aardio             ← 主程序入口
    ├── libcimbar_dll.aardio    ← DLL 接口封装（raw.api 声明）
    ├── encoder.aardio          ← 编码逻辑模块
    ├── decoder.aardio          ← 解码逻辑模块
    └── camera.aardio           ← 摄像头采集模块
```

aardio 程序运行时从 `../dist_green/` 加载 DLL 和依赖。

---

## 2. DLL 接口封装 (`libcimbar_dll.aardio`)

定义完整的 DLL 函数声明，通过 `raw.api()` 声明 13 个导出函数：

```aardio
// 编码函数 (7个)
cimbar_encode_configure     = dll.api("cimbar_encode_configure", "int(int mode, int compression)")
cimbar_encode_init          = dll.api("cimbar_encode_init", "int(string filename, INT fnsize, int encodeId)")
cimbar_encode_chunk_size    = dll.api("cimbar_encode_chunk_size", "int()")
cimbar_encode_feed          = dll.api("cimbar_encode_feed", "int(pointer buffer, INT size)")
cimbar_encode_next_frame    = dll.api("cimbar_encode_next_frame", "int(pointer imgBuf, INT imgSize)")
cimbar_encode_image_width   = dll.api("cimbar_encode_image_width", "int()")
cimbar_encode_image_height  = dll.api("cimbar_encode_image_height", "int()")

// 解码函数 (6个)
cimbar_decode_configure     = dll.api("cimbar_decode_configure", "int(int mode)")
cimbar_decode_bufsize       = dll.api("cimbar_decode_bufsize", "int()")
cimbar_decode_scan          = dll.api("cimbar_decode_scan", "int(pointer imgdata, INT w, INT h, int format, pointer buf, INT bufsize)")
cimbar_decode_fountain      = dll.api("cimbar_decode_fountain", "long(pointer buf, INT size)")
cimbar_decode_filename      = dll.api("cimbar_decode_filename", "int(INT fileId, pointer buf, INT bufsize)")
cimbar_decode_read          = dll.api("cimbar_decode_read", "int(INT fileId, pointer buf, INT bufsize)")
```

注意：aardio 中需要用 `..raw.buffer()` 分配原生内存传指针。

---

## 3. UI 布局设计

使用 `win.ui.tabs` 分为两个标签页：

### 3.1 编码标签页（参考 index.html 效果）

```
┌─────────────────────────────────────────┐
│ [选择文件]  模式: [B ▼]  压缩: [16]     │ ← 工具栏
│ FPS: [========|====] 15                 │ ← 帧率滑块
├─────────────────────────────────────────┤
│                                         │
│          ┌───────────────────┐          │
│          │                   │          │
│          │   cimbar 帧图像   │          │ ← gdip 自定义控件
│          │   自动轮播显示    │          │   (plus 控件 + gdip)
│          │                   │          │
│          └───────────────────┘          │
│                                         │
├─────────────────────────────────────────┤
│ 文件: xxx.txt (5120 字节)  帧: 2  状态: 已就绪 │ ← 状态栏
└─────────────────────────────────────────┘
```

**交互流程**（参考 main.js 的 `importFile` + `nextFrame`）：
1. 点击"选择文件" → `fsys.dlg.open()` → 读取文件路径
2. 分块读取文件 → `cimbar_encode_feed()` 喂入 DLL
3. 循环 `cimbar_encode_next_frame()` → 获取 BGR 像素数据
4. 用 `gdip.bitmap` 从像素数据构造位图 → 绘制到 plus 控件
5. 用 `win.ui.timer` 按 FPS 控制帧切换速度
6. 所有帧显示完毕后从头循环（参考 `_fes->restart()` 行为）

### 3.2 解码标签页（参考 recv.html 效果）

```
┌─────────────────────────────────────────┐
│ 模式: [B ▼] [Auto]  [开始] [停止]      │ ← 工具栏
├─────────────────────────────────────────┤
│          ┌───────────────────┐          │
│          │                   │          │
│          │    摄像头预览     │          │
│          │   ┌┐          ┌┐  │          │ ← 四角锚点角标
│          │   └┘          └┘  │          │   (用 gdip 画线)
│          │                   │          │
│          │   ┌┐          ┌┐  │          │
│          │   └┘          └┘  │          │
│          └───────────────────┘          │
├─────────────────────────────────────────┤
│ 进度: [████████████░░░░░░░░] 60%       │ ← 解码进度条
│ 状态: 扫描中... 已收集 3 个 fountain 块   │ ← 状态栏
└─────────────────────────────────────────┘
```

**交互流程**（参考 recv.js 的 `on_frame` + `Sink.on_decode`）：
1. 启动摄像头 → DirectShow/Media Foundation 获取视频帧
2. 每帧 BGR 数据 → `cimbar_decode_scan()` 提取符号
3. 如有数据 → `cimbar_decode_fountain()` 累积解码
4. 进度条更新（fountain 解码器内部会提供进度信息）
5. 四角锚点颜色随状态变化：灰色(待机) → 黄色(检测到码) → 绿色(解码中) → 青色(完成)
6. 进度 100% → `cimbar_decode_read()` 读取文件 → 自动保存到桌面或指定目录

---

## 4. 关键技术点

### 4.1 编码帧显示（gdip 渲染 BGR 像素）

参照 [libcimbar_cli.cpp](file:///D:/Projects/aardio/libcimbar_cli/src/cli/libcimbar_cli.cpp#L125-L131) 的帧保存逻辑：

```aardio
// 获取帧数据（BGR 格式）
var frameSize = imgW * imgH * 3
var buf = ..raw.buffer(frameSize)
var ret = dll.cimbar_encode_next_frame(buf, frameSize)

// gdip.bitmap 可以从 raw buffer 构造
var bmp = gdip.bitmap(buf, imgW, imgH)  // BGR 格式
// 绘制到自定义 plus 控件
plusCtrl.setBitmap(bmp)
```

aardio 的 `gdip.bitmap` 接受 BGR 格式的原始像素数据，直接传入即可。

### 4.2 摄像头采集

aardio 访问摄像头的几种方式：
- **方案A**: `com.media` / `wmf.mediaCapture` — 使用 Media Foundation API（Win8+）
- **方案B**: `com.directShow` — 使用 DirectShow（兼容性更好）
- **方案C**: `win.videoCapture` 或 `webcam` 扩展库

推荐使用 DirectShow 方案，因为可以精确控制帧格式（BGR/RGB），避免 YUV 转换开销。基本流程：

```aardio
// 创建摄像头采集
import com.directShow
var cap = com.directShow.capture()
cap.setCallback(function(frameData, width, height, stride){
    // frameData 通常是 BGR 格式
    // 传给 decoder 处理
})
cap.start(deviceIndex)
```

### 4.3 四角锚点绘制

参考 recv.html 的 `_updateCrosshairPositions` 和 CSS 角标样式 ：
- 两个角标位于左上和右下，由 `#crosshair1`、`#crosshair2` 控制
- 位置根据摄像头画面和当前模式的 aspect ratio 动态计算
- 状态颜色：灰色(待机) → 黄色(`scanning_xhairs`) → 绿色(`active_xhairs`)

在 aardio 中用 `gdip.graphics` 在 plus 控件上绘制半透明角标线：

```aardio
var g = gdip.graphics(plusCtrl)
// 画四个角的 L 形线条
g.drawLine(pen, x1, y1, x1+30, y1)
g.drawLine(pen, x1, y1, x1, y1+30)
// ... 其他三个角
// 根据状态切换画笔颜色
```

### 4.4 解码进度

fountain 解码器的进度信息可通过类似 `Sink.get_report()` 的方式获取，或者直接跟踪 `cimbar_decode_fountain()` 的返回值：
- 返回 0 = 还需要更多数据
- 返回 >0 = 解码完成

如果需要更细粒度的进度，可以检查 fountain decoder 内部的 `block_count` vs `blocks_required` 比率。但这需要修改 DLL 或通过额外接口暴露。

**备选方案**：通过统计"成功提取数据的帧数 vs 总扫描帧数"来估计进度，或者直接在 DLL 中用 `cimbar_decode_get_progress()` 暴露进度百分比。

---

## 5. 模式选择

四种 cimbar 模式（参考 main.js `setMode`）：
| 值 | 名称 | 分辨率 | 容量/帧 | 说明 |
|----|------|--------|---------|------|
| 68 | B    | 1024×1024 | ~7500B | 默认标准 |
| 67 | Bm   | 1024×720  | ~5200B | 中尺寸 |
| 66 | Bu   | 736×637   | ~4000B | 紧凑 |
| 4  | 4C   | 1024×1024 | ~5600B | 4色兼容 |

解码模式额外支持 **Auto**（0）自动检测模式（参考 recv.js 每帧轮换模式尝试）。

---

## 6. 文件清单

| 文件 | 说明 |
|------|------|
| `gui/main.aardio` | 主窗口、UI 布局、事件处理 |
| `gui/libcimbar_dll.aardio` | DLL 函数声明封装 |
| `gui/encoder.aardio` | 编码逻辑：文件读取、DLL 调用、帧缓存、定时器轮播 |
| `gui/decoder.aardio` | 解码逻辑：摄像头采集、scan/fountain 调用、进度跟踪 |
| `gui/camera.aardio` | 摄像头抽象层（DirectShow 封装） |

---

## 8. 核心代码片段

### 8.1 `libcimbar_dll.aardio` — DLL 声明

```aardio
// libcimbar_dll.aardio — 封装 libcimbar.dll 的 13 个导出函数

namespace libcimbar {

    // 加载 DLL（从 dist_green 目录相对路径）
    var dllPath = ..io.localpath("../dist_green/libcimbar.dll")
    var dll = ..raw.loadDll(dllPath)
    
    // === 编码函数 (7个) ===
    encode_configure  = dll.api("cimbar_encode_configure", "int(int mode, int compression)")
    encode_init       = dll.api("cimbar_encode_init", "int(string filename, int fnsize, int encodeId)")
    encode_chunk_size = dll.api("cimbar_encode_chunk_size", "int()")
    encode_feed       = dll.api("cimbar_encode_feed", "int(pointer buffer, int size)")
    encode_next_frame = dll.api("cimbar_encode_next_frame", "int(pointer imgBuf, int imgSize)")
    encode_img_width  = dll.api("cimbar_encode_image_width", "int()")
    encode_img_height = dll.api("cimbar_encode_image_height", "int()")
    
    // === 解码函数 (6个) ===
    decode_configure  = dll.api("cimbar_decode_configure", "int(int mode)")
    decode_bufsize    = dll.api("cimbar_decode_bufsize", "int()")
    decode_scan       = dll.api("cimbar_decode_scan", "int(pointer imgdata, int w, int h, int format, pointer buf, int bufsize)")
    decode_fountain   = dll.api("cimbar_decode_fountain", "long(pointer buf, int size)")
    decode_filename   = dll.api("cimbar_decode_filename", "int(int fileId, pointer buf, int bufsize)")
    decode_read       = dll.api("cimbar_decode_read", "int(int fileId, pointer buf, int bufsize)")
    
    // === 辅助 ===
    alloc = function(size){ return ..raw.buffer(size) }
    
    _configured = false
    configure = function(mode, compression=16){
        if(!_configured){
            _configured = true
            // 汇编级静态初始化（仅需一次）
            encode_configure(mode, compression)
        }
    }
}
```

### 8.2 `encoder.aardio` — 编码逻辑

```aardio
// encoder.aardio — 文件编码为 cimbar 帧序列
import libcimbar_dll
import gdip

namespace encoder {

    var frames = {}          // 已生成的帧位图缓存 {gdip.bitmap, ...}
    var currentIndex = 1     // 当前正在显示的帧索引
    var encoding = false     // 是否正在编码
    
    // 编码一个文件，返回帧数量
    encodeFile = function(filePath, mode=68, compression=16){
        frames = {}
        libcimbar_dll.configure(mode, compression)
        
        // 1. 读取文件
        var f = ..io.file(filePath, "rb")
        var basename = ..io.splitpath(filePath).file + ..io.splitpath(filePath).ext
        var fileSize = f.size
        
        // 2. 初始化编码会话
        var ret = libcimbar_dll.encode_init(basename, #basename, -1)
        if(ret < 0) return ret
        
        // 3. 分块喂入数据
        var chunkSize = libcimbar_dll.encode_chunk_size()
        var buf = libcimbar_dll.alloc(chunkSize)
        
        while(true){
            var n = f.read(buf, chunkSize)
            if(n <= 0) break
            var r = libcimbar_dll.encode_feed(buf, n)
            if(r == 0) break
        }
        // 触发 fountain 流创建
        if(n < chunkSize) libcimbar_dll.encode_feed(null, 0)
        f.close()
        
        // 4. 生成所有帧
        var w = libcimbar_dll.encode_img_width()
        var h = libcimbar_dll.encode_img_height()
        var frameSize = w * h * 3
        var fb = libcimbar_dll.alloc(frameSize)
        
        while(true){
            var ret = libcimbar_dll.encode_next_frame(fb, frameSize)
            if(ret <= 0) break
            
            // 从 BGR 像素数据创建 gdip.bitmap
            var bmp = gdip.bitmap(fb, w, h, 24, null /*无调色板*/)
            ..table.push(frames, bmp)
        }
        
        encoding = true
        currentIndex = 1
        return #frames
    }
    
    // 获取当前帧（定时器每次调用）
    getCurrentBitmap = function(){
        if(#frames == 0) return null
        var bmp = frames[currentIndex]
        // 循环轮播
        currentIndex = (currentIndex % #frames) + 1
        return bmp
    }
    
    stop = function(){
        frames = {}
        encoding = false
    }
}
```

### 8.3 `decoder.aardio` — 解码逻辑

```aardio
// decoder.aardio — 摄像头帧实时解码
import libcimbar_dll
import gdip

namespace decoder {

    var progress = 0         // 解码进度 0-100
    var completed = false    // 是否已完成
    var decodedFileName = ""
    var decodedData = null   // raw.buffer
    
    // 配置并初始化
    init = function(mode=68){
        libcimbar_dll.configure(mode, 0)  // 编码配置（仅 assembly init）
        libcimbar_dll.decode_configure(mode)
        progress = 0
        completed = false
        decodedData = null
    }
    
    // 处理一帧摄像头画面
    // imgData: raw.buffer (BGR 像素)
    // 返回: 是否检测到 cimbar 码
    processFrame = function(imgData, width, height){
        if(completed) return false
        
        var scanBufSize = libcimbar_dll.decode_bufsize()
        var scanBuf = libcimbar_dll.alloc(scanBufSize)
        
        // 扫描提取符号（传入 3=BGR 格式）
        var bytes = libcimbar_dll.decode_scan(imgData, width, height, 3, scanBuf, scanBufSize)
        if(bytes <= 0) return false
        
        // fountain 累积解码
        var fileId = libcimbar_dll.decode_fountain(scanBuf, bytes)
        if(fileId <= 0){
            // 估算进度（简化：每次成功 scan 计数）
            progress = math.min(progress + 10, 99)
            return true
        }
        
        // 解码完成！
        progress = 100
        completed = true
        
        // 获取文件名
        var nameBuf = libcimbar_dll.alloc(256)
        var nameLen = libcimbar_dll.decode_filename(fileId, nameBuf, 256)
        if(nameLen > 0){
            decodedFileName = ..string.unpack(nameBuf, nameLen)
        } else {
            decodedFileName = "decoded_file"
        }
        
        // 读取文件数据
        var readBuf = libcimbar_dll.alloc(65536)
        var allData = {}
        while(true){
            var n = libcimbar_dll.decode_read(fileId, readBuf, 65536)
            if(n <= 0) break
            ..table.push(allData, ..string.unpack(readBuf, n))
        }
        decodedData = ..string.join(allData)
        return true
    }
    
    // 保存解码结果到文件
    saveDecoded = function(dirPath){
        if(!completed || !decodedData) return false
        var outPath = ..io.joinpath(dirPath, decodedFileName)
        ..string.save(outPath, decodedData)
        return true
    }
}
```

### 8.4 `camera.aardio` — 摄像头采集

```aardio
// camera.aardio — 摄像头采集（DirectShow）
namespace camera {

    var _cap = null
    var _callback = null
    var _running = false
    
    // 列出可用摄像头
    listDevices = function(){
        import com.directShow
        var devices = {}
        var devEnum = com.directShow.createDeviceEnumerator()
        // 枚举设备 Name, Path
        return devices
    }
    
    // 启动采集
    start = function(deviceIndex, frameCallback, width=1920, height=1080){
        _callback = frameCallback
        _running = true
        
        import com.directShow
        
        // 创建 VideoGrabber 类
        var grabber = class {
            ctor(width, height){
                this.width = width
                this.height = height
            }
            FrameReceived = function(frameData, pixelWidth, pixelHeight, stride){
                if(!_running) return
                // frameData 是 BGR 格式的 raw buffer
                _callback(frameData, pixelWidth, pixelHeight)
            }
        }
        
        _cap = com.directShow.capture(grabber(width, height), deviceIndex)
        _cap.start()
    }
    
    stop = function(){
        _running = false
        if(_cap) _cap.stop()
    }
}
```

### 8.5 `main.aardio` — 主窗口 UI 骨架

```aardio
// main.aardio — 主程序入口
import win.ui
import win.ui.tabs
import win.ui.statusbar
import gdip
import encoder
import decoder
import camera

// === 创建主窗口 ===
var mainForm = win.form(text="libcimbar GUI";right=960;bottom=700)
mainForm.add(
    // tabs 容器
    tabStrip = {cls="tabs";left=0;top=0;right=960;bottom=28;edge=1};
)

// === 编码标签页 ===
var pageEncode = mainForm.tabStrip.addPage("编码 (文件→帧)")
pageEncode.add(
    // 工具栏
    btnOpen = {cls="button";text="选择文件";left=12;top=40;right=100;bottom=65};
    cmbMode = {cls="combobox";left=110;top=40;right=180;bottom=65};
    txtCompression = {cls="edit";text="16";left=190;top=40;right=220;bottom=65;num=1};
    lblFps = {cls="static";text="FPS:";left=235;top=42;right=270;bottom=62};
    trackFps = {cls="trackbar";left=270;top=40;right=420;bottom=65;min=5;max=20;pos=15};
    lblFpsVal = {cls="static";text="15";left=425;top=42;right=450;bottom=62};
    // 帧显示区
    picFrame = {cls="plus";left=12;top=75;right=940;bottom=600;edge=1};
    // 状态栏
    status = {cls="statusbar";left=0;bottom=700;right=960;top=675;parts={300,600}};
)
// 设置模式下拉框选项
pageEncode.cmbMode.items = {"B(1024×1024)","Bm(1024×720)","Bu(736×637)","4C(兼容)"}
pageEncode.cmbMode.selIndex = 1

// --- 编码逻辑绑定 ---
pageEncode.btnOpen.oncommand = function(){
    import fsys.dlg
    var filePath = fsys.dlg.open("所有文件|*.*||", , mainForm.hwnd)
    if(!filePath) return
    
    var modeVals = {68, 67, 66, 4}
    var mode = modeVals[pageEncode.cmbMode.selIndex]
    var comp = tonumber(pageEncode.txtCompression.text) ? 16
    
    var n = encoder.encodeFile(filePath, mode, comp)
    if(n <= 0){
        mainForm.msgErr("编码失败")
        return
    }
    
    pageEncode.status.setText("文件: " + filePath, 1)
    pageEncode.status.setText("帧数: " + n, 2)
    
    // 启动帧轮播定时器
    startFrameTimer()
}

// FPS 滑块控制
pageEncode.trackFps.oncommand = function(){
    pageEncode.lblFpsVal.text = pageEncode.trackFps.pos
}

// 帧轮播定时器
var frameTimer = null
startFrameTimer = function(){
    if(frameTimer) frameTimer.disable()
    var interval = math.floor(1000 / pageEncode.trackFps.pos)
    frameTimer = mainForm.setInterval(function(){
        var bmp = encoder.getCurrentBitmap()
        if(bmp){
            // 缩放位图以适配控件大小，保持宽高比
            var rc = pageEncode.picFrame.getRect()
            var bmpScaled = gdip.bitmap(rc.width(), rc.height())
            var g2 = gdip.graphics(bmpScaled)
            g2.drawImage(bmp, 0, 0, rc.width(), rc.height())
            g2.destroy()
            pageEncode.picFrame.setBitmap(bmpScaled)
        }
    }, interval)
}

// === 解码标签页 ===
var pageDecode = mainForm.tabs.addPage("解码 (摄像头扫描)")
pageDecode.add(
    btnStart = {cls="button";text="开始";left=12;top=40;right=80;bottom=65};
    btnStop = {cls="button";text="停止";left=85;top=40;right=155;bottom=65};
    cmbDecMode = {cls="combobox";left=170;top=40;right=240;bottom=65};
    chkAuto = {cls="checkbox";text="Auto";left=250;top=42;right=300;bottom=60};
    // 预览 + 锚点
    picCamera = {cls="plus";left=12;top=75;right=940;bottom=580;edge=1};
    // 进度条
    progDecode = {cls="progress";left=12;top=590;right=940;bottom=610};
    txtStatus = {cls="static";text="就绪";left=12;top=615;right=940;bottom=640;autoResize=false};
)
pageDecode.cmbDecMode.items = {"B","Bm","Bu","4C"}
pageDecode.cmbDecMode.selIndex = 1

// --- 解码锚点绘制 ---
// 参考 recv.html _updateCrosshairPositions
pageDecode.picCamera.onDraw = function(graphics, rc){
    if(!cameraActive) return
    
    // 计算 cimbar 区域在控件中的位置（按 aspect ratio 居中）
    var modeAspects = {1.0, 1.413, 1.1516, 1.0} // B, Bm, Bu, 4C
    var aspect = modeAspects[pageDecode.cmbDecMode.selIndex]
    var cW = rc.width()
    var cH = rc.height()
    var imgW, imgH
    if(cW/cH > aspect){
        imgH = cH; imgW = cH * aspect
    } else {
        imgW = cW; imgH = cW / aspect
    }
    var ox = (cW - imgW) / 2
    var oy = (cH - imgH) / 2
    
    // 笔颜色随状态
    var color = MSVCRT_anchorState == "scanning" ? 0xFFFFFF00  // 黄色
              : MSVCRT_anchorState == "active" ? 0xFF00FF00    // 绿色
              : 0xFF888888  // 灰色（待机）
    var pen = gdip.pen(color, 3)
    var len = 30  // L 形线长
    
    // 四角 L 形线条
    graphics.drawLine(pen, ox, oy, ox + len, oy)
    graphics.drawLine(pen, ox, oy, ox, oy + len)
    
    graphics.drawLine(pen, ox + imgW, oy, ox + imgW - len, oy)
    graphics.drawLine(pen, ox + imgW, oy, ox + imgW, oy + len)
    
    graphics.drawLine(pen, ox, oy + imgH, ox + len, oy + imgH)
    graphics.drawLine(pen, ox, oy + imgH, ox, oy + imgH - len)
    
    graphics.drawLine(pen, ox + imgW, oy + imgH, ox + imgW - len, oy + imgH)
    graphics.drawLine(pen, ox + imgW, oy + imgH, ox + imgW, oy + imgH - len)
    
    pen.destroy()
}

// --- 解码逻辑绑定 ---
var cameraActive = false
var MSVCRT_anchorState = "idle"

pageDecode.btnStart.oncommand = function(){
    var modeVals = {68, 67, 66, 4}
    var mode = pageDecode.chkAuto.checked ? 0 : modeVals[pageDecode.cmbDecMode.selIndex]
    decoder.init(mode)
    cameraActive = true
    
    camera.start(0, function(imgData, w, h){
        // 预览：更新 plus 控件
        var bmp = gdip.bitmap(imgData, w, h, 24)
        pageDecode.picCamera.setBitmap(bmp)
        
        // 解码
        var hit = decoder.processFrame(imgData, w, h)
        if(hit){
            MSVCRT_anchorState = decoder.completed ? "active" : "scanning"
        } else {
            MSVCRT_anchorState = "idle"
        }
        
        // 更新进度
        pageDecode.progDecode.pos = decoder.progress
        if(decoder.completed){
            var saveDir = ..io.getSpecial(0x0000/*桌面*/)
            decoder.saveDecoded(saveDir)
            pageDecode.txtStatus.text = "解码完成！已保存: " + decoder.decodedFileName
            pageDecode.txtStatus.textcolor = 0xFF00AA00
        } else {
            pageDecode.txtStatus.text = "扫描中... 进度 " + decoder.progress + "%"
        }
    })
}

pageDecode.btnStop.oncommand = function(){
    camera.stop()
    cameraActive = false
    MSVCRT_anchorState = "idle"
}

// === 显示窗口 ===
mainForm.show()
win.loopMessage()
```

---

## 9. 实施步骤

1. **创建 `libcimbar_dll.aardio`** — 声明所有 13 个 DLL 导出函数
2. **创建 `camera.aardio`** — 封装摄像头采集（DirectShow），提供帧回调
3. **创建 `encoder.aardio`** — 实现编码全流程（文件→喂入→帧生成→位图缓存）
4. **创建 `decoder.aardio`** — 实现解码全流程（scan→fountain→进度→保存）
5. **创建 `main.aardio`** — 组装 UI：tabs 布局、工具栏、plus 控件、定时器、状态栏
6. **测试验证** — 编码→帧显示正常、解码→摄像头→识别→保存成功