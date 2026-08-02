# AeroGUI SDK packaging and boundary

The installed package exposes six product targets and no Aero implementation
archives:

```cmake
find_package(Aero 0.3 CONFIG REQUIRED)

target_link_libraries(MyControls PRIVATE Aero::Gui Aero::Meta)
target_link_libraries(DesktopApp PRIVATE Aero::App)
target_link_libraries(EngineHost PRIVATE Aero::Integration)
target_link_libraries(AudioFeature PRIVATE Aero::Audio)
```

- `Aero::Base` — allocator, strings, containers, ownership and ABI foundation.
- `Aero::Gui` — retained WPF/XAML object model, controls, markup and drawing.
- `Aero::Meta` — typed metadata and module authoring layered over Gui.
- `Aero::Integration` — View, XAML/font/texture providers and native renderer integration.
- `Aero::App` — optional default native desktop lifetime.
- `Aero::Audio` — optional audio product independent from Application lifetime.

Internal Gui, Controls, Markup, Runtime, text-provider and rendering domains
compile as build-only object components. They are folded into the product
binaries and never appear in `AeroTargets.cmake`. Static packages additionally
carry only the vendored archives required to resolve private third-party
symbols. Their imported names are `_PrivateFreeType`, `_PrivateHarfBuzz` and,
when applicable, `_PrivateExpat`; they are not Aero SDK layers and carry no
source-compatibility promise. Shared packages export only the six product
targets.

The installed header set is declared explicitly in
`cmake/AeroPublicHeaders.cmake`; the build does not recursively install the
source include directory. The physical public tree and this whitelist must
match exactly. There is no installed `Aero/Detail` directory, and standard
controls are published through six canonical family headers beneath
`Aero/Controls`. See `docs/spec/PUBLIC_HEADER_MODEL.md` for declaration and
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
    return app.Run();
}
```

`Aero::Application` and `Aero::Window` are ordinary WPF-facing XAML objects.
`Application::Run()` uses the private default desktop host. Optional backend,
allocator and diagnostics selection is passed through `Aero::App::RunOptions`;
the SDK does not expose a launcher object. The host maintains one native window,
View, render target and input-services record per top-level Window, so
`Application::Windows` and the three `ShutdownMode` policies have real
multi-window semantics.

Audio and other optional subsystems are separate products; constructing an
Application never creates unrelated platform devices.

## Integration and backend opt-in

Engine/editor/native hosts link `Aero::Integration`, initialize one environment,
load XAML through `Markup::XamlReader`, and create Views directly:

```cpp
#include <Aero/Integration.hpp>
#include <Aero/Integration/D3D11.hpp>
#include <Aero/Markup/XamlReader.hpp>

Aero::Gui environment;
environment.AddModule(MyModule);
environment.Initialize();

Aero::Integration::ViewOptions options;
options.renderDevice =
    Aero::Integration::CreateD3D11WindowDevice(endpointOptions).Value();

auto view = environment.CreateView(options).Value();
Aero::Markup::XamlReader reader(*view);
auto document = reader.Load("MainWindow.xaml").Value();
view->SetContent(std::move(document), {1280.0, 720.0});
view->Update(16U);
```

Concrete backend factories remain opt-in:

- `Aero/Integration/D3D11.hpp`;
- `Aero/Integration/OpenGL33.hpp`.

The default integration headers do not expose immutable RenderFrame storage,
RenderDevice command streams, caches or native backend resource handles.
Submission is synchronous on the caller-selected thread; applications and
engines own render threads, queues and frame-coalescing policy.

## XAML tools

Applications compile XAML with:

```cmake
aero_add_xaml(MyApp
    ORIGIN_PREFIX "pack://application:,,,/My.App;component"
    SOURCES Views/Main.xaml Themes/App.xaml)
```

Cross-compiling builds set `AERO_HOST_XAMLC_EXECUTABLE` to a host-native
`aero-xamlc`. Target-platform executables are never run by the build. Generated
AXIR paths preserve source-relative directories so equal basenames do not
collide.

## Build and install ownership

The source domains are organized with object components rather than installable
support libraries:

```text
AeroGuiKernelObjects + AeroTextObjects + AeroControlsObjects
+ AeroMarkupKernelObjects + AeroMarkupObjects + AeroInspectorObjects
    -> Aero::Gui

AeroAppModelObjects + AeroModuleSetObjects + AeroRuntimeObjects
+ AeroRenderingObjects + built-in text-provider objects
    -> Aero::Integration

private desktop host + OS window/input adapters
    -> Aero::App
```

This model has three consequences:

1. an installed consumer sees product concepts rather than repository topology;
2. static and shared packages use the same public dependency graph;
3. internal refactoring does not create or rename imported SDK targets.

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

There is no standalone platform target. Platform-neutral contracts are part of
Integration; reusable memory-backed services are implemented there, while the
default OS window, clipboard and IME adapters are private App sources.
