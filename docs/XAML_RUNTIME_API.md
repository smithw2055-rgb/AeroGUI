# XAML Runtime API

当前 SDK 分为 Product、Module、Integration 三层。产品运行时只有
`RuntimeEnvironment -> View` 一条路径，不提供独立 Host、公开 manager 或
Update/Render 双阶段入口。

## Product SDK

入口为 `Aero/Runtime.hpp`：

```cpp
Aero::RuntimeEnvironment environment;
environment.AddModule(myModule);
environment.Initialize();

auto created = environment.CreateView();
if (!created) return created.GetStatus();
auto view = std::move(created).Value();

auto document = view->Load("Views/Main.xaml", diagnostics);
if (!document) return document.GetStatus();
view->SetContent(std::move(document).Value(), availableSize);
view->RunFrame();
```

`RuntimeEnvironment` 冻结模块并共享不可变 schema/document cache。
`View` 独占资源、交互、布局、文本和渲染状态，并公开：

- `Load`、`Parse`、`LoadCompiled`;
- `SetContent`、`LoadContent`、`Unmount`;
- application/theme/system 资源和内置主题；
- `Resize`、`RunFrame`、输入分发、`AdvanceTime`;
- `Root`、`FindNamed`、`NamedObjectCount`.

`UiDocument` 是 move-only 事务结果。加载成功只产生待挂载文档；
`SetContent` 成功后才提交 Binding、DynamicResource 等副作用。失败不会留下
部分文档。

Product 头不公开 schema/document manager、render snapshot、renderer、RHI、
surface 或 GPU handle。`ViewFrameResult` 只返回安全 POD 诊断。

## Module SDK

入口为 `Aero/ModuleSdk.hpp`：

```cpp
Aero::Base::Result<void> RegisterModule(
    Aero::MetadataContext& context) noexcept {
    return Aero::Describe<MyControl>(context)
        .Property(MyControl::EnabledProperty,
                  Aero::PropertyOptions(true))
        .Factory()
        .Result();
}

constexpr auto module =
    Aero::DefineModule("Aero.MyModule", &RegisterModule);
```

Module SDK 聚合 typed metadata/property/event、控件基类、Style/Template
authoring 与窄 Drawing authoring。`MetadataContext` 是 callback-scoped opaque
状态；模块不能访问 catalog、registration store、DP/event registry、执行计划、
runtime manager 或 GPU resource registry。

自定义控件可通过 `FrameworkElement::BuildDisplayList()` 和
`DisplayListBuilder` 生成安全绘制指令。image/mesh/glyph ID 是 View 资源系统
管理的不透明 token，模块不能上传或注册 GPU 资源。

## Integration SDK

默认入口为 `Aero/Integration.hpp`：

```cpp
#include <Aero/Integration.hpp>
#include <Aero/Integration/D3D11.hpp>

auto endpoint =
    Aero::Integration::CreateD3D11WindowEndpoint(endpointOptions);
if (!endpoint) return endpoint.GetStatus();

Aero::Integration::ViewHostOptions options;
options.renderEndpoint = std::move(endpoint).Value();
options.clipboard = &clipboard;
options.textInputMethodHost = &ime;
options.text.primaryFamily = "Segoe UI";

auto created = Aero::Integration::ViewHost::CreateView(
    environment, options);
```

`RenderEndpoint` 是引用计数的 opaque PImpl。View 在创建任何 XAML 控件前验证
并绑定 endpoint，并持有强引用。一个 endpoint 只能绑定一个 View。

- Headless：无 GPU device，仅 CPU 诊断和文本资源 sink。
- Embedded：宿主拥有 device/target/Present，AeroGUI 不 Present。
- Window：endpoint 拥有 surface/swapchain/context 和 Present。

`Integration.hpp` 不聚合具体 backend。第一方工厂需显式包含
`Integration/D3D11.hpp` 或 `Integration/OpenGL33.hpp`。第三方 backend 需显式
包含 `Integration/HostedGraphics.hpp`，只接触版本化 C-compatible graphics
command ABI，不接触 UI tree、内部 render snapshot 或 RHI class。

source provider 位于 `Aero::Integration`；provider registry、file/embedded
provider、cache 与依赖图都是 View 私有实现。热重载通过
`Integration::ReloadCoordinator` 和 `ViewHost` 工作。

## Public Schema

`Aero/Markup/Schema.hpp` 的公共面仅用于类型、成员和 content-member 查询。
对象创建、成员写入、初始化、NameScope/resource scope、deferred content 和
markup-extension invocation 由 `src/markup` 的私有 access 完成。

公共 `LoadOptions` 只包含 base URI、安全策略和资源限制。manager、templated
parent、effect lifetime 与 commit 状态属于私有 `LoadContext`。
