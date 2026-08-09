# Final SDK surface

AeroGUI exposes WPF/XAML authoring semantics through a small C++17 product
surface. Physical implementation domains, build object components and native
presentation adapters are not SDK products.

## Product entry points

| Product | Header | Purpose |
| --- | --- | --- |
| `Aero::Gui` | `<Aero/Gui.hpp>` | backend-neutral WPF/XAML object model, controls, View and runtime |
| `Aero::Render` | `<AeroRender/Render.hpp>` | backend-neutral renderer, device and target contracts; no separate DLL |
| `Aero::RenderD3D11` | `<AeroRender/D3D11.hpp>` | opt-in D3D11 device/target factories |
| `Aero::RenderOpenGL33` | `<AeroRender/OpenGL33.hpp>` | opt-in OpenGL 3.3 device/target factories |
| `Aero::Meta` | `<Aero/Meta.hpp>` | custom type/member authoring facade over Gui |
| `Aero::App` | `<AeroApp/App.hpp>` | optional default desktop Application/Window host |
| `Aero::Audio` | `<AeroAudio/Audio.hpp>` | optional audio module |

The installed CMake package exports `Aero::Base`, `Aero::Gui`, the
`Aero::Render` interface target, both Render backend products, `Aero::App` and
optional modules. `Aero::Meta` is an authoring namespace within Gui, not a
linker target. There is no Integration product or independent Meta linker
target. Internal object components, metadata stores, XAML builders,
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
source-private `App::RenderContext`.

## Embedded model

Embedded hosts link `Aero::Gui` plus one Render backend, create one process-level
Gui and one or more Views, and own the RenderDevice/RenderTarget pair:

```cpp
Aero::Gui gui;
gui.Initialize();
auto root = gui.LoadXaml<Aero::FrameworkElement>(
    "app:///MainView.xaml").Value();
auto view = gui.CreateView(root, options).Value();

auto device = Aero::Render::D3D11::CreateDevice(deviceOptions).Value();
auto target = Aero::Render::D3D11::CreateTarget(
    device, targetOptions).Value();
view->GetRenderer().Init(device);

view->SetSize(size);
view->Update(totalTimeSeconds);
auto& renderer = view->GetRenderer();
if (renderer.UpdateRenderTree() && renderer.RenderOffscreen()) {
    renderer.Render(*target);
}
```

Public D3D11/OpenGL embedded headers expose no native-window factory or present
policy. Default desktop window/swap-chain construction belongs to App's private
RenderContext.

## Metadata model

Custom modules receive `Aero::Meta::Registration` and author types through
`Aero::Meta::Register<T>()`. Process composition owns the underlying registry;
mutable tables and executable metadata remain implementation details.

```cpp
Aero::Result<void> RegisterTypes(
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
remains source-private. The former native-target wrapper, borrowed-target path
and `DeviceRenderer` compatibility spelling are removed rather than retained as
aliases.

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
