# AeroGUI SDK packaging and boundary

The installed package has three supported entry points:

```cmake
find_package(Aero 0.3 CONFIG REQUIRED)

target_link_libraries(ProductApp PRIVATE Aero::Runtime)
target_link_libraries(CustomControls PRIVATE Aero::ModuleSdk)
target_link_libraries(NativeHost PRIVATE Aero::IntegrationSdk)
```

- Product SDK: `Aero/Runtime.hpp` and `Aero::Runtime`.
- Module SDK: `Aero/ModuleSdk.hpp` and `Aero::ModuleSdk`.
- Integration SDK: `Aero/Integration.hpp` and
  `Aero::IntegrationSdk`.

Render, RHI, platform surface implementations and module-catalog
implementations are package-private dependencies. They may be emitted in the
CMake export so static libraries can resolve their implementation graph, but
their imported names use `Aero::IntegrationDetail*` or
`Aero::RuntimeDetail*`; they are not supported SDK components.

There are no `Aero::Rhi*`, `Aero::Render*`, `Aero::ModuleCatalog` or legacy
host aggregation targets.

## Product runtime

`RuntimeEnvironment` owns immutable application composition. After module
registration and `Initialize()`, the product path is:

```cpp
Aero::RuntimeEnvironment environment;
environment.AddModule(MyModule());
environment.Initialize();

auto view = environment.CreateView(); // built-in headless endpoint
view.Value()->LoadContent("app:///Main.xaml", {1280.0, 720.0});
view.Value()->RunFrame();
```

Product headers expose `RuntimeEnvironment`, `View`, `UiDocument`, input values
and safe frame diagnostics. They do not expose schema managers, document
caches, RenderPlan, RHI devices or backend resource handles.

## Module authoring

`ModuleSdk.hpp` aggregates typed metadata/property/event authoring,
Style/Template authoring, control base classes and narrow drawing commands.
`ModuleRegistration` is the only module descriptor:

```cpp
Aero::Base::Result<void> RegisterMyModule(
    Aero::MetadataContext& context) noexcept;

constexpr Aero::ModuleRegistration MyModule =
    Aero::DefineModule("My.Module", &RegisterMyModule);
```

`MetadataContext` is callback-scoped and opaque. Registry, catalog, registration
store and frozen execution data remain implementation details. Schema tools use
the private catalog implementation while consuming the same
`ModuleRegistration` values as Runtime.

Application schema generation names a function that returns a module
registration:

```cmake
aero_add_schema_manifest(MyAppSchema
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/MyApp.aeroschema"
    MODULE_HEADER "MyApp/Modules.hpp"
    MODULE_FUNCTION "MyApp::MakeModule"
    LIBRARIES MyAppModules)
```

```cpp
Aero::ModuleRegistration MakeModule() noexcept;
```

## Integration and backend opt-in

Normal native hosts link `Aero::IntegrationSdk`, create a backend endpoint from
an opt-in header, and pass it to `Integration::ViewHost::CreateView()`.
`Integration.hpp` never aggregates a concrete backend.

The first-party factory headers are:

- `Aero/Integration/D3D11.hpp`;
- `Aero/Integration/OpenGL33.hpp`.

Third-party backend authors explicitly include
`Aero/Integration/HostedGraphics.hpp`. Its versioned C-compatible callback
table is the only third-party graphics authoring boundary. It does not expose
the UI tree, internal render snapshot, renderer managers, caches or RHI
classes.

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

The SDK installs public headers, built-in themes, package configuration and
enabled host tools. Internal source headers are never installed.

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
