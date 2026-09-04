# AeroGUI-R 精简化重构总方案（修订版）

> **当前状态**：
> - **已落地**：C1 属性系统优化（1.1–1.3）已验证合入；`Metadata.cpp`、`TemplateProgram.cpp`、`XamlObjectWriter.cpp` 上帝文件已完成首轮模块拆分；老旧 Facet 文档与过渡代码已清理。
> - **待实施**：P0 门禁松绑 → P1 死代码拔除 → P2 属性与元数据收尾 → P3 调度模型纠偏 → P4 渲染单快照攻坚 → P5 上层业务收敛。
>
> **核心原则与红线**：
> 1. **WPF 语义零回归**；对外常用公共 API 零破坏性改名。
> 2. **保留跨线程任务封送能力**：Dispatcher 不得暴力切断外部工作线程调度入场券。
> 3. **每步出口闭环**：Linux Release 全量构建 + `AeroFrameworkConformanceTests` + `CheckArchitecture.cmake` + 领域微基准 / 像素级门禁。

---

## 0. 代码现状与基线规模（实测更新）

### 0.1 最新规模指标
- `include`：461 个 `.hpp`；`src`：154 个 `.cpp` + 私有 `hpp/inl`。
- 门禁脚本：`CheckArchitecture.cmake`（2415 行）；`AeroGuiTargets.cmake`（670 行）；`Meta.hpp`（1848 行）。

### 0.2 上帝文件跟踪对比
| 文件路径 | 初始行数 | 当前行数 | 当前状态与说明 |
|---|---|---|---|
| `src/gui/markup/TemplateProgram.cpp` | 4126 | **16** | ✅ 拆解为 `TemplateCompiler.inl`、`TemplateSupport.inl`、`StyleSupport.inl` |
| `src/gui/meta/Metadata.cpp` | 4025 | **26** | ✅ 拆解为 `MetaTable.inl`、`Registry.inl`、`TypeRegistry.inl`、`BehaviorTable.inl` 等 |
| `src/gui/markup/XamlObjectWriter.cpp` | 7918 | **4830** | 🟡 标记扩展已抽离（`BindingExtension.inl`、`DynamicResourceExtension.inl` 等），仍待进一步解耦 |
| `src/gui/markup/XamlLoader.cpp` | 5110 | 5110 | 待拆分（语法树分析与对象构建混合） |
| `src/gui/markup/XamlSchemaContext.cpp` | 4221 | 4221 | 待拆分（命名空间解析与类型元数据查询） |
| `src/gui/controls/Scroll.cpp` | 3579 | 3579 | 待拆分（`IScrollInfo` 计算与视觉滚动条混合） |
| `src/gui/controls/Items.cpp` | 3408 | 2910 | 🟡 部分模板逻辑移出，仍待解耦容器与生成逻辑 |
| `src/gui/controls/TextBox.cpp` | 3131 | 3131 | 待拆分（文本编辑模型、光标定位与绘制混合） |
| `src/gui/data/Binding.cpp` | 2542 | 2542 | 待拆分（表达式求值与生命周期管理） |
| `src/gui/media/StoryboardHost.cpp` | 2463 | 2463 | 待切分（主逻辑 + Actions/Completions/Events 物理碎片） |
| `src/render/RenderTree.cpp` | 2209 | 2209 | 待优化（每帧全量快照与哈希） |
| `src/gui/media/AnimationEngine.cpp` | 2102 | 2102 | 待拆分（时钟驱动与属性插值通道） |
| `src/render/FrameEncoder.cpp` | 1870 | 1870 | 待消除 $O(n^3)$ 子树遍历与冗余批次计算 |

---

## 1. 已落地成果（C1 + 初始拆解阶段）

### 1.1 `EffectiveValueEngine::Flush` 算法降度 $O(n^2) \to O(n)$
- **代码位置**：`src/gui/core/PropertySystem.cpp:1389`
- **改进**：取消逐轮扫描最小单调序列号的多余 $O(n^2)$ 查找，改为 FIFO 双指针连续消费 + 结束时单次 `compactPrefix` 压缩；支持本轮新入队原地处理；丢弃 `!Queued` 失效条目。
- **关联优化**：简单 Local 属性入队免除 `EnsureRare` 堆分配。

### 1.2 `PropertyProviderSet` swap-remove + Winner 缓存
- **代码位置**：`include/Aero/Diagnostics/PropertyValueSource.hpp`
- **改进**：
  - `RemoveAt` 从 $O(n)$ 移动压缩改为 $O(1)$ 末尾元素交换。
  - 新增 `winner_` 缓存下标，热路径 `Winner()` 由 $O(k) \to O(1)$。
  - 增量维护与批量 `RefreshWinner()` 机制分离。
- **微基准实测**：20k contributions 下，`Winner()` 由 87µs 降至 0µs；全删操作由 523µs 降至 352µs。

### 1.3 `PropertyProviderSession` Hash 索引化
- **代码位置**：`src/gui/core/state/PropertyEngine.hpp:230`
- **改进**：
  - `setterRecords_` / `states_` 均转换为哈希表查找，彻底消除线性扫描。
  - `ObjectState` 冗余清理，`PruneState` 降为 $O(1)$。

### 1.4 上帝文件首轮物理瘦身
- `TemplateProgram.cpp`、`Metadata.cpp`、`XamlObjectWriter.cpp` 成功分离出 15+ 个聚焦模块文件，编译边界初步建立。

---

## 2. P0 基础设施：门禁松绑与测试环境固化（后续一切的前置）

- **P0.1 `CheckArchitecture.cmake` 规则升级**：
  - 将 `require_file` 物理路径硬编码白名单降为**语义/接口断言**。
  - 删除对已拆分或拟合并文件（如 `StoryboardHost.Actions.cpp`、`ViewDocuments.cpp`、`InteractivityEngine.*`）的硬性绑定（约 501–562、648–669、755–912 段）。
  - 保留并加固 `forbid Detail/Runtime/Impl/RenderSurface/GraphicsDevice` 等核心分层命名约束。
- **P0.2 消除测试环境路径敏感性（CI 定时炸弹）**：
  - 修复 `AeroFrameworkConformanceTests` 强依赖执行目录（CWD）在源码根目录的缺陷，统一采用基于 `AERO_SOURCE_DIR` 宏或统一测试资源基准路径解析，确保 CTest 任意工作路径均可 100% 稳定通过。
- **P0.3 统一头文件与目标组管理**：
  - `AeroPublicHeaders.cmake` 支持 umbrella 头文件聚合；
  - `AeroGuiTargets.cmake` 17 组细粒度源文件合并归并为 5 组（`core / ui / markup / media_text / render`）。

---

## 3. P1 零风险清理与死代码拔除（轻量先行）

- **P1.1 彻底清理 `UnicodeAnalysis` 孤岛**：
  - `src/gui/text/UnicodeAnalysis.cpp` 经实测全网零调用，直接自研代码已由 `TextLayout` 承载，连同 CMake 配置彻底移除（减负约 300 行）。
- **P1.2 统一字素集（Grapheme Cluster）判定**：
  - 合并 `TextLayout`、`EditableText` 中的重复实现，补全阿拉伯语 Combining 字符范围（`0x064B-065F`、`0x0670`）。
- **P1.3 清理 `ViewFrame` 虚假阶段轮询**：
  - 删除 `BeginFrame`、`Input`、`EndFrame` 等无注册 Hook 但每帧空跑耗时计量的无用驱动开销。

---

## 4. P2 属性系统与元数据收尾（C & M 系列深化）

- **P2.1 `StoredValueEntry` 内联 Local 值**：
  - 在 `StoredValueEntry` 中直接内嵌基础 `PropertyValue inlineLocal`，使 90% 简单属性读写完全免于 `StoredValueRare` 堆分配。
  - `DependencyObject::valueStore_` 统一为 Engine 句柄。
- **P2.2 侵入式队列链表**：
  - 在 Entry 级增加 `queuedNext` 指针，以侵入式单链表彻底替代 `EffectiveValueEngine::pending_` 动态向量，彻底消灭队列扩容分配开销。
- **P2.3 Registry 单一事实源**：
  - `DependencyPropertyRegistry` 降为 `TypeRegistry` 的轻量只读视图。
  - 属性别名在 `Register/AddOwner` 时完成规范化（Canonicalization），移除 `DependencyObject.cpp` 运行时的二次查表与 `CanonicalPropertyKey` 动态映射。
- **P2.4 线程局部池收归 Engine Slab**：
  - 将 `StoredValueRarePool` 与 `PropertyStorePool` 从全局 `thread_local` 转换为 Dispatcher/Engine 拥有的 `SlabAllocator`，生命周期随引擎严格绑定，避免线程穿透与跨线程泄露。
- **P2.5 `Meta.hpp` 公共立面精简**：
  - 公共头文件仅暴露常用的 6 个流畅配置入口，其余 30+ 内部实现重载与 `#ifdef AERO_GUI_IMPLEMENTATION` 严格移入私有头文件。

---

## 5. P3 调度器与线程模型纠偏（D 系列理性重构）

> **纠偏说明**：原计划过度裁撤任务队列与互斥保护，将导致 AeroGUI 彻底丧失外部工作线程向 UI 线程封送回调的标准途径。本阶段旨在**去过度设计，保核心能力**。

- **P3.1 跨线程任务队列轻量化（保留并精简）**：
  - **剥离过度设计**：移除 10 级 priority 复杂优先级排序及 delayed 压缩算法（`Insert/Promote/Compact/Discard` 等 500+ 行死重代码）。
  - **确立核心契约**：保留极简、线程安全的 MPSC（多生产者单消费者）任务队列，支持 `Post`、`PostDelayed` 与 `Cancel`。
  - **打通主循环驱动**：在 `src/app/DesktopHost.cpp` 的帧循环开始处正确挂接 `ProcessPending()` 调用，闭环异步与定时器回调能力。
- **P3.2 消除双重调度与 Hook 表**：
  - 废弃 `Dispatcher::RunFramePhase` 间接 Hook 机制；
  - `ViewFrame` 改为直接、显式、顺序调用各子引擎（`AnimationEngine::Flush`、`BindingEngine::Flush`、`LayoutEngine::Flush`、`RenderTree::Flush`）。
  - 合并 DataBind 两个离散阶段为单次调用。
- **P3.3 防御机制收敛**：
  - `CheckAccess` / `VerifyAccess` 双重防御逻辑精简，跨 Dispatcher 归属检查收敛至 `Attach/Initialize` 一次性完成。
  - `DispatcherReentrancyGuard` 与 `MutationScope` 合并；`FrameTimings` 按需采样移入 Inspector。

---

## 6. P4 渲染单快照攻坚（R 系列核心优化，Scheme B 收尾）

> **质量守护要求**：本阶段为风险最高的视觉核心层，必须在实施前建立**端到端像素对比门禁（Pixel Diffs / Golden Images）**，杜绝渲染回归。

- **P4.1 根治 `FrameEncoder` 遍历退化（$O(n^3) \to O(n)$）**：
  - 彻底重写 `UiFrameEncoder::Record`：移除 `parentIndexes` 倒序数组与 `isInSubtree` 递归检查。
  - 引入显式树深度/递归遍历栈，在单次深度优先遍历中直接流式录制子树指令与作用域。
- **P4.2 热路径去重与哈希瘦身**：
  - `ValidateRenderFrame` 中移除每帧全量双循环 $O(n^2)$ 节点 ID 检查，改为构建时局部前置断言或 Debug-only 检查。
  - 优化 `RenderFrame::StableHash`，渐变 ramp 仅参与版本哈希，不再逐帧遍历 1024 字节像素。
- **P4.3 消除每 Commit 全树物化快照**：
  - `DescribeVisual` 作为唯一视觉事实源；
  - `Commit` 仅刷新 dirty DisplayList 与 `FrameDescriptor`，不再拷贝整棵树的镜像。
- **P4.4 后端管线与状态机归一**：
  - D3D11 与 OpenGL33 抽取共享的 `StateCache`（融合 Shader 分支、Sampler、Blend/Stencil 状态）；
  - 清理 `D3D11RenderContext.cpp` 中的历史调试代码；
  - 几何路径合并：统一 `Fill/Stroke -> DisplayList` 单入口，消除 `DrawGeometry` 的二次冗余 Flatten。

---

## 7. P5 文本、输入与上层模块收尾（T / I / V / B / X / S / Z）

- **P5.1 文本管线收敛 (T)**：
  - 回退字体链（Fallback Faces）去重：消除 PipelineState、Headless、Proxy 间的 4 份拷贝，全链路统一为 `Span` 视图。
  - `TextBlockLayout` 与 `TextRenderer::ShapeAndPrepare` 扁平化，字形排版与 GPU 上传职责清晰分离，移除虚假 GC。
  - HarfBuzz / FreeType 适配器解耦，Shaper 接口允许缺省降级。
- **P5.2 输入路由与故事板分治 (I)**：
  - 缩短输入事件转发层级：`DesktopHost` 直通 `InputRouter`，消除 `ViewInput` 中 12 个空壳包装函数。
  - `StoryboardHost.cpp`（2463 行）与配套物理切片文件整合成两个清晰组件：`StoryboardTimingEngine`（时钟计算）与 `StoryboardRuntime`（属性作用器）。
  - 全局 Command 路由表瘦身，直调 `CanExecute/Execute`。
- **P5.3 视图、数据绑定与资源层收尾 (V / B / X / S / Z)**：
  - `ViewState` 扁平化：收敛 25 个离散指针，消除 `GuiState`、`ViewState`、`ElementTree` 间的平行链条。
  - `BindingPath::Compile` 建立全局 `(rootType, path, schemaHash)` 编译缓存，避免重复解析。
  - `ResourceHost` 4 字典合并为层级化 `Environment`；
  - 继续推进剩余上帝文件（`XamlLoader.cpp`、`XamlSchemaContext.cpp`、`Scroll.cpp`、`TextBox.cpp`）的结构性拆分。

---

## 8. 推荐实施拓扑与推进节奏

```mermaid
graph TD
    P0[P0: 门禁松绑 + 测试 CWD 路径解耦] --> P1[P1: 零风险清理: 删 UnicodeAnalysis / 字素集统一 / ViewFrame 空阶段精简]
    P1 --> P2[P2: 属性系统与元数据收尾: Local内联 / 侵入式队列 / TypeRegistry合一]
    P2 --> P3[P3: Dispatcher 纠偏: 轻量 MPSC 队列 + 接入主循环 Pump + 去双重调度]
    P3 --> P4[P4: 渲染攻坚: 消除 FrameEncoder O(n³) / 去全树快照 / 像素门禁守护]
    P4 --> P5[P5: 文本 / 输入 / 绑定 / 剩余上帝文件拆解]
```

### 退出与验收矩阵
| 阶段 | 核心验证重点 | 出口门禁标准 |
|---|---|---|
| **P0** | CMake 检查松绑，测试执行脱敏 | 任意目录运行 `AeroFrameworkConformanceTests` 100% Pass，`CheckArchitecture` 通过 |
| **P1** | 死代码彻底清空，文本边界修复 | 全量 Release 构建零警告，Text Conformance 测试通过 |
| **P2** | 属性读写零多余分配，注册表合一 | Property 微基准（吞吐与分配计数）、XAML 全量加载用例通过 |
| **P3** | 跨线程异步调度通路正常，主循环无空耗 | 增加外部线程 `Post` 验证用例，帧调度阶段耗时显著下降 |
| **P4** | 复杂树渲染帧率提升，消除嵌套扫描 | 像素级比对（Pixel Diffs）零误差，Release 帧率基准提升 |
| **P5** | 架构整洁度达标，上帝文件彻底降权 | 架构行数预算检查，SDK 示例与 samples 编译运行无瑕疵 |
