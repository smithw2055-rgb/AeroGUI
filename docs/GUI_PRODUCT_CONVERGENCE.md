# S3 Gui product convergence

## Decision

`Aero::Gui` is the single backend-neutral WPF/XAML runtime target.

It owns:

- the WPF/XAML object model;
- controls, markup and metadata execution;
- `Gui`, `View` and `IRenderer`;
- XAML, texture and font provider contracts;
- backend-neutral RenderDevice and RenderTarget contracts;
- the built-in FreeType and HarfBuzz runtime pipeline.

`Aero::App` remains optional and adds only the default `Application`/`Window`
lifetime, native window policy and OS input adapters.

## Removed product concept

The build and installed package no longer create:

```cmake
AeroIntegration
Aero::Integration
```

`Aero/Integration/*.hpp` remains an API-grouping path during the public-header
migration. Header layout is not a reason to create a second binary or imported
CMake target.

Embedded hosts now link Gui plus one explicit backend:

```cmake
target_link_libraries(MyHost PRIVATE Aero::RenderD3D11)
```

Desktop applications continue to link:

```cmake
target_link_libraries(MyApp PRIVATE Aero::App)
```

Because `Aero::App` publicly links `Aero::Gui`, applications do not need to list
both targets.

## Build ownership

The build-only object components remain useful for compile ownership and
parallelism:

```text
Gui kernel + Controls + Markup + backend-neutral rendering + providers
    -> Aero::Gui

D3D11 implementation + factories -> Aero::RenderD3D11
OpenGL33 implementation + factories -> Aero::RenderOpenGL33

Application/Window model + desktop host + native window/input
    -> Aero::App
```

They are not exported as SDK products.

## S4 header ownership

The public-header migration is complete. Installed headers no longer use
`Aero/Integration.hpp` or `Aero/Integration/...`. Contracts are physically
owned by View, Input, Platform, Markup, Media, Text and Render paths.

Source files may temporarily include the retired paths through non-installed
forwarders under `src/compat/include`; those forwarders are build scaffolding,
not an SDK compatibility promise or a second product layer.

The next stage may migrate the remaining `Aero::Integration` C++ namespace and
then delete the private forwarders.
