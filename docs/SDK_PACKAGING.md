# AeroGUI SDK packaging and boundary

The installed package exposes backend-neutral Gui and Render contracts, two
backend products, App, Base and optional Audio:

```cmake
find_package(Aero 0.3 CONFIG REQUIRED)

target_link_libraries(MyControls PRIVATE Aero::Gui)
target_link_libraries(RenderExtension PRIVATE Aero::Render)
target_link_libraries(EngineHost PRIVATE Aero::RenderD3D11)
target_link_libraries(DesktopApp PRIVATE Aero::App)
target_link_libraries(AudioFeature PRIVATE Aero::Audio)
```

- `Aero::Base` — allocator, strings, containers, ownership and ABI foundation.
- `Aero::Gui` — the backend-neutral WPF/XAML runtime: object model, controls,
  markup, View and providers.
- `Aero::Render` — backend-neutral renderer/device/target contracts implemented
  by Gui; this is an interface target, not another binary.
- `Aero::RenderD3D11` / `Aero::RenderOpenGL33` — opt-in native backend factories.
- `Aero::Meta` is the metadata authoring namespace shipped by `Aero::Gui`, not a separate link target.
- `Aero::App` — optional default native desktop lifetime layered over Gui.
- `Aero::Audio` — optional audio product independent from Application lifetime.

Internal Gui, Controls, Markup, text-provider and backend-neutral rendering
domains compile directly into `Aero::Gui` and never appear in
`AeroTargets.cmake`. Static packages additionally carry only
the vendored archives required to resolve private third-party symbols. Their
imported names are `_PrivateFreeType`, `_PrivateHarfBuzz` and, when applicable,
`_PrivateExpat`; they are not Aero SDK layers and carry no source-compatibility
  promise. Shared packages export only the public product targets.

## Shared-library boundary

The shared build follows the same package shape as the public Noesis render
packages: the core runtime owns the abstract render contracts, while each
native backend is a separate linker product whose public surface is only its
factory API.

```text
AeroBase.dll
AeroAudio.dll                -> AeroBase.dll
AeroGui.dll                  -> AeroBase.dll
AeroRenderD3D11.dll          -> AeroGui.dll (CreateDevice, CreateTarget only)
AeroRenderOpenGL33.dll       -> AeroGui.dll (CreateDevice, CreateTarget only)
AeroApp.dll                  -> AeroGui.dll + enabled backend DLLs
```

Concrete D3D11/OpenGL device and target classes stay under `src/render` and
are not installed or exported. Backend targets acquire and retire a native
`FrameTarget`; `AeroGui` alone owns `ViewRenderer` and onscreen frame
submission. This prevents a backend DLL from importing GUI implementation
classes just to draw a target.

On Windows, every shared product runs a post-link `dumpbin /exports` guard.
Each backend DLL must export exactly two named functions, `CreateDevice` and
`CreateTarget`, and the guard rejects concrete backend, renderer, host and
source-state names. The backend DLLs and `AeroGui.dll` are released as one SDK
version; their source-private implementation ABI is not an extension point.

The white-box conformance executables intentionally instantiate source-private
renderer/backend types, so they run from a static build. Shared configurations
instead build and run public header/link consumers and validate the real DLL
export tables; internal tests are not a reason to widen product exports.

The installed header set is declared explicitly in
`cmake/AeroPublicHeaders.cmake`; the build does not recursively install the
source include directory. The physical public tree and this whitelist must
match exactly. There is no installed `Aero/Detail` directory, and standard
controls are published through type-named headers beneath `Aero/Controls`. See
`docs/spec/PUBLIC_HEADER_MODEL.md` for declaration and
header-growth rules.

## Gui and custom controls

Normal WPF-style code includes `Aero/Gui.hpp`. Custom controls add the typed
metadata entry points explicitly:

```cpp
#include <Aero/Gui.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>

Aero::Result<void> RegisterMyModule(
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
#include <AeroApp/App.hpp>

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

Engine, editor and native hosts link `Aero::Gui` plus one backend product.
Backend headers and linker products are both explicit opt-ins:

```cpp
#include <Aero/Gui.hpp>
#include <AeroRender/D3D11.hpp>
#include <Aero/Markup.hpp>

Aero::Gui gui;
gui.AddModule(MyModule);
gui.Initialize();

Aero::Render::D3D11::DeviceOptions deviceOptions;
auto device = Aero::Render::D3D11::CreateDevice(deviceOptions).Value();
Aero::Render::D3D11::TargetOptions targetOptions;
auto target = Aero::Render::D3D11::CreateTarget(
    device, targetOptions).Value();
auto root = gui.LoadXaml<Aero::FrameworkElement>(
    "MainWindow.xaml").Value();
auto view = gui.CreateView(root).Value();
view->GetRenderer().Init(device);

view->SetSize({1280.0, 720.0});
view->Update(totalTimeSeconds);
auto& renderer = view->GetRenderer();
if (renderer.UpdateRenderTree() && renderer.RenderOffscreen()) {
    renderer.Render(*target);
}
```

Concrete backend factories remain opt-in:

- `AeroRender/D3D11.hpp`;
- `AeroRender/OpenGL33.hpp`.
- `Aero::RenderD3D11` or `Aero::RenderOpenGL33` at link time.

The installed CMake package intentionally does not export `Aero::Integration`.
Domain-owned host headers do not expose
immutable RenderFrame storage, command streams, caches or native backend
resource handles. Submission is synchronous on the caller-selected thread;
applications and engines own render threads, queues and frame-coalescing policy.


The installed public paths are ownership-oriented:

- `Aero/ViewOptions.hpp` and `AeroRender/RenderTarget.hpp` — View and presentation contracts;
- `Aero/Markup/XamlProvider.hpp` and `Aero/Markup/ReloadCoordinator.hpp`;
- `Aero/Media/TextureProvider.hpp`;
- `Aero/Media/FontProvider.hpp`;
- `Aero/InputInterop.hpp` and `AeroApp/WindowInterop.hpp`;
- `AeroRender/D3D11.hpp` and `AeroRender/OpenGL33.hpp`.

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

Shared product builds default `AERO_BUILD_TOOLS` to `OFF`. The offline tools
consume source-only schema/compiler contracts and are therefore built in a
separate static host-tools configuration; widening the product DLL export table
for them is not supported. Supply the resulting executables through
`AERO_HOST_XAMLC_EXECUTABLE` and `AERO_HOST_SCHEMA_GEN_EXECUTABLE` when a shared
build should precompile assets.

## Build and install ownership

The source domains are organized with object components rather than installable
support libraries:

```text
AeroGuiKernelObjects + AeroTextObjects + AeroControlsObjects
+ AeroMarkupKernelObjects + AeroMarkupObjects + AeroInspectorObjects
+ AeroModuleSetObjects + AeroRuntimeObjects + backend-neutral rendering
+ built-in text-provider objects
    -> Aero::Gui

native D3D11 backend + factories -> Aero::RenderD3D11
native OpenGL33 backend + factories -> Aero::RenderOpenGL33

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
- Module descriptor ABI: `4`
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
