# Final SDK surface

AeroGUI exposes WPF/XAML authoring semantics through a small C++17 product
surface. Physical implementation domains, build object components and platform
backends are not SDK concepts.

## Product entry points

| Product | Header | Purpose |
| --- | --- | --- |
| `Aero::Gui` | `<Aero/Gui.hpp>` | WPF/XAML object model and controls |
| `Aero::Meta` | `<Aero/Meta.hpp>` | custom type and member registration |
| `Aero::Integration` | `<Aero/Integration.hpp>` | embedded View, input and RenderDevice integration |
| `Aero::App` | `<Aero/App.hpp>` | optional default desktop Application/Window host |

The installed CMake package exports `Aero::Base`, `Aero::Audio`, `Aero::Gui`,
`Aero::Meta`, `Aero::Integration` and `Aero::App`. Internal object components,
metadata tables, XAML builders and rendering backends are not exported.

## Desktop model

Application code uses the familiar WPF spine:

```cpp
Aero::Application
Aero::Window
Aero::DependencyObject
Aero::UIElement
Aero::FrameworkElement
Aero::Controls::*
```

`Application::Run()` owns the optional native desktop loop. Each top-level
Window owns one View and native peer; `Application::Windows`, `MainWindow` and
`ShutdownMode` operate on the real window collection.

## Embedded model

Embedded hosts create one process-level `Aero::Gui`, then one or more Views:

```cpp
Aero::Gui gui;
gui.Initialize();
auto view = gui.CreateView(options).Value();

Aero::Markup::XamlReader xaml(*view);
auto document = xaml.Load("app:///MainView.xaml").Value();
view->SetContent(std::move(document), size);
view->Update(elapsedMilliseconds);
```

The public View surface is limited to content, size, update, input and
RenderDevice attachment. XAML, resource layers, source providers and fragment
mounting belong to `Markup::XamlReader`.

## Metadata model

Custom modules receive `Aero::Meta::Registration` and author types through
`Aero::Meta::Register<T>()`. Process composition owns one
`Aero::Meta::Registry`. Both are canonical public types, not aliases of private
Core types.

```cpp
Aero::Base::Result<void> RegisterTypes(
    Aero::Meta::Registration& registration) noexcept {
    return Aero::Meta::Register<MyControl>(registration)
        .Factory()
        .Result();
}
```

Mutable tables, behavior records, dependency-property storage and executable
metadata remain implementation details.

## Rendering model

The retained rendering path has one conceptual boundary:

```text
UI objects -> RenderTree -> immutable RenderFrame -> Renderer -> RenderDevice
```

`RenderDevice` is the only public GPU attachment. Native surfaces, backend
state, resource caches and command encoding remain private. AeroGUI creates no
hidden rendering thread or submission queue; the host owns scheduling.

## Stability rule

The following transitional names must not reappear in public headers:

```text
GUI
RuntimeEnvironment
ViewHost
ViewFrameResult
RenderEndpoint
 MetadataContext
 MetadataRuntime
```

Architecture checks enforce this rule together with the narrowed View and
FrameworkElement surfaces.
