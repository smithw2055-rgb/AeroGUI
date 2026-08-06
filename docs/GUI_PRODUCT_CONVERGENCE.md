# S3 Gui product convergence

## Decision

`Aero::Gui` is the single embeddable AeroGUI product target.

It owns:

- the WPF/XAML object model;
- controls, markup and metadata execution;
- `Gui`, `View` and `IRenderer`;
- XAML, texture and font provider contracts;
- RenderDevice and RenderSurface implementations;
- D3D11 and OpenGL backend factories;
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

Embedded hosts now link:

```cmake
target_link_libraries(MyHost PRIVATE Aero::Gui)
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
Gui kernel + Controls + Markup + Runtime + Rendering + providers
    -> Aero::Gui

Application/Window model + desktop host + native window/input
    -> Aero::App
```

They are not exported as SDK products.

## Follow-up

The next stage may move the remaining public headers from
`Aero/Integration/...` into their semantic products:

- View options into Gui;
- XAML providers into Markup;
- font providers into Text;
- texture providers into Media;
- backend factories into Render;
- native window contracts into App.

That physical migration must preserve the product boundary established here:
it must not recreate an Integration library, facade target or service layer.
