# Rendering 与 Platform 规范

- **状态**：Architecture Baseline
- **Runtime language**：C++17
- **Renderer**：retained-mode native GPU / WebGL 2
- **Skia**：不支持

本章定义 UI/render 隔离、场景事务、RenderPlan、`AeroRHI`、原生与兼容 GPU backend、文本/几何资源和宿主接口。D3D11、OpenGL、GLES、GLX/EGL/WGL 与 WebGL 2 的详细合同见 [`COMPATIBILITY_BACKENDS.md`](COMPATIBILITY_BACKENDS.md)。

## 1. 产品定义

AeroGUI 是原生 GPU UI engine。这里的“GPU UI”表示：

- 产品级 rasterization、sampling、blending、clip、mask、offscreen、effect 和 composition 由 native GPU API 或 WebGL 2 执行；
- XAML、Dependency Property、Binding、layout、text shaping、scene diff 和必要的 CPU tessellation 仍由 CPU/WASM 执行；
- 不使用 Skia 作为生产、reference 或 fallback renderer；
- 不要求每一种 path/text 算法从第一版起都用 compute shader；
- strategic 与 compatibility backend 使用同一 RenderPlan 合同；
- 优先保证跨平台一致、可缓存和可验证，再逐步增加 analytic/compute path。

## 2. Backend 等级

### Strategic native

```text
AeroRHI_D3D12
AeroRHI_Vulkan
AeroRHI_Metal
AeroRHI_ConsolePrivate
```

### Compatibility

```text
AeroRHI_D3D11
AeroRHI_OpenGL33
AeroRHI_GLES30
AeroRHI_WebGL2
```

### Validation / optional

```text
AeroRHI_Null
AeroRHI_Sokol  # optional, default OFF
```

Compatibility backend 是正式支持路径，不是临时 prototype；但高级优化可通过 capability manifest 降级。WebGL 1、OpenGL fixed-function/compatibility profile 不进入 v1。

## 3. Retained rendering

UI 侧缓存 Visual 状态；`SceneBuilder` 只处理 dirty visuals，生成不可变 `RenderTransaction`。Render domain 缓存 render tree、drawing data 和资源状态，repaint 不回调 user control。

同一合同支持：

- single thread：build 后立即 apply/record；
- dual thread：transaction 进入宿主管理的 queue；
- multi-window/view：共享 device/resource cache，每个 view 有独立 scene/root/target；
- embedded mode：记录到宿主 command context 或 current GL context，不自行 Present；
- standalone sample mode：platform adapter 可拥有 swapchain/surface 和 present loop；
- browser mode：由 `requestAnimationFrame`/host callback 推进，不阻塞 event loop。

## 4. Visual 到场景

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

## 5. RenderTransaction

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
- resource release 延迟到 GPU/context 安全点；
- transaction 可序列化用于 replay/test；
- render thread 不持有 `Object*`；
- malformed transaction 在 debug/test path 可验证，不破坏 render tree。

## 6. Render tree 与 RenderPlan

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

当前 source model 固定为一条调用链，不允许 backend 自行实现第二套 UI lowering：

```text
Presentation::RenderManager::Commit
 -> immutable Presentation::RenderPlan
 -> SurfaceSession::AcquireFrame（导入 RHI frame target）
 -> Render::Renderer::Record(plan, frame.target)
 -> Rhi::GraphicsCommandBuffer
 -> Rhi::RhiDevice::Submit
 -> Rhi::IGraphicsBackend::Submit
 -> SurfaceSession::Present / Discard（按 fence 回收 frame target）
```

`RenderManager` 不持有 backend；`Renderer` 不直接持有 backend，只依赖 `RhiDevice`；`SurfaceSession` 直接协调 platform target 的导入、present/discard 与延迟回收，不再叠加 platform presenter 或 surface queue。D3D11、OpenGL、GLES、WebGL2、hosted 与可选 sokol adapter 都消费同一命令流。

每帧：

```text
Apply RenderTransaction
 -> Resolve dirty render nodes
 -> Update glyph/image/geometry caches
 -> Build clip/effect/offscreen graph
 -> Lower drawings to RenderPlan
 -> Batch without violating painter order
 -> Record GPU/WebGL commands
 -> Retire safe resources
```

`RenderPlan` 是短生命周期的 backend-independent frame plan，不是长期序列化场景格式。

## 7. Compatibility lowering

RenderPlan 不得假设 compute、storage buffer、bindless、indirect draw 或 persistent mapping。兼容 backend 可执行：

- descriptor table → texture unit/resource slot binding；
- storage data → UBO/constant buffer/texture/expanded vertex data；
- compute tessellation → CPU mesh cache；
- bindless atlas → texture-set batch split；
- advanced mask → stencil/alpha-mask offscreen；
- indirect draw → explicit draw loop。

降级只允许影响性能和 capability，不得改变 WPF 可观察视觉语义。

## 8. Painter order 与 batching

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

## 9. AeroRHI 边界

`AeroRHI` 是 AeroGUI 自有的最小 GPU abstraction，不是完整 3D engine。

```cpp
class RhiDevice final {
public:
    Result<BufferHandle> CreateBuffer(const BufferDesc&) noexcept;
    Result<TextureHandle> CreateTexture(const TextureDesc&) noexcept;
    Result<SamplerHandle> CreateSampler(const SamplerDesc&) noexcept;
    Result<PipelineHandle> CreatePipeline(const PipelineDesc&) noexcept;
    Result<RenderTargetHandle> ImportRenderTarget(
        const ExternalRenderTargetDesc&) noexcept;
    Result<void> DestroyResource(RhiHandle, FenceValue retireAfter) noexcept;
    Result<FenceValue> Submit(const GraphicsCommandBuffer&) noexcept;
};

class IGraphicsBackend : public IRhiBackend {
public:
    virtual Result<void> ImportRenderTarget(
        RenderTargetHandle, const ExternalRenderTargetDesc&) noexcept = 0;
    virtual Result<void> Submit(
        const GraphicsCommandBuffer&, FenceValue) noexcept = 0;
};
```

`GraphicsCommandEncoder` 是值类型 recorder，不是第二个 backend facade。资源创建、外部 frame target 导入、配置 rollback、全局 fence 分配和 deferred destruction 都由 `RhiDevice` 收口；`SurfaceSession` 是 acquire/present 生命周期的唯一协调者。具体 binary ABI 可使用 C function table/opaque handle。GL/WebGL backend 可以内部维护 state cache，但上层合同不暴露 GL global state。

### 9.1 RhiCaps

至少包括：

```text
backend tier / API version / shader profile
maxTextureSize / maxRenderTargets
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
context/device loss recovery
```

Renderer 根据 capability 选择 clip、mask、atlas、offscreen、tessellation 和 binding 策略，不把所有平台限制在最低共同能力。

## 10. Host-owned device/context mode

游戏引擎和主机集成默认使用 embedded/external mode：

- 宿主创建并拥有 device、queue、GL context、swapchain/render target；
- 宿主提供每帧 command context、current GL context 或 recorder；
- AeroGUI 记录命令，不擅自 submit/present；
- 宿主提供 frame index、fence/safe-retire policy；
- state transition/ownership transfer 合同显式；
- backend adapter 不改变未声明的宿主 GPU/GL state；
- device/context loss 和 shutdown 由宿主通知；
- 多 UI view 可在同一 frame/queue/context policy 下记录。

standalone sample MAY 使用 AeroGUI platform helper 创建窗口、surface 和 device/context，但该模式不进入核心 runtime 假设。

## 11. Strategic backend

### 11.1 D3D12

目标：Windows 和 Xbox/GDK adapter。使用 explicit descriptor、resource state、command list 和 fence；GDK/console 类型不进入公开通用 header。

### 11.2 Vulkan

目标：Windows、Linux、Android。支持 host-provided instance/device/queue；surface/swapchain 属于 Platform adapter；validation layer 只用于开发配置。

### 11.3 Metal

目标：macOS、iOS、iPadOS、tvOS。支持 host-provided device/command buffer/encoder/target；处理 tile-based GPU 和 drawable 生命周期；Objective-C 类型仅存在于 backend/platform bridge。

### 11.4 Console private

受限 SDK backend 位于访问受控仓库，使用同一 AeroRHI contract、平台离线 shader compiler 和 SDK allocator/thread/filesystem policy。

## 12. D3D11 backend

`AeroRHI_D3D11`：

- feature level 10_0 minimum，11_0/11_1 preferred；
- v1 不支持 9_x baseline；
- 基础路径只要求 vertex/pixel shader；
- compute/UAV/tiled resources 通过 optional feature query；
- 支持 owned 与 borrowed device/context；
- embedded mode 定义 state preservation 或 host reset policy；
- HLSL 离线编译为 DXBC；
- 必须清理 RT/SRV hazard，不能依赖 driver/debug layer 自动修复。

## 13. OpenGL/GLES backend 与 Platform context

### 13.1 Desktop GL

`AeroRHI_OpenGL33`：

- OpenGL 3.3 Core + GLSL 3.30；
- 不使用 fixed-function/compatibility API；
- 使用 function table、state cache 和 default VAO；
- extension 只作为 optional optimization；
- context 必须在 current thread 使用；
- embedded mode 选择 documented-state restore 或 host-reset contract。

### 13.2 GLES

`AeroRHI_GLES30`：

- OpenGL ES 3.0 + GLSL ES 3.00；
- 主要用于 Android/EGL、嵌入式和 Linux/EGL；
- 与 WebGL 2 共享 canonical shader feature subset；
- GLES 3.1/3.2 是可选增强；
- mobile surface loss/pause/resume 可恢复。

### 13.3 GLX/EGL/WGL

```text
AeroPlatform_GLX  Linux/X11 + GL3.3
AeroPlatform_EGL  Android/Wayland/headless + GLES/OpenGL
AeroPlatform_WGL  Windows + GL3.3
```

- GLX baseline 为 1.4；通过运行时查询 `GLX_ARB_create_context` 创建 3.3 core context；
- GLX 负责 FBConfig、drawable、make-current、swap interval、resize 和 swap；
- Wayland 不使用 GLX；
- WGL 使用 bootstrap context 加载 modern context extension；
- owned/borrowed display/context/surface 分开建模；
- host-provided current context 可跳过这些 adapter。

## 14. WebGL 2 backend

`AeroRHI_WebGL2`：

- C++17 → WebAssembly；
- WebGL 2 + GLSL ES 3.00；
- WebGL 1 不支持；
- HTMLCanvasElement/OffscreenCanvas；
- baseline 不要求 compute、SSBO、bindless、persistent mapping 或 blocking wait；
- `requestAnimationFrame`/host callback 驱动 frame；
- 主渲染线程为首个正式 baseline；Worker/OffscreenCanvas 是 optional capability；
- JS bridge 使用受控 handle table，不把 JS object pointer 泄漏到公共 C++ ABI；
- context loss 后所有 WebGL object 和 extension 失效；
- restore 后重新 query caps/extensions，并重建 shader、buffer、texture、FBO、VAO、sampler 和 atlas；
- 必须处理 `webglcontextlost`/`webglcontextrestored`；
- 测试使用 `WEBGL_lose_context`；
- sync/resource retirement 使用后续 frame polling 或 N-frame delay，不 busy-wait 当前 JavaScript task。

## 15. Null backend

`AeroRHI_Null` 验证：

- RenderPlan 合法性；
- resource lifetime/fence/generation；
- pass nesting；
- pipeline/resource compatibility；
- transaction replay；
- headless CI。

它不产生最终像素。

## 16. Skia 禁止项

AeroGUI MUST NOT：

- link Skia；
- 暴露 Skia type；
- 使用 Skia 生成 golden image；
- 以 Skia 作为 fallback renderer；
- 让 geometry/text/render semantics 依赖 Skia 行为。

需要 reference image 时，使用自有受限 CPU rasterizer、结构快照、锁定 native/WebGL backend image 或多 backend consensus。

## 17. sokol adapter

`sokol_gfx` 可通过 `AeroRHI_Sokol` 适配，但默认关闭。其公开覆盖 D3D11、GL3.3、GLES3/WebGL2、Metal 和 WebGPU，适合：

- 早期 bring-up；
- sample/tool/WASM experiment；
- 额外 RenderPlan 验证；
- 与第一方兼容 backend 做差异测试。

禁止：

- `sg_*` type 进入 AeroRHI/public API；
- sokol 成为 strategic backend 的下层；
- 用 sokol 替代第一方 D3D11/GL/GLES/WebGL2 长期合同；
- runtime 使用 `sokol_app` 管理嵌入式主循环；
- 将 sokol 等同于 console support；
- 因 sokol 限制删除 AeroRHI capability。

`AERO_WITH_SOKOL=OFF` 时所有正式 backend 必须独立构建。

## 18. Geometry pipeline

第一阶段：

```text
Path commands
 -> flatten/curve analysis
 -> fill/stroke tessellation on CPU
 -> cached vertex/index mesh
 -> GPU/WebGL raster/blend
```

后续 MAY 增加 analytic path、compute tessellation 或 GPU encoded outline，但必须有 deterministic fallback、按 caps 选择、不改变 hit-test/layout bounds，并覆盖退化曲线/self-intersection/NaN/极大坐标。

`IGeometryTessellator` 可由自研实现或可选 libtess2 adapter 提供。libtess2 不能定义 public geometry model。

## 19. Clip、mask、offscreen 与 effect

Clip strategy 可选择：

- scissor；
- stencil；
- alpha mask texture；
- analytic clip；
- nested offscreen composition。

选择基于 geometry、nesting、target format、sample count、tile-based hint 和 backend caps。

Offscreen 用于 opacity group、effect/shadow/blur、complex mask、color-space conversion 和 cache-as-bitmap。所有 transient target 进入显式 budget/pool。

## 20. Shader 与 pipeline

### Native binary backends

- D3D11: offline DXBC；
- D3D12: offline DXIL；
- Vulkan: offline SPIR-V；
- Metal: packaged MSL/metallib；
- Console: offline private package。

### Source-consuming APIs

- OpenGL 3.3: generated/validated GLSL 330 source，driver compile/link；
- GLES 3.0: generated/validated GLSL ES 300 source，driver compile/link；
- WebGL 2: embedded generated/validated GLSL ES 300 source，browser compile/link。

GL/GLES/WebGL 是“不运行时编译 shader binary”规则的显式 API 例外。离线工具仍必须完成 canonical-source conversion、syntax validation、reflection、binding layout、minification、hash 和 versioning。WebGL compile/link error 转为结构化 diagnostic。

## 21. Text 与 image

文本边界：

```text
IUnicodeService
ITextBreaker
FontManager
IFontProvider
ITextShaper
IGlyphRasterizer
IGlyphAtlas
```

`AeroText` 公共合同层只依赖 `AeroBase`。`IFontProvider`、`ITextShaper` 和
`IGlyphRasterizer` 通过同一个 `FontProviderIdentity { id, version }`
注册到 `FontManager`；face handle 同时携带 provider identity、face ID 和
generation，因此 provider 重载、缓存失效或关闭后的陈旧 face 会被明确拒绝。
Manager 不拥有 provider 对象，宿主必须保证注册对象存活到 unregister 或
shutdown。所有字体路径、语言和 shaping 输入使用 UTF-8，所有输出容器使用宿主
可注入的 `AeroBase` allocator。

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
- WebGL atlas 在 context restore 后可从 CPU/source cache 重建。

## 22. Animation

Animation value 作为 Dependency Property effective value overlay。composition-eligible animation MAY 下沉 render domain，但 UI 可观察值、completion 和 event 顺序必须与主 timeline 协调。时间由宿主 `ITimeSource` 注入；Web background throttling 必须处理时间跳变。

## 23. Platform contracts

至少定义：

```text
IApplicationHost
IWindowHost / IWebCanvasHost
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
IGlContextHost
```

规则：

- Core 不暴露 native window/device/context handle；
- native handle 使用 opaque wrapper 或 backend-private bridge；
- platform/browser callback marshal 到 Dispatcher；
- URI provider 默认不访问网络；Web host fetch 必须由 policy 显式允许；
- 同一平台可有多个 host adapter；
- 宿主拥有 event loop、thread、GPU device/context 和 frame scheduling；
- AeroGUI 不创建隐藏永久线程；
- mobile suspend/resume、surface loss、orientation change 和 Web context loss 明确建模。

## 24. Input、IME 与 accessibility

Platform 层负责 raw keyboard、pointer/touch/gamepad、IME composition 和 accessibility API；Presentation 层负责语义和 route。

IME 至少支持 composition start/update/commit/cancel、caret rectangle、candidate positioning、UTF-8/UTF-16 conversion、mobile virtual keyboard 和 browser composition events。

Accessibility tree 不直接暴露 render node，由 control semantics + logical/visual context 构建，并桥接 UI Automation、AT-SPI、Apple accessibility、browser DOM/accessibility bridge 或 console service。

## 25. Device/context loss 与资源生命周期

- every GPU/WebGL handle 包含 generation；
- CPU source/cached representation 是否可重建必须记录；
- upload 完成前 resource 不进入 draw；
- native release 等待 fence；GL/WebGL 使用安全 frame/context policy；
- device/context loss 后 invalid handle fail-fast；
- recoverable resource 通过 provider 重新创建；
- unrecoverable external resource 通知 host/application；
- WebGL restore 重新获取所有 extension object；
- shutdown 验证无 pending callback 访问已销毁 runtime。

## 26. Rendering 验收

M4 至少满足：

- UI 与 render 不共享 user object pointer；
- transaction drop/merge/replay tests；
- RenderPlan validator 与 `AeroRHI_Null` tests；
- D3D12、Vulkan、Metal strategic backend conformance；
- D3D11 FL10_0/11_0 compatibility conformance；
- GL3.3 on GLX/WGL；
- GLES3 on EGL/Android；
- WebGL2 browser tests；
- device/context loss/recreate tests；
- `WEBGL_lose_context` repeated recovery tests；
- GL embedded state leak/restore tests；
- offscreen、clip、opacity、effect 和 painter-order golden tests；
- geometry/glyph/image cache budget tests；
- mobile suspend/surface recreate tests；
- 24h stress 无持续 resource growth；
- 同一 fixture 的 layout/glyph placement 跨 backend 一致；
- pixel diff 在锁定 tolerance 内；
- build tree 中不存在 Skia dependency；
- `AERO_WITH_SOKOL=OFF` 时所有正式 backend 可构建；
- WebGL 2 不静默降级到 WebGL 1。
