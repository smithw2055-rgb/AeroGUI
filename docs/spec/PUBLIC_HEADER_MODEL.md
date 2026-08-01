# Public Header Model

## Status

Accepted for the `0.3` SDK convergence line.

This specification defines the physical C++ header boundary. WPF determines
public type names and observable behavior; it does not require AeroGUI to copy
.NET assembly boundaries or create one C++ header per type.

## Product entry points

The supported product umbrellas are:

- `<Aero/Gui.hpp>` — retained WPF/XAML authoring surface;
- `<Aero/App.hpp>` — optional default desktop lifetime;
- `<Aero/Integration.hpp>` — embedding and renderer endpoint integration;
- `<Aero/Meta.hpp>` — typed metadata and custom-module authoring.

`Markup.hpp`, concrete Integration backend headers and Text provider headers
are explicit specialist surfaces. They are not transitively
included by ordinary WPF-style application code.

## Canonical declaration ownership

Each public type has one declaration owner. The SDK does not retain a parallel
set of old-path forwarding headers or namespace-projection headers.

The root WPF spine is owned by:

```text
Aero/DependencyObject.hpp
Aero/Visual.hpp
Aero/UIElement.hpp
Aero/FrameworkElement.hpp
Aero/Application.hpp
Aero/Window.hpp
```

The corresponding class declaration is physically present in that header. For
example, `UIElement.hpp` does not forward to `Layout.hpp`, and
`FrameworkElement.hpp` does not forward to a rendering implementation header.

Domain values are grouped by stable authoring concepts rather than by internal
subsystems:

```text
Aero/Layout.hpp
Aero/Resources.hpp
Aero/Style.hpp
Aero/Styling.hpp
Aero/Data.hpp
Aero/Input.hpp
Aero/Media.hpp
Aero/Animation.hpp
Aero/Documents.hpp
Aero/Shapes.hpp
```

`Style.hpp` and `Styling.hpp` are deliberately separate real declaration
owners. The first contains Style, Setter and Trigger authoring; the second
contains FrameworkTemplate, ControlTemplate, DataTemplate and visual-state
authoring. Combining them would recreate a Control/Style/Template include
cycle.

`Input/Values.hpp` is also a deliberate dependency partition: the value types
are needed by `Visual` before the command and routed-input object model can
include `UIElement`.

## Controls family headers

The complete public Controls directory contains exactly six family headers:

```text
Aero/Controls/Base.hpp
Aero/Controls/Panels.hpp
Aero/Controls/Primitives.hpp
Aero/Controls/Items.hpp
Aero/Controls/Standard.hpp
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
- native Win32/X11 window implementations;
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

- no installed `Aero/Core`, `Aero/Platform` or `Aero/Detail` tree;

- no public `Aero/Detail` headers;
- exactly six Controls family headers;
- a bounded top-level header count;
- no duplicate direct includes in public headers;
- no retired forwarding/compatibility paths;
- no manager, mount, display-list or typed runtime attachment leakage through
  WPF authoring headers;
- exact equality between the source public tree and the installation whitelist.

Adding a public header is therefore an API decision: it must update the
whitelist, fit an existing product/domain model, and pass the public-header
consumer build.
