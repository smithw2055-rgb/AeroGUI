# AeroGUI SDK Packaging and ABI Contract

The installable SDK exports CMake targets under the `Aero::` namespace and
keeps host tools separate from target libraries. A native consumer can use:

```cmake
find_package(Aero 0.2 CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE Aero::Runtime)
```

Applications compile XAML with:

```cmake
aero_add_xaml(MyApp
    ORIGIN_PREFIX "pack://application:,,,/My.App;component"
    SOURCES Views/Main.xaml Themes/App.xaml)
```

Cross-compiling builds set `AERO_HOST_XAMLC_EXECUTABLE` to a host-native
`aero-xamlc`. Target-platform executables are never run by the build.

## Version domains

- Product version: `0.2.0`
- Public C++ ABI: `1`
- Module descriptor ABI: `2`
- XAML Facet/Schema ABI: `8`
- Compiled XAML cache format: `7`

Module and XAML Facet registrations reject incompatible ABI versions before the
schema is frozen. AXIR headers carry the XAML schema ABI independently from the
metadata descriptor and facet versions.

## Exported components

The baseline package exports Base, Core, Platform, Text, Presentation, Controls,
Inspector, MarkupKernel, Markup, ModuleCatalog, Runtime, Rhi, RhiOpenGL33,
Render, RenderOpenGL33, and TextRender. Platform-specific backends are exported
when their build options are enabled.

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
