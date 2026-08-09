# XAML Runtime API

The public SDK is organized as Base, Gui, Meta, App and optional Audio products.
WPF-facing application and control code does not construct runtime managers or
renderer implementation objects.

## Gui and Meta authoring

Hosts include `Aero/Gui.hpp` for the runtime entry point. Control code includes
the WPF type headers it uses (or `Aero/Controls.hpp`); custom types additionally
include `Aero/Meta.hpp` and `Aero/Module.hpp`:

```cpp
Aero::Result<void> RegisterModule(
    Aero::Meta::Registration& context) noexcept {
    auto type = Aero::Meta::Register<MyControl>(context);
    type.Property(
            MyControl::EnabledProperty,
            Aero::Meta::FrameworkPropertyMetadata(true))
        .Factory();
    return type.Result();
}

constexpr auto module =
    Aero::DefineModule("Aero.MyModule", &RegisterModule);
```

Metadata contexts are callback-scoped. Catalogs, stores, dependency-property
provider sessions, XAML facets and runtime managers are private implementation.

## Default App lifetime

A standalone desktop program links `Aero::App` and runs its WPF-facing
`Application` object directly:

```cpp
#include <AeroApp/App.hpp>

Aero::Application application;
static_cast<void>(application.SetStartupUri("MainWindow.xaml"));

const Aero::ModuleRegistration modules[] = {module};
Aero::App::RunOptions options;
options.modules = modules;
auto run = application.Run(options);
return run ? run.Value() : 1;
```

`Application` and `Window` retain WPF/XAML semantics. Native-window creation,
endpoint choice and event pumping live in the private desktop host; no public
launcher, low-level host facade or service locator is required.


## WPF-facing application and control semantics

`Application` and `Window` remain derivable XAML types. The default App runtime
accepts registered subclasses by semantic metadata inheritance rather than by
exact runtime type. `Application::Windows`, `MainWindow` and `ShutdownMode` describe application
lifetime. The desktop host maintains one native window, View and rendering
attachment per top-level Window, so last-window, main-window and explicit
shutdown modes retain distinct WPF semantics.

Custom rendering follows the WPF-shaped protected hook:

```cpp
class Meter : public Aero::FrameworkElement {
protected:
    void OnRender(
        Aero::DrawingContext& context) noexcept override {
        static_cast<void>(context.DrawRectangle(
            {0.0, 0.0, GetRenderSize().width, GetRenderSize().height},
            {0.2F, 0.6F, 0.9F, 1.0F}));
    }
};
```

The public `DrawingContext` records semantic drawing operations. Display-list
builders, render resource identifiers and GPU command streams remain runtime
implementation. `ICommand` is likewise manager-free, and public Binding types
contain authoring state rather than scheduler descriptors.

## Embedded hosting

Embedded engine/editor hosts link `Aero::Gui` plus `Aero::RenderD3D11` or
`Aero::RenderOpenGL33`, choose an explicit RenderDevice and RenderTarget, then
drive the View from their own frame loop:

```cpp
#include <Aero/Gui.hpp>
#include <AeroRender/D3D11.hpp>

Aero::Gui environment;
environment.AddModule(module);
environment.Initialize();

auto device = Aero::Render::D3D11::CreateDevice(deviceOptions).Value();
auto target = Aero::Render::D3D11::CreateTarget(
    device, targetOptions).Value();
auto root = environment.LoadXaml<Aero::FrameworkElement>(
    "app:///MainView.xaml").Value();
auto view = environment.CreateView(root).Value();
view->GetRenderer().Init(device);

view->Update(totalTimeSeconds);
auto& renderer = view->GetRenderer();
if (renderer.UpdateRenderTree() && renderer.RenderOffscreen()) {
    renderer.Render(*target);
}
```

`Gui` freezes module/schema composition. Each View owns resource, interaction,
layout, text and immutable-frame state. The host owns frame scheduling and
native target acquisition callbacks; public Render backend headers expose no
native-window/present policy.

## XAML load transaction

Ordinary hosts use `Gui::LoadXaml<T>()` or `Gui::LoadComponent()` and receive a
typed object-tree root. Gui retains the complete pending load transaction until
that root is passed to `CreateView(root)` or `View::SetContent(root)`; mounting
then binds View services and commits binding, dynamic-resource, NameScope, and
visual-edge effects. `Markup::XamlDocument` and `Markup::XamlReader` remain the
move-only document path for Parse, compiled XAML, hot reload, and tooling.
Failure leaves no partially mounted document.

The public schema surface resolves types and members. Object construction,
member writes, initialization, NameScope/resource scopes, deferred content and
Facet lookup remain private to the loader and ObjectWriter.
