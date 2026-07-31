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

The complete manager declarations remain in private source headers:

- `src/ui/RuntimeManagers.hpp`
- `src/controls/RuntimeManagers.hpp`

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

The text editor still exposes its current platform-neutral composition seam.
Moving the remaining text-provider policy types behind the Integration boundary
is follow-on work and is not required to preserve the Stage C ABI cleanup.

## Invariants

- `<Aero/Gui.hpp>` does not expose constructible runtime coordination services.
- Custom controls render through `FrameworkElement::OnRender(DrawingContext&)`.
- Public commands and bindings contain authoring semantics, not runtime managers
  or scheduler descriptors.
- Public headers do not expose display-list commands or render resource IDs.
- Runtime implementation files include private manager and render headers
  explicitly.
- Installed headers and build-tree headers expose the same authoring surface.
- Product targets, metadata ownership, TypeIds, and observable behavior remain
  unchanged by private storage consolidation.
