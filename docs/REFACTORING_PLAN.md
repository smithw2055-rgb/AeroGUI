# AeroGUI-R 精简化重构总方案（修订版）

> **当前状态**（tree-aligned, branch `cursor/wpf-kernel-no-facet-f940`）：
> - **已落地（P0–P3 + 渲染/绑定首轮 + 导航拆分）**：
>   - **P0 / P1**：`CheckArchitecture.cmake` 门禁解耦；测试 CWD 解耦（`AERO_TEST_SOURCE_DIR`）；死代码 `UnicodeAnalysis` 物理删除；字素 Combining 字符集统一。
>   - **P2 / 属性系统**：`Flush` FIFO 双指针；Winner $O(1)$；`inlineLocal` 内联 Local；侵入式队列 `QueueLink::next`；`PropertySlab`；`TypeBuilder` 收拢。
>   - **P3**：FIFO 跨线程 MPSC + `ProcessPending()`；`ViewFrame` 显式直调子引擎；**已删除** 未使用的 `Dispatcher::RegisterFrameHook` / `RunFramePhase` / hook 表（保留 `ProcessPending` / 跨线程 `Post`；`FrameTimings()` 仍供 Inspector 读取空快照）。
>   - **渲染 / 绑定首轮**：`RenderTree::RefreshInPlace`；`FlattenGeometryContours`；`BindingPath::Compile` 全局哈希缓存。
>   - **上帝文件导航拆分 → 真 TU**：Scroll / Binding / XamlLoader / XamlSchema / Storyboard / TextBox 已升格；`TemplateProgram` 本轮升格为 `StyleSupport.cpp` / `TemplateSupport.cpp` / `TemplateCompiler.cpp`（删除 amalgam 宿主与对应 `.inl`）。
>   - **XamlObjectWriter 真 TU 拆分**：独立 `.cpp` 编译单元；本轮将过细 `XamlScopes.cpp` 并回 `XamlObjectWriterInternal.cpp`（保留 MarkupEval / PropertyApply / BuilderCore / NameScope 等中等拆分）。
> - **待实施 / 剩余缺口**：
>   - **XamlObjectWriter 周边**：扩展实现仍以 `XamlMarkupExtensions.cpp` + `*Extension.inl` 单 TU 包装（有意保留，暂不碎成一对一 cpp）。
>   - **Items.cpp → 真 TU（B3）**：`Items.cpp`（ItemsControl/ItemCollection 核心）+ `ItemContainerGenerator.cpp`（Runtime/facade/Create）。
>   - **Amalgam `.inl` → 真 TU（已落地）**：
  - Scroll / Binding / XamlLoader / XamlSchemaContext / StoryboardHost / TextBox（同上轮）
  - **TemplateProgram（本轮）**：`StyleSupport.cpp` / `TemplateSupport.cpp` / `TemplateCompiler.cpp`
>   - **过细 TU 回并（本轮）**：`XamlScopes`→`XamlObjectWriterInternal`；`AnimationEngine.Clock`→`AnimationEngine.cpp`；`BindingObjects`→`Binding.cpp`。保留 `AnimationEngine.Apply.cpp` 与 Writer 中等拆分。
>   - **共享 GPU StateCache**：`src/render/common/StateCache.hpp`；D3D11/OpenGL33 DrawBatch 共用 blend/depth-stencil/sampler/pipeline keys（本轮 B2）。
>   - **ClipToBounds FBO**：offscreen 目标按 size/format 池化复用；opacity≤0 跳过 offscreen（本轮 B2）。
>   - **Meta.hpp shrink（B4）**：`Meta.hpp` 收为 façade；`MetadataAuthoringSession`/helpers → `src/gui/meta/TypeBuilderDetail.hpp`；registry-only 记录 → `MetadataRegistrations.hpp`；`AERO_GUI_IMPLEMENTATION` 稀有 overload → `TypeBuilderInternal.inc`（经 `aero-meta-authoring` 暴露，不进 `include/Aero` 白名单）。
>   - **samples/ missing**：本树无 `samples/` 目录。
>   - **Metadata / ControlsMetadata 注册表 `.inl`**：有意保持 `#include` 表聚合，**不**拆成大量 cpp（本轮明确不碰）。
>   - `Metadata.cpp` 等若仍有非注册表 `.inl` 聚合，可再升格；TemplateProgram 已升格。

> **核心原则与红线**：
> 1. **WPF 语义零回归**；对外常用公共 API 零破坏性改名（死 API 删除除外）。
> 2. **保留跨线程任务封送**：`Post` / `ProcessPending` + `TestDispatcherCrossThreadPost`。
> 3. **每步出口闭环**：全量构建 + `AeroFrameworkConformanceTests` + `CheckArchitecture.cmake`。

---

## 0. 正确命名（与树一致）

| 文档旧称（勿再用） | 树内真实符号 |
|---|---|
| `FastRefreshInPlace` | `RenderTree::RefreshInPlace` |
| `FlattenGeometrySharedCore` | `FlattenGeometryContours` |
| `queuedNext`（Entry 链） | `EffectiveValueEngine::QueueLink::next` |

---

## 1. 已完成范围（P0–P3 及首轮跟进）

### 1.1 属性系统（P2）
- `EffectiveValueEngine::Flush` FIFO 双指针；Winner swap-remove + 下标缓存。
- `StoredValueEntry::inlineLocal`；`QueueLink` / `QueueLink::next` 侵入式队列；`PropertySlab`。

### 1.2 调度与帧驱动（P3）
- 跨线程 FIFO MPSC；`ViewFrame` 直调 `Flush` / `DataBindHook` / `AnimationFrameHook` / `LayoutHook` / `RenderCommitHook`。
- **死 API 删除**：`RegisterFrameHook`、`RemoveFrameHook`、`RunFramePhase`、`RegisteredFrameHookCount` 及 hook 存储；`ProcessPending` / `Post*` 完整保留。

### 1.3 渲染与绑定首轮
- `RefreshInPlace`；`FlattenGeometryContours`；`BindingPath::Compile` 缓存。

### 1.4 导航级 `.inl` → 真 TU
- Scroll / Binding / XamlLoader / XamlSchema / Storyboard / TextBox：已升格。
- **TemplateProgram（本轮升格）**：

| TU | 职责 |
|---|---|
| `markup/StyleSupport.cpp` | Style / HierarchicalDataTemplate 等支持逻辑（原 `StyleSupport.inl`） |
| `markup/TemplateSupport.cpp` | ControlTemplate 蓝图宿主与资源解析（原 `TemplateSupport.inl`） |
| `markup/TemplateCompiler.cpp` | 模板编译器主体（原 `TemplateCompiler.inl` ~2687） |

已删除 amalgam 宿主 `TemplateProgram.cpp` 与上述 `.inl`。

### 1.5 XamlObjectWriter → 真编译单元 + 过细回并
| TU | 职责 |
|---|---|
| `XamlObjectWriterInternal.{hpp,cpp}` | WriterDetail 共享辅助 + **`NamespaceScope` / `ResourceResolver`（原 XamlScopes 已并入）** |
| `XamlObjectWriterBuilderCore.cpp` | ObjectBuilder 加载与节点栈（Start/End Object/Member） |
| `XamlObjectWriterPropertyApply.cpp` | 属性/内容写入、Complete*、WriteValue* |
| `XamlObjectWriterMarkupEval.cpp` | `ParseMarkupValue` / `EvaluateMarkupExtension`（稳定边界，保留） |
| `XamlObjectWriterNameScope.cpp` | 名称/资源作用域、命名空间、事务、延迟收尾辅助 |
| `XamlObjectWriter.cpp` | `ObjectWriter` + `DeferredContentPlan` 门面 |
| `XamlMarkupExtensions.cpp` | 既有 `*Extension.inl` 合集（**有意单 TU**，暂不一对一碎分） |

### 1.6 B2 Render（本轮）
- `src/render/common/StateCache.hpp`：`BlendStateKey` / `DepthStencilStateKey` / `ShaderPipelineKey` / `SamplerBindKey` + `StateCache::Update*`。
- D3D11 / OpenGL33 `DrawBatch` 经 StateCache 去重状态切换；`SetRenderTarget` / Begin* 时 `Reset()`。
- `UiFrameEncoder` offscreen 池：`BeginOffscreenTargetFrame` + 按 size/mask 复用；保留 opacity≤0 跳过与默认 FBO 不清空语义。
- Debug `VerifyRefresh`：默认关闭；`AERO_VERIFY_REFRESH=1` 全开，`=sample` 每 16 次抽样。

### 1.7 B3 Items / AnimationEngine → 真编译单元（+ Clock 回并）
| TU | 职责 |
|---|---|
| `controls/Items.cpp` | ItemsControl / ItemCollection / ItemsPresenter / Content projection helpers |
| `controls/ItemContainerGenerator.cpp` | ItemContainerGeneratorRuntime、facade、`CreateItemContainerGenerator` |
| `media/AnimationEngineInternal.hpp` | `AnimationEngine::Track` + EngineDetail 插值/时序 helpers |
| `media/AnimationEngine.cpp` | 生命周期、Begin*、Pause/Resume/Seek/Stop/Remove、轨道表 + **Tick/AdvanceBy/FrameHook（原 Clock 已并回）** |
| `media/AnimationEngine.Apply.cpp` | `Ease` + `ApplyTrack` 属性插值通道（保留中等拆分） |

另：`BindingObjects.cpp`（~41 LOC：`BooleanToVisibilityConverter` / `RelativeSource`）已并入 `Binding.cpp`。



### 1.8 B4 Meta.hpp shrink（本轮）
| 路径 | 职责 |
|---|---|
| `include/Aero/Meta.hpp` | 公共 façade：回调/`TypeRegistration`/`Property`/`Field`、`Registration`/`RegistrationValues`、`FrameworkPropertyMetadata`、`TypeBuilder` 公共 fluent、`Register`/`DefineComponentModule` |
| `src/gui/meta/TypeBuilderDetail.hpp` | `MetadataAuthoringSession`、`Create*DescriptionSession`、Value/OrdinaryProperty helpers |
| `src/gui/meta/MetadataRegistrations.hpp` | Registry-only 记录（Enum/Event/Method/Accessor/Notification…） |
| `src/gui/meta/TypeBuilderInternal.inc` | `AERO_GUI_IMPLEMENTATION` 稀有 fluent（raw `Factory`/`Content(MemberId)`/…） |

`AeroGui` 经 `BUILD_INTERFACE:src` + 安装前缀 `include/aero-meta-authoring` 解析 `"gui/meta/..."`；不进入 `AeroPublicHeaders` / `include/Aero` 白名单。

---

## 2. 剩余工作清单

1. ~~**Items.cpp** 解耦生成器与容器~~（B3：Items + ItemContainerGenerator 真 TU）。
2. ~~**AnimationEngine** 拆插值通道~~（B3：Core+Clock 合 TU / Apply 真 TU；Clock 过细已回并）。
3. ~~共享 GPU StateCache / ClipToBounds FBO 池化~~（B2 已落地；可选：池上限与更细的轴对齐 clip 短路）。
4. ~~**Meta.hpp** 瘦身~~（B4：façade + `src/gui/meta` authoring detail；TypeBuilder 公共 fluent 仍在 `Meta.hpp`）。
5. 恢复或补齐 **samples/**（当前树缺失）。
6. ~~将 amalgamated **TemplateProgram `.inl`** 升格为真实 `.cpp` TU~~（本轮：Style/Template Support + Compiler）。
7. 可选：把扩展 `.inl` 本体也迁入对应 `.cpp`，去掉 include 包装（当前仍偏好单 `XamlMarkupExtensions` TU）。
8. **不**拆分 `meta/*.inl` / `controls/metadata/*.inl` 注册表。

---

## 3. 验收出口

- Release 构建 `AeroGui` + `AeroFrameworkConformanceTests`。
- 运行 conformance 二进制；架构门禁持续 Passed。
- 构建树中可见多个 `XamlObjectWriter*.cpp.o`（及 Scopes / Extension / Internal `.o`）。
- **不**默认开 PR / **不**默认 push。
- 本地 conformance：Writer/调度相关用例通过；`TestTutorialSampleXamlLoadApply` 因树内 **无 `samples/`** 目录而失败（预存缺口，非本轮回归）。
