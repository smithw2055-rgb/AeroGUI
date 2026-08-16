# Render Pipeline Simplification (Scheme B)

本文记录 AeroGUI 渲染管线的简化方案 B（参照 NoesisGUI 结构的直接遍历录制）。
方案已按"一次完成、不分 PR1/PR2"实施（见下文"实施结果"）；本文件保留为
设计基线，行号注释指向实施前版本。

## 背景

当前渲染链共 5 层：

```text
retained UI tree
  -> RenderTree::Commit()        物化平面 RenderFrame（nodes + 拼接 commands + ramp）
  -> UiFrameEncoder::Record()    从 parentId 反推 transform/clip/opacity/effect 栈 + 父链扫描
  -> RenderBatch                 一次 device draw
  -> D3D11 / OpenGL33 RenderDevice
```

`RenderFrame`（`src/render/RenderTree.hpp:129-160`）是每次 Commit 重建的线性快照；
`UiFrameEncoder`（`src/render/FrameEncoder.cpp`，3050 行）消费该快照时：

- 用线性 parentId 查找重建 `frameGeometry`（FrameEncoder.cpp:940-971，最坏 O(n²)）；
- 用父链扫描计算每个 effect 子树的 surface bounds（FrameEncoder.cpp:1002-1046）；
- 逐节点重复 id 校验与全量防御校验（FrameEncoder.cpp:1405-1499）。

NoesisGUI 的可见结构（`NsRender/RenderDevice.h` 单接口 + `Batch`；私有 Renderer
遍历 retained 树直接 emit `Batch`，跨线程快照走 `IRenderProxyCreator`/
`RenderTreeUpdater` 保留式代理）证明：**扁平的线性帧 + 独立 encoder 栈反推不是
retained 渲染的必要层**。AeroGUI 单线程现状下可以直接从保留树录制。

### 关键事实

- **单线程**：`ViewRenderer::renderThread_`（`src/gui/ViewRenderer.hpp:58`）只保存
  线程 id，仓库没有创建任何 `std::thread`。`RenderOffscreen`/`Render` 与
  `View::Update` 运行在宿主同一线程，直接遍历保留树录制是安全的。
- **conformance 测试缝**：`tools/conformance/main.cpp` 用
  `CurrentFrameForConformance(view)` 做诊断打印（main.cpp:1044-1065），并用独立
  `UiFrameEncoder::Record(frame, target)` 离线渲染到 probe 后端做像素 readback
  （main.cpp:633-669 `RenderAndReadback`）。这是 DisplayList 正确性的核心验证
  资产，不能删除。

## 目标架构

```text
retained UI tree (Visual + 缓存的每视觉 DisplayList)
  |-- Commit()         刷新 dirty DisplayList + 计算 FrameDescriptor(version/尺寸/ramp/计数)   [UI 线程]
  |-- RecordTree       递归遍历携带继承状态：PushVisual -> emit DisplayList -> 子节点 -> PopVisual  [宿主线程]
  |-- CaptureDiagnosticFrame()  按需生成旧 RenderFrame（仅离线录制与诊断） -> UiFrameEncoder::Record
  v
RenderBatch -> RenderDevice（不变）
```

生产热路径不再物化平面 `RenderFrame`，不再从 parentId 反推状态；`RenderFrame`
降级为"按需诊断/离线录制快照"，仅 conformance 与 Inspector 使用。

## 实施步骤

### PR1 — 渲染核心（P1-P3）

**P1 节点状态单一事实源**
- 将 `BuildSubtree` 中的 mask 采样 / effect 快照 / transform / opacity / blend
  抽取（`src/render/RenderTree.cpp:1469-1663`）为
  `RenderTree::DescribeVisual(visual) -> NodeRenderState`。
- 生产录制与诊断快照共用同一实现，消除两套描述逻辑。

**P2 FrameDescriptor + 停止物化**
- `RenderTree::Commit()` 仅刷新 dirty DisplayList 并维护 `FrameDescriptor`
  （version、logical/pixel/dpi、nodeCount、commandCount、gradientRamp 列表）。
- 删除 `RenderNodeSnapshot` 平面化与命令拼接；`RenderFrame` 改由
  `CaptureDiagnosticFrame()` 按需生成。
- 渲染顺序等价性：现实现"父节点命令先于子节点命令"（RenderTree.cpp:1675-1712），
  遍历录制天然保持同序，无需命令流。

**P3 遍历录制（核心）**
- `UiFrameEncoder` 新增 `RecordTree(RenderTree&, FrameTarget)` /
  `RecordTreeOffscreen` / `RecordTreeOnscreen`，以递归 + 即时状态栈替代
  `frameGeometry` 反推（FrameEncoder.cpp:940-971）与 effect 边界父链扫描
  （FrameEncoder.cpp:1002-1046，改为 effect 子树的后续遍历累积 bounds）。
- `PushVisual`/`PopVisual` 栈内复合 transform、交 clip、乘 opacity、跟踪 effect
  归属（替代 FrameEncoder.cpp:1405-1499 的 nodes 重建与重复 id 校验）。
- 保留：批合并（矩形实例 / 同纹理 image 合并）、mask pass、effect offscreen
  surface 生命周期、gradient ramp 上传、`LastStatistics` 语义。
- `RenderTree::Record` 负责遍历，复用 `BuildSubtree` 的
  invisible/overlay/popup 过滤规则（RenderTree.cpp:1684-1712）。

**PR1 出口条件**：`RecordTree` 与旧路径并存，旧 conformance 全部通过（离线
`Record(frame, target)` 路径不动）；以像素门禁与 10k 虚拟化基准为回归基线。

### PR2 — 接线与门禁（P4-P6）

**P4 设备层与 View 接线**
- `RenderDeviceBase::BeginSurfaceFrame` / `CompleteSurfaceFrame`、
  `RenderDevice::Analyze` 改收 `FrameDescriptor`（仅用 version/commandCount）。
- `RenderTargetServices::Render(target, renderer, frame)` 改收 descriptor
  （`src/render/RenderTarget.cpp:97`）。
- `ViewRenderer::UpdateRenderTree` / `RenderOffscreen` / `Render` /
  `RenderOnscreenFrame` / `RenderOffscreenFrame`：版本比较改用 descriptor，
  `RenderOffscreen`/`Render` 调 `RenderTree::Record`。
- `View::Update` 版本比较改用 descriptor（`src/gui/View.cpp:8649-8681`）；
  `ViewState::CurrentFrame` 返回 descriptor，新增 `CurrentDiagnosticFrame`。

**P5 conformance 与诊断适配**
- `tools/conformance/main.cpp`：诊断打印与 `GradientRamps()` 校验改用
  `CurrentDiagnosticFrame`；离线 `RenderAndReadback` 路径保留；新增走生产
  `RecordTree` 的像素门禁用例。
- `Inspector.hpp:124-126`、`ProductionDiagnostics.hpp:279` 适配 descriptor /
  诊断快照。

**P6 清理与文档**
- 删除生产路径死代码：平面命令拼接、`currentFrame_` -> `descriptor_`、
  `ValidateRenderFrame` 收敛为诊断专用。
- 更新 `docs/ARCHITECTURE.md` 渲染管线段（替换 89-96 的五层描述）。

**PR2 出口条件**：生产路径全量走 `RecordTree`，`aero-conformance`（像素门禁、
background-blur、nested-effect、设备丢失/恢复）全绿，公开 ABI（`IRenderer`
形态）不变。

## 实施结果

以下条目已在一次重构中完成（对应代码均落盘，编译/像素门禁在 Windows 环境验证）：

- **P1**：`RenderTree::DescribeVisual(visual, gradientRamps) -> NodeRenderState`
  （`src/render/RenderTree.cpp`）同时服务生产录制与诊断快照。
- **P2**：`RenderTree::Commit()` 只刷新 dirty DisplayList 并维护 `FrameDescriptor`
  （version/logical/pixel/dpi/nodeCount/commandCount/glyphCommandCount/frameHash/
  gradientRamp 列表）；`RenderFrame` 仅由 `CaptureDiagnosticFrame()` 按需生成
  （其尾部调用 `ValidateRenderFrame` 自检）。
- **P3**：`UiFrameEncoder::RecordTree(RenderTree&, FrameTarget)` 以递归 + 即时状态栈
  录制（`TreeBoundsCollector` 后序累积 effect 子树 bounds + live mesh 收集，
  `TreeCommandRecorder` 全树遍历 emit）；effect 顺序为反向 ordinal 逐 surface
  录制 + applyEffect/applyMask，主 pass 合成；`nodePath`/parentId 反推与全量
  重复 id 校验由递归栈 + 祖先链校验替代。`RecordOffscreen`/`RecordOnscreen`/
  `RecordTreeOffscreen`/`RecordTreeOnscreen`/`RequiresFrameSurface`/`viewSurfaces`
  已删除。
- **P4**：`RenderDeviceBase::BeginSurfaceFrame/CompleteSurfaceFrame` 与
  `RenderTargetServices::Render` 已收 `FrameDescriptor`；`RenderOnscreenFrame`
  直调 `RecordTree(Tree(), target)`，`RenderOffscreenFrame` 收敛为版本校验 no-op，
  保留 `RenderOffscreen`→`Render` 顺序与 `offscreenReady_` 门；mesh 退休回收恢复为
  `meshResources_->CollectRetired(frameEncoder_->LiveMeshes())`
  （`MeshGpuResources::CollectRetired` 改收 `Span<const RenderMeshId>`）。
- **P5**：`tools/conformance/main.cpp` 编译破坏修复（FrameDescriptor 适配、
  `VerifyRenderDeviceState` 收 descriptor、`frame` 作用域修正）；新增生产等价像素门
  `VerifyTreeEquivalenceD3D11`（离线 `Record` readback A vs 生产 `RecordTree`
  readback B，容差 2）。`ProductionDiagnostics.hpp:288-291` 已适配 descriptor
  （frameHash）。`View.cpp` 新增 `RenderTreeForConformance` 缝。
- **P6**：`CurrentFrame`/`currentFrame_`、`Record*` 门面、`viewSurfaces` 已删除。

**待 Windows 验证**：整库编译（out/build Ninja + build/ vcxproj，目标
`aero-conformance`）与像素门禁全绿；AeroLogin 0xC0000005 崩溃复测。

## 开放决策

- **诊断缝保留**（推荐）：保留 `RenderFrame` + `UiFrameEncoder::Record` 作离线 /
  门禁缝，生产路径彻底走 `RecordTree`；像素门禁零改写。
- **彻底迁移**：连离线缝一并删除，conformance 全改走生产路径；改动面更大，
  需重写像素门禁调用。

## 风险与验证

- 效果/offscreen 边界：由全局扫描改为递归子树累积，需 background-blur /
  nested-effect 门禁重点回归。
- 渲染顺序：父先于子的既有序保持；用 10k 虚拟化基准验证无回归。
- 未来 M4 双线程：直接遍历依赖单线程；届时在 `RecordTree` 入口加
  "保留树 + generation 快照" 代理（Noesis `RenderTreeUpdater` 模式），本步结构不变。

## 不变项

- `RenderBatch` / `UiDrawContext` / 后端单 TU（D3D11、OpenGL33）。
- `DisplayList` 缓存与 dirty 失效机制。
- `IRenderer` 公共 ABI 形态（`UpdateRenderTree` / `RenderOffscreen` / `Render`
  语义）。
- 像素门禁的离线录制缝。
