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

A standalone desktop program links `Aero::App` and runs its WPF-facing
`Application` object directly:

```cpp
#include <Aero/App.hpp>

Aero::Application application;
static_cast<void>(application.SetStartupUri("MainWindow.xaml"));

const Aero::ModuleRegistration modules[] = {module};
Aero::App::RunOptions options;
options.modules = modules;
return application.Run(options);
```

`Application` and `Window` retain WPF/XAML semantics. Native-window creation,
endpoint choice and event pumping live in the private desktop host; no public
launcher, low-level host facade or service locator is required.


## WPF-facing application and control semantics

`Application` and `Window` remain derivable XAML types. The default App runtime
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
Aero::Integration::ViewOptions options;
options.renderEndpoint = std::move(endpoint).Value();
auto created = environment.CreateView(options);
```

`RuntimeEnvironment` freezes module/schema composition. Each View owns resource,
interaction, layout, text and frame state. `RenderEndpoint` remains opaque.
Concrete backends are opt-in and never leak renderer/graphics layer objects into Gui.

## XAML load transaction

`UiDocument` is a move-only load result. Loading creates an unmounted object
graph; `View::SetContent` commits binding, dynamic-resource and mount side
effects. Failure leaves no partially mounted document.

The public schema surface resolves types and members. Object construction,
member writes, initialization, NameScope/resource scopes, deferred content and
Facet lookup remain private to the loader and ObjectWriter.
