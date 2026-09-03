# AeroGUI-R 精简化重构总文档

> 状态：C1 步骤 1–3 已落地并验证；其余为待实施计划。
> 约定：WPF 语义零回归；常用公共 API 零破坏性改名；每步出口为
> Linux Release 全量构建 + `AeroFrameworkConformanceTests`（源码目录运行）
> + `CheckArchitecture.cmake` + 对应域基准。

## 0. 基线规模（实测）

- `include` 461 个 `.hpp`；`src` 154 个 `.cpp` + 106 个私有 `hpp/inl`。
- 上帝文件：`XamlObjectWriter.cpp:7918`、`XamlLoader.cpp:5110`、
  `XamlSchemaContext.cpp:4221`、`TemplateProgram.cpp:4126`、
  `Scroll.cpp:3579`、`Items.cpp:3408`、`TextBox.cpp:3131`、
  `Metadata.cpp:4025`、`Binding.cpp:2542`、`StoryboardHost.cpp:2463`、
  `RenderTree.cpp:2209`、`FrameEncoder.cpp:1870`、`AnimationEngine.cpp:2102`。
- 门禁：`CheckArchitecture.cmake:2414` 行；`AeroGuiTargets.cmake:662` 行，17 个
  `_aero_gui_*_sources` 分组；`Meta.hpp:1848` 行。

---

## 1. 已落地（C1 属性+Session，2026-09 实施）

### 1.1 `EffectiveValueEngine::Flush` O(n²)→O(n)（`src/gui/core/PropertySystem.cpp:1389`）

- 原因：`pending_` 按单调 `queueSequence` 追加且 `QueueObjectProperty`
  拒重，天然有序；原每轮线性扫描找最小序号属多余 O(n²)。
- 改法：FIFO 头指针消费 + 结束时一次 `compactPrefix` 压缩；
  `Apply` 中新入队条目本轮继续处理（与原 `while(true)` 语义一致）；
  失败时保留失败条目+未处理尾部并返回错误；顺手丢弃已失效
  （`!Queued`）的滞留条目（旧代码永久滞留）。
- 附带：`QueueObjectProperty` 不再调用 `SetQueueSequence`
  （`PropertySystem.cpp:1033`），简单 Local 属性入队恢复零堆分配
  （原每次强制 `EnsureRare` 分配 `StoredValueRare`）。

### 1.2 `PropertyProviderSet` swap-remove + Winner 缓存
（`include/Aero/Diagnostics/PropertyValueSource.hpp`）

- `RemoveAt` 由 O(n) 移位改为 O(1) 末尾交换（`Winner()` 按
  `IsStronger(rank/origin/ordinal)` 全量比较，与顺序无关）。
- 新增 `winner_` 缓存下标：`Winner()` O(k)→O(1)
  （热路径：`RecomputeEffectiveValueCore`、
  `GetAnimationBaseValueInternal` 每次重算各调一次）；
  `Set` 单次比较增量维护；单点删除做换位修正；
  批量删除（`RemoveOrigin/Remove/RemoveRank`）用不维护缓存的
  `EraseAtUnchecked` 循环 + 结束时一次 `RefreshWinner()`。
- 微基准（20k contributions，Release）：`Winner` 87µs→0µs；
  `RemoveOrigin(全删)` 523µs→352µs。
- 边界用例（独立校验程序，全过）：rank/origin/ordinal 优先级、
  删 winner 重扫、批量删、同 token 值替换、`Clear`、尾部 winner 换位。

### 1.3 `PropertyProviderSession` 记录 Hash 索引化
（`src/gui/core/state/PropertyEngine.hpp:230`，公共 API 不变）

- `setterRecords_: Vector` → `HashMap<ContributionKey, Token>` O(1)
  （setter 语义：每对单贡献，重复 Set 复用 token）。
- `triggerRecords_: Vector` →
  `HashMap<ContributionKey, Vector<Token>>`（trigger 语义：多记录堆叠，
  `TriggerEngine::EvaluateTriggers` 先全清再按声明顺序逐个 Set，
  后声明 ordinal 更大即获胜；对内向量通常长度 1）。
- `states_: Vector<ObjectState>` → `HashMap<DO*, ObjectState>` O(1)；
  `ObjectState` 删冗余 `object` 成员，加 `liveContributions` 计数，
  `PruneState` O(1)。
- 删除 `FindRecord/ClearRecords/ClearObjectRecords/HasRecords/
  RemoveState/RemoveAt` 六个 helper；`ClearObjectProviders` 改为
  每表 $<×>次遍历 + `Erase`（tombstone，无 rehash，迭代安全），
  回滚/错误语义与旧代码一致。
- 构建注意：`HashMap::Entry::value_` 为私有，外部经
  `entry->Value()` 访问。

### 1.4 验证记录

| 检查 | 结果 |
|---|---|
| Linux Release 全量构建（AeroGui + tools + tests） | 通过 |
| `AeroFrameworkConformanceTests`（源码目录运行） | pass，与基线一致 |
| `CheckArchitecture.cmake` | passed（含 include-closure 预算） |
| CTest 整包说明 | `AeroFrameworkConformanceTests` 在 CTest 默认 CWD 下失败为**预存环境问题**（用例依赖源码目录相对 `samples/` 路径；切 CWD 不带改动可复现），非回归 |

---

## 2. P0 门禁松绑（后续一切的前置）

- `CheckArchitecture.cmake`：`require_file` 从文件名白名单降为语义断言；
  保留 `forbid Detail/Runtime/Impl/RenderSurface/GraphicsDevice` 词汇检查；
  删除对 `ViewFrame/ViewDocuments/StoryboardHost.*/InteractivityEngine.*
  /TemplateProgram` 具体文件名的强绑定（约 501–562、648–669、755–912 段）。
- `AeroPublicHeaders.cmake`：物理==声明逐文件校验改为 umbrella 校验。
- `AeroGuiTargets.cmake`：17 个 `_aero_gui_*_sources` 合并为 5 组
  （`core / ui / markup / media_text / render`）。
- 新增兼容断言（不锁文件名）：`Controls.hpp` 含 `Button/Grid/ListBox/TextBox`；
  `Gui.hpp` 含 `LoadXaml<T>/LoadComponent/CreateView`；
  `FrameworkElement` 含 `Get/SetWidth/DataContext/FindName/FindResource`；
  `Binding` 含 `Path/Mode/ElementName`。

---

## 3. C系列续项（属性域收尾）

1. **Entry 内联 Local**：`StoredValueEntry` 内嵌 `inlineLocal`，
   90% 简单 Local 免 `Rare` 分配；`DO::valueStore_(void*)` 改 Engine 句柄。
2. **侵入式队列**：`queuedNext` 链替代 `pending_` vector
   （步骤 1 的 FIFO 头指针已铺垫）；删 `queueSequence/nextQueueSequence_`。
3. **Registry 合并**：`DependencyPropertyRegistry` 降为 `TypeRegistry` 视图；
   别名在 `Register/AddOwner` 时 canonical 化，删运行时二次 Find
   （`DependencyObject.cpp:1080-1084, 1114-1119`）与 `CanonicalPropertyKey`
   逐次映射；删 `ToTypeRegistryFlags` 双份 flag。
4. **池合一**：`StoredValueRarePool + PropertyStorePool` 双 `thread_local`
   池 → Engine 内 `Slab<Rare> + Slab<ObjectStore>`。

---

## 4. D系列：Dispatcher/Threading（下一优先级）

现状：`Threading.hpp:279` + `Dispatcher.cpp:1048`；9 Phase
（`Threading.hpp:34-45`）+ 10 Priority（`:20-32`）；7 处
`RegisterFrameHook` 只占 6 Phase（DataBind 被 `Binding.cpp:771` 与
`TriggerEngine.cpp:255` 占两次）；`Begin/Input/End` 零注册仍每帧驱动+计时。

- **D1 零调用任务队列**：`Post/PostDelayed/PostAt/ProcessPending/Cancel`
  在 `src/` 无生产调用方；连带 `ready_/delayed_` 双队列、
  `Insert/Promote/Compact/Discard/ReadyLess/DelayedLess`
 （`Dispatcher.cpp:253-496,803-1024`）、`maxCallbacks`、`DelayOverflow`。
  `ViewFrame` 早已直调引擎。**删。**
- **D2 单线程持 mutex**：`Threading.hpp:157-158` 明示永不建线程，
  却全方法持 `mutex_:222`（约20处）；另有线程令牌
  （`Dispatcher.cpp:14-27,95-97`）、可插拔时钟/唤醒
  （`DispatcherOptions{now/wake}:102-107`，`NotifyWake:978-982`）。
  **删，保留 `ownerThread_` 单断言。**
- **D3 双重调度**：`Dispatcher::RunFramePhase` Hook 表
  （`Dispatcher.cpp:603-728`）与 `ViewFrame.cpp:1098-1163` 硬编码
  `phases[9]` 并存，且顺序不一致（ViewFrame 是
  Animation/Lifecycle/Layout，枚举是 Animation/Layout/Lifecycle）。
  **删空 `Begin/Input/End` 驱动；`DataBind×2` 合一钩；
  `hooks_/activeHook_/phaseHook` 退化为 ViewFrame 直接顺序调用。**
- **D4 防御收敛**：`CheckAccess + VerifyAccess` 双写
  （`Dispatcher.cpp:225-233`，`Binding.cpp:764,797` 典型）；
  跨 Dispatcher 比对 6 处收敛到 `Attach/Initialize` 一次；
  `DispatcherReentrancyGuard`（唯一使用
  `DependencyObject.cpp:745-746`）并入 `MutationScope`；
  `FrameTimings`（唯一消费 `Inspector.cpp:232-234`）移入 Inspector 按需采样。
- 步骤：D-a 任务队列 → D-b 同步原语 → D-c Phase 折叠 → D-d 防御收敛。

---

## 5. R系列：渲染单快照（Scheme B 收尾）

详见 `docs/RENDER_PIPELINE_SIMPLIFICATION.md`。现状仍双付费：

- **R1**：`RenderTree::BuildSubtree`（`RenderTree.cpp:1801-2095`）每 Commit
  全树物化快照（`RenderTree.hpp:111-188`）；`UiFrameEncoder::Record`
  `parentIndexes` 倒序找父 O(n²)（`FrameEncoder.cpp:1593-1631`）+
  `subtree*` 逐节点扫子树；常驻 `StableHash` 全量哈希（`:656-703`）+
  `ValidateRenderFrame` id 唯一 O(n²)（`:733-745`）。
- **R2**：D3D11（`D3D11RenderDevice.cpp:743-838`）与 OpenGL33
  （`:947-1023`）shader 分支、`Sampler64`、Blend/Stencil 状态机同构手写两套；
  `D3D11RenderContext.cpp:301-364` 残留 BMP dump 调试代码。
- **R3**：`RenderContext` 4 标志状态机 + `RenderDevice` 7 虚函数
  （`AeroRender/RenderDevice.hpp:427-450`）+ `RenderTarget::State` +
  `ViewRenderer` 三转发（`ViewRenderer.cpp:168-196`）。
  压为 `BeginSurfaceFrame/CompleteSurfaceFrame(FrameDescriptor)`。
- **R4**：`Flatten` vs `TessellateStroke(14参)` vs `PaintBrush*/SampleBrush`
  vs `DrawGeometry` 内两次 Flatten（`DrawingContext.cpp:276-297`）；
  Clip 网格三处各存；`TextRenderer::ShapeAndPrepare`
  （`TextRenderer.cpp:123-273`）成形+图集+上传+命中区一函数，
  `CollectGarbage=0` 假回收。
- 方向：`DescribeVisual` 单一事实源 → `Commit` 只刷 dirty DisplayList +
  `FrameDescriptor` → `RecordTree` 递归栈直录；后端抽公共 `StateCache`；
  几何合为 `Fill/Stroke→DisplayList` 单入口；像素门禁守护。

---

## 6. M系列：Meta 注册表合一

- **M1 双存**：`TypeRegistry::PropertyInfo`（`MetadataState.hpp:18-30`）vs
  `DependencyPropertyRegistration/DependencyProperty`
  （`DependencyProperty.hpp:366-447`）；一次注册写两处
  （`PropertySystem.cpp:394-530`）；双索引同键；flag 双编码；
  规范键三层别名。方向：DP Registry 降为 TypeRegistry 视图（§3.3）。
- **M2 作者 API 过载**：`Meta.hpp:1848`——`TypeBuilder` ~30 重载、
  `MetadataAuthoringSession` ~21 链式、`FrameworkPropertyMetadata` 14 fluent；
  普通开发者仅需 6 个；`#ifdef AERO_GUI_IMPLEMENTATION` 内部分支泄漏到公头
 （`:1265,1549,1635,1669`）。方向：公头只留立面，其余退私头。
- **M3 文件错配**：`BuiltinModules.cpp:25` 两转发单独立文件 →
  并入 `BuiltinMetadata.cpp`；`Value.cpp:335` 通用转换 → `base/`；
  `Module.cpp:253` 拓扑排序独立为 `ModuleSet.cpp`；
  `Metadata.cpp:4025` 按 `=====` 切 `MetaTable/Registry` 两 `.cpp`。

---

## 7. T系列：文本管线收敛

- **T1 回退链≥4份**：`TextPipelineState::fallbackFaces:687` →
  `Headless` 拷贝（`:539-544,647`）→ `Proxy` 逐调用临时（`:498,510`）→
  `TextLayoutRequest`（`TextLayout.hpp:48-49`）→ `Render::TextConfig`；
  `LoadedFont` 缓存与 `FreeType::FaceRecord` 重复。方向：只留 Pipeline
  一份，余改 `Span` 视图。
- **T2 断行二选一**：`UnicodeAnalysis` 全套**全网零调用**，
  `TextLayout` 自研 `Tokenize/hasRtl/getDirection`；删一套约200行。
- **T3 字素三套且不一致**：`TextLayout:33-59` vs
  `UnicodeAnalysis:204-272` vs `EditableText:48-123`；
  combining 范围两处不一致（缺 `0x064B-065F/0x0670`）。方向：合一。
- **T4 立面重复**：`TextBlockLayout::{Request,Result}` 是 `TextLayout`
  超集扁平化；`HeadlessTextBlockLayout`（`TextPipeline.cpp:528-651`）与
  `TextRenderer::ShapeAndPrepare`（`TextRenderer.cpp:88-224`）各展一遍。
- **T5 可插拔名存实亡**：`friend HarfBuzzAdapter` + `FT_Face` 强转
  （`HarfBuzzAdapter.cpp:108-110`）；`RegisterProvider` 强制三件套同 Identity
  （`FontManager.cpp:82-115`）。方向：shaper 存接口 + 允许缺省。

---

## 8. I系列：输入/动画/故事板

- **I1 三层转发**：`DesktopHost:475-513` → `View::` →
  `ViewInput.cpp:152-351` 12薄包装 → 匿名 Dispatch →
  `InputRouter` → 三 State `Dispatch`。`Escape/Cancel` 双判
  （`ViewInput:92-98` vs `InputState.hpp:371-385`）；focus 两套队列；
  overlay 双份 Vector。方向：Host 直发 `InputRouter`；
  `FocusHost/OverlayHost` 并入 `InputRouter::Flush`。
- **I2 Commands 全局表**：`StaticRoutedCommands/Interned/Resolve*`
  （`Commands.cpp:49-135`）+ 双注入 + `InputRouterOf` 回查循环。
  方向：`ProcessInput` 删除直调 `CanExecute/Execute`。
- **I3 StoryboardHost 按方法名切分**：主文件 2463 行
  （`BeginTimeline:1159-2148` 约990行、17个 `Begin*` 分支、
  `ResolveAnimationProperty:81-1030` 约950行）+ `.Actions:698` +
  `.Events:452`（复刻 Interactivity 条件语义）+ `.Completions:137`
  （轮询完成，本应回调）。方向：4→1（或 Timing+Runtime 2）。
- **I4 三引擎 Flush 同构**（D 系列的延伸）：
  Binding/Trigger/Animation 三套 `pending+Flush+static Hook+Register`
 （`Binding.cpp:770,1579,1825`、`TriggerEngine.cpp:254,295,314`、
  `AnimationEngine.cpp:296,1928-2091`），`ViewFrame:1136-1350` 散调约8处。
  方向：收敛到 Dispatcher Phase 单队列。

---

## 9. V/B/X/S/Z 系列收尾

- **V1 View 碎片**：7 文件 → `View+ViewFrame` 2 文件；
  `ViewState:219-259` 25 指针 → `Gui*+Tree*+FrameArgs`；
  `GuiState/ViewState/ElementTree` 平行指针 → 只存链；
  `DesktopHost` 三角重叠 → Host 只做窗口+上下文+泵。
- **B1 Binding**：双描述符（`BindingEngine.hpp:183-254`）→ 单
  `BindingDescriptor{sourceKind+path}`；`BindingPath::Compile:143`
  按源类型重编 → 按 `(rootType,path,schemaHash)` 全局缓存；
  `MultiBindingProxy:140-152` 额外 DO → 直聚 handles；
  `CollectionView` vs `Selector` currency → 单 DefaultViews。
- **X1 资源**：`Find/TryFindResource×4` → 只留 `ResourceKey` 核心；
  `ResourceHost` 4 字典 → `Environment{layers[]}`；
  `XamlRuntime` 折叠进 `DocumentCache`。
- **S1**：`Templates.cpp:2733` + `Style.cpp:1107` 会话统一（C3 半治后续）；
  `Path.cpp:930` + `Shapes.cpp:711` 几何与控件分离。
- **Z1**：`sdk-consumers/` 11 桩 → 3（Gui/Render/App）。

---

## 10. 公共接口 WPF 兼容红线（精简中不得触碰）

`Get/Set/ClearWidth`、`Click()+=`、`Grid::SetRow`、
`Binding(Path/Mode/ElementName)`、`Gui::LoadXaml<T>/LoadComponent/CreateView`、
`FindName/FindResource`、`Application::Run/StartupUri`、
`OnRender(DrawingContext&)`、`Meta::Register`；`Set(void)+*Checked(Result)+
LoadXaml(Result)` 三轨话术；头聚合只加 umbrella、不删细头（先转发一版本）。

---

## 11. 总执行顺序与出口

```
P0门禁 → C收尾(§3) → D-a→D-b→D-c→D-d → R单快照 → M合一 → T收敛 → I → V/B/X/S → Z固化
```

- 每步：Linux Release 全量构建 + conformance（源码目录运行）+
  `CheckArchitecture` + 对应域基准（帧 Flush / 像素门禁 / 样式应用）。
- D 无语义风险可连续落地；R 需像素门禁守护；M 需等价性测试先行（同 C1 模式）。
