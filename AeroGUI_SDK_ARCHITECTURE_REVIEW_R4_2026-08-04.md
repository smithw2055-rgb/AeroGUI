# AeroGUI SDK 与架构复审建议

> 审查基线：`smithw2055-rgb/AeroGUI`  
> 分支：`codex/local-no-samples-tests`  
> 日期：2026-08-04  
> 审查目标：继续对齐 WPF / NoesisGUI，从 SDK 产品与架构角度完成“减法式收口”，避免重新引入 Runtime、Access、Service、Manager、Context、Catalog、Endpoint 等过度概念。  
> 本文只提出重构建议，不包含代码提交，也未向 GitHub 写入任何内容。

---

## 1. 总体结论

当前分支已经越过“实验性框架搭建”阶段，公共 SDK 的主方向基本正确：

- 已形成 `<Aero/Gui.hpp>`、`<Aero/App.hpp>`、`<Aero/Integration.hpp>`、`<Aero/Meta.hpp>` 四个主要产品入口；
- 已形成 `Aero::Gui`、`Aero::App`、`Aero::Integration`、`Aero::Meta` 对应的 CMake 产品边界；
- `Application`、`Window`、`DependencyObject`、`Visual`、`UIElement`、`FrameworkElement` 等主干类型已按 WPF 语义组织；
- `View::Update()`、`Renderer::UpdateRenderTree()`、`RenderOffscreen()`、`Render()` 的使用流程与 NoesisGUI 的集成模式高度接近；
- RHI、RenderFrame、DisplayList、后端状态、原生窗口实现已基本下沉到私有实现；
- 公共头文件白名单、命名空间清单和架构检查已经具备产品 SDK 所需的边界意识。

因此，**下一轮不应再做一次大范围命名空间、模块或“层次化框架”重建**。当前真正需要处理的是内部与公共模型之间仍然存在的重复、泄漏和不一致。

建议把下一阶段定义为：

> **R4：SDK Convergence / Product Closure**  
> 目标不是增加新层，而是删除重复层、统一对象语义、稳定 ABI，并让 Compiled XAML 真正成为预解析执行格式。

最重要的五项工作是：

1. 公共对象中只保留 WPF 可观察状态，移除 View/Render/Layout/Template 等运行时附件泄漏；
2. 合并 `View::Impl` 与 `Runtime::Detail::ViewData`，让一个 View 只有一个私有实现所有者；
3. 统一 Style、Template、VisualState 的公共对象模型，删除“公共对象 + 私有 XamlXXXObject 代理对象”的双模型；
4. 用统一的 `Aero::Value` 表达 WPF 的 `object` 语义，修复 Content、Header、DataContext、CommandParameter 等类型不一致；
5. 将当前 Compiled XAML 的“序列化 Node 流”升级为“已解析 TypeId/MemberId/Value/Instruction 程序”。

---

## 2. 当前值得保留的设计

### 2.1 四个产品入口应保持不变

当前四个产品入口是合理的：

| 产品入口 | 责任 |
|---|---|
| `<Aero/Gui.hpp>` / `Aero::Gui` | WPF/XAML 对象模型、控件、布局、绑定、样式、资源、动画 |
| `<Aero/App.hpp>` / `Aero::App` | 默认桌面生命周期、`Application::Run()`、原生窗口 |
| `<Aero/Integration.hpp>` / `Aero::Integration` | 游戏引擎、已有窗口系统、RenderDevice、Provider、View 集成 |
| `<Aero/Meta.hpp>` / `Aero::Meta` | 自定义类型、控件、属性、事件和模块注册 |

不建议再次拆出：

- `Aero::Runtime`
- `Aero::Presentation`
- `Aero::Services`
- `Aero::Hosting`
- `Aero::XamlRuntime`
- `Aero::RenderCore`
- `Aero::ControlRuntime`

这些名称会重新把内部实现概念暴露为产品概念。

### 2.2 View / Renderer 使用流程应保持

目前集成侧流程：

```cpp
Aero::Gui gui;
gui.AddModule(...);
gui.Initialize();

auto view = gui.CreateView(options);
view.Value()->GetRenderer().Init(device);

view.Value()->Update(elapsedMs);
view.Value()->GetRenderer().UpdateRenderTree();
view.Value()->GetRenderer().RenderOffscreen();
view.Value()->GetRenderer().Render();
```

这个模型适合游戏引擎、编辑器和自定义宿主，也与 NoesisGUI 使用习惯接近，不建议把它重新包装为：

```text
Runtime -> Session -> ViewHost -> RenderEndpoint -> Presenter
```

`Gui + View + Renderer + RenderDevice` 已经足够。

### 2.3 Facet 继续作为私有执行能力

当前 XAML Facet 已经采用按类型冻结、FacetMask、紧凑索引和分列存储的方式，方向正确。

Facet 应继续承担：

- 生命周期；
- NameScope；
- ResourceScope；
- Deferred Content；
- Implicit Resource Key；
- Property Target；
- MarkupExtension ProvideValue。

但 Facet 应始终是：

> **Meta 描述经过 Schema 冻结后生成的私有执行能力表。**

它不应成为普通控件作者必须理解和手动拼装的公共框架。

---

## 3. P0：公共对象中仍泄漏过多运行时状态

### 3.1 当前问题

多个公共类虽然声明了 `struct Impl;`，但真正的运行时字段仍直接存在于安装头文件中。

例如：

- `include/Aero/Visual.hpp`
  - `ElementTree* tree_`
  - `renderRuntime_`
  - `renderNodeId_`
  - render revision、dirty、queued、attached 等标志
- `include/Aero/UIElement.hpp`
  - `layoutManager_`
  - `viewServices_`
  - `routedHandlers_`
  - measure/arrange 队列与执行状态
- `include/Aero/Controls/Core.hpp`
  - `templateRuntime_`
  - `visualStateRuntime_`
  - `templateHandleValue_`
- `include/Aero/Controls/Common.hpp`
  - `Menu::interactions_`
- 多个模板、生成器、VisualState 类型使用裸 `void* state_` 或 `void* impl_`

这会带来四个问题：

1. 公共对象布局与内部实现强耦合，后续改动会直接改变 SDK ABI；
2. 同一个运行时状态在对象、ElementTree、RenderTree、ViewData 中存在重复所有权；
3. 控件类逐渐变成内部服务的挂载点；
4. 外部开发者虽然不能访问这些私有字段，但仍会看到复杂、不稳定的头文件结构。

### 3.2 推荐规则

不要为每个字段再创建一个 Access、Service 或 Manager。只需要建立一条简单规则：

> **对象拥有自身语义状态；View 拥有与某个 View 绑定的执行状态。**

建议保留在对象中的状态：

- DependencyObject 的有效值和本地值；
- Visual 的逻辑父子关系和可观察树关系；
- UIElement 的 DesiredSize、RenderSize、LayoutSlot 等高频布局结果；
- FrameworkElement 的 Resources、TemplatedParent、作者声明的 Trigger；
- 控件自身真正的语义状态。

建议移入 `View::Impl` 私有表中的状态：

- LayoutEngine、RenderTree、BindingEngine、StyleEngine、TemplateEngine 指针；
- RenderNodeId、Render attachment、render queue 状态；
- routed-handler 私有存储；
- template runtime handle；
- visual-state runtime session；
- control interaction state；
- text/path/image runtime resource attachment；
- View 相关生命周期 token。

对象侧最多保留一个紧凑附件：

```cpp
struct RuntimeAttachment {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0;
    std::uint32_t flags = 0;
};
```

也可以沿用已有 `VisualHandle`。关键是不要在每个控件上继续增加多个运行时裸指针。

### 3.3 `Impl` 使用规则需要统一

建议采用混合方案，而不是“所有类强制 PImpl”：

- 高频、数量巨大的轻量对象：直接保存稳定且必要的语义字段；
- 复杂、低数量、ABI 易变化的类型：只保留一个 `Impl*`；
- 禁止一个类同时出现：
  - `struct Impl;`
  - 多个 `void* xxxRuntime_`
  - 大量内部程序和缓存字段。

简单判定标准：

```text
简单控件：不需要 Impl
复杂服务对象：一个 Impl*
普通 UIElement：稳定热字段 + 一个 RuntimeAttachment
禁止：多个 manager/service/runtime 指针
```

---

## 4. P0：合并 `View::Impl` 与 `ViewData`

### 4.1 当前结构

当前 `View` 实际存在两层私有状态：

```text
View
└─ View::Impl*
   ├─ Renderer
   ├─ Gui state
   └─ Runtime::Detail::ViewData*
      ├─ ElementTree
      ├─ LayoutEngine
      ├─ RenderTree
      ├─ BindingEngine
      ├─ InputRouter
      ├─ TemplateEngine
      ├─ StyleEngine
      ├─ XAML loader state
      ├─ resource layers
      ├─ overlay / tooltip
      └─ storyboard sessions
```

构造 `View` 时还会分别分配：

1. `View::Impl`
2. `Runtime::Detail::ViewData`

`ViewAccess.hpp` 再通过 `operator->()` 把两者连接起来。

这正是用户此前不希望继续保留的 Runtime / Access 双层模型。

### 4.2 推荐结构

直接改为：

```text
View
└─ View::Impl*
   ├─ Gui shared state
   ├─ Renderer
   ├─ ElementTree
   ├─ LayoutEngine
   ├─ RenderTree
   ├─ BindingEngine
   ├─ InputRouter
   ├─ TemplateEngine
   ├─ StyleEngine
   ├─ XAML loading state
   ├─ resource layers
   └─ transient sessions
```

也就是：

- 删除 `Runtime::Detail::ViewData`；
- `View::Impl` 直接成为 View 的唯一执行所有者；
- 删除 `ViewAccess.hpp` 中仅为转发 `ViewData*` 而存在的结构；
- `Renderer::Impl` 仍可保留，因为 Renderer 是独立的公共 facade；
- `View.cpp` 可以继续保持一个实现文件，不需要拆成十几个 Manager 文件。

### 4.3 不拆 `View.cpp`，但整理职责

不建议把 `View.cpp` 拆成：

```text
ViewRuntime
ViewServices
ViewContext
ViewManager
ViewAccess
ViewLifecycle
ViewCoordinator
```

可以继续保留单个 `View.cpp`，只按代码段整理：

```text
1. View::Impl state
2. initialization / shutdown
3. content mount / unmount
4. frame update
5. input
6. resources / XAML
7. animation and trigger integration
8. overlay / popup / tooltip
9. public View methods
```

仅把明显属于其他既有领域的算法移回已有文件：

- Storyboard PropertyPath 解析 -> animation 实现；
- Trigger 条件比较 -> trigger/style 实现；
- Popup/ToolTip 行为 -> controls/input 实现；
- XAML source/cache 操作 -> markup 实现。

这些应是普通私有函数，不需要新增 Manager 类。

---

## 5. P0：公共 Style / Template / VisualState 模型仍存在双轨制

### 5.1 Runtime Plan 不应出现在公共头文件

当前以下类型位于公共头文件：

- `StyleSetter`
- `StyleTriggerSetter`
- `TriggerPlan`
- `Style::AddTrigger(TriggerPlan)`
- `Style::SealRuntime(...)`
- Style 内部 program、compiled setter/trigger vectors

但注释已经说明它们属于私有 Style Engine 的紧凑运行时表示。

这说明物理边界与设计目标不一致。

建议把以下内容移到 `src/gui/private/Style.hpp` 或现有私有 Style 实现：

```text
StyleSetter
StyleTriggerSetter
TriggerPlan
compiled style program
provider tokens
runtime sealing entry
```

公共 `Style` 只保留 WPF 熟悉的对象：

- `TargetType`
- `BasedOn`
- `Resources`
- `Setters`
- `Triggers`
- `IsSealed`
- typed convenience APIs

公共作者对象在 Seal 时编译为私有 `StyleProgram`，但 `StyleProgram` 不应进入安装头。

### 5.2 VisualState 不应同时存在两套对象

当前公共侧存在：

```cpp
struct VisualState;
struct VisualTransition;
struct VisualStateGroup;
class VisualStateManager;
```

Markup 私有实现中又存在用于 XAML 构建的：

```text
XamlVisualStateObject
XamlVisualStateGroupObject
XamlVisualTransitionObject
XamlVisualStates
```

这会产生典型双模型问题：

```text
XAML 对象模型 -> 私有代理对象 -> 转换 -> 公共结构 -> 再编译为运行时状态
```

推荐让公共类型本身成为真正的 XAML 作者对象：

```cpp
class VisualState : public Base::Object;
class VisualTransition : public Base::Object;
class VisualStateGroup : public Base::Object;
```

它们拥有清晰集合：

```text
VisualState
├─ Name
├─ Storyboard
└─ Setters

VisualTransition
├─ From
├─ To
├─ GeneratedDuration
├─ GeneratedEasingFunction
└─ Storyboard

VisualStateGroup
├─ Name
├─ States
└─ Transitions
```

然后：

```text
公共作者对象 -> Seal/Compile -> 私有 VisualStateProgram
```

删除中间的 `XamlVisualStateXXXObject` 代理对象。

### 5.3 Template 类型归属需在 SDK 冻结前最终校正

当前：

- `FrameworkTemplate` 在 `Aero::Controls`
- `DataTemplate` 在 `Aero::Controls`
- `VisualStateManager` 在 `Aero::Controls`

对于熟悉 WPF 的用户，这些归属不够自然。

推荐最终归属：

```text
Aero::FrameworkTemplate
Aero::DataTemplate
Aero::VisualState
Aero::VisualStateGroup
Aero::VisualTransition
Aero::VisualStateManager

Aero::Controls::ControlTemplate
Aero::Controls::ItemsPanelTemplate
```

如果为了 include cycle 暂时无法移动，也应通过前向声明和实现下沉解决，不应以内部循环为理由长期改变公共语义。

---

## 6. P0：统一 WPF `object` 语义为 `Aero::Value`

### 6.1 当前不一致

当前 SDK 对 WPF 中的 `object` 使用了三种表达：

```text
Meta::Value
Base::Ref<Base::Object>
Base::String
```

例如：

- `ContentControl::Content` 使用 `Meta::Value`
- `HeaderedItemsControl::Header` 使用 `Base::String`
- `ToolBar::Header` 使用 `Meta::Value`
- `MenuItem::CommandParameter` 使用 `Base::Ref<Base::Object>`
- `FrameworkElement::DataContext` 使用 `Base::Ref<Base::Object>`
- `FrameworkElement::Tag` 使用 `Meta::Value`
- Binding 的 FallbackValue 使用 `Meta::Value`
- `FontFamilyProperty` 使用字符串，但 SDK 同时存在 `Media::FontFamily`

这会导致：

- 字符串、数值、enum、struct 无法统一放入 Header、CommandParameter、DataContext；
- XAML Writer 需要对每个属性做特殊分支；
- Binding、Resource、Style、Template 之间需要重复装箱和转换；
- `Meta` 机制泄漏进普通控件作者 API。

### 6.2 推荐公共名称

将通用值类型作为普通 SDK 基础类型公开：

```cpp
namespace Aero {
using Value = Base::Value;
using ValueKind = Base::ValueKind;
}
```

普通 WPF API 使用：

```cpp
Aero::Value
```

而不是：

```cpp
Aero::Meta::Value
```

`Meta` 命名空间仅负责：

- 类型描述；
- TypeId / MemberId；
- 注册；
- 编解码；
- Schema。

### 6.3 建议统一为 Value 的属性

建议统一：

```text
Content
Header
DataContext
Tag
ToolTip
CommandParameter
Binding.Source
Binding.ConverterParameter
FallbackValue
TargetNullValue
Resource value
Setter.Value
```

同时保留便利重载：

```cpp
void SetHeader(Value value) noexcept;
void SetHeader(Base::StringView text) noexcept;
void SetCommandParameter(Value value) noexcept;
```

这样既保留 WPF 的 `object` 语义，又不会让普通 C++ 使用变得繁琐。

### 6.4 FontFamily 应只有一个表示

当前同时存在：

```text
Media::FontFamily
FrameworkElement::FontFamilyProperty<Base::String>
```

建议只保留一个公共语义类型：

```cpp
Aero::Media::FontFamily
```

或者将其设计成轻量不可变值类型。

不建议继续使用“公开 FontFamily 类，但属性实际存字符串”的双表示。

### 6.5 RelativeSource.AncestorType 不应是字符串

当前 `RelativeSource::AncestorType` 为字符串，运行时还需要重新解析类型名。

建议使用：

```cpp
Meta::TypeReference
```

或稳定 `TypeId`。

XAML 文本仍可通过 Schema 转换为 TypeReference，但运行时 BindingPath 不再依赖字符串类型查找。

---

## 7. P1：Compiled XAML 需要升级为真正的执行格式

### 7.1 当前格式的实质

当前 Compiled XAML 已具备：

- magic/version；
- schema identity/hash；
- origin URI；
- dependency URI；
- node count；
- source position；
- NodeKind；
- 每个 Node 的多组字符串；
- Schema 兼容性验证。

这是一个可靠的“预解析 XML Node 缓存”，但还不是高性能的 XAML 执行格式。

当前每个 Node 仍序列化：

```text
prefix
localName
namespaceUri
namespacePrefix
namespaceUri
value text
source position
```

加载时仍会经过 Node/ObjectBuilder，并执行类型、成员和文本值处理。

因此当前格式的主要收益是：

- 跳过 XML tokenizer；
- 提供缓存和兼容性检查。

但它还不能充分实现：

- 显著降低文件尺寸；
- 消除运行时类型名查找；
- 消除成员名查找；
- 消除常见值字符串转换；
- 直接调用冻结后的 Facet/Accessor。

### 7.2 推荐 AXB2 格式

建议将下一版定义为 `Aero XAML Binary v2`，由以下部分组成：

```text
Header
├─ magic / format version
├─ TypeId algorithm version
├─ schema hash
├─ flags
└─ section offsets

Dependency Table
String Table
Type Table
Member Table
Value Table
Template/Deferred Blob Table
Instruction Stream
Optional Debug Source Map
```

#### Type Table

```text
local type index -> stable TypeId
```

加载时只做一次：

```text
TypeId -> frozen schema type slot
```

不再对每个对象查找 `{namespace}localName`。

#### Member Table

```text
local member index -> stable MemberId / DependencyPropertyHandle
```

加载时直接取得：

- property accessor；
- collection/content accessor；
- routed event；
- attached property；
- value converter；
- property target facet。

#### Value Table

预编码常见值：

```text
Boolean
Int32 / UInt32 / Int64
Double
Enum raw value
Color
Thickness
CornerRadius
GridLength / Length
TypeReference
ResourceKey
String table index
Object/null marker
MarkupExtension program
```

正常加载路径不再调用字符串转换器。

#### Instruction Stream

建议使用紧凑指令：

```text
CreateObject TypeIndex
BeginInit
BeginMember MemberIndex
SetValue ValueIndex
SetObject
AddContent
AddCollectionItem
PushNameScope
RegisterName StringIndex
PushResourceScope
AddResource ValueIndex
ProvideMarkupExtension FacetIndex
BeginDeferredContent BlobIndex
EndMember
EndInit
EndObject
```

指令直接消费冻结后的 Schema Slot 和 Facet Slot。

### 7.3 Debug 信息单独存储

当前每个 Node 都携带完整 SourcePosition，会增加体积。

建议：

- Debug 构建：保留压缩 SourceMap；
- Release 构建：可移除行列信息；
- 错误至少保留：
  - XAML URI
  - instruction offset
  - type/member token
- SourceMap 使用 delta/varint 编码。

### 7.4 Compiled XAML 的验收指标

建议把以下指标写入 R4 验收标准：

1. 正常 AXB2 加载路径不创建 XML Token；
2. 不创建通用 Node 对象；
3. 不按字符串解析类型；
4. 不按字符串解析成员；
5. 常见基础值不执行文本转换；
6. Facet 查找使用冻结索引；
7. Release AXB 文件尺寸显著小于当前 Node 格式；
8. Schema 不兼容时可回退源 XAML；
9. Template/Deferred Content 可独立延迟实例化；
10. SourceMap 可选，不影响运行时执行布局。

---

## 8. P1：元数据身份应编译期稳定，注册只补充行为

### 8.1 当前风险

当前 enum/value 类型通过：

```text
AERO_DECLARE_TYPE_ENUM
AERO_DECLARE_TYPE_VALUE
ResolveRuntimeTypeInfo(Token())
BindRuntimeTypeInfo(...)
```

在运行期将 C++ token 绑定到 XAML TypeId、名称和命名空间。

该设计虽然解决了集中注册问题，但存在初始化顺序风险：

- `TypeOf<T>()` 在注册前可能得不到稳定 TypeId；
- 类型身份与 Registry 初始化顺序绑定；
- Compiled XAML 编译器、离线工具和运行时需要重复建立映射；
- 外部类型仍可能需要手写 `TypeTraits`。

### 8.2 推荐原则

应明确分离：

```text
类型身份：编译期稳定
类型行为：模块注册期提供
```

建议提供：

```cpp
AERO_DECLARE_ENUM_NAMED(
    Theme,
    "urn:mygame:ui",
    "Theme")

AERO_DECLARE_VALUE_NAMED(
    ViewModelState,
    "urn:mygame:ui",
    "ViewModelState")
```

宏直接生成：

```text
constexpr TypeId
constexpr XAML namespace
constexpr local name
```

模块注册仅补充：

- enum value table；
- value semantics；
- text converter；
- fields/properties；
- factory；
- interfaces。

这样离线 xamlc 与运行时天然共享同一 TypeId。

### 8.3 保留现有 typed Fluent 注册

当前：

```cpp
Meta::Register<MyControl>(registration)
    .Factory()
    .Property(...)
    .Event(...)
    .Result();
```

方向正确，应保留。

需要简化的是类型声明和身份，不需要再增加：

```text
DescriptorFactory
RegistrationCatalog
MetadataSessionProvider
TypeContractBuilder
```

一个 `Registration`、一个 `Register<T>()`、一个链式结果即可。

---

## 9. P1：Application / Window 语义还需收口

### 9.1 Application.Resources 应始终可用

当前 `Application::GetResources()` 返回 `Ref<ResourceDictionary>`，可能为空，XAML Resource Facet 也需要处理“资源字典不存在”。

WPF 用户通常预期：

```cpp
application.GetResources().Add(...);
```

推荐：

```cpp
ResourceDictionary& GetResources() noexcept;
const ResourceDictionary& GetResources() const noexcept;
void SetResources(Base::Ref<ResourceDictionary> value) noexcept;
```

内部保证资源字典始终存在，或者直接按值持有共享 ResourceDictionary handle。

### 9.2 MainWindow 所有权应明确

当前：

```cpp
void SetMainWindow(Window* value);
```

内部尝试从 borrowed pointer 获取 `Ref`，失败时仍保留裸指针。

这使栈对象、Ref 管理对象和宿主对象的生命周期语义不明确。

推荐 canonical API：

```cpp
void SetMainWindow(Base::Ref<Window> window) noexcept;
Window* GetMainWindow() const noexcept;
```

如果确实需要非拥有窗口，单独提供明确命名：

```cpp
void SetMainWindowBorrowed(Window* window) noexcept;
```

不要让一个普通 Setter 同时承担 owned/borrowed 两种含义。

### 9.3 Application.Current 不建议使用 thread_local

当前 `Application::Current()` 基于 `thread_local Application*`。

这意味着其他线程访问时得到空值，与“当前桌面应用”语义不一致。

建议 App 产品明确：

- 同一进程只允许一个活跃的默认 `Application`；
- `Application::Current()` 返回该实例；
- UI 对象访问仍通过 Dispatcher 做线程验证；
- 多 View / 多引擎实例由 `Gui` 支持，不由多个 `Application` 支持。

### 9.4 无异常 SDK 需要 Checked 边界

当前：

- `Application::Run()` 遇到失败返回 `-1`；
- `Window::Show()` 返回 `Result<void>`；
- `Close()` 返回 `void`。

建议统一 WPF-shaped API 与可诊断 API：

```cpp
int Run() noexcept;
Base::Result<int> RunChecked(...) noexcept;

void Show() noexcept;
Base::Result<void> ShowChecked() noexcept;

void Close() noexcept;
Base::Result<void> CloseChecked() noexcept; // 仅在确有可观察失败时
```

普通 WPF 用户使用简单方法，工具、引擎和诊断代码使用 Checked 方法。

---

## 10. P1：公共 API 命名和失败模型需要统一

### 10.1 Collection API

当前集合命名存在：

```text
GetCount / GetItem
Count / At
Reset() -> void
Reset(span) -> bool
Add() -> Result
Add() -> void
```

建议统一为：

```cpp
std::uint32_t GetCount() const noexcept;
T* GetItem(std::uint32_t index) const noexcept;
bool GetIsEmpty() const noexcept;

Base::Result<void> Add(...);
Base::Result<void> Insert(...);
Base::Result<bool> Remove(...);
void Clear() noexcept;
```

作者集合如 Style.Setters、Triggers 可以提供 WPF 风格 facade，但底层命名保持一致。

### 10.2 bool 不应用于表达构建失败

例如：

```text
Style::Set(...) -> bool
TriggerBuilder::Set(...) -> bool
ItemCollection::Reset(span) -> bool
```

这些操作可能失败于：

- 类型不兼容；
- 对象已 Seal；
- 内存不足；
- 属性无效；
- Schema 未冻结。

建议返回 `Base::Result<void>`。

`bool` 仅用于真实的二值结果：

- 是否找到；
- 是否移除；
- 是否已经应用；
- 是否匹配。

### 10.3 AddHandler 的错误处理需要修正

当前 `UIElement::AddHandler()` 对 `AddHandlerChecked()` 的任何失败都调用 `ReportOutOfMemory()`。

这会把 InvalidArgument、InvalidState 等错误误报为 OOM。

推荐：

- `AddHandlerChecked()` 保持返回 Result；
- `AddHandler()` 只在实际 `OutOfMemory` 时调用 OOM handler；
- 其他失败进入 diagnostics/debug assertion；
- 不要把所有无异常错误都转化为进程终止。

---

## 11. P2：构建系统保持产品简单，内部目标可适度合并

### 11.1 公共 CMake 目标不变

继续保持：

```text
Aero::Base
Aero::Gui
Aero::Meta
Aero::Integration
Aero::App
```

不应把内部 object library 导出为 SDK target。

### 11.2 内部 object library 不需要继续增长

当前内部已有多个 object target：

```text
AeroGuiKernelObjects
AeroControlsObjects
AeroMarkupKernelObjects
AeroMarkupObjects
AeroModuleSetObjects
AeroRuntimeObjects
AeroRenderingObjects
AeroAppModelObjects
AeroInspectorObjects
```

它们没有公开泄漏，因此不是最高优先级。

但后续不应继续新增：

```text
AeroStyleRuntimeObjects
AeroTemplateRuntimeObjects
AeroFacetRuntimeObjects
AeroViewServicesObjects
```

如需收口，可合并为少量内部构建域：

```text
AeroGuiObjects
AeroRuntimeObjects
AeroRenderingObjects
AeroAppObjects
```

是否合并应以减少依赖绕行和构建重复为依据，而不是为了目录对称。

---

## 12. 可立即处理的代码卫生问题

这些修改风险低，可作为 R4 第一个提交：

### 12.1 重复或错误声明

- `Controls/Core.hpp` 中 `Control` 连续出现两次 `struct Impl;`
- `DesktopHost.hpp` 末尾存在对自身 namespace/type 的重复 `using`
- `GuiData.hpp` 在 `#pragma once` 之前执行 include
- `View.cpp` 存在大量重复 include：
  - `ControlsPrivate.hpp`
  - `GuiPrivate.hpp`
  - `MarkupPrivate.hpp`
- 多个源文件重复 include 同一私有聚合头

### 12.2 文件命名与真实职责不一致

- `ViewAccess.hpp` 实际拥有 `View::Impl` 和 `Renderer::Impl`，并非单纯 Access；
- `GuiData.hpp` 实际定义 `Gui::Impl`；
- `Runtime::Detail::ViewData` 实际是 View 的完整实现。

合并后建议：

```text
src/runtime/Gui.cpp       // 直接定义/使用 Gui::Impl
src/runtime/View.cpp      // 直接定义/使用 View::Impl
src/runtime/Renderer.cpp  // 或现有 Renderer 实现
```

私有头只在两个以上翻译单元确实共享时保留。

### 12.3 增加架构检查

可增加以下规则：

- WPF 公共头中禁止 `void* xxxRuntime_`；
- 公共 Style/Template 头中禁止 `*Plan`、`*Program`；
- 控件公共头中禁止 `Manager*`、`Service*`、`Runtime*` 字段；
- 禁止公共作者 API 使用 `Meta::Value`，统一为 `Aero::Value`；
- 禁止同时声明 `Impl` 又包含多个 opaque runtime pointer；
- 禁止私有 XAML 代理对象与同名公共作者对象并存。

---

## 13. 推荐的最终内部结构

不需要按 WPF assembly 一比一拆分，也不需要按每个行为建立服务层。

推荐保持如下简单结构：

```text
include/Aero
├─ Base
├─ Application.hpp
├─ Window.hpp
├─ DependencyObject.hpp
├─ Visual.hpp
├─ UIElement.hpp
├─ FrameworkElement.hpp
├─ Controls
├─ Data.hpp
├─ Input.hpp
├─ Resources.hpp
├─ Style.hpp
├─ Styling.hpp
├─ Animation.hpp
├─ Markup.hpp
├─ View.hpp
├─ Renderer.hpp
├─ Integration
└─ Meta.hpp

src
├─ base
├─ gui
│  ├─ property
│  ├─ tree/layout
│  ├─ binding
│  ├─ style
│  └─ events/input
├─ controls
├─ markup
├─ runtime
│  ├─ Gui.cpp
│  └─ View.cpp
├─ render
├─ integration
├─ app
└─ platform
```

这里的子目录只是源码归属，不等于公共产品层次。

---

## 14. 建议实施阶段

## R4-A：SDK 表面清理

目标：不改变核心执行逻辑，先清除明显泄漏和重复。

工作：

1. 清理重复 include、重复 `Impl`、重复 namespace alias；
2. 将 `TriggerPlan`、`StyleSetter` 等运行时计划移入私有实现；
3. 统一 Collection 命名；
4. 修复 AddHandler 错误映射；
5. 增加公共头架构检查；
6. 补充三个最小 SDK consumer：
   - WPF desktop app；
   - custom control/module；
   - embedded View/Renderer。

## R4-B：View 所有权收口

目标：移除 `View::Impl -> ViewData` 双层。

工作：

1. 将 `ViewData` 合并到 `View::Impl`；
2. 删除 `Runtime::Detail::ViewData`；
3. 删除仅为转发存在的 `ViewAccess` 层；
4. View 私有引擎统一由 `View::Impl` 创建和销毁；
5. 把 per-control runtime pointer 改为 View-owned attachment；
6. 保持 `View.cpp` 为单一编排文件。

## R4-C：WPF 作者对象模型收口

目标：普通用户只看到 WPF/Noesis 熟悉的对象。

工作：

1. 将公共 VisualState 类型改为真正作者对象；
2. 删除 `XamlVisualStateXXXObject` 代理；
3. 统一 Style、Template、Trigger 的 Seal/Program 路径；
4. 统一 `Aero::Value`；
5. 修正 DataTemplate、FrameworkTemplate、VisualStateManager 类型归属；
6. 统一 FontFamily、Header、DataContext、CommandParameter 语义。

## R4-D：Compiled XAML v2

目标：真正降低尺寸和运行时解析成本。

工作：

1. 固定 compile-time TypeId / MemberId；
2. 定义 AXB2 section 格式；
3. 建立 Type/Member/Value table；
4. 建立 instruction stream；
5. 直接调用 frozen accessor/facet；
6. SourceMap 可选；
7. 兼容失败时回退源 XAML；
8. built-in themes 默认使用 AXB2。

## R4-E：SDK 冻结

目标：形成可发布 SDK。

验收：

- 四个产品入口稳定；
- 普通 App 不需要理解 Gui、View、RenderDevice；
- 集成用户只需要 Gui、View、Renderer、RenderDevice；
- 自定义控件只需要 Control、DP/Event、Meta::Register；
- 公共头中没有 View/Render/Layout/Template runtime service；
- 编译 XAML 正常路径没有 XML、类型名和成员名查找；
- 公共对象模型与 XAML 对象模型只有一套；
- 安装包不包含私有头；
- CMake target 不暴露内部 object library。

---

## 15. 明确不建议的方向

以下方向会重新增加复杂度，建议禁止：

1. 再增加 Runtime、Access、Service、Contract、Context、Catalog、Endpoint 层；
2. 为每个控件或行为建立独立 Manager；
3. 为了文件行数把一个职责拆成多个互相转发的头；
4. 把 WPF 的 assembly 或完整继承实现机械复制到 C++；
5. 公开 RenderCommand、DisplayList、RenderTree、RHI state；
6. 保留公共对象和 XAML 私有代理对象两套模型；
7. 在 Application 上增加 graphics/platform/service locator；
8. 为兼容旧 API 长期保留 forwarding header 和 namespace alias；
9. 将 Facet 变成普通控件开发者必须直接注册的概念；
10. 把当前 Node 序列化格式直接称为最终 Compiled XAML。

---

## 16. 最终建议

当前项目不缺少新的架构概念，缺少的是最后一次坚定的收口：

```text
公共层：
WPF 类型、属性、事件、资源、样式、模板、绑定、Application、Window

集成层：
Gui、View、Renderer、RenderDevice、Provider

扩展层：
Module、Meta::Register

私有实现：
Facet、Schema slot、ObjectWriter、StyleProgram、TemplateProgram、
View runtime state、RenderTree、DisplayList、RHI、原生平台适配
```

建议下一步优先完成：

```text
R4-A 公共运行时计划下沉
R4-B View::Impl / ViewData 合并
R4-C Value + Template + VisualState 单对象模型
```

完成这三步后，再开始 AXB2。原因是 Compiled XAML 的 Type/Member/Facet 指令格式必须建立在已经稳定的公共对象模型和元数据身份之上。

---

## 17. 本次审查涉及的主要文件

```text
docs/spec/PUBLIC_HEADER_MODEL.md
docs/spec/PUBLIC_NAMESPACE_MODEL.md
docs/spec/RUNTIME_ACCESS_MODEL.md

cmake/AeroPublicHeaders.cmake
cmake/AeroPublicNamespaces.cmake
cmake/CheckArchitecture.cmake
cmake/AeroGuiTargets.cmake
cmake/AeroRuntimeTargets.cmake
cmake/AeroRenderingTargets.cmake
cmake/AeroProductTargets.cmake

include/Aero/Gui.hpp
include/Aero/App.hpp
include/Aero/Application.hpp
include/Aero/Window.hpp
include/Aero/Visual.hpp
include/Aero/UIElement.hpp
include/Aero/FrameworkElement.hpp
include/Aero/View.hpp
include/Aero/Renderer.hpp
include/Aero/Meta.hpp
include/Aero/Value.hpp
include/Aero/Markup.hpp
include/Aero/Style.hpp
include/Aero/Styling.hpp
include/Aero/Triggers/TriggerBase.hpp
include/Aero/Controls/Core.hpp
include/Aero/Controls/Items.hpp
include/Aero/Controls/Common.hpp
include/Aero/Integration/ViewOptions.hpp
include/Aero/Integration/RenderDevice.hpp

src/runtime/Gui.cpp
src/runtime/GuiData.hpp
src/runtime/View.cpp
src/runtime/ViewAccess.hpp
src/markup/MarkupPrivate.hpp
src/markup/MarkupParser.cpp
src/markup/MarkupSchema.cpp
src/markup/MarkupLoader.cpp
src/markup/XamlReader.cpp
src/integration/RenderDevice.cpp
src/app/Application.cpp
src/app/ApplicationRun.cpp
src/app/DesktopHost.hpp

tools/sdk-consumers/ProductConsumer.cpp
tools/sdk-consumers/IntegrationConsumer.cpp
tools/sdk-consumers/MetaConsumer.cpp
```
