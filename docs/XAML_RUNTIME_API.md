# XAML Runtime API

The public SDK is organized as Gui, App, Integration and Meta. WPF-facing
application and control code does not construct runtime managers or renderer
objects.

## GUI and Meta authoring

Normal control code includes `Aero/Gui.hpp`; custom types additionally include
`Aero/Meta.hpp` and `Aero/Module.hpp`:

```cpp
Aero::Base::Result<void> RegisterModule(
    Aero::Meta::Context& context) noexcept {
    auto type = Aero::Meta::Describe<MyControl>(context);
    type.Property(
            MyControl::EnabledProperty,
            Aero::Meta::PropertyOptions(true))
        .Factory();
    return type.Result();
}

constexpr auto module =
    Aero::DefineModule("Aero.MyModule", &RegisterModule);
```

Metadata contexts are callback-scoped. Catalogs, stores, dependency-property
provider sessions, XAML facets and runtime managers are private implementation.

## Default App lifetime

A standalone desktop program links `Aero::App`:

```cpp
#include <Aero/App.hpp>

Aero::App::Launcher launcher;
launcher.AddModule(module);
auto result = launcher.Run();
```

`Application` and `Window` retain WPF/XAML semantics. Launcher owns native
window creation, endpoint choice and event pumping behind a PImpl; no public
low-level host facade or service locator is required.


## WPF-facing application and control semantics

`Application` and `Window` remain derivable XAML types. The default launcher
accepts registered subclasses by semantic metadata inheritance rather than by
exact runtime type. `Application::MainWindow` and `ShutdownMode` describe
application lifetime; the first desktop host is single-window, so
`OnLastWindowClose` and `OnMainWindowClose` currently converge on the same
default-host behavior while `OnExplicitShutdown` keeps the application loop
alive until `Shutdown()` is called.

Custom rendering follows the WPF-shaped protected hook:

```cpp
class Meter final : public Aero::FrameworkElement {
protected:
    Aero::Base::Result<void> OnRender(
        Aero::DrawingContext& context) noexcept override {
        return context.DrawRectangle(
            {0.0, 0.0, RenderSize().width, RenderSize().height},
            {0.2F, 0.6F, 0.9F, 1.0F});
    }
};
```

The public `DrawingContext` records semantic drawing operations. Display-list
builders, render resource identifiers and GPU command streams remain runtime
implementation. `ICommand` is likewise manager-free, and public Binding types
contain authoring state rather than scheduler descriptors.

## Integration

Embedded hosts link `Aero::Integration` and explicitly compose a View:

```cpp
#include <Aero/Integration.hpp>
#include <Aero/Integration/D3D11.hpp>

Aero::RuntimeEnvironment environment;
environment.AddModule(module);
environment.Initialize();

auto endpoint =
    Aero::Integration::CreateD3D11WindowEndpoint(endpointOptions);
Aero::Integration::ViewHostOptions options;
options.renderEndpoint = std::move(endpoint).Value();
auto created = Aero::Integration::ViewHost::CreateView(environment, options);
```

`RuntimeEnvironment` freezes module/schema composition. Each View owns resource,
interaction, layout, text and frame state. `RenderEndpoint` remains opaque.
Concrete backends are opt-in and never leak renderer/RHI objects into Gui.

## XAML load transaction

`UiDocument` is a move-only load result. Loading creates an unmounted object
graph; `View::SetContent` commits binding, dynamic-resource and mount side
effects. Failure leaves no partially mounted document.

The public schema surface resolves types and members. Object construction,
member writes, initialization, NameScope/resource scopes, deferred content and
Facet lookup remain private to the loader and ObjectWriter.
