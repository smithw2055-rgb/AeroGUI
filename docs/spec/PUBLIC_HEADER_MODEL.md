# Public Header Model

## Status

Accepted for the `0.3` SDK convergence line.

This specification defines the physical C++ header boundary. WPF determines
public type names and observable behavior; it does not require AeroGUI to copy
.NET assembly boundaries or create one C++ header per type.

## Product entry points

The supported product umbrellas are:

- `<Aero/Gui.hpp>` — retained WPF/XAML authoring surface;
- `<Aero/App.hpp>` — `Application::Run()` and optional default desktop lifetime;
- `<Aero/Integration.hpp>` — embedding and renderer endpoint integration;
- `<Aero/Meta.hpp>` — typed metadata and custom-module authoring.

`Markup.hpp`, concrete Integration backend headers and provider contracts under
`Aero/Integration/Providers` are the SDK boundary. Text shaping providers
are explicit specialist surfaces. `Integration/Platform.hpp` contains only
platform-neutral clipboard and text-input contracts; native Win32/X11 adapters are
private. These specialist headers are not transitively included by ordinary
WPF-style application code.

## Canonical declaration ownership

Each public type has one declaration owner. The SDK does not retain a parallel
set of old-path forwarding headers or namespace-projection headers. The former
`Aero/App/Launcher.hpp` surface is removed; desktop hosting is private and
configured with the value-type `App::RunOptions` declared by `App.hpp`.

The root WPF spine is owned by:

```text
Aero/DependencyObject.hpp
Aero/Visual.hpp
Aero/UIElement.hpp
Aero/FrameworkElement.hpp
Aero/Application.hpp
Aero/Window.hpp
Aero/View.hpp
```

The corresponding class declaration is physically present in that header. For
example, `UIElement.hpp` does not forward to `Layout.hpp`, and
`FrameworkElement.hpp` does not forward to a rendering implementation header.
`View` is likewise owned by `<Aero/View.hpp>`; the former
`Integration/View.hpp` forwarding path and `ViewHost` facade are removed.
`Gui::CreateView(options)` is the single integration factory.

Domain values are grouped by stable authoring concepts rather than by internal
subsystems:

```text
Aero/Layout.hpp
Aero/Resources.hpp
Aero/Style.hpp
Aero/Styling.hpp
Aero/Data.hpp
Aero/Input.hpp
Aero/Animation.hpp
Aero/Documents.hpp
Aero/Shapes.hpp
```

`Style.hpp` and `Styling.hpp` are deliberately separate real declaration
owners. The first contains Style, Setter and Trigger authoring; the second
contains FrameworkTemplate, ControlTemplate, DataTemplate and visual-state
authoring. Combining them would recreate a Control/Style/Template include
cycle.

`Input.hpp` owns both the input value types and the command/navigation object
model. It forward-declares `UIElement` and owns the input value declarations
before including `RoutedEvent.hpp`, so the routed-event header does not need a
second input-values header or a cyclic include.

Media is a specialist surface made up of concrete headers such as
`Media/Brushes.hpp`, `Media/Geometry.hpp`, `Media/Images.hpp` and
`Media/Transforms.hpp`. WPF-visible text values are owned by
`Controls/Core.hpp` and consumed through `Controls/Text.hpp`; the text
provider, shaping and editing implementation remains private under `src/text`.
Generic `Media.hpp` and `Text/Text.hpp` aggregation headers are not part of
the installed SDK.

## Controls family headers

The complete public Controls directory contains exactly six family headers:

```text
Aero/Controls/Core.hpp
Aero/Controls/Panels.hpp
Aero/Controls/Primitives.hpp
Aero/Controls/Items.hpp
Aero/Controls/Common.hpp
Aero/Controls/Text.hpp
```

`Aero/Controls.hpp` is the only umbrella. New controls join an existing family
unless they establish a genuinely independent authoring domain. File growth by
itself is not a reason to create another public package.

## Private implementation placement

There is no installed `Aero/Detail` directory. The following concepts are
private and live under `src/`:

- object-tree and mount transactions;
- layout, input, binding, style and template runtime coordination;
- effective-value provider sessions;
- XAML facets and frozen runtime plans;
- display lists, render commands, GPU resource identifiers and backend state;
- native Win32/X11 window, clipboard and IME implementations;
- built-in metadata bootstrap and runtime safety checks.

A private header should normally be shared by at least two translation units.
A helper used by one translation unit belongs in that `.cpp` file. Private
`Access`, `Fwd`, `Internal`, `Session`, `Store` and `Manager` files are not
created merely to mirror conceptual layers.

## Installation boundary

`cmake/AeroPublicHeaders.cmake` is the explicit SDK whitelist. The physical
`include/Aero` tree must match it exactly. `install(DIRECTORY include/Aero ...)`
is prohibited because moving a private file under `include/` must never
silently publish it.

The architecture check enforces:

- no installed legacy metadata, platform, or detail tree;
- no public `Aero/Detail` headers;
- exactly six Controls family headers;
- the canonical namespace manifest, with no public `using namespace` directives;
- no public `Aero::Render` or product-layer `Detail` namespace;
- no duplicate direct includes in public headers;
- no retired forwarding/compatibility paths;
- no native Win32/X11 adapter types in the public platform-service contract;
- no manager, mount, display-list or typed runtime attachment leakage through
  WPF authoring headers;
- exact equality between the source public tree and the installation whitelist.

Property setters and WPF lifecycle hooks use direct values: public `SetXxx`,
`ClearXxx`, `Reset` and notification methods return `void`, `ApplyTemplate`
returns `bool`, and measure/arrange/render hooks use `Size`/`void`. `Result<T>`
is reserved for the canonical parsing/conversion `Try*` names, streams,
resources, registration and other boundaries where the caller must observe
failure. Dependency-property
validation is completed before commit so a rejected assignment leaves the
previous effective value unchanged.

Adding a public header is therefore an API decision: it must update the
whitelist and namespace manifest when needed, fit an existing product/domain
model, and pass the public-header consumer build. Header and source file size
are organized by responsibility and dependency; the architecture check does
not impose a line-count budget.
