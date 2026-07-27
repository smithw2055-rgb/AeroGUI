# XAML Runtime API

## ResourceUri

头文件：`Aero/Base/ResourceUri.hpp`

```cpp
auto base = Aero::Base::ResourceUri::Parse(
    "pack://application:,,,/Aero.App;component/Views/Main.xaml");
auto child = Aero::Base::ResourceUri::Resolve(
    base.Value(), "../Themes/Dark.xaml");
```

`Canonical()` 用于缓存键、provider 路由和递归检测。`Scheme()`、`Assembly()`
和 `Path()` 用于 provider 选择。`IsNetwork()` 只报告 URI 属性；是否允许加载
由 `XamlLoadPolicy` 决定。

## Module composition 与 SchemaBundle

头文件：

- `Aero/Module.hpp`
- `Aero/SchemaBundle.hpp`
- `Aero/Markup/Schema/XamlRegistrationContext.hpp`

模块在同一个 descriptor 中声明 Metadata 与 XAML 两阶段注册：

```cpp
Aero::ModuleRegistration module;
module.name = "My.Controls";
module.registerModule = &RegisterMetadata;
module.registerXaml = &RegisterXaml;

environment.AddModule(module);
environment.Initialize();
```

`RegisterMetadata` 定义类型和成员；`RegisterXaml` 通过
`XamlRegistrationContext::TryAdd()` 贡献正交的 XAML Facet。
`SchemaBundle` 只允许 `Prepare()` 后再 `Finalize()`，成功后其 Metadata、Schema
和 activation facets 都不可变，可供多个 View 与宿主工具共享。

## Source providers

头文件：`Aero/Markup/Runtime/XamlLoader.hpp`

```cpp
class MyProvider final : public Aero::Markup::IXamlSourceProvider {
public:
    Aero::Base::Result<Aero::Markup::XamlSource> Load(
        const Aero::Base::ResourceUri& uri) const noexcept override;
};

MyProvider provider;
host.RegisterXamlSourceProvider(
    provider, "pack", "Aero.App");
```

provider 对象必须至少存活到最后一次可能的 XAML 加载。重复或歧义注册返回
失败，不存在隐式覆盖。

`EmbeddedXamlSourceProvider` 支持 `TryAdd`、`TryAddText` 和 `Freeze`；
`FileXamlSourceProvider` 执行文件大小限制。只需要 callback/context 的模块可使用
`XamlSourceProviderFacet`，无需再声明 provider 子类。

## XamlLoader

```cpp
Aero::Markup::XamlLoader loader(schema, providers, diagnostics);

auto fromUri = loader.Load("Views/Main.xaml", options);
auto fromText = loader.Parse(source, baseUri, options);
auto component = loader.LoadComponent(existingRoot, uri, options);
auto compiled = loader.LoadCompiled(bytes, originUri, options);
```

`XamlLoadOptions` 包含：

- `policy`：network/file/pack 权限
- `limits`：XML、compiled、source bytes、对象数、资源数和依赖深度限制
- `baseUri`
- ambient `resources`
- 可选 `Core::ActivationProviderRegistry`
- 可选 `Core::ObjectActivationContext`
- `templatedParent`

公开的 `XamlActivationProviderRegistry` 包装已移除。metadata factory 是默认
创建路径；只有需要 host services 的类型才注册 Core activation facet。

成功返回的 `XamlLoadResult` 持有：

- root object
- document NameScope
- document resources
- visual content plan
- canonical URI
- dependency URI list

调用方应保留整个 result，而不是只保存裸 root 指针。

低层 object-writer API 同样只返回 `XamlLoadResult`。旧的 root-only
`XamlObjectWriter::Load`、`XamlLoadSession::LoadRoot` 和独立 visual-tree
load wrapper 已移除，避免调用方无意丢失名称、资源和视觉内容计划。

## Schema facets 与扩展结果

头文件：

- `Aero/Markup/Schema/XamlFacetStore.hpp`
- `Aero/Markup/Extensions/XamlExtensionContext.hpp`

`XamlSchemaContext` 解析 metadata descriptor，并把 XAML 特有行为委托给冻结的
`XamlFacetStore`。可注册的能力包括 member、member provider、markup-extension，
以及独立的 lifecycle、NameScope、ResourceScope、deferred-content、隐式资源键和
property-target type facet。旧 `XamlTypeFacet` 作为兼容聚合入口，内部会拆解为
这些正交能力，因此派生类型只覆盖自己声明的切面。

Markup extension 返回 `XamlProvidedValue`：

- `Value`：writer 继续执行普通成员写入。
- `Expression`：writer 将 `Core::PropertyExpression` 安装到有效值引擎。
- `Handled`：extension 已完成目标操作，可提供 rollback token。

这些副作用属于当前 `XamlLoadSession` 事务，后续加载失败时会逆序撤销。有效值
目标由 `XamlSchemaContext::ResolvePropertyTarget()` 根据 metadata 解析，不需要
公开 DependencyObject cast callback。

## RuntimeEnvironment、RuntimeView 与 UiDocument

头文件：

- `Aero/RuntimeEnvironment.hpp`
- `Aero/UiDocument.hpp`

```cpp
Aero::RuntimeEnvironment environment;
environment.AddModule(myModule);
environment.Initialize();

Aero::RuntimeView view(environment);
view.Initialize(options);

auto document = view.Host().LoadUiDocument(mainUri);
view.Host().Mount(std::move(document).Value(), availableSize);
```

`RuntimeEnvironment` 持有引用计数的共享状态、冻结的 `SchemaBundle` 与 document
cache；`CreateView()` 返回持有该共享状态的 `RuntimeView`，因此轻量 Environment
外壳可以先释放。每个 View 仍拥有独立的资源层、Binding、输入、布局和渲染状态。
`UiDocument` 保留 root、NameScope、文档资源、规范 URI、依赖列表和挂载计划，
并明确绑定创建它的 View；跨 View 挂载会失败。Binding 与 DynamicResource 等
副作用在加载阶段只形成 deferred plan，成功挂载时才提交。

## RuntimeHost

头文件：`Aero/RuntimeHost.hpp`

```cpp
host.LoadXaml("pack://application:,,,/Aero.App;component/Main.xaml");
host.ParseXaml(sourceText, baseUri);
host.LoadCompiledXaml(bytes, originUri);
```

字符串内容入口已命名为 `ParseXaml`；`LoadXaml` 始终表示 URI/provider 加载。
对应的 mount 入口为 `LoadAndMountXaml`、`ParseAndMountXaml` 和
`LoadAndMountCompiledXaml`。

资源层 API：

```cpp
host.LoadResources(RuntimeResourceLayer::System, "Themes/System.xaml");
host.LoadResources(RuntimeResourceLayer::Application, "App.xaml");
host.LoadResources(RuntimeResourceLayer::Theme, "Themes/Dark.xaml");
host.LoadResources(
    RuntimeResourceLayer::Theme,
    "Themes/Generic.xaml",
    RuntimeResourceLoadMode::Merge);

host.LoadCompiledResources(
    RuntimeResourceLayer::Application, bytes, originUri);
host.LoadCompiledResources(
    RuntimeResourceLayer::Theme,
    bytes,
    originUri,
    RuntimeResourceLoadMode::Merge);

host.LoadBuiltInTheme(Aero::BuiltInTheme::Dark);
```

`LoadResources` 和 `LoadCompiledResources` 通过 layer 与 mode 明确表达
application/theme/system 和替换/合并语义。
`LoadBuiltInTheme` 优先使用构建时生成并嵌入的 Light/Dark + Generic compiled
documents；compiled payload 未生成或 schema 不兼容时，通过同 origin URI 回退
到内嵌源 XAML。资源层必须在文档加载/挂载前配置。

## ResourceDictionary

头文件：`Aero/Presentation/Resources.hpp`

`ResourceKey::FromString` 与 `ResourceKey::FromType` 构造两种首版键。
`TryAdd` 添加本地条目，`TryAddMerged` 添加 merged dictionary，`SetSource`
记录规范 Source URI，`Seal` 使字典只读。

字典变更会提升 generation 并通知订阅者。对 sealed 字典的修改返回
`ReadOnly`。Style.TargetType 和 DataTemplate.DataType 使用类型键形成隐式
资源。

## Style 与 Template

头文件：

- `Aero/Presentation/Style.hpp`
- `Aero/Controls/Templates.hpp`
- `Aero/Controls/Items.hpp`
- `Aero/Markup/Resources/XamlPresentationObjectModel.hpp`

Style/Template 的 builder API 只允许在 seal 前使用。`StyleManager` 与
`TemplateManager` 负责 provider 生命周期；应用代码不应直接模拟 setter
优先级或手工挂载模板视觉树。

自定义 schema host 通过一次
`XamlPresentationObjectModel::Register(schema, activation)` 注册完整
Presentation 对象模型，不再分别持有 Style 与 Template XAML extension。

`ControlTemplate::Program()` 返回 deferred `TemplateProgram`。
`DataTemplate::Program()` 和 `ItemsPanelTemplate::Program()` 返回共享的
`DeferredObjectProgram`。


## Schema Manifest 与 host xamlc

头文件：`Aero/Markup/Schema/XamlSchemaManifest.hpp`

`XamlSchemaManifest::Capture()` 从冻结的 `XamlSchemaContext` 导出只读验证
snapshot。manifest 保存稳定 descriptor 和 schema identity，不保存 factory、
callback、service pointer 或目标平台 ABI。

```cpp
auto manifest = Aero::Markup::XamlSchemaManifest::Capture(
    schemaBundle.XamlSchema());
auto bytes = manifest.Value().Serialize();
```

Host 工具可以独立反序列化：

```cpp
auto manifest = Aero::Markup::XamlSchemaManifest::Deserialize(bytes);
auto compiled = Aero::Markup::XamlCompiledDocument::Compile(
    reader, manifest.Value(), originUri);
```

命令行：

```text
aero-schema-gen App.aeroschema
aero-xamlc --schema App.aeroschema --origin <uri> Main.xaml Main.axir
aero-xamlc --schema App.aeroschema --check Main.xaml
```

应用级 manifest 通过 `aero_add_schema_manifest()` 从与 Runtime 相同的
`ModuleCatalog` 生成。这样自定义控件、属性和 XAML namespace 可以在 host
`aero-xamlc` 中验证，而不需要加载目标平台二进制。


## 构建选项

- `AERO_BUILD_TOOLS`：是否构建本机 `aero-xamlc` 与 `aero-schema-gen`。
- `AERO_PRECOMPILE_BUILTIN_THEMES`：是否在构建时生成内置 AXIR。
- `AERO_HOST_XAMLC_EXECUTABLE`：交叉编译时使用的宿主机 xamlc。
- `AERO_HOST_SCHEMA_GEN_EXECUTABLE`：生成内置 manifest 的宿主机工具。
- `AERO_BUILTIN_SCHEMA_MANIFEST`：直接使用预生成的内置 manifest。

`Aero::MarkupKernel` 可供只需要 XML/node/compiled 基础能力的组件链接；完整
对象写入、资源与 Presentation 集成使用 `Aero::Markup`。

## Document Cache 与完整文档重载

头文件：

- `Aero/Markup/Runtime/XamlDocumentCache.hpp`
- `Aero/XamlReloadCoordinator.hpp`

`RuntimeEnvironment` 自动创建一个跨 View 共享的 document cache：

```cpp
Aero::RuntimeEnvironment environment;
environment.Initialize();

auto stats = environment.Documents().Statistics();
```

独立 `RuntimeHost` 也拥有自己的 cache；共享 Schema 的 Host 可显式注入 cache。
Loader 的 `XamlLoadOptions::documentCache` 是低层接入点。

手动失效：

```cpp
auto invalidated = cache.Invalidate(changedUri, true);
Base::Vector<Aero::Base::ResourceUri> affected;
cache.CollectAffected(changedUri, affected);
```

完整文档开发期重载：

```cpp
Aero::RuntimeView view(environment);
view.Initialize(options);

Aero::XamlReloadCoordinator reload(view.Host());
reload.Start("Views/Main.xaml", {1280.0f, 720.0f}, diagnostics);

// 文件监听器也可以调用 NotifySourceChanged(uri)。
auto polled = reload.Poll(diagnostics);
```

`Poll()` 不创建线程；它在调用线程读取 revision，并且仅在检测到变化时重新加载
root document。新 `UiDocument` 完整成功后才替换当前挂载内容。

`RuntimeHost::ReplaceMountedDocument()` 也可独立使用：

```cpp
auto replacement = host.LoadUiDocument("Views/Main.xaml", diagnostics);
host.ReplaceMountedDocument(
    std::move(replacement).Value(), availableSize);
```

丢弃 `UiDocument`、Unmount 或替换文档时，文档持有的 Binding 和
DynamicResource committed effects 会自动逆序撤销。
