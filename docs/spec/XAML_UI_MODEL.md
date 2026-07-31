# XAML 与 UI 语义模型规范

本章定义 XAML、Resource、Binding、Layout、Routed Event、Style、Template 和首批 Controls。

## 1. XAML pipeline

```text
Byte stream
 -> XML tokenizer
 -> XAML node stream
 -> Schema/type resolution
 -> Object writer
 -> Initialization transaction
 -> Object tree
```

MUST 使用流式 node pipeline，不要求完整 DOM。

首批语法：

- object element、attribute property、property element；
- content property、collection add、attached property；
- `x:Name` / NameScope、`x:Key`、`x:Null`；
- enum、number、color、length、Thickness 等转换；
- XML namespace、base URI 和 source span；
- `StaticResource` 和基础 `Binding`。

后续加入：

- `DynamicResource`、`RelativeSource`、`TemplateBinding`、`x:Type`；
- `x:Class` 与 generated factory；
- merged dictionaries；
- Style/Template/Trigger；
- compiled XAML IR。

## 2. Schema 与 ObjectWriter

`XamlSchemaContext` 把 XML namespace + local name 解析为 TypeInfo，把 member name 解析为 property/event/attached member。

ObjectWriter MUST：

- 使用 metadata factory 创建对象；
- 调用 `BeginInit/EndInit`；
- 维护 object/member stack；
- 支持 deferred content；
- 注册 NameScope；
- 保留 source span；
- 检测 duplicate name、property/collection 冲突；
- 失败时回滚未挂接对象；
- 不调用控件私有 API。

## 3. MarkupExtension

```cpp
class MarkupExtension : public Object {
public:
    virtual Result<Value> ProvideValue(const ServiceProvider&) = 0;
};
```

ServiceProvider 至少提供 target object/property、root、NameScope、schema、base URI、source span 和 ambient resource providers。

Runtime XAML 与 `aero-xamlc` MUST 共享 node/object-writer 语义；compiled XAML 只替换 parsing/type lookup 成本，不改变行为。BAML 不在 v1 范围。

## 4. ResourceDictionary

Key 支持 string、TypeId 隐式 key 和受控可哈希值。

MUST：

- 单 dictionary key 唯一；
- merged dictionaries 后出现者优先；
- 查找检测循环；
- seal 后读取低锁；
- dynamic replacement 更新 generation；
- source URI 进入缓存与诊断。

查找至少经过：

```text
target local
 -> logical ancestors
 -> template/templated parent context
 -> application
 -> theme
 -> system
```

精确顺序用 WPF probes 固化。

`StaticResource` 在加载/模板实例化时解析；`DynamicResource` 保存 local expression，在资源 generation 或树上下文变化时重新求值。

## 5. Binding

Binding 包含 target、source selection、path、mode、update trigger、converter、fallback、validation 和 diagnostics context。

Source 互斥选择：

- explicit Source；
- ElementName；
- RelativeSource；
- inherited DataContext；
- templated parent；
- collection current item（后续）。

Path compiler 把字符串编译为 accessor chain，例如：

```text
Customer.Address.Street
Orders[0].Name
/CurrentItem
```

Accessor 使用 TypeRegistry 或注册 adapter，不进行任意字符串反射调用。

通知接口返回 RAII subscription：

```cpp
class INotifyPropertyChanged {
public:
    virtual Subscription SubscribePropertyChanged(PropertyChangedHandler) = 0;
};
```

`Binding` declaration 可共享；每个 target property 使用独立 `BindingExpression`。Expression MUST：

- context 变化时重新 attach；
- path 中间节点变化时只重建后续链；
- update 合并到 DataBind priority；
- 使用弱订阅；
- 检测 cycle 和 TwoWay echo；
- 错误产生 diagnostic/fallback，不终止 frame loop；
- converter 和 validation 只在 UI thread 执行。

M3 支持 OneTime、OneWay、TwoWay、OneWayToSource，以及 PropertyChanged、LostFocus、Explicit trigger。

## 6. Layout

`UIElement` 提供 Measure/Arrange；`FrameworkElement` 处理 Width/Height、Min/Max、Margin、Alignment、Style/Template、layout rounding 和 layout transform。

```cpp
void Measure(Size available);
void Arrange(Rect finalRect);

virtual Size MeasureCore(Size available);
virtual Size ArrangeCore(Rect finalRect);
```

Panel 实现 `MeasureOverride` / `ArrangeOverride`。

MUST：

- Measure 在 Arrange 前；
- 相同约束且未 dirty 时复用缓存；
- invalidation 根据 metadata 向上合并为 dirty roots；
- layout pass 中 tree mutation 延迟；
- NaN、negative、overflow 和 oscillation 可诊断；
- 内部坐标使用 device-independent units；
- DPI 由 platform 注入。

M2 Panel/element：

- Canvas、StackPanel、Grid 的 fixed/auto/star 核心；
- Border/Decorator、ContentPresenter、TextBlock；
- Window、Image、Shape 基类。

M3 增加 DockPanel、WrapPanel、ScrollViewer、ItemsPresenter 和 VirtualizingStackPanel。

## 7. Routed Event 与输入

```cpp
enum class RoutingStrategy { Direct, Bubble, Tunnel };
```

RoutedEvent 使用 registry token。Event route 在 raise 开始时快照，处理期间 tree mutation 不改变当前 route。

支持：

- class/instance handlers；
- handled 和 `handledEventsToo`；
- source/original source；
- tunnel + bubble；
- route diagnostics。

平台输入统一为 pointer、keyboard、text/IME、focus 和 activation。InputManager 顺序：

```text
normalize coordinates
 -> hit test
 -> capture/focus resolution
 -> preview route
 -> bubble route
 -> command gesture
 -> default control behavior
```

Keyboard focus、logical focus 和 pointer capture 分开管理。Detach/disable/hide 时自动修复。

## 8. Command

M3 支持 `ICommand` 与 RoutedCommand：

- CanExecute/Execute；
- command binding；
- input gesture；
- route query；
- requery coalescing。

## 9. Style 与 Template

Style 含 TargetType、BasedOn、Setters、Triggers 和 Resources。首次应用前 seal：

- 检测 BasedOn 循环；
- flatten setter/trigger；
- 验证 property owner/value type；
- 生成 immutable runtime plan。

ControlTemplate/DataTemplate 编译为 deferred factory plan。实例化建立 templated parent、template NameScope、child index、TemplateBinding 和 trigger targets。

Trigger 顺序：

1. property trigger；
2. data trigger；
3. multi trigger；
4. event trigger/action。

Trigger 必须作为 DP provider 参与优先级，不能永久覆盖 local value。

## 10. Control 分工

Control 实现状态和交互；Template 定义外观；Style 提供复用配置。可模板化外观不得硬编码在控件 `OnRender` 中。

M3 控件：

- Control、ContentControl；
- ButtonBase/Button；
- ItemsControl/ListBox；
- ScrollViewer；
- UserControl；
- TextBox 仅在 IME 基础完成后进入。

Items pipeline：

```text
ItemsSource
 -> CollectionView
 -> ItemContainerGenerator
 -> Panel/VirtualizingPanel
 -> Container Style/Template
```

Generator 使用 stable item key/generation，避免虚拟化错配。

## 11. UI 模型验收

M2：

- XAML → object tree → layout → reference image 全链路；
- 50+ golden XAML；
- 20+ WPF layout probes；
- invalid XAML 有 source diagnostics；
- headless tests 可运行。

M3：

- Binding modes/update triggers 测试通过；
- DynamicResource 和 template swap 无泄漏；
- event order 与 WPF probe 对齐；
- 1k item sample 可交互；
- unknown/partial feature 明确报错。

## Compiled XAML cache compatibility

Compiled XAML and any persistent XAML cache MUST store an
`XamlCompiledCacheIdentity` and validate it before decoding IR. Compatibility
requires all of the following to match the runtime:

- `XamlCompiledCacheFormatVersion`;
- `TypeIdAlgorithmVersion`;
- `MetadataDescriptorFormatVersion`;
- `MetadataFacetFormatVersion`;
- the sealed `MetadataDomain::ComputeSchemaHash()` value, which includes module
  IDs, module schema versions, structural descriptors, and sealed facets.

A format or algorithm mismatch is an unsupported cache format. A schema-hash
mismatch is stale cache data and MUST trigger recompilation or runtime XAML
fallback; it MUST NOT be decoded optimistically. Callback addresses and host
activation registrations are intentionally excluded from the static schema
identity.
