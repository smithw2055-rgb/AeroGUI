# AeroGUI SDK packaging and boundary

The installed package has four explicit product targets:

```cmake
find_package(Aero 0.3 CONFIG REQUIRED)

target_link_libraries(MyControls PRIVATE Aero::Gui Aero::Meta)
target_link_libraries(DesktopApp PRIVATE Aero::App)
target_link_libraries(EngineHost PRIVATE Aero::Integration)
```

- `Aero::Gui` — retained WPF/XAML objects, controls, markup and drawing.
- `Aero::App` — optional default native desktop lifetime.
- `Aero::Integration` — View, RenderEndpoint and host/provider integration.
- `Aero::Meta` — typed metadata and module authoring layered over Gui.

Legacy runtime/module/integration aliases and the low-level host facade are
retired. Runtime composition, module catalogs, renderer/RHI implementations
and third-party provider adapters may still be exported under
`IntegrationDetail*` or `RuntimeDetail*` names when static linking requires
them, but those names are package implementation details and are not supported
application APIs.

The installed header set is declared explicitly in
`cmake/AeroPublicHeaders.cmake`; the build does not recursively install the
source include directory. The physical public tree and this whitelist must
match exactly. There is no installed `Aero/Detail` directory, and standard
controls are published through six canonical family headers beneath
`Aero/Controls`. See `docs/spec/PUBLIC_HEADER_MODEL.md` for the declaration and
header-growth rules.

## GUI and custom controls

Normal WPF-style code includes `Aero/Gui.hpp`. Custom controls add the typed
metadata entry points explicitly:

```cpp
#include <Aero/Gui.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>

Aero::Base::Result<void> RegisterMyModule(
    Aero::Meta::Context& context) noexcept;

constexpr Aero::ModuleRegistration MyModule =
    Aero::DefineModule("My.Module", &RegisterMyModule);
```

`Meta::Context` is callback-scoped. Catalogs, registration stores, frozen
execution data, XAML facets and dependency-property provider state remain
implementation details.

## Default application framework

A standalone desktop application links `Aero::App` and uses one simple entry
point:

```cpp
#include <Aero/App.hpp>

int main() {
    Aero::App::Launcher app;
    auto result = app.Run();
    return result ? result.Value() : 1;
}
```

`Aero::Application` and `Aero::Window` are ordinary WPF-facing XAML objects.
`Launcher` privately owns native windows, event pumping, View composition and
endpoint selection. Audio and other optional subsystems are separate modules;
constructing an Application never creates platform devices.

## Integration and backend opt-in

Engine/editor/native hosts link `Aero::Integration`, create or receive an
endpoint and create a View through `Integration::ViewHost`:

```cpp
#include <Aero/Integration.hpp>
#include <Aero/Integration/D3D11.hpp>

Aero::RuntimeEnvironment environment;
environment.AddModule(MyModule);
environment.Initialize();

auto endpoint =
    Aero::Integration::CreateD3D11WindowEndpoint(endpointOptions);
Aero::Integration::ViewHostOptions options;
options.renderEndpoint = std::move(endpoint).Value();
auto view = Aero::Integration::ViewHost::CreateView(environment, options);
```

Concrete backend factories remain opt-in:

- `Aero/Integration/D3D11.hpp`;
- `Aero/Integration/OpenGL33.hpp`;
- `Aero/Integration/HostedGraphics.hpp` for versioned third-party callbacks.

The default integration headers do not expose the internal render snapshot,
render managers, RHI devices, caches or backend resource handles.

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
