# Public Runtime Surface

## Product rule

Public Aero headers describe WPF/XAML authoring semantics. Runtime composition
services that coordinate queues, input, binding, styles, templates, control
interaction, display lists, or GPU resources are implementation details. They
live under `src/` and are consumed by Runtime, Markup, Controls, and Integration
internally.

## Private runtime groups

UI runtime services include routed-event dispatch, commands, hit testing and
input routing, layout, binding, animation, style, and theme-style coordination.
Controls runtime services include template application and concrete interaction
state for buttons, scrolling, selection, menus, trees, and text editing.

The remaining coordination declarations live in private source headers, chiefly
`src/gui/GuiPrivate.hpp`, `src/controls/ControlsPrivate.hpp`, and
`src/controls/ControlBehavior.hpp`. They are implementation aggregation points,
not product modules, and may be merged further without changing the SDK.

Public classes retain only opaque attachment state where the current object
layout requires a runtime connection. Public `ICommand`, Binding, and custom
drawing APIs do not accept manager, scheduler, provider-session, display-list,
or render-resource implementation types. Type-keyed theme-style lookup and the
Gallery compatibility metadata types are also private implementation details.

Custom controls render through `FrameworkElement::OnRender(DrawingContext&)`.
`DrawingContext` is the authoring abstraction; immutable display-list commands,
resource identifiers, batching, and backend submission remain below the public
SDK boundary.

## Deliberate exceptions

`VisualStateManager` remains public because it maps to WPF authoring and runtime
semantics. `FontManager` remains public until the text-provider SDK boundary is
reviewed.

The current text-provider and platform-neutral composition contracts remain
specialist opt-in SDK surfaces. Native Win32/X11 window implementations and
per-view runtime wiring are private source headers.

## Invariants

- `<Aero/Gui.hpp>` does not expose constructible runtime coordination services.
- Custom controls render through `FrameworkElement::OnRender(DrawingContext&)`.
- Public commands and bindings contain authoring semantics, not runtime managers
  or scheduler descriptors.
- Public headers do not expose display-list commands or render resource IDs.
- Runtime implementation files include private manager and render headers
  explicitly.
- Installed headers and the physical build-tree public headers are the exact
  same whitelist.
- The installed tree contains no `Aero/Detail` directory.
- Product targets, metadata ownership, TypeIds, and observable behavior remain
  unchanged by private storage consolidation.
