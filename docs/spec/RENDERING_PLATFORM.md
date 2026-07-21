# Rendering 与 Platform 规范

本章定义 UI/render 隔离、场景事务、render backend 和宿主接口。

## 1. Retained rendering

UI 侧缓存 Visual 状态；SceneBuilder 只处理 dirty visuals，生成不可变 `RenderTransaction`。Render domain 缓存 render tree 和 drawing data，repaint 不回调用户控件。

同一合同支持：

- 单线程：build 后立即 apply/render；
- 双线程：transaction 进入宿主管理的队列；
- 多窗口：共享 device/resource cache，但每个 view 有独立 scene/root。

## 2. Visual 到场景

Visual 高层状态包括：

- transform、clip、opacity、visibility、z-order；
- effect/offscreen requirement；
- content drawing list；
- child order；
- layout/render bounds。

`DrawingContext` 记录不可变 drawing commands，不立即调用 GPU。

一个 Visual 可以产生零个、一个或多个 render nodes。

## 3. RenderTransaction

```cpp
struct RenderTransaction {
    uint64_t epoch;
    Vector<CreateNode> created;
    Vector<UpdateNode> updated;
    Vector<ReparentNode> reparented;
    Vector<DeleteNode> deleted;
    Vector<ResourceUpload> uploads;
    Vector<ResourceRelease> releases;
};
```

MUST：

- 可跨线程 move；
- NodeId/ResourceId 含 generation，避免 ABA；
- apply 保持 create/update/reparent/delete 顺序；
- 连续 transaction 可安全合并；
- resource release 延迟到 GPU fence 安全点；
- transaction 可序列化用于测试；
- render thread 不持有 `Object*`。

## 4. Render tree

Node 至少包含 stable ID、parent/child order、transform、clip、opacity、bounds、draw primitives、resource handles 和 dirty generation。

Render tree 只接受 transaction 修改。Backend 不直接遍历 Visual tree。

## 5. Frame passes

```text
Apply transactions
 -> Update glyph/image/geometry caches
 -> Build clip/effect/offscreen plan
 -> RenderOffscreen
 -> Bind main target
 -> RenderOnscreen
 -> Submit/Present
 -> Retire fenced resources
```

Offscreen pass 用于 opacity groups、effects、shadows 和中间 surface。

## 6. Painter order 与 batching

渲染遵循 painter order。Batch key 至少包括 pipeline/material、texture set、clip mode、blend mode、target、sample count 和 color space。

只允许在不改变可见结果时合并或重排。Clip/effect 边界必须阻止非法 batch crossing。

## 7. Backend contract

```cpp
class IRenderBackend {
public:
    virtual Result<void> Initialize(const RenderDeviceDesc&) = 0;
    virtual Result<FrameContext> BeginFrame(const SurfaceDesc&) = 0;
    virtual Result<void> Render(const RenderPlan&, FrameContext&) = 0;
    virtual Result<void> EndFrame(FrameContext&) = 0;
};
```

建议先用 Skia reference backend 验证语义和跨平台 bring-up，再增加 D3D12/Vulkan/Metal backend。Skia 是否为默认依赖仍需 ADR；Core/Presentation 永远不能依赖 Skia 类型。

Backend MUST 处理 device loss，并返回可恢复或 terminal 状态。

## 8. Animation

Animation value 作为 DP effective value 的 overlay。系统逐步实现 Timeline、Clock、Animation<T>、Storyboard、easing、fill behavior 和 seek/pause。

完全 composition-eligible 的动画 MAY 下沉 render domain，但 UI 可观察值、completion 和 event 顺序必须与主 timeline 协调。时间由宿主 `ITimeSource` 注入，测试可使用 deterministic clock。

## 9. Text 与 image

文本边界：

```text
ITextService
IFontCollection
ITextShaper
IGlyphRasterizer
```

图像边界：

```text
IImageCodec
IImageSource
ITextureUploader
```

MUST：

- layout 测量和最终 glyph placement 使用同一坐标合同；
- 锁定测试字体、DPI 和 locale；
- Unicode segmentation/bidi、shaping、rasterization 可使用成熟库；
- decoder 视为不可信输入边界；
- Dynamic texture callback 只能在文档声明的线程执行；
- image/font cache 具有显式预算和 eviction trace。

## 10. Platform contracts

至少定义：

```text
IApplicationHost
IWindowHost
IEventSource
ICursorService
IClipboard
ITextInputService
IFileSystem
IAssetProvider
ITimeSource
IDpiService
IAccessibilityBridge
```

规则：

- Core 不暴露 native window handle 类型；
- native handle 使用 opaque wrapper；
- platform callback marshal 到 Dispatcher；
- URI provider 默认不访问网络；
- 同一平台可有多个 host adapter；
- 宿主拥有 event loop、thread 和 GPU device；
- AeroGUI 不创建隐藏永久线程。

## 11. Input/IME 与 accessibility

Platform 层负责原始键盘、pointer、IME composition 和 accessibility API；Presentation 层负责语义和路由。

IME 至少支持：

- composition start/update/commit/cancel；
- caret rectangle；
- candidate window positioning；
- UTF-8/UTF-16 边界转换测试。

Accessibility tree 不直接暴露 render nodes，而由 control semantics + logical/visual context 构建，并桥接 UI Automation、AT-SPI 或 macOS accessibility。

## 12. Rendering 验收

M4 至少满足：

- UI 与 render 无共享用户对象指针；
- transaction drop/merge/replay 测试；
- device loss/recreate 测试；
- offscreen、clip、opacity 和 painter-order golden tests；
- 24h stress 无持续资源增长；
- Windows/Linux 同一 fixture 布局一致；
- reference/GPU backend 像素差在锁定容差内。
