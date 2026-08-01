# XAML Document Cache and Full-Document Reload

本文记录 AeroGUI 开发期 XAML 缓存、依赖失效和完整文档热重载的产品契约。
该机制不对已实例化对象图做原地字段补丁，也不在后台线程隐式修改 Runtime。
调用方在自己的开发循环中显式调用 `Poll()` 或 `NotifySourceChanged()`。

## 目标

- 相同 XAML source revision 重复加载时跳过 XML tokenization 和 node 编译。
- 缓存内容只包含可重放 AXIR，不保存 View 对象、BindingEngine 或 GPU 状态。
- 一个 `Gui` 的缓存可以供多个 `View` 复用；调用必须位于同一宿主线程，或由宿主提供外部同步。
- ResourceDictionary `Source` 等依赖形成正反向 URI 图。
- 任意依赖变化时，可以确定性失效所有受影响的上层文档。
- 新文档先完整加载，成功后再替换已挂载文档。
- 新挂载失败时尝试恢复旧文档。

## 缓存模型

`XamlDocumentCache` 使用规范 `ResourceUri`、source revision 与 source-provider
identity 作为命中条件。不同 View 即使对同一 URI 返回相同 revision，只要它们的
provider identity 不同，就不会重放彼此的缓存内容。缓存项保存：

- serialized AXIR
- source revision 与 provider identity
- canonical origin URI
- dependency URI list
- LRU access sequence

每次命中仍会用当前 `Meta::Registry` 反序列化并检查 AXIR identity，因此旧
Schema 或旧 cache format 不会被静默重放。缓存写入是优化操作：一次 XAML
加载已成功时，缓存分配或编译失败不会反向破坏该加载结果。

缓存提供 entry-count 和 compiled-byte 双预算。超出预算时按最久未访问顺序
淘汰。依赖图与实例对象分离；缓存永远不保存 `UiDocument` 或 View 指针。

## Revision probe

`Integration::ISourceProvider::Revision()` 是可选能力：

- Embedded provider 直接返回注册 revision。
- File provider 使用文件大小和最后写入时间生成 revision。
- callback provider 可以提供独立 revision callback。
- 不支持 probe 的 provider 返回 `Unsupported`，Loader 自动回退到 `Load()` 并
  对 source bytes 计算内容 hash。

当 revision probe 与缓存匹配时，Loader 不读取 source body，直接重放 AXIR。

## 依赖图

`XamlDependencyGraph` 保存：

```text
Document URI -> consumed dependency URIs
Dependency URI -> dependent document URIs
```

`CollectAffected(changed)` 从 changed URI 沿反向边遍历，返回 changed source 与
全部传递依赖者。Cache invalidation 使用同一结果从最外层 dependent 向 changed
source 逆序删除，避免提前丢失反向边。

## 文档副作用所有权

成功 XAML load 产生的 Binding 和 DynamicResource 不会立即写入 View service。
`XamlLoadSession` 只把 deferred effect plan 移入 `XamlLoadResult` 和 `UiDocument`；
`View::SetContent()` 在视觉树、UI service 与交互注册全部成功后提交
这些 effects。提交中途失败会逆序撤销已提交项。

文档清理顺序为：

1. 若已挂载，逆序撤销 Binding / expression effect
2. 释放声明式 content edges
3. 释放 NameScope、resources 与 root object

未挂载文档只清理 pending expression/context，不会调用已经销毁的 View manager。
每个 `UiDocument` 记录创建它的 View lifetime，跨 View 挂载被明确拒绝；View
shutdown 会先使该 lifetime 失效，从而避免晚释放 document 访问悬空 service。

## 完整文档替换

`View::SetContent()` 接受一个已经成功加载的 `UiDocument`：

1. 验证 replacement 属于当前 View，且 root 是 Visual。
2. replacement effects 保持 deferred，不影响当前文档。
3. 卸载旧视觉树，但暂时保留旧 document 所有权。
4. 挂载 replacement，并在挂载完成后提交 effects。
5. 成功后清理旧 document；失败时清理 candidate 并重新挂载旧 document。

该模型保证解析、资源依赖和对象实例化错误不会触碰当前 UI。替换窗口只覆盖
实际 visual mount 切换，不声称是跨 GPU present 的无缝双缓冲交换。

## Reload coordinator

```cpp
Aero::Integration::ReloadCoordinator reload(view.Host());
reload.Start("Views/Main.xaml", {1280.0f, 720.0f});

// 由宿主开发循环显式调用。
auto result = reload.Poll(diagnostics);
```

也可以由 IDE、文件监听器或 Asset Database 主动通知：

```cpp
reload.NotifySourceChanged(changedUri, diagnostics);
```

Coordinator 会：

- 检查已跟踪 root/dependency revisions
- 通过反向依赖图失效受影响 cache entries
- 从 root URI 构建新的 `UiDocument`
- 调用 `View::SetContent()`
- 成功后刷新 dependency/revision snapshot

AeroGUI 不创建隐藏线程，也不规定文件监听实现；桌面、游戏引擎、移动平台和
浏览器宿主可以使用各自的事件来源。Coordinator 的所有调用必须发生在
`View` 所属线程；跨线程文件事件应先投递回该线程。

## 当前边界

本阶段提供完整文档替换，不包含：

- 保留控件实例的细粒度 tree diff
- 自动迁移局部 UI state
- 多窗口同时提交的全局事务
- GPU frame-level 双场景无缝切换
- 后台线程解析或隐式定时器

后续可在 `UiDocument` 之上增加 state capture/restore，但不应把该策略放回
`XamlObjectWriter`。
