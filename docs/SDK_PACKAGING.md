# AeroGUI SDK packaging and boundary

The installed package exposes four product targets and no Aero implementation
archives:

```cmake
find_package(Aero 0.3 CONFIG REQUIRED)

target_link_libraries(MyControls PRIVATE Aero::Gui)
target_link_libraries(EngineHost PRIVATE Aero::Gui)
target_link_libraries(DesktopApp PRIVATE Aero::App)
target_link_libraries(AudioFeature PRIVATE Aero::Audio)
```

- `Aero::Base` — allocator, strings, containers, ownership and ABI foundation.
- `Aero::Gui` — the complete embeddable WPF/XAML runtime: object model,
  controls, markup, View, providers and native GPU rendering.
- `Aero::Meta` is the metadata authoring namespace shipped by `Aero::Gui`, not a separate link target.
- `Aero::App` — optional default native desktop lifetime layered over Gui.
- `Aero::Audio` — optional audio product independent from Application lifetime.

Internal Gui, Controls, Markup, Runtime, text-provider and rendering domains
compile as build-only object components. They are folded into `Aero::Gui` and
never appear in `AeroTargets.cmake`. Static packages additionally carry only
the vendored archives required to resolve private third-party symbols. Their
imported names are `_PrivateFreeType`, `_PrivateHarfBuzz` and, when applicable,
`_PrivateExpat`; they are not Aero SDK layers and carry no source-compatibility
promise. Shared packages export only the four product targets.

The installed header set is declared explicitly in
`cmake/AeroPublicHeaders.cmake`; the build does not recursively install the
source include directory. The physical public tree and this whitelist must
match exactly. There is no installed `Aero/Detail` directory, and standard
controls are published through type-named headers beneath `Aero/Gui`. See
`docs/spec/PUBLIC_HEADER_MODEL.md` for declaration and
header-growth rules.

## Gui and custom controls

Normal WPF-style code includes `Aero/Gui.hpp`. Custom controls add the typed
metadata entry points explicitly:

```cpp
#include <Aero/Gui.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>

Aero::Base::Result<void> RegisterMyModule(
    Aero::Meta::Registration& context) noexcept;

constexpr Aero::ModuleRegistration MyModule =
    Aero::DefineModule("My.Module", &RegisterMyModule);
```

`Meta::Registration` is callback-scoped. Catalogs, registration stores, frozen
execution data, XAML facets and dependency-property provider state remain
implementation details.

## Default application framework

A standalone desktop application links `Aero::App` and uses the WPF-shaped
Application entry point:

```cpp
#include <Aero/App.hpp>

int main() {
    Aero::Application app;
    static_cast<void>(app.SetStartupUri("MainWindow.xaml"));
    auto run = app.Run();
return run ? run.Value() : 1;
}
```

`Aero::Application` and `Aero::Window` are ordinary WPF-facing XAML objects.
`Application::Run()` uses the private default desktop host. Optional backend,
allocator and diagnostics selection is passed through `Aero::App::RunOptions`;
the SDK does not expose a launcher object. The host maintains one native window,
View, render target and input-services record per top-level Window, so
`Application::Windows` and the three `ShutdownMode` policies have real
multi-window semantics.

`Application::GetResources()` always returns a dictionary reference.
`SetMainWindow(Ref<Window>)` is the only public assignment API. The default
desktop host uses a source-private non-owning adapter when required. The WPF-shaped
`Run()`, `Window::Show()` and `Window::Close()` methods remain simple; code that
needs diagnostics uses the Result-returning `Run()`, `Show()` and `Close()` APIs.
Only one default `Application` may be active process-wide; multiple independent
embedded UI instances remain a `Gui`/`View` responsibility.

Audio and other optional subsystems are separate products; constructing an
Application never creates unrelated platform devices.

## Embedding and backend opt-in

Engine, editor and native hosts link only `Aero::Gui`. Backend headers remain
opt-in C++ API groups; they do not correspond to a second product binary:

```cpp
#include <Aero/Gui.hpp>
#include <Aero/Render/D3D11.hpp>
#include <Aero/Markup.hpp>

Aero::Gui gui;
gui.AddModule(MyModule);
gui.Initialize();

Aero::Render::D3D11DeviceOptions deviceOptions;
auto device = Aero::Render::CreateD3D11Device(deviceOptions).Value();
Aero::Render::D3D11RenderTargetOptions targetOptions;
auto target = Aero::Render::CreateD3D11RenderTarget(
    device, targetOptions).Value();
auto view = gui.CreateView().Value();
view->GetRenderer().Init(device);

Aero::Markup::XamlReader reader(gui);
auto document = reader.Load("MainWindow.xaml").Value();
view->SetContent(std::move(document), {1280.0, 720.0});
view->Update(16U);
view->GetRenderer().UpdateRenderTree();
view->GetRenderer().RenderOffscreen();
view->GetRenderer().Render(*target);
```

Concrete backend factories remain opt-in:

- `Aero/Render/D3D11.hpp`;
- `Aero/Render/OpenGL33.hpp`.

The installed CMake package intentionally does not export `Aero::Integration`.
Domain-owned host headers do not expose
immutable RenderFrame storage, command streams, caches or native backend
resource handles. Submission is synchronous on the caller-selected thread;
applications and engines own render threads, queues and frame-coalescing policy.


The installed public paths are ownership-oriented:

- `Aero/ViewOptions.hpp` and `Aero/RenderTarget.hpp` — View and presentation contracts;
- `Aero/Markup/XamlProvider.hpp` and `Aero/Markup/ReloadCoordinator.hpp`;
- `Aero/Media/TextureProvider.hpp`;
- `Aero/Text/FontProvider.hpp`;
- `Aero/Input/Platform.hpp` and `Aero/Platform/NativeWindow.hpp`;
- `Aero/Render/D3D11.hpp` and `Aero/Render/OpenGL33.hpp`.

There is no installed `Aero/Integration.hpp` umbrella or
`Aero/Integration/` directory. Canonical SDK names use `Aero`, `Aero::Input`,
`Aero::Platform`, `Aero::Markup`, `Aero::Media`, `Aero::Text` and
`Aero::Render`. The repository also has no compatibility include tree; native
backend declarations live in one source-private implementation header.

## XAML tools

Applications compile XAML with:

```cmake
aero_add_xaml(MyApp
    ORIGIN_PREFIX "pack://application:,,,/My.App;component"
    SOURCES Views/Main.xaml Themes/App.xaml)
```

Cross-compiling builds set `AERO_HOST_XAMLC_EXECUTABLE` to a host-native
`aero-xamlc`. Target-platform executables are never run by the build. Generated
AXB2 `.axb` paths preserve source-relative directories so equal basenames do not
collide.

## Build and install ownership

The source domains are organized with object components rather than installable
support libraries:

```text
AeroGuiKernelObjects + AeroTextObjects + AeroControlsObjects
+ AeroMarkupKernelObjects + AeroMarkupObjects + AeroInspectorObjects
+ AeroModuleSetObjects + AeroRuntimeObjects + AeroRenderingObjects
+ built-in text-provider objects + native backend factories
    -> Aero::Gui

AeroAppModelObjects + private desktop host + OS window/input adapters
    -> Aero::App
```

This model has three consequences:

1. an installed consumer sees product concepts rather than repository topology;
2. static and shared packages use the same public dependency graph;
3. internal refactoring does not create repository-shaped support products.

## Version domains

- Product version: `0.3.0`
- Public C++ ABI: `2`
- Module descriptor ABI: `3`
- XAML Facet/Schema ABI: `9`
- Runtime ABI: `4`
- Compiled XAML cache format: `7`
- Compiled document encoding: `2`

The generated `Aero/Version.hpp`, runtime and host tools consume the same
values. Module and schema registration reject incompatible versions before
freezing.

## Platform implementation ownership

There is no standalone platform target. Platform-neutral contracts and reusable
memory-backed services are part of `Aero::Gui`; the default OS window, clipboard
and IME adapters remain private `Aero::App` sources.
