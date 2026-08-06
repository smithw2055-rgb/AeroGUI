# WPF C++ SDK 视角审查与重构建议

> 审查基线：`HEAD`（当前精简源码分支，runtime vertical slice 阶段）。
> 视角：让熟悉 WPF(.NET) / NoesisGUI 的开发者能够快速上手。
> 参考架构：NoesisGUI 公开架构文档（Rendering Architecture、Dependency System、Element Tree、RenderDevice API、C++ Architecture Guide）。

## 1. 结论摘要

AeroGUI-R 是一个 clean-room 的 WPF/XAML 语义 C++17 运行时 + 原生 GPU 引擎（约 12.5 万行，191 个源文件）。架构方向整体正确，与 NoesisGUI 的公开架构模型高度同构：

- 四树分离（Object / Logical / Visual / Render tree）正确；
- `DependencyObject` + 稀疏 `EffectiveValueEntry` + `EffectiveValueEngine` 是 WPF DP 系统的 C++ 实现；
- 渲染链 `Visual → RenderTree::Commit → immutable RenderFrame → Renderer → RenderDevice(backend)` 与 Noesis 的 `View → UpdateRenderTree → RenderOffscreen → Render` 对齐；
- 公共 ABI 采用 opaque handle + 版本化 C function table + 无异常/RTTI，符合 ADR-0001。

**主要差距不在内核，而在"让 WPF 开发者上手"的外层**：命名碰撞、属性访问样板、元数据声明与注册分离、缺少 `x:Class`/`InitializeComponent`、样例过期、主循环轮询、文档语言割裂。

## 2. 已做对的（建议保持不动）

| 方面 | 依据 |
|---|---|
| DP 声明/注册分离，编译期 MemberId | `include/Aero/UIElement.hpp:259-277`、`include/Aero/DependencyProperty.hpp:42-104` |
| 属性失效映射（AffectsMeasure → Measure 等） | `src/gui/PropertySystem.cpp:1529-1552` |
| 变更通知顺序（consumer → metadata.changed → NotifyValueChanged → OnPropertyInvalidated） | `src/gui/PropertySystem.cpp:1502-1527` |
| 唯一渲染命令通道、UI 指针不跨界 | `src/render/RenderDevice.hpp:122-123` |
| 资源代际失效（device/surface loss → generation bump → 全量重建） | `src/render/RenderDevice.hpp`（DeferredDestroy）、`src/integration/RenderDevice.cpp:278-305` |
| 宿主边界（`Gui` / `View` / providers） | `include/Aero/View.hpp:48-226` |
| 元数据 fluent 注册 | `include/Aero/Meta.hpp:1560-1584` |
| 能力驱动后端选择而非名字嗅探 | `src/render/RenderDevice.hpp:699-709`（SelectGraphicsBackend） |
| 两阶段呈现（offscreen + onscreen） | `src/render/Renderer.cpp:1043-1226`，与 Noesis 一致 |

## 3. 评审发现的问题

### D1. 命名碰撞：四个 "Renderer/Device"（高危，最易造成困惑）

| 符号 | 层 | 位置 |
|---|---|---|
| `Aero::Renderer`（公开门面） | host facade | `include/Aero/Renderer.hpp`，实现在 `src/runtime/View.cpp:8659-8896` |
| `Aero::Render::Renderer`（GPU 编码器） | 私有渲染 | `src/render/Renderer.hpp` / `Renderer.cpp` |
| `Aero::Integration::RenderDevice`（公开宿主设备） | 集成 | `include/Aero/Integration/RenderDevice.hpp`，`src/integration/RenderDevice.cpp` |
| `Aero::Graphics::GraphicsDevice`（私有 RHI） | 私有渲染 | `src/render/RenderDevice.hpp` |

WPF 只有一个渲染概念（`System.Windows.Media`），Noesis 只有 `IRenderer` + `RenderDevice`。四者目前靠命名空间 + 约定区分，对新手是硬屏障。

**建议**：公开门面改名 `Aero::ViewRenderer`，或把私有 GPU 编码器改名 `RenderEncoder`，并保留 `Aero::Renderer` 为唯一对外符号。

### D2. 属性访问模式与 WPF 直觉的差距

当前每个属性需要手写 setter 包装，全是样板：

```cpp
// src/gui/Layout.cpp:537-568
void UIElement::SetVisibility(Visibility value) noexcept {
    SetValue(VisibilityProperty, value);
}
```

WPF 是 `Visibility = x`；NoesisGUI C++ 提供 `NsSet<T>(obj, property, value)` 宏 + typed accessor，显著降低样板。

**建议**：
- 提供 `NsSet` 风格属性宏（typed + `Meta::Value` 双路径），自动生成 `SetX/GetX` 包装，消除 106 个公开头的重复样板；
- 让宏同时声明 DP 常量与 accessor，形成"一处声明、两处使用"（见 D3）。

### D3. 元数据注册与声明分离，属性来源难以追踪

DP 在公开头声明（`Members::Property<T>` constexpr），注册却散落在 `src/controls/metadata/Primitives.inl` 等生成式 `.inl` 中（`Meta::Register<Panel>(...).Property(Panel::BackgroundProperty, ...)`）。WPF 开发者期望 `RegisterProperty` 出现在类定义附近（WPF/Noesis 均为类内静态成员）。

**建议**：引入 `AERO_REGISTER_DEPENDENCY_PROPERTY(Owner, "Name", Type, Options...)` 宏，声明处就近注册；`.inl` 仅做聚合排序与 Schema 顺序保证。

### D4. 缺少 `x:Class` / `InitializeComponent` / code-behind

探索确认 `x:Class` + 生成工厂目前是 future item（`docs/spec/XAML_UI_MODEL.md` 中列为后续）。这是 WPF 上手最大的断点：WPF 开发者习惯 `InitializeComponent()` 自动把 `x:Name` 注入字段。

**建议**：在 `tools/xamlc`（`aero-xamlc`）落地生成 `InitializeComponent()` + 命名字段绑定，分阶段：先字段注入，再 code-behind 工厂。参考 Noesis 的 `RegisterComponent<T>()` 形态。

### D5. 属性系统语义与 WPF 可观察性差异

- 本地值同步求值，但 Style/Template/Trigger/Binding/Animation 贡献**推迟到 `PropertyChanges` 阶段**（`EffectiveValueEngine` 排队，`src/gui/PropertySystem.cpp:1562-2247`；阶段 3，`src/runtime/View.cpp:8313-8322`）。WPF 是 eager。tree attach 后立即读取样式驱动属性可能拿到旧值——对 WPF 开发者是隐蔽坑。
- 每实例 `Base::Vector<EffectiveValueEntry>` + 线性查找（`include/Aero/DependencyProperty.hpp:775-791`）。对象高频属性多时是热点。

**建议**：
- 提供显式 `SynchronizeProperties()` flush API，或至少在 `docs/WPF_QUICK_START.md` 高亮 flush 点；保持 phase-batched 以获得性能；
- 对象少量高频属性走固定槽位 fast path（类似 WPF 的 `EffectiveValueEntry` pool / Noesis 紧凑 per-object 存储），其余走 sparse 表。

### D6. `Result<T>` 噪音

所有 setter 均为 `noexcept + Base::Result<T>`。对库正确，但对 WPF 上手者是噪音（WPF 属性赋值不返回错误）。

**建议**：增加可选"异常包装层"——debug 下 throw、release 下薄断言，公共 ABI 核心保持无异常（不违反 ADR-0001）。

### D7. 公开头 106 个，无 PCH 聚合

NoesisGUI 提供 `NoesisPCH.h` 单头聚合。当前 AeroGUI 需要 `include <Aero/Gui.hpp>` 这类伞形头，但无 PCH 编译路径。

**建议**：在 `include/Aero/AeroPCH.hpp` 聚合全部公开 API，并给 CMake 增加 PCH 选项，显著降低编译时间与上手门槛。

### D8. 主循环 idle 时 `sleep(1ms)` 轮询

`src/app/DesktopHost.cpp:875-877`：无事件时 `sleep 1ms`。Noesis 建议事件驱动阻塞；1ms 轮询在功耗/低延迟上表现差。

**建议**：改为 `WaitEvent` 阻塞 + 帧时钟唤醒，或暴露 `RenderFrameInterval` 策略供宿主选择。

### D9. 文档语言割裂

ADR（`docs/adr/`）与 `docs/spec/RENDERING_PLATFORM.md` 等关键规范为中文，`docs/WPF_CPP_PORT_SPEC.md` 为英文。对非中文贡献者是屏障，且规范语言不唯一会导致解释分歧。

**建议**：正式规范英文化或提供双语索引页，明确"规范以英文为准"。

### D10. 样例过期 / 缺失

`samples/HelloWorld/main.cpp` 引用已删除的 `Aero/RuntimeEnvironment.hpp` 与 `Aero::Presentation`（已核验）；`Menu3D` / `Scoreboard` / `ControlGallery` 目录基本为空。WPF 上手第一件事就是跑样例——这是当前最大的 onboarding 断点。

**建议**：修复或重写 `HelloWorld`（对齐当前 SDK 的 `Aero::App` / `Gui` / `View` API），补齐 `Menu3D` / `Scoreboard` / `ControlGallery`。

### D11. 第三方控件 authoring 指引缺失

`src/controls/metadata/Primitives.inl` 标注为生成式迁移产物，但没有面向第三方控件的 authoring 模板（对应 WPF 的 `CustomControl` 教程 / Noesis 的 `DelayedButton` 范例）。

**建议**：新增 `docs/CONTROL_AUTHORING.md` + 一个最小自定义控件样例，说明：类型注册、DP 声明/注册、RoutedEvent、模板接入、`OnApplyTemplate`、布局与状态。

## 4. 与 NoesisGUI 架构对照

| 关注点 | NoesisGUI | AeroGUI-R | 结论 |
|---|---|---|---|
| 双线程模型（UI / render） | `View`(UI) + `IRenderer::UpdateRenderTree / RenderOffscreen / Render` | `View::Update` + `Aero::Renderer` 同三阶段 | 已对齐，保留 |
| RenderDevice 抽象 | 宿主实现抽象类，`GLFactory::CreateDevice` | `Integration::RenderDevice` opaque + D3D11 / OpenGL33 工厂 | 已对齐；缺 Metal / Vulkan / D3D12 / WebGL2（ADR-0004 已计划） |
| 反射 / 元数据 | `NS_IMPLEMENT_REFLECTION` + `TypeMetaData` / `DependencyData` | `AERO_DECLARE_TYPE` + `Meta::Registry` | 已对齐，宏体系可再完善（D3） |
| 组件工厂 | `RegisterComponent<T>()`（XAML 可实例化） | `Gui::AddModule` + `ModuleRegistration` | 已对齐；可加 `RegisterComponent` 便捷宏（D4） |
| PCH | `NoesisPCH.h` | 无 | 待补（D7） |
| 属性访问 | `NsSet / NsGet` 宏 + typed accessor | `SetX / GetX` + `Meta::Value` | 可借鉴宏（D2） |
| 类库命名空间 | `Noesis` + `NoesisApp`（框架另置） | `Aero` + `Aero::App` | 已对齐 |
| DP 存储 | 紧凑 per-object + fast path | sparse vector + 线性扫描 | 可借鉴 fast path（D5） |
| 渲染线程 | 宿主负责，核心不创建线程 | 核心不创建隐藏线程（README 基线） | 已对齐 |
| 组件/资源 | XamlProvider / FontProvider 可替换 | `Integration::*Provider` 可替换 | 已对齐 |

**结论**：AeroGUI-R 在架构血缘上与 NoesisGUI 模型一致；差距集中在 DX 层（宏、PCH、样例、文档）与后端矩阵。

## 5. 重构建议（按优先级）

### P0 — 上手即用（约 2-4 周）

1. 修复 / 重写 `samples/HelloWorld`，补齐 `Menu3D` / `Scoreboard` / `ControlGallery`（对齐当前 SDK）。
2. 引入 `AeroPCH.hpp` 聚合头 + CMake PCH 选项（D7）。
3. 提供 `NsSet` 风格属性宏（typed + Value 双路径），消除 setter 样板（D2）。
4. 文档双语索引页，明确规范语言（D9）。
5. `aero-xamlc` 落地 `x:Class` + `InitializeComponent()` 生成（先字段注入，再 code-behind）（D4）。

### P1 — 语义收敛与架构清晰（约 1-2 个月）

6. 统一 Renderer / Device 命名（D1）。
7. DP 声明处就近注册宏 + `Meta::Register` 聚合，消除 `.inl` 追踪断裂（D3）。
8. 属性求值延迟语义：加显式 `SynchronizeProperties()` 或文档化 flush 点（D5）。
9. `EffectiveValueEntry` fast path（高频属性固定槽位，替代纯线性扫描）（D5）。
10. 可选异常包装层（debug throw / release 断言），核心 ABI 保持无异常（D6）。
11. 新增 `docs/CONTROL_AUTHORING.md` + 最小自定义控件样例（D11）。

### P2 — 工程化与性能（持续）

12. DesktopHost 主循环改事件驱动阻塞 + 帧策略（D8）。
13. GLX / EGL / WGL adapter 与 GLES3 / WebGL2 后端按 ADR-0004 补齐。
14. 渲染器每帧 constant upload 批量化（当前 `MaxRectangleBatchInstances=64` 封顶）与 clip 常量策略评估（`MaxShaderClips=32`）。
15. 性能门禁：把 conformance harness 的 per-frame 统计接入 CI 基线。

## 6. 关键文件索引

| 关注点 | 文件 |
|---|---|
| 公开门面 Renderer | `include/Aero/Renderer.hpp`、`src/runtime/View.cpp:8659-8896` |
| 私有渲染 / RHI | `src/render/Renderer.cpp`、`src/render/RenderDevice.hpp` |
| 集成设备 | `include/Aero/Integration/RenderDevice.hpp`、`src/integration/RenderDevice.cpp` |
| 帧阶段管线 | `src/runtime/View.cpp:8228-8503` |
| 属性系统 | `src/gui/PropertySystem.cpp`、`include/Aero/DependencyProperty.hpp` |
| 元数据注册 | `include/Aero/Meta.hpp`、`src/controls/metadata/Primitives.inl` |
| 布局 | `src/gui/Layout.cpp` |
| App 框架主循环 | `src/app/DesktopHost.cpp:842-889` |
| 文档 / 规范 | `docs/WPF_CPP_PORT_SPEC.md`、`docs/adr/0001-0004` |
