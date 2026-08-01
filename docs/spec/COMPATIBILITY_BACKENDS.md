# Compatibility Backend 合同

本规范定义 D3D11、OpenGL 3.3、OpenGL ES 3.0 和 WebGL 2 对统一
`RenderFrame → Renderer → RenderDevice` 链路的实现约束。Backend 名称是实现
类型，不是独立安装目标。

## 1. 共同合同

所有 backend MUST：

- 消费同一 Renderer 生成的 command stream；
- 支持 rectangle、image、mesh、glyph、scissor、blend 和 render target；
- 验证 capability、format、sample count 和 resource generation；
- 提供 explicit loss/restore；
- 支持 external target/embedded mode；
- 不接收 UI object、Binding、Style 或 Visual pointer；
- 不改变 painter order 和 WPF 可观察视觉结果；
- 对 unsupported capability 返回稳定错误，而不是静默降级为错误结果。

## 2. D3D11

基线：

```text
feature level 10_0 minimum
11_0/11_1 preferred
VS/PS baseline
offline DXBC
BGRA8/RGBA8/R8 and Depth24Stencil8 baseline
owned swapchain or borrowed device/context/target
```

Embedded host 必须选择：

- host 在 Aero 提交后重置 state；或
- Aero 保存并恢复文档化的必要 state。

Device loss 使所有 native resources 失效；恢复后通过 generation 重建。
WARP 可用于 CI 和 reference fixture，但不改变产品 API。

## 3. OpenGL 3.3 Core

基线：

```text
OpenGL 3.3 Core
GLSL 330
VAO/VBO/IBO/UBO
FBO and texture sampling
GLsync when available
owned WGL/GLX context or borrowed host context
```

Backend 必须验证：

- context 当前线程；
- context generation；
- Core Profile，不使用 fixed function；
- function resolver 完整性；
- documented state-cache ownership。

WGL/GLX 只是 private context/surface adapters。Engine 提供现有 context 时不需要
它们。

## 4. OpenGL ES 3.0

基线：

```text
OpenGL ES 3.0
GLSL ES 300
EGL or host context
mobile/embedded texture and uniform limits
```

与 WebGL 2 共享 canonical shader feature subset。ES 3.1/3.2 capability 只能作为
优化，不能成为基础 UI path 的必要条件。

## 5. WebGL 2

基线：

```text
C++17 → WebAssembly
WebGL 2
GLSL ES 300
HTMLCanvasElement or OffscreenCanvas
requestAnimationFrame host loop
```

规则：

- 不回退到 WebGL 1；
- 不 blocking wait 或 busy-wait fence；
- context loss 时取消默认浏览器处理、停止提交并使全部 resource generation
  失效；
- restore 后重新查询 caps/extensions，重新 compile/link 固定 shader package，
  并按需重建资源；
- resource retirement 使用后续 frame polling 或安全延迟；
- browser host 拥有主循环、canvas 和 worker policy。

## 6. Surface/context adapters

| Adapter | 组合 |
| --- | --- |
| WGL | Windows + OpenGL 3.3 |
| GLX | Linux/X11 + OpenGL 3.3 |
| EGL | Android/Wayland/headless + GLES/OpenGL |
| Web host | Canvas + WebGL 2 |

Adapter 负责：

```text
create/borrow context
make-current and thread validation
resize
swap interval
acquire target
present
loss/restore
```

它不创建 Renderer，不定义 command vocabulary，也不形成独立产品 target。

## 7. Optional sokol adapter

`sokol_gfx` MAY 实现私有 RenderDevice adapter，用于：

- bring-up；
- WebAssembly 实验；
- RenderFrame 差异验证；
- 示例和开发工具。

它 MUST NOT：

- 定义公共 Aero API；
- 成为所有 backend 的 mandatory lower layer；
- 限制 D3D12/Vulkan/Metal/private-console capability；
- 把 `sg_*` 类型带入公共头；
- 让 `sokol_app` 接管嵌入式宿主主循环。

## 8. Conformance matrix

每个 compatibility backend 至少验证：

```text
capability query
resource create/configure/destroy
command validation
rectangle/image/mesh/glyph rendering
clip/blend/painter order
external target
resize/present
borrowed-state policy
fence/deferred release
loss/restore generation
fixed RenderFrame hash and pixel fixtures
```
