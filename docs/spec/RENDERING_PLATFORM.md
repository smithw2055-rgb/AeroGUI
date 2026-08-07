# Rendering 与 Platform 规范

- **状态**：S10 Architecture Baseline
- **语言**：C++17
- **Renderer**：retained-mode native GPU / WebGL 2
- **Skia**：不支持

## 1. 唯一渲染链路

```text
Visual/UI state
→ DrawingContext records immutable drawing values
→ RenderTree::Commit builds RenderFrame
→ IRenderer synchronizes one View
→ RenderDevice owns GPU lifetime
→ source-private Render::Renderer lowers and records commands
→ RenderTarget selects the embedded/window target
→ native backend submits and presents/returns to host
```

AeroGUI 不维护第二套 RenderTransaction、RenderPlan、HostedGraphics command
ABI 或独立通用 RHI 产品。UI object、Binding object、Control pointer 和 user
callback 不得跨越 RenderFrame 边界。

## 2. RenderTree 与 RenderFrame

RenderTree 位于 UI domain，持有紧凑 retained state，并按 invalidation 增量更新。
RenderFrame 是一次 commit 的 immutable snapshot，包含稳定节点标识、顺序、
layout/clip/transform/opacity/effect、复制后的 drawing commands、资源 ID 与
version/hash。

RenderFrame MUST：

- 不保存 `Object*` 或 callback；
- 可验证 command/resource 范围；
- 保持 painter order；
- 支持确定性 hash/replay；
- 对丢失 device/context 使用 generation 失效旧资源。

## 3. Renderer

`Render::Renderer` 是唯一语义级 backend renderer，负责：

- drawing command → draw packet；
- clip/effect/offscreen graph；
- rectangle/image/mesh/glyph batching；
- pipeline/resource binding；
- glyph/image/mesh resource registration。

低层 `FrameEncoder.cpp` 是 Renderer 拥有的 command encoder 实现，不是第二个
Renderer 生命周期。旧 `DeviceRenderer` spelling 仅保留 source-only alias。

Batching 不得跨越 clip、opacity group、effect、render target、read-after-write
或 painter-order barrier。

## 4. RenderDevice 与 RenderTarget

RenderDevice 是 host-thread-affine UI GPU device 对象，负责设备状态、generation、
资源和提交生命周期。它不是面向应用开发者的通用 3D engine。

RenderTarget 是唯一安装的目标对象：

```text
RenderTarget
  → backend RenderTarget::Impl
     → source-private Graphics::SurfaceSession（需要 acquire/present 时）
```

原 `NativeRenderTarget` 不再是对象层，只是 `RenderTarget::Impl` 的 source-only
兼容 alias。`RenderSurface` 不再是 SDK 类型。

D3D11/OpenGL embedded API 采用显式 device + target：

```text
Create*Device(...)
Create*RenderTarget(device, options)
```

不得由 target factory 隐式创建第二个 device 生命周期。

## 5. Desktop 与 embedded hosting

### Desktop

默认 `Aero::App` 的 source-private `App::Detail::RenderContext` 负责：

```text
native window target creation
resize
RenderDevice handoff
IRenderer::Render(RenderTarget&)
WaitIdle / shutdown
```

Public Render backend 头不暴露 `NativeWindow`、window surface factory 或
PresentMode。

### Embedded

Engine/editor host 提供 device/context/target callback，并自行决定调度和最终
presentation。Embedded backend 不自行 present。

## 6. Platform adapter

平台代码只处理 native ownership：

```text
window/canvas
DPI and resize
input, IME and clipboard
WGL/GLX/EGL/context current
swapchain/surface/present
browser context-loss events
```

平台实现不形成额外安装产品，也不定义 drawing primitive。

## 7. 线程与帧调度

核心不创建 worker thread。宿主选择调度点；mutable UI object 只能由 View owner
thread 修改。queue、frame dropping、coalescing 和 render latency policy 属于宿主。
多个 View 可以共享 RenderDevice，但每个 View 保持独立 presentation state。

## 8. 资源与缓存

资源句柄包含 index/type/generation。Destroy 可延迟到 fence，旧 generation 不得
重新绑定到新资源。Text/image/mesh runtime 使用显式资源 contract，不使用
service ID 或 `QueryInternalService()`。

Context/device loss 后：

1. 丢弃旧 native handles；
2. 增加 generation；
3. 恢复 device/native target；
4. 按需重建 atlas、glyph run、image、pipeline；
5. 不提交旧 generation 的 frame/resource。

## 9. Diagnostics

渲染统计不属于默认 RenderDevice authoring API。需要时显式包含：

```cpp
#include <Aero/Diagnostics/Rendering.hpp>
```

并调用：

```text
GetRenderDeviceStatistics(device)
GetLastRenderFrameStatistics(device)
```

## 10. Shader policy

| Backend | Release form |
| --- | --- |
| D3D11 | offline DXBC |
| D3D12 | offline DXIL |
| Vulkan | offline SPIR-V |
| Metal | packaged metallib/platform package |
| OpenGL 3.3 | generated/validated GLSL 330 source, runtime link |
| GLES 3.0 | generated/validated GLSL ES 300 source, runtime link |
| WebGL 2 | embedded validated GLSL ES 300 source, browser link |
| Console | private offline package |

Runtime GL/WebGL compile/link 是 API 约束，不允许动态生成任意 shader source。

## 11. 架构与性能门禁

- frame/input/layout/render 热路径无同步日志 I/O；
- RenderFrame/command list 复用容量；
- dirty update 避免全树扫描；
- texture/glyph atlas 有 generation 与 fence-safe reuse；
- borrowed GL mode 只保存/恢复文档化必要 state；
- browser 不 busy-wait fence；
- 新抽象必须消除实际重复职责；
- 永久门禁只验证最终 SDK/dependency invariant，不冻结历史迁移文件名、Object
  Library 名称或内部 allocation 策略。
