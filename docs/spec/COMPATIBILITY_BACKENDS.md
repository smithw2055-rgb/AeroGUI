# Compatibility Graphics Backends 规范

- **状态**：Architecture Baseline
- **语言**：C++17
- **覆盖**：D3D11、OpenGL 3.3、OpenGL ES 3.0、GLX/EGL/WGL、WebGL 2
- **不覆盖**：WebGL 1、OpenGL compatibility/fixed-function pipeline

本章定义 AeroGUI 的正式兼容图形后端。兼容后端与 D3D12/Vulkan/Metal 使用同一 `RenderTransaction`、retained render tree、`RenderPlan` 和 `AeroRHI` 上层合同，但允许根据能力选择更保守的执行策略。

## 1. 支持等级

| 等级 | Backend | 目标 |
| --- | --- | --- |
| Strategic | D3D12 | Windows、Xbox adapter、现代游戏引擎 |
| Strategic | Vulkan | Windows、Linux、Android |
| Strategic | Metal | macOS、iOS、iPadOS、tvOS |
| Strategic | ConsolePrivate | 受限游戏主机 SDK |
| Compatibility | D3D11 | Windows 桌面、UWP/旧引擎 adapter、广泛硬件 |
| Compatibility | OpenGL 3.3 Core | Windows/WGL、Linux/X11/GLX |
| Compatibility | OpenGL ES 3.0 | Android/EGL、嵌入式、Linux/EGL |
| Compatibility | WebGL 2 | Browser + WebAssembly |
| Validation | Null | Headless contract/resource validation |
| Optional | sokol | Bring-up、sample、tool、额外验证 |

“Compatibility” 不等于实验性。它们是正式 backend，必须有明确的 capability、CI 和发布状态；但不要求实现 strategic backend 的全部高级优化。

## 2. 共同最低渲染能力

兼容后端必须支持 AeroGUI 的基础 UI path：

- indexed triangle rendering；
- vertex + fragment/pixel shader；
- RGBA8 render target 和 texture；
- normalized/scissored viewport；
- alpha blending；
- stencil 或 alpha-mask clip 中至少一种；
- depth-stencil format 或不使用 depth 的 ordered UI path；
- dynamic vertex/index/uniform upload；
- texture atlas；
- offscreen render-to-texture；
- sampler state；
- framebuffer/readback 用于测试；
- context/device loss notification；
- generation-safe resource handles。

不作为 compatibility baseline：

- compute shader；
- storage buffer/image；
- bindless descriptor；
- indirect draw；
- persistent mapped buffer；
- timeline semaphore；
- programmable sample positions；
- mesh/task shader。

这些能力通过 `RhiCaps` 选择性启用。

## 3. RenderPlan 降级策略

RenderPlan 不能假设所有后端具备显式 barrier、descriptor heap 或 compute。Lowering 需要提供：

```text
DrawPacket
  pipeline key
  vertex/index ranges
  uniform block
  texture/sampler bindings
  clip state
  target/pass id
  blend/color state
```

兼容后端 MAY：

- 将 descriptor table 降级为 texture-unit binding；
- 将 storage data 降级为 uniform buffer、texture 或 CPU-expanded vertex data；
- 将 compute tessellation 降级为 CPU mesh cache；
- 将 bindless atlas 降级为多批次 texture set；
- 将 advanced mask 降级为 stencil 或 alpha-mask offscreen；
- 将 indirect draw 降级为显式 draw loop。

降级不得改变 WPF 可观察布局、event、property 或视觉语义；只允许影响性能和 capability manifest。

## 4. D3D11 backend

Target：`AeroRHI_D3D11`

### 4.1 最低要求

- Direct3D 11 API；
- `D3D_FEATURE_LEVEL_10_0` minimum；
- `D3D_FEATURE_LEVEL_11_0` preferred；
- Shader Model 4.x minimum / 5.0 preferred；
- feature level 9_x 不进入 v1。

D3D11 API 的 feature level 是功能集合，不等于性能等级。Backend 创建或接收 device 后必须记录实际 feature level，并查询 optional feature，而不是按操作系统或 GPU 名称推断。

### 4.2 Device 模式

```text
OwnedDevice
  AeroGUI creates ID3D11Device + context

BorrowedDevice
  Host supplies ID3D11Device and immediate context contract
```

Borrowed mode：

- host/device/context lifetime 显式；
- AeroGUI 不调用 Present，除非 standalone surface adapter 明确授权；
- multithread protection 是否启用由 integration contract 决定；
- deferred context MAY 用于并行 command generation，但不是 baseline；
- resources 不在不同 device 间共享；
- state restore policy 可选 `PreserveRequiredState` 或 `HostResetsState`。

### 4.3 State cache

D3D11 是隐式状态 API。Backend 必须缓存并最小化：

- input layout；
- vertex/index buffer；
- VS/PS 和 constant buffer；
- SRV/sampler；
- rasterizer/blend/depth-stencil state；
- viewport/scissor；
- render target/depth view。

资源写后读和 RT/SRV hazard 必须显式 unbind，不能依赖 debug layer 自动修复。

### 4.4 Shader

- HLSL 离线编译为 DXBC；
- FL10_0 与 FL11_0 profile 分开生成；
- reflection metadata 转换为 Aero binding layout；
- release 不从磁盘动态编译 HLSL；
- shader cache key 包含 source revision、profile、defines 和 binding schema。

## 5. Desktop OpenGL 3.3 Core

Target：`AeroRHI_OpenGL33`

### 5.1 Baseline

- OpenGL 3.3 Core Profile；
- GLSL 3.30；
- Vertex Array Object；
- framebuffer object；
- sampler object；
- uniform buffer；
- instancing MAY 用于 batching，但 renderer 必须有非复杂 fallback。

禁止：

- compatibility profile 依赖；
- fixed-function matrix/light/texture environment；
- immediate mode；
- display list；
- client-side vertex array；
- `GL_QUADS`；
- extension-only baseline feature。

### 5.2 Function loading

OpenGL function pointer 由 platform/context adapter 或 host loader 提供。`AeroRHI_OpenGL33` 不绑定特定 loader 库的 public API。

入口初始化时必须：

- 验证 version/profile；
- 查询 limits/extensions；
- 建立 `RhiCaps`；
- 安装可选 debug callback；
- 建立 default VAO/state cache；
- 验证 required framebuffer/format support。

### 5.3 Embedded state contract

OpenGL 全局/context state 容易污染宿主。支持两种模式：

```text
ExclusiveContext
  AeroGUI owns all GL state during context lifetime

EmbeddedContext
  Host and AeroGUI share current context
```

EmbeddedContext 必须选择：

- `AeroRestoresDocumentedState`：保存并恢复文档列出的最小 state set；或
- `HostResetsStateAfterAero`：AeroGUI 不做昂贵完整查询，宿主在调用后恢复其 pipeline。

禁止尝试通过大量 `glGet*` 保存“全部状态”作为默认路径。state ownership 必须可测且性能可预测。

## 6. OpenGL ES 3.0

Target：`AeroRHI_GLES30`

- OpenGL ES 3.0；
- GLSL ES 3.00；
- Android/EGL 是首要目标；
- 与 WebGL 2 共享 canonical shader feature subset；
- runtime 查询 texture/format/renderbuffer limits；
- 不假定 desktop GL extension 存在；
- tile-based GPU hint 影响 offscreen、clear/load/store 和 transient target 策略；
- context recreation 必须重建全部 GL object；
- Android surface loss 与 application pause/resume 独立测试。

GLES 3.1/3.2 能力可作为优化，不改变 GLES 3.0 baseline。

## 7. Window-system adapters

### 7.1 GLX

Target：`AeroPlatform_GLX`

GLX 负责 Linux/X11 环境：

- X Display / screen；
- FBConfig 选择；
- X11 Window/Pixmap/Pbuffer drawable；
- GLXContext 创建和共享；
- `glXMakeContextCurrent`；
- buffer swap；
- swap interval extension；
- context/current-thread 生命周期；
- X11 resize/expose/error integration。

要求：

- GLX 1.4；
- 运行时查询 `GLX_ARB_create_context`，请求 OpenGL 3.3 Core Profile；
- 查询 extension string 后才能使用 swap-control/robustness 等扩展；
- FBConfig 必须匹配 color、alpha、depth/stencil、double-buffer 和 sRGB 需求；
- `Owned` 和 `Borrowed` display/context/drawable 分开建模；
- 不在 Wayland 上伪装使用 GLX。

### 7.2 EGL

Target：`AeroPlatform_EGL`

用途：

- Android；
- Wayland/Linux；
- headless/pbuffer；
- 嵌入式系统；
- 宿主提供 EGLDisplay/EGLContext/EGLSurface。

EGL adapter 与 `AeroRHI_GLES30` 主要组合，也可在实现支持时承载 desktop OpenGL profile。

### 7.3 WGL

Target：`AeroPlatform_WGL`

用途：Windows desktop OpenGL 3.3 Core。

- bootstrap legacy context 仅用于加载 WGL extension；
- 使用 `WGL_ARB_create_context` 创建正式 core context；
- pixel format 选择与 HWND/HDC lifetime 显式；
- D3D backend 不依赖 WGL。

### 7.4 Host-provided context

游戏引擎可直接提供已 current 的 GL/GLES context 和 function table，此时 platform adapter 不创建 window、surface 或 context。调用方必须满足 documented current-thread 和 presentation contract。

## 8. WebGL 2 backend

Target：`AeroRHI_WebGL2`

### 8.1 Baseline

- WebGL 2.0；
- GLSL ES 3.00；
- HTML Canvas 或 OffscreenCanvas；
- C++17 编译为 WebAssembly；
- WebGL 1 不进入 v1；
- 不使用 browser-vendor-prefixed extension 作为 baseline。

WebGL 2 接近 OpenGL ES 3.0，但存在额外安全和一致性规则，因此必须有独立 backend。

### 8.2 Host bridge

```text
JavaScript/HTML Host
  canvas/offscreen canvas
  context creation attributes
  requestAnimationFrame
  resize/devicePixelRatio
  input/IME/accessibility bridge
  context lost/restored events
        |
        v
AeroPlatform_Web
        |
        v
AeroRHI_WebGL2 (WASM-facing handle/function layer)
```

JS bridge 应最小化，不使用 Embind 作为核心 ABI 前提。资源 ID 在 C++ 和 JS 之间使用受控 handle table，避免直接持有不稳定 JS object pointer。

### 8.3 Frame scheduling

- 由 `requestAnimationFrame` 或 host callback 驱动；
- Runtime 的 `Run()` 在 Web profile 下不得阻塞浏览器主线程；
- input、layout、transaction、render 在一次或分阶段 callback 中推进；
- 后台 tab/throttling 必须允许时间源跳变并限制 catch-up；
- Worker/OffscreenCanvas 是 optional profile；
- baseline 不依赖 pthread、SharedArrayBuffer 或跨源隔离 header。

### 8.4 Resource 与 context loss

WebGL context 可随时丢失。处理流程：

```text
webglcontextlost
 -> preventDefault when restoration is desired
 -> stop recording/submission
 -> invalidate every WebGL handle generation
 -> preserve CPU/source resource descriptors
 -> wait for restored event

webglcontextrestored
 -> recreate context-dependent state
 -> re-query extensions and caps
 -> rebuild shaders/programs/buffers/textures/FBOs/VAOs/samplers
 -> rebuild glyph/image/geometry atlases
 -> resume with a full RenderTree resource rebind
```

之前创建的 WebGL resource 和 extension object 不得在 restore 后复用。

测试必须启用 `WEBGL_lose_context` 并模拟至少：

- idle loss；
- upload 中 loss；
- frame recording 中 loss；
- restore 后 extension/caps 改变；
- repeated loss/restore；
- shutdown while lost。

### 8.5 Sync 与 retirement

WebGL 2 sync object 在浏览器事件循环中有特殊约束。Backend 不允许：

- 在当前 frame/task busy-wait；
- 假设新 fence 同一 frame 可观察为 signaled；
- 使用 `glFinish` 风格同步作为常规资源回收策略。

资源回收使用：

- N-frame deferred deletion；
- optional `WebGLSync` polling from later animation frames；
- generation validation；
- bounded pending-delete queue；
- context loss 时整体 invalidation。

### 8.6 Shader

WebGL API 接收 GLSL ES source，因此：

- canonical shader 在离线工具中转换为 GLSL ES 3.00；
- 离线执行语法检查、reflection、binding-layout validation、minify 和 hash；
- release package 内嵌固定版本 source；
- 浏览器运行时 compile/link；
- compile/link log 转换为结构化 diagnostic；
- 可使用 parallel shader compile extension 作为 optional optimization，但不作为 baseline；
- program binary cache 不作为跨浏览器合同。

### 8.7 WebGL 2 capability baseline

不要求 extension 的核心路径必须在以下约束内工作：

- RGBA8 color target；
- DEPTH24_STENCIL8 或兼容 depth-stencil；
- 2D/array texture；
- UBO；
- VAO；
- sampler；
- framebuffer/blit where core permits；
- multiple render target 仅在实际 RenderPlan 需要且 caps 满足时使用。

Optional extension 示例：

- anisotropic filtering；
- compressed textures；
- float/half-float color renderability；
- timer query；
- multi-draw；
- parallel shader compile。

每个 extension 必须显式启用、记录并在 context restore 后重新获取。

## 9. Shader dialect matrix

| Backend | Runtime form | Shader dialect |
| --- | --- | --- |
| D3D11 | offline binary | HLSL → DXBC SM4/5 |
| D3D12 | offline binary | HLSL → DXIL |
| Vulkan | offline binary | canonical source → SPIR-V |
| Metal | packaged source/binary per platform policy | MSL / metallib |
| OpenGL 3.3 | generated source, driver compile/link | GLSL 330 Core |
| GLES 3.0 | generated source, driver compile/link | GLSL ES 300 |
| WebGL 2 | embedded generated source, browser compile/link | GLSL ES 300 WebGL profile |
| Console | offline private package | platform-specific |

所有方言由同一 shader schema 生成并验证：

- semantic/location；
- uniform block layout；
- texture/sampler binding；
- vertex format；
- color/clip defines；
- precision qualifiers；
- feature/capability guards。

## 10. sokol bootstrap profile

`sokol_gfx` 可帮助快速启动：

```text
AeroRHI_Sokol
  D3D11
  OpenGL 3.3
  GLES 3 / WebGL 2
  Metal
  WebGPU (experimental for AeroGUI)
```

允许用途：

- 第一个 triangle/batch/offscreen bring-up；
- RenderPlan adapter 验证；
- sample/tool；
- browser prototype；
- 与第一方 backend 做差异测试。

正式要求：

- `AERO_WITH_SOKOL=OFF` 时所有正式 backend 独立构建；
- 不把 `sg_*` handle 序列化或暴露；
- 不让 sokol feature set 成为 `AeroRHI` 上限；
- 第一方 D3D11/GL/GLES/WebGL2 backend 可以阶段性晚于 sokol prototype，但 release status 必须明确。

## 11. Build options

```cmake
option(AERO_RHI_D3D11 "Build Direct3D 11 backend" ON)
option(AERO_RHI_OPENGL33 "Build desktop OpenGL 3.3 backend" ON)
option(AERO_RHI_GLES30 "Build OpenGL ES 3.0 backend" ON)
option(AERO_RHI_WEBGL2 "Build WebGL 2 backend" ON)

option(AERO_PLATFORM_GLX "Build X11/GLX adapter" ON)
option(AERO_PLATFORM_EGL "Build EGL adapter" ON)
option(AERO_PLATFORM_WGL "Build Win32/WGL adapter" ON)
option(AERO_PLATFORM_WEB "Build browser host adapter" ON)

option(AERO_WITH_SOKOL "Build optional sokol adapter" OFF)
```

Platform presets 决定默认值；不应在不相关平台尝试查找所有 SDK/header。

## 12. Capability manifest

每个 runtime package 记录：

```json
{
  "rhi": {
    "backend": "webgl2",
    "tier": "compatibility",
    "apiVersion": "2.0",
    "shaderProfile": "glsl-es-300-webgl",
    "extensions": [],
    "limits": {},
    "contextLossRecovery": true
  },
  "platform": {
    "surface": "html-canvas",
    "threading": "main-thread",
    "ownedContext": true
  }
}
```

实际 schema 需版本化；示例仅说明必需信息。

## 13. CI 与验证矩阵

### D3D11

- FL10_0；
- FL11_0/11_1；
- hardware + available software adapter path；
- borrowed/owned device；
- debug-layer clean frame；
- state preservation contract。

### OpenGL

- Linux X11 + GLX 1.4 + GL 3.3 Core；
- Windows + WGL + GL 3.3 Core；
- embedded context state leak test；
- extension-off baseline；
- context recreation。

### GLES

- Android EGL + ES 3.0；
- headless EGL where CI supports；
- tile/offscreen budget；
- pause/resume/surface loss。

### WebGL 2

- at least two browser engines in automated CI where available；
- canvas resize/devicePixelRatio；
- context lost/restored；
- no-extension baseline；
- shader compile diagnostic；
- frame throttling/time jump；
- pixel tests with bounded tolerance；
- main-thread baseline；
- optional Worker/OffscreenCanvas job。

## 14. 验收条件

兼容后端达到 Production Supported 前必须：

- 通过 common RHI conformance；
- 通过 core XAML → layout → RenderPlan → image vertical slice；
- 不依赖 Skia；
- sokol 关闭时可构建运行；
- context/device loss 可恢复或明确报告 terminal 状态；
- resource leak/stress 通过；
- shader binding layout 与 strategic backend 一致；
- capability manifest 准确；
- known visual/performance differences 有文档和测试；
- WebGL 2 不静默降级到 WebGL 1。