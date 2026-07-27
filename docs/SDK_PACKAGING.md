# AeroGUI SDK Packaging and ABI Contract

The installable SDK exports CMake targets under the `Aero::` namespace and
keeps host tools separate from target libraries. A native consumer can use:

```cmake
find_package(Aero 0.3 CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE Aero::EngineHost)
```

Applications compile XAML with:

```cmake
aero_add_xaml(MyApp
    ORIGIN_PREFIX "pack://application:,,,/My.App;component"
    SOURCES Views/Main.xaml Themes/App.xaml)
```

Cross-compiling builds set `AERO_HOST_XAMLC_EXECUTABLE` to a host-native
`aero-xamlc`. Target-platform executables are never run by the build. Generated
AXIR paths preserve each source's relative directory, so equal basenames in
`Views/` and `Themes/` do not collide. Ninja and Makefile builds also consume
the depfile emitted by `aero-xamlc`.

## Version domains

- Product version: `0.3.0`
- Public C++ ABI: `2`
- Module descriptor ABI: `3`
- XAML Facet/Schema ABI: `9`
- Runtime ABI: `4`
- Compiled XAML cache format: `7`
- Compiled document encoding: `2`

All values above are generated from `cmake/AeroVersions.cmake` into the public
`Aero/Version.hpp`, installed package variables, runtime, and host tools. Module
and XAML Facet registrations reject incompatible ABI versions before the schema
is frozen. AXIR headers carry the XAML schema ABI independently from the metadata
descriptor and facet versions. `aero-xamlc --version` reports the active product,
schema, cache, and encoding versions.

## Exported components

Normal applications link `Aero::EngineHost`; custom-control and schema-only
libraries link `Aero::ModuleSdk`. `Aero::Runtime` remains available for hosts
that supply their own render integration. Lower-level Base, Core, Presentation,
Controls, Markup, Render, and RHI targets remain exported for advanced embedders,
but they are not the default product entry points. Platform-specific backends
are exported when their build options are enabled.

The SDK installs public headers, built-in theme XAML, the CMake package files,
and `aero-xamlc` when tools are enabled.

## Runtime composition

The installed SDK exposes `Aero::SchemaBundle`, `Aero::RuntimeEnvironment`,
`Aero::RuntimeView`, and `Aero::UiDocument`. A module descriptor contributes
metadata and XAML facets through two versioned callbacks. The same frozen schema
bundle is consumed by runtime views and host-side XAML tooling.

`RuntimeEnvironment` is application/process scoped. `RuntimeView` is surface or
window scoped and retains independent input, resource, binding, layout, and render
state. `UiDocument` is move-only document ownership and can be loaded before a
view is mounted.

## Host schema manifests

Target-platform modules do not have to be loaded into the host `aero-xamlc`.
A native host-tools build exports the immutable validation surface to an
`.aeroschema` file, then passes that file to XAML compilation:

```cmake
aero_add_schema_manifest(MyAppSchema
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/MyApp.aeroschema"
    MODULE_HEADER "MyApp/Modules.hpp"
    MODULE_FUNCTION "MyApp::RegisterModules"
    LIBRARIES MyAppModules)

aero_add_xaml(MyApp
    SCHEMA "${MyAppSchema_OUTPUT}"
    ORIGIN_PREFIX "pack://application:,,,/My.App;component"
    SOURCES Views/Main.xaml)
add_dependencies(MyApp MyAppSchema)
```

The module function has the following contract:

```cpp
Aero::Base::Result<void> RegisterModules(
    Aero::ModuleCatalog& modules) noexcept;
```

The generated manifest contains TypeId/MemberId values, namespaces, type
inheritance, property/event descriptors, effective content members, and the
compiled-XAML schema identity. Factories, callbacks, raw pointers, allocators,
and runtime service instances are never serialized.

`aero-schema-gen` emits and validates the built-in Aero schema:

```text
aero-schema-gen Aero.aeroschema
aero-schema-gen --check Aero.aeroschema
aero-schema-gen --describe Aero.aeroschema
```

For cross-compilation, generate application manifests in a native host-tools
build and pass the resulting file to `aero_add_xaml(SCHEMA ...)`. The target
build never executes a target-platform module or schema generator.


## Product dogfood and install gate

`ControlGallery` uses the same public workflow as an application: it registers
its custom-control module, generates a `.aeroschema`, compiles XAML through
`aero_add_xaml()`, creates a view through `RuntimeEnvironment::CreateView()`, and
links the product-facing SDK targets. It no longer builds a sample-specific
XAML compiler.

The test suite installs Aero into an isolated prefix, configures a separate
`find_package(Aero)` consumer, compiles two `Main.xaml` files in different
relative directories, links `Aero::EngineHost`, and runs the resulting program.
This gate verifies package exports, host tools, schema paths, version headers,
and collision-free generated XAML outputs rather than only testing the source
tree build.
