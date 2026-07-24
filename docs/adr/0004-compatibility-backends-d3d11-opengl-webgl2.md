# ADR-0004：D3D11、OpenGL、GLX 与 WebGL 2 兼容后端

- **状态**：Accepted
- **日期**：2026-07-21
- **决策者**：AeroGUI maintainers
- **修订关系**：扩展 ADR-0002 的正式 backend 列表

## 背景

AeroGUI 除现代显式图形 API 外，还需要覆盖已有 Windows 游戏引擎、较保守的桌面 GPU 环境、Linux/X11、Android/OpenGL ES 和浏览器/WebAssembly。D3D11、OpenGL 和 WebGL 仍然具有现实集成价值，但它们的状态模型、上下文生命周期和能力边界与 D3D12/Vulkan/Metal 不同。

GLX 不是绘制 API，而是 X11 上创建 OpenGL context、选择 framebuffer configuration、绑定 drawable 和交换缓冲的 window-system interface。WebGL 2 也不是普通桌面 OpenGL：它提供接近 OpenGL ES 3.0 的浏览器 API，并附加安全、资源、上下文丢失和事件循环约束。

## 决策

### 1. Backend 等级

AeroGUI 采用两级正式后端：

**Strategic native backends**

```text
AeroRHI_D3D12
AeroRHI_Vulkan
AeroRHI_Metal
AeroRHI_ConsolePrivate
```

**Compatibility backends**

```text
AeroRHI_D3D11
AeroRHI_OpenGL33
AeroRHI_GLES30
AeroRHI_WebGL2
```

`AeroRHI_Null` 是验证后端，不产生产品像素。

兼容后端是正式受支持后端，必须通过统一 RenderPlan/RHI conformance suite；但新性能特性优先在 strategic backends 实现，兼容后端允许通过 capability manifest 声明受限路径。

### 2. D3D11

- 提供第一方直接实现 `AeroRHI_D3D11`，不强制经由 sokol。
- 首个稳定版本要求 Direct3D feature level 10_0 或更高；11_0/11_1 为推荐路径。
- feature level 9_x 不进入 v1 正式范围。
- 基础 UI renderer 只要求 vertex/pixel shader；compute、UAV、tiled resources 和高级 optional feature 必须通过能力查询。
- shader 使用离线编译 DXBC package；按 feature level 提供兼容 profile。
- 支持 external `ID3D11Device` / immediate context 集成和 AeroGUI-owned standalone device 两种模式。
- embedded mode 必须定义 D3D11 state ownership、resource hazard 和 context threading 合同。

### 3. Desktop OpenGL

- 提供第一方 `AeroRHI_OpenGL33`。
- 最低基线为 OpenGL 3.3 Core Profile 与 GLSL 3.30。
- 不依赖 compatibility profile、fixed-function pipeline、display list、immediate mode 或已废弃 API。
- extension 只能作为可选优化，不能改变 baseline 可见语义。
- embedded mode 必须使用 state shadow/cache，并定义进入和退出 AeroGUI rendering 时需要保存、恢复或由宿主保证的 GL state。
- context 只能在其 current thread 上使用；AeroGUI 不隐式迁移 context。

### 4. OpenGL ES

- 提供第一方 `AeroRHI_GLES30`。
- 最低基线为 OpenGL ES 3.0 与 GLSL ES 3.00。
- 主要目标是 Android、Linux/EGL、嵌入式系统和与 WebGL 2 共享的 shader/feature 子集。
- iOS/macOS 产品路径优先 Metal；Apple 上的 OpenGL/OpenGL ES 只可作为宿主提供的遗留兼容配置，不作为新平台默认后端。

### 5. GLX、EGL 与 WGL

这些属于 `AeroPlatform` / surface-context adapter，不属于 `AeroRHI` 绘制后端：

```text
AeroPlatform_GLX   Linux + X11 + desktop OpenGL
AeroPlatform_EGL   Wayland/Linux, Android, headless, GLES/OpenGL where available
AeroPlatform_WGL   Windows + desktop OpenGL
```

- GLX 基线为 GLX 1.4；创建现代 core context 时要求运行时查询并使用 `GLX_ARB_create_context`。
- GLX adapter 负责 FBConfig、context、drawable、swap interval、make-current、resize 和 context loss/error bridge。
- GLX 不进入 Wayland 路径；Wayland 使用 EGL。
- Android 使用 EGL + GLES 3.0。
- 游戏引擎提供现有 GL context 时可跳过所有 window-system adapter。
- context/surface ownership 必须显式区分 `Owned` 与 `Borrowed`。

### 6. WebGL 2

- 提供第一方正式 backend `AeroRHI_WebGL2`。
- v1 基线只支持 WebGL 2；WebGL 1 不作为 fallback 或正式 capability。
- WebGL 2 路径面向 C++17 → WebAssembly，并通过最小 JS/HTML host bridge 接入 `HTMLCanvasElement` 或 `OffscreenCanvas`。
- renderer 基线只依赖 WebGL 2 core 与 GLSL ES 3.00；extension 通过显式 capability query 使用。
- 不要求 compute shader、SSBO、bindless、persistent mapping 或 blocking GPU wait。
- resource retirement 使用 frame-delayed/generation 机制；不能依赖当前 JavaScript task 中同步等待新 fence 完成。
- 所有 WebGL resource 在 context loss 后视为失效；context restore 时必须重新查询 extension/caps，并从 CPU/source cache 重建 shader、buffer、texture、atlas 和 pipeline state。
- 必须监听并处理 `webglcontextlost` / `webglcontextrestored`；测试使用 `WEBGL_lose_context`。
- frame scheduling 由浏览器 host 的 `requestAnimationFrame` 或等价宿主调度驱动；Runtime 不实现阻塞主循环。
- 第一阶段以浏览器主渲染线程为正式基线；Worker + OffscreenCanvas 是可选 capability，不要求 SharedArrayBuffer。

### 7. Shader policy

- D3D11/D3D12/Vulkan/Metal/console 使用离线编译平台 binary/package。
- OpenGL/GLES 包含离线验证、反射和生成后的 GLSL source package，运行时由 driver 编译/link。
- WebGL 2 是“发行版不运行时编译 shader”规则的明确例外：浏览器 API 只能接收 GLSL ES source，因此项目必须离线生成、校验、固定版本和最小化 source，但在浏览器中完成 compile/link。
- 所有 dialect 必须来自同一 canonical shader metadata，绑定布局由生成器验证。

### 8. sokol 的关系

`sokol_gfx` 公开支持 D3D11、GL 3.3、GLES3/WebGL2、Metal 和 WebGPU，因此 `AeroRHI_Sokol` 对兼容后端 bring-up 很有价值。但：

- 它仍然默认关闭；
- 不能替代第一方 D3D11/OpenGL/GLES/WebGL2 backend 的长期合同；
- 可以作为并行验证 adapter、样例后端或阶段性 bootstrap；
- `sokol_app` 可用于独立样例中的 GLX/WGL/WebGL canvas 创建，但不进入嵌入式 Runtime 合同；
- D3D12、Vulkan 和 console 仍由第一方原生 backend 实现。

## 原因

- D3D11 能接入大量已有 Windows 引擎和工具，并可通过 feature level 支持更广硬件。
- OpenGL 3.3 Core 是 Linux/X11 和遗留桌面环境的实用兼容基线。
- GLES 3.0 与 WebGL 2 可共享大部分 shader 语义和 RenderPlan lowering 策略。
- 将 GLX/EGL/WGL 放在 Platform 层可避免把 window-system 生命周期污染 RHI。
- WebGL 2 的浏览器安全和事件循环约束要求独立 backend，而不是简单将 desktop GL function pointer 重用。

## 后果

### 正面

- AeroGUI 覆盖现代 API、遗留引擎、Linux/X11、Android 和浏览器。
- D3D11/GL/WebGL 可作为低门槛集成路径。
- GL/GLES/WebGL 共享 shader/layout 设计，但各自保留正确的资源和上下文语义。
- sokol 可帮助 bootstrap，而不会决定正式架构。

### 代价

- backend 和 CI 矩阵扩大。
- OpenGL state restoration、driver 差异和 context lifecycle 增加测试成本。
- WebGL 需要 JS/WASM bridge、浏览器测试、context-loss 恢复和 runtime shader link。
- 兼容后端不能实现所有 strategic backend 优化，必须维护明确 capability/fallback 路径。

## 被否决方案

- **GLX 作为 RHI backend**：GLX 只负责 X11/OpenGL context 和 drawable，不负责 UI 绘制抽象。
- **只支持 WebGL 1**：功能与 shader 模型过弱，会显著增加另一套 renderer 路径；v1 只支持 WebGL 2。
- **WebGL 2 直接复用 desktop GL backend 而无独立合同**：忽略浏览器对象、context loss、extension 和事件循环语义。
- **所有兼容后端只通过 sokol 提供**：会使正式支持依赖第三方抽象，并妨碍精确的宿主集成和长期优化。
- **D3D11 feature level 9_x 作为 v1 baseline**：会扩大 shader 与资源限制矩阵，收益不足。

## 验证

- D3D11 FL10_0 与 FL11_0 conformance；
- OpenGL 3.3 Core on GLX 和 WGL；
- GLES 3.0 on EGL/Android；
- WebGL 2 浏览器自动化与 pixel/RenderPlan tests；
- `WEBGL_lose_context` loss/restore/rebuild tests；
- GL state leak/restore tests in embedded mode；
- GLX owned/borrowed context and resize tests；
- shader dialect reflection/layout equivalence；
- all compatibility backends with `AERO_WITH_SOKOL=OFF`；
- optional sokol adapter comparison job。