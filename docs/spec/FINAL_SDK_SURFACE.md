# Final SDK surface

AeroGUI exposes WPF/XAML authoring semantics through a small C++17 product
surface. Physical implementation domains, build object components and native
presentation adapters are not SDK products.

## Product entry points

| Product | Header | Purpose |
| --- | --- | --- |
| `Aero::Gui` | `<Aero/Gui.hpp>` | WPF/XAML object model, controls, View and embeddable runtime |
| `Aero::Meta` | `<Aero/Meta.hpp>` | custom type/member authoring facade over Gui |
| `Aero::App` | `<Aero/App.hpp>` | optional default desktop Application/Window host |
| `Aero::Audio` | `<Aero/Audio/Audio.hpp>` | optional audio module |

The installed CMake package exports product targets such as `Aero::Base`,
`Aero::Gui`, `Aero::Meta`, `Aero::App` and optional modules. `Aero::Integration`
is retired. Internal object components, metadata stores, XAML builders,
Renderer internals and native window/surface adapters are not exported products.

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
Window owns one View while source-private App hosting owns its native window and
`App::Detail::RenderContext`.

## Embedded model

Embedded hosts create one process-level `Aero::Gui`, one or more Views, and an
explicit RenderDevice/RenderTarget pair for the selected backend:

```cpp
Aero::Gui gui;
gui.Initialize();
auto view = gui.CreateView(options).Value();

auto device = Aero::Render::CreateD3D11Device(deviceOptions).Value();
auto target = Aero::Render::CreateD3D11RenderTarget(
    device, targetOptions).Value();
view->GetRenderer().Init(device);

Aero::Markup::XamlReader xaml(*view);
auto document = xaml.Load("app:///MainView.xaml").Value();
view->SetContent(std::move(document), size);
view->Update(elapsedMilliseconds);
view->GetRenderer().UpdateRenderTree();
view->GetRenderer().RenderOffscreen();
view->GetRenderer().Render(*target);
```

Public D3D11/OpenGL embedded headers expose no native-window factory or present
policy. Default desktop window/swap-chain construction belongs to App's private
RenderContext.

## Metadata model

Custom modules receive `Aero::Meta::Registration` and author types through
`Aero::Meta::Register<T>()`. Process composition owns the underlying registry;
mutable tables and executable metadata remain implementation details.

```cpp
Aero::Base::Result<void> RegisterTypes(
    Aero::Meta::Registration& registration) noexcept {
    return Aero::Meta::Register<MyControl>(registration)
        .Factory()
        .Result();
}
```

## Rendering model

The retained path is intentionally short:

```text
UI objects
  -> RenderTree
  -> immutable RenderFrame
  -> IRenderer
  -> RenderDevice / source-private Render::Renderer
  -> RenderTarget
```

`RenderSurface` is retired from the installed SDK. Native acquire/present state
remains source-private. The old `NativeRenderTarget` and `DeviceRenderer`
spellings are zero-cost source aliases, not additional objects or lifetimes.

Rendering statistics are opt-in through `<Aero/Diagnostics/Rendering.hpp>`;
they are not part of the normal RenderDevice authoring surface.

AeroGUI creates no hidden rendering thread or submission queue; the host owns
scheduling.

## Stability rule

The following transitional product names must not reappear in installed headers:

```text
Integration
RuntimeEnvironment
ViewHost
ViewFrameResult
RenderEndpoint
RenderSurface
MetadataContext
MetadataRuntime
```

Permanent architecture gates enforce final dependency and SDK invariants rather
than preserving historical migration file names or internal build targets.
