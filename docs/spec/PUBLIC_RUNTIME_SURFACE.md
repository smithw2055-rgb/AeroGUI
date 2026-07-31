# Public Runtime Surface

## Product rule

Public Aero headers describe WPF/XAML authoring semantics. Runtime composition
services that coordinate queues, input, binding, styles, templates, or control
interaction are implementation details. They live under `src/` and are consumed
by Runtime, Markup, and Controls internally.

## Private manager groups

UI runtime managers include routed-event dispatch, commands, hit
testing and input routing, layout, binding, animation, style, and theme-style
coordination. Controls runtime managers include template application and concrete
interaction services for buttons, scrolling, selection, menus, trees, and text
editing.

The complete declarations are grouped in private source headers:

- `src/ui/RuntimeManagers.hpp`
- `src/controls/RuntimeManagers.hpp`

Public classes retain only forward declarations where private friendship or an
opaque pointer is required. Moving a manager declaration must not change a
control TypeId, dependency property, routed event, metadata identity, or object
layout.

## Deliberate exceptions

`VisualStateManager` remains public because it maps to WPF authoring and runtime
semantics. `FontManager` remains public until the text-provider SDK boundary is
reviewed.

The existing text-inline classes and the button-derived `Hyperlink` are unchanged
in this stage. The WPF `Documents` model is intentionally deferred.

## Invariants

- `<Aero/Gui.hpp>` does not expose constructible runtime coordination services.
- Runtime implementation files include private manager headers explicitly.
- Installed headers and build-tree headers expose the same authoring surface.
- Product targets, metadata ownership, TypeIds, and observable behavior remain
  unchanged.
