# Rendering 与 Platform 规范

- **状态**：Architecture Baseline
- **Runtime language**：C++17
- **Renderer**：retained-mode native GPU
- **Skia**：不支持

本章定义 UI/render 隔离、场景事务、RenderPlan、`AeroRHI`、原生 GPU backend、文本/几何资源和宿主接口。

## 1. 产品定义

AeroGUI 是原生 GPU UI engine。这里的“GPU UI”表示：

- 产品级 rasterization、sampling、blending、clip、mask、offscreen、effect 和 composition 由 native GPU API 执行；
- XAML、Dependency Property、Binding、layout、text shaping、scene diff 和必要的 CPU tessellation 仍由 CPU 执行；
- 不使用 Skia 作为生产、reference 或 fallback renderer；
- 不要求每一种 path/text 算法从第一版起都用 compute shader；
- 优先保证跨平台一致、可缓存和可验证，再逐步增加 GPU analytic/compute path。

## 2. Retained rendering

UI 侧缓存 Visual 状态；`SceneBuilder` 只处理 dirty visuals，生成不可变 `RenderTransaction`。Render domain 缓存 render tree、drawing data 和资源状态，repaint 不回调 user control。

同一合同支持：

- single thread：build 后立即 apply/record；
- dual thread：transaction 进入宿主管理的 queue；
- multi-window/view：共享 device/resource cache，每个 view 有独立 scene/root/target；
- embedded mode：记录到宿主 command context，不自行 Present；
- standalone sample mode：platform adapter 可拥有 swapchain 和 present loop。

## 3. Visual 到场景

Visual 高层状态包括：

- transform、clip、opacity、visibility、z-order；
- effect/offscreen requirement；
- immutable drawing list；
- child order；
- layout/render bounds；
- cache hint 和 dirty generation。

`DrawingContext` 只记录 drawing command，不立即调用 GPU。一个 Visual 可以产生零个、一个或多个 render nodes。

基础 drawing primitive：

```text
FillGeometry
StrokeGeometry
DrawGlyphRun
DrawImage
DrawMesh
PushClip / PopClip
PushOpacity / PopOpacity
PushTransform / PopTransform
BeginEffect / EndEffect
```

高层 primitive 在 RenderPlan 阶段 lowering 为 backend-independent draw packet、pass 和 resource operation。

## 4. RenderTransaction

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
- 只使用 AeroBase owning type，不含 STL ABI；
- NodeId/ResourceId 包含 generation，避免 ABA；
- apply 保持 create/update/reparent/delete 的定义顺序；
- 连续 transaction 可安全合并；
- resource release 延迟到 GPU fence 安全点；
- transaction 可序列化用于 replay/test；
- render thread 不持有 `Object*`；
- malformed transaction 在 debug/test path 可验证，不破坏 render tree。

## 5. Render tree 与 RenderPlan

Render node 至少包含：

```text
stable ID / generation
parent + ordered children
transform / clip / opacity
bounds / dirty bounds
immutable drawing payload
resource handles
cache state
effect/offscreen state
```

Render tree 只接受 transaction 修改。Backend 不遍历 Visual tree。

每帧：

```text
Apply RenderTransaction
 -> Resolve dirty render nodes
 -> Update glyph/image/geometry caches
 -> Build clip/effect/offscreen graph
 -> Lower drawings to RenderPlan
 -> Batch without violating painter order
 -> Record native GPU commands
 -> Retire fenced resources
```

`RenderPlan` 是短生命周期的 backend-independent frame plan，不是长期序列化场景格式。

## 6. Painter order 与 batching

渲染遵循 painter order。Batch key 至少包括：

- pipeline/material；
- texture/sampler set；
- clip mode；
- blend mode；
- render target；
- sample count；
- color space；
- vertex/index format。

只允许在不改变可见结果时合并或重排。Clip、effect、opacity group、render target 和 read-after-write 边界必须阻止非法 crossing。

## 7. AeroRHI 边界

`AeroRHI` 是 AeroGUI 自有的最小 GPU abstraction，不是完整 3D engine。

```cpp
class IRhiDevice {
public:
    virtual RhiCaps GetCaps() const noexcept = 0;
    virtual Result<BufferHandle> CreateBuffer(const BufferDesc&) = 0;
    virtual Result<TextureHandle> CreateTexture(const TextureDesc&) = 0;
    virtual Result<SamplerHandle> CreateSampler(const SamplerDesc&) = 0;
    virtual Result<PipelineHandle> CreatePipeline(const PipelineDesc&) = 0;
    virtual Result<void> DestroyDeferred(RhiHandle, FenceValue) = 0;
};

class IRhiCommandContext {
public:
    virtual Result<void> BeginPass(const PassDesc&) = 0;
    virtual Result<void> BindPipeline(PipelineHandle) = 0;
    virtual Result<void> BindResources(Span<const ResourceBinding>) = 0;
    virtual Result<void> Draw(const DrawDesc&) = 0;
    virtual Result<void> EndPass() = 0;
};
```

具体 public ABI 可使用 C function table/opaque handle；以上仅表达 C++ source model。

### 7.1 RhiCaps

至少包括：

```text
maxTextureSize
maxRenderTargets
uniform/storage alignment
supported sample counts
texture formats and color spaces
stencil availability
compute support
storage buffer/image support
indirect draw support
bindless/descriptor indexing
framebuffer fetch/input attachment
tile-based GPU hint
timestamp query
external texture/import support
```

Renderer 根据 capability 选择 clip、mask、atlas、offscreen、tessellation 和 binding 策略，不把所有平台限制在最低共同能力。

## 8. Host-owned device mode

游戏引擎和主机集成默认使用 embedded/external mode：

- 宿主创建并拥有 device、queue、swapchain/render target；
- 宿主提供每帧 command context 或 recorder；
- AeroGUI 记录命令，不擅自 submit/present；
- 宿主提供 frame index、fence value、resource retirement callback；
- state transition/ownership transfer 合同显式；
- backend adapter 不改变宿主全局 GPU state；
- device loss 和 shutdown 由宿主通知；
- 多 UI view 可在同一 frame/queue 记录。

standalone sample MAY 使用 AeroGUI platform helper 创建窗口和 device，但该模式不进入核心 runtime 假设。

## 9. 正式 backend

### 9.1 D3D12

目标：Windows 和 Xbox/GDK adapter。

- 使用 explicit descriptor、resource state、command list 和 fence；
- Windows 与 Xbox 可共享高层 backend 设计，平台 SDK 细节隔离；
- GDK/console 类型不进入公开仓库通用 header。

### 9.2 Vulkan

目标：Windows、Linux、Android。

- 支持 host-provided instance/device/queue；
- allocator、pipeline cache、descriptor strategy 可由宿主配置；
- Android surface/swapchain 属于 platform adapter；
- validation layer 只用于开发配置。

### 9.3 Metal

目标：macOS、iOS、iPadOS、tvOS。

- 支持 host-provided `MTLDevice`、command buffer/encoder 和 target；
- 处理 tile-based GPU、memoryless attachment 和 drawable 生命周期；
- Objective-C 类型仅存在于 backend/platform bridge。

### 9.4 Console private

PlayStation、Nintendo 和其他受限 SDK backend 位于私有、访问受控仓库：

- 实现同一 AeroRHI contract；
- 不把 NDA header/type 泄漏到公开仓库；
- 使用平台离线 shader compiler；
- allocator、thread、filesystem 和 packaging 遵循 SDK 要求。

### 9.5 Null backend

`AeroRHI_Null` 验证：

- RenderPlan 合法性；
- resource lifetime/fence；
- pass nesting；
- pipeline/resource compatibility；
- transaction replay；
- headless CI。

它不产生最终像素。

## 10. Skia 禁止项

AeroGUI MUST NOT：

- link Skia；
- 暴露 Skia type；
- 使用 Skia 生成 golden image；
- 以 Skia 作为 fallback renderer；
- 让 geometry/text/render semantics 依赖 Skia 行为。

需要确定性 reference image 时，使用：

1. AeroGUI 自有受限 CPU reference rasterizer；或
2. 锁定 driver/hardware/backend 的 native GPU golden；或
3. geometry、glyph placement、RenderPlan 与 pixel test 的分层验证。

CPU reference rasterizer 只实现测试所需子集，不作为产品 backend。

## 11. sokol adapter

`sokol_gfx` 可通过 `AeroRHI_Sokol` 适配，但默认关闭。

允许：

- 早期 bring-up；
- sample/tool/WASM experiment；
- 其公开 backend 环境中的额外 RenderPlan 验证；
- 独立 sample 使用 `sokol_app`。

禁止：

- `sg_*` type 进入 AeroRHI/public API；
- sokol 成为 D3D12/Vulkan/Metal backend 的下层；
- runtime 使用 `sokol_app` 管理宿主主循环；
- 将 sokol 等同于 console support；
- 因 sokol 限制删除 AeroRHI 所需 capability；
- 序列化 sokol resource ID。

Adapter 必须能从 build 中完全移除，且移除后核心 target 依然构建。

## 12. Geometry pipeline

第一阶段：

```text
Path commands
 -> flatten/curve analysis
 -> fill/stroke tessellation on CPU
 -> cached vertex/index mesh
 -> native GPU raster/blend
```

后续 MAY 增加 analytic path、compute tessellation 或 GPU encoded outline，但必须：

- 与 CPU path 共享 geometry semantics；
- 有 deterministic fallback；
- 按 RhiCaps 选择；
- 不改变 hit-test/layout bounds；
- 有退化曲线、self-intersection、NaN、极大坐标测试。

`IGeometryTessellator` 可由自研实现或可选 libtess2 adapter 提供。libtess2 不能定义 public geometry model。

## 13. Clip、mask、offscreen 与 effect

Clip strategy 可选择：

- scissor；
- stencil；
- alpha mask texture；
- analytic clip；
- nested offscreen composition。

选择基于 geometry、nesting、target format、sample count、tile-based hint 和 backend caps。

Offscreen 用于：

- opacity group；
- effect/shadow/blur；
- complex mask；
- intermediate color-space conversion；
- cache-as-bitmap。

所有 transient target 进入显式 budget/pool，不能无界增长。

## 14. Shader 与 pipeline

- shader 源和生成物有显式 version；
- production package 使用离线编译；
- 不要求 runtime shader JIT；
- 每个 backend 使用其平台支持的 bytecode/source package；
- shader reflection 输出统一 binding metadata；
- pipeline cache key 稳定并包含 shader version、format、blend、sample 和 clip mode；
- debug shader hot reload 不能改变 release packaging contract；
- console shader binary 不提交公开仓库。

## 15. Text 与 image

文本边界：

```text
IUnicodeService
ITextBreaker
ITextShaper
IFontDatabase
IFontFace
IGlyphOutlineProvider
IGlyphRasterizer
IGlyphAtlas
```

图像边界：

```text
IImageCodec
IImageSource
ITextureUploader
```

MUST：

- layout 测量与最终 glyph placement 使用同一坐标合同；
- 锁定测试字体、DPI、locale、hinting 和 color space；
- HarfBuzz 可作为 optional shaper；
- FreeType 可作为 optional font/raster provider；
- HarfBuzz 不替代 bidi、line break 或 paragraph formatter；
- decoder/font 输入视为不可信；
- glyph/image cache 有显式 budget、eviction 和 trace；
- color glyph、emoji、variable font capability 显式声明；
- dynamic texture callback 的线程和生命周期必须文档化。

## 16. Animation

Animation value 作为 Dependency Property effective value overlay。系统逐步实现 Timeline、Clock、Animation<T>、Storyboard、easing、fill behavior 和 seek/pause。

composition-eligible animation MAY 下沉 render domain，但 UI 可观察值、completion 和 event 顺序必须与主 timeline 协调。时间由宿主 `ITimeSource` 注入，测试使用 deterministic clock。

## 17. Platform contracts

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
IGpuHost / IRhiHost
```

规则：

- Core 不暴露 native window/device handle；
- native handle 使用 opaque wrapper 或 backend-private bridge；
- platform callback marshal 到 Dispatcher；
- URI provider 默认不访问网络；
- 同一平台可有多个 host adapter；
- 宿主拥有 event loop、thread、GPU device 和 frame scheduling；
- AeroGUI 不创建隐藏永久线程；
- mobile suspend/resume、surface loss 和 orientation change 明确建模。

## 18. Input、IME 与 accessibility

Platform 层负责 raw keyboard、pointer/touch/gamepad、IME composition 和 accessibility API；Presentation 层负责语义和 route。

IME 至少支持：

- composition start/update/commit/cancel；
- caret rectangle；
- candidate window positioning；
- UTF-8/UTF-16 conversion tests；
- mobile virtual keyboard show/hide；
- suspend/resume state repair。

Accessibility tree 不直接暴露 render node，由 control semantics + logical/visual context 构建，并桥接 UI Automation、AT-SPI、Apple accessibility 或 console platform service。

## 19. Device loss 与资源生命周期

- every GPU handle 包含 generation；
- CPU source/cached representation 是否可重建必须记录；
- upload 完成前 resource 不进入 draw；
- release 等待 fence；
- device loss 后 invalid handle 必须 fail-fast；
- recoverable resource 通过 provider 重新创建；
- unrecoverable external resource 通知 host/application；
- shutdown 必须验证无 pending command callback 访问已销毁 runtime。

## 20. Rendering 验收

M4 至少满足：

- UI 与 render 不共享 user object pointer；
- transaction drop/merge/replay tests；
- RenderPlan validator 与 `AeroRHI_Null` tests；
- D3D12、Vulkan、Metal backend conformance；
- device loss/recreate tests；
- offscreen、clip、opacity、effect 和 painter-order golden tests；
- geometry/glyph/image cache budget tests；
- mobile suspend/surface recreate tests；
- 24h stress 无持续 resource growth；
- 同一 fixture 的 layout/glyph placement 跨 backend 一致；
- native GPU pixel diff 在锁定 tolerance 内；
- build tree 中不存在 Skia dependency；
- `AERO_WITH_SOKOL=OFF` 时所有核心与正式 backend 可构建。