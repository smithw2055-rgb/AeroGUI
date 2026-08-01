# ADR-0002：原生 GPU AeroGraphics，不支持 Skia

- **状态**：Accepted
- **日期**：2026-07-21
- **决策者**：AeroGUI maintainers

## 背景

AeroGUI 的目标是面向桌面、移动设备、游戏引擎和游戏主机的高性能 retained-mode UI engine。项目需要复用宿主 GPU device、queue、command context、render target 和 frame scheduler，同时保留对 D3D12、Vulkan、Metal 与专有主机 API 的原生能力访问。

## 决策

1. AeroGUI 定位为 clean-room、NoesisGUI 风格产品方向的原生 GPU XAML UI engine，但不复制 NoesisGUI 的代码、私有算法、shader 或数据格式。
2. 生产级 rasterization 和 composition 通过自有 `AeroGraphics` 与 native GPU API 执行。
3. 第一方公开 backend 为：
   - `AeroGraphics_D3D12`；
   - `AeroGraphics_Vulkan`；
   - `AeroGraphics_Metal`；
   - `AeroGraphics_Null`。
4. Xbox、PlayStation、Nintendo 和其他受限平台通过访问受控仓库中的 `AeroGraphics_ConsolePrivate` adapter 实现。
5. Skia 不作为生产 renderer、reference renderer、fallback renderer 或 golden-image oracle，也不进入依赖图。
6. UI 线程构建不可变 `RenderTransaction`；render domain 维护 retained render tree，并构建短生命周期 `RenderFrame`；backend 不遍历 Visual tree，也不持有 UI `Object*`。
7. 游戏引擎默认使用 embedded mode：宿主拥有 device、queue、command submission、render target 和 Present；AeroGUI 只记录 UI 所需命令并遵守显式同步合同。
8. Shader 必须支持离线编译和平台 package；发行版不要求 runtime shader JIT。
9. 初期 geometry 可使用 CPU tessellation + GPU rasterization/cache；后续 analytic/compute path 是 capability-driven 可选优化，而不是基础兼容前提。
10. Headless/reference 验证通过 `AeroGraphics_Null`、结构快照、自有受限 CPU rasterizer和锁定 native GPU golden 分层完成。

## AeroGraphics 范围

`AeroGraphics` 只抽象 UI renderer 所需能力：

- buffer、texture、sampler、pipeline；
- render pass、target、viewport、scissor；
- resource binding、draw、upload、readback；
- barrier/state/ownership contract；
- fence、resource retirement、device loss；
- capability query 与 external resource import。

它不是通用 3D engine，不负责 scene graph、camera、lighting、material authoring 或宿主 swapchain policy。

## 原因

- D3D12、Vulkan、Metal 和主机 API 都强调显式资源、命令与同步；直接 backend 能更好地集成现有游戏引擎。
- Skia 的抽象、依赖体积和生命周期模型会成为 AeroGUI 自有 retained renderer 的额外中间层。
- 自有 RenderFrame/graphics layer 可针对 UI 的 painter order、clip、mask、offscreen、glyph atlas 和 geometry cache 设计。
- 宿主拥有 Present 与 frame scheduling，能避免中间件与游戏引擎争夺主循环和 GPU state。

## 后果

### 正面

- 原生利用平台 GPU 能力；
- 可接入桌面、移动和受限主机；
- 能与宿主共享 device、resource 和 command stream；
- 渲染架构不受单一第三方 renderer 限制；
- 可建立统一的 RenderFrame/backend conformance suite。

### 代价

- 必须实现和维护多个 backend；
- shader、resource state、device loss 和 driver 差异测试成本更高；
- 需要自有 reference/golden 策略；
- console backend 需要独立受限工程与认证。

## 被否决方案

- **Skia 作为核心或 reference backend**：与“不支持 Skia”和自有原生 GPU engine 的目标冲突。
- **只实现一个跨平台第三方 GPU abstraction**：无法保证 D3D12、Vulkan、Metal、现有引擎 command integration 和主机目标。
- **CPU/software renderer 作为产品主路径**：不符合移动和游戏 UI 的性能定位。
- **从第一版起全部 compute tessellation**：会增加弱移动 GPU 和多 backend bring-up 风险；应先建立可缓存、可验证的 CPU tessellation 路径。

## 验证

- RenderTransaction merge/replay；
- RenderFrame snapshot 和 validator；
- `AeroGraphics_Null` resource/pass/fence tests；
- D3D12/Vulkan/Metal backend conformance；
- clip、mask、offscreen、effect、painter order golden；
- device loss与 mobile surface recreation；
- 24 小时 GPU resource stress；
- build/link graph 证明不存在 Skia。