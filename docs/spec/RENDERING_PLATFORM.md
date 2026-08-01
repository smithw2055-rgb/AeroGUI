# Rendering 与 Platform 规范

- **状态**：Architecture Baseline
- **语言**：C++17
- **Renderer**：retained-mode native GPU / WebGL 2
- **Skia**：不支持

## 1. 唯一渲染链路

```text
Visual/UI state
→ DrawingContext records immutable drawing values
→ RenderTree::Commit builds RenderFrame
→ Renderer validates, batches and lowers
→ RenderDevice records native commands
→ backend submits and presents/returns to host
```

AeroGUI 不维护第二套 RenderTransaction、RenderPlan、HostedGraphics command
ABI 或通用 RHI 产品。UI object、Binding object、Control pointer 和 user callback
不得跨越 RenderFrame 边界。

## 2. RenderTree 与 RenderFrame

RenderTree 位于 UI domain，持有紧凑 retained state，并按 invalidation 增量更新。
RenderFrame 是一次 commit 的 immutable snapshot，至少包含：

```text
stable node ID and generation
parent/child ordering
layout slot, clip, transform, opacity and effect state
copied drawing commands
image, mesh and glyph resource IDs
epoch/version and deterministic hash
```

RenderFrame MUST：

- 可 move 到宿主管理的渲染线程；
- 不保存 `Object*` 或 callback；
- 可验证 command/resource 范围；
- 保持 painter order；
- 支持确定性 hash 与 replay fixture；
- 对丢失 device/context 使用 generation 失效旧资源。

## 3. Renderer

Renderer 负责 UI-specific lowering，而不是 GPU API ownership：

- drawing command → draw packet；
- clip/effect/offscreen graph；
- rectangle/image/mesh/glyph batching；
- pipeline/resource binding plan；
- glyph/image/mesh resource registration；
- statistics 与 deterministic validation。

Batching 不得跨越 clip、opacity group、effect、render target、read-after-write 或
painter-order barrier。

## 4. RenderDevice

RenderDevice 是 Renderer 所需的最小私有 GPU 合同：

```text
capabilities
buffer/texture/sampler/pipeline resources
upload commands
render pass
bind and draw commands
fence and deferred destruction
device/context loss generation
external render target import
```

它不是面向应用开发者的 3D engine，也不暴露为独立安装目标。Backend-specific
state、shader、native handles 和 loader function tables 位于 `src/render/<backend>`。

Compatibility baseline 不要求：

```text
compute
storage buffers/images
bindless resources
indirect draws
persistent mapping
blocking GPU waits in WebGL
```

高级路径必须通过 capabilities 选择，不能按 GPU 名称猜测。

## 5. Backend 等级

| 等级 | Backend | 说明 |
| --- | --- | --- |
| Strategic | D3D12 | Windows 与受控 GDK adapter |
| Strategic | Vulkan | Windows、Linux、Android |
| Strategic | Metal | Apple platforms |
| Strategic | private console | 受限仓库和授权 SDK |
| Compatibility | D3D11 | Windows，feature level 10_0+ |
| Compatibility | OpenGL 3.3 Core | WGL、GLX 或 host context |
| Compatibility | OpenGL ES 3.0 | EGL/Android/embedded |
| Compatibility | WebGL 2 | WebAssembly/browser |
| Validation | Null RenderDevice | command/resource lifecycle |
| Optional | sokol adapter | bring-up 与差异验证 |

Skia、WebGL 1、OpenGL fixed-function/compatibility profile 不进入生产图。

## 6. Platform adapter

平台层只处理 native ownership：

```text
window/canvas
DPI and resize
input, IME and clipboard
WGL/GLX/EGL/context current
swapchain/surface/present
browser context-loss events
```

它不定义 drawing primitive，也不形成独立 Aero SDK target。

### 6.1 Owned 与 borrowed

- **Owned**：默认 App 创建 window/context/surface 并负责 present；
- **Borrowed**：engine host 提供 device/context/target，并声明 state ownership；
- embedded backend 不自行 present；
- native handle 只通过 Integration 专用接口或 opaque POD 传递。

## 7. 线程与帧调度

核心不创建 worker thread。宿主选择：

```text
single thread: View.Update → Renderer → present
split thread: View.Update → immutable RenderFrame queue → Renderer
browser: requestAnimationFrame drives Update/render
```

规则：

- UI thread 之外不读写 mutable UI object；
- queue、frame dropping、coalescing 和 render latency policy 属于宿主；
- 多个 View 可共享 native device/cache，但每个 View 有独立 scene/root/target；
- loss/restore 前必须停止提交并提升 generation；
- shutdown 必须等待或安全延迟仍在 GPU 使用的资源。

## 8. 资源与缓存

资源句柄包含 index、type 和 generation。Destroy 可延迟到 fence，旧 generation
永远不能重新绑定到新资源。

Text/image/mesh runtime 通过显式服务合同注册资源，不使用 service ID 或
`QueryInternalService()`。Context/device loss 后：

1. 丢弃旧 native handles；
2. 增加 generation；
3. 重建 device/surface；
4. 下一次 layout/render 按需重建 atlas、glyph run、image 和 pipeline；
5. 不把旧 frame 提交到新 generation。

## 9. Shader policy

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

## 10. 性能门禁

- View stable services 使用一次对齐分配；
- frame/input/layout/render 热路径无同步日志 I/O；
- RenderFrame 和 command list 复用容量；
- dirty update 避免全树扫描；
- batching statistics 可观测；
- texture/glyph atlas 有明确 generation 和 fence-safe reuse；
- borrowed GL mode 只保存/恢复文档化的必要 state；
- browser 不 busy-wait fence；
- 任何新抽象必须消除实际重复职责，不能只增加转发接口。

## 11. 验证

每个 backend 至少共享：

```text
RenderFrame validation/hash fixtures
rectangle/image/mesh/glyph fixtures
resource generation and deferred destruction
loss/restore
state-binding and draw-call statistics
embedded/owned target lifecycle
```

像素、shader、native API 和平台特定行为由对应 backend conformance suite 验证。
