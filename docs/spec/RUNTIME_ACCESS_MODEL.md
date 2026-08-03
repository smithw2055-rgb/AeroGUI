# Runtime Access Ownership Model

## Purpose

WPF/XAML authoring headers expose controls, dependency properties, routed
commands, styles, templates, drawing operations, and value types. Runtime
coordination is owned by view composition and must not become a second public
framework of managers, services, or access objects.

## Ownership

The current implementation keeps complete UI and control runtime declarations
behind the source-only domain aggregates:

- `src/gui/GuiPrivate.hpp`
- `src/controls/ControlsPrivate.hpp`

GUI implementation seams use `Aero::GuiPrivate::Detail`; controls use
`Aero::Controls::Detail`. Public objects that require an attachment retain
opaque state and grant friendship to one access owner per domain rather than
naming a concrete manager in their public declaration.

Private aliases may keep implementation call sites readable, but they do not
define product namespaces and are not authoring APIs. New behavior should first
be implemented as ordinary private functions or state owned by the existing
view runtime. A new manager is justified only when it has an independently
useful lifecycle, replacement boundary, or scheduling responsibility.

## Stage C simplification

Stage C removes typed interaction-manager fields from common controls and
internalizes display-list, theme-style registry, UI metadata bootstrap, and
Gallery compatibility types. XAML Facets are stored in one compact per-type
record instead of parallel manager-like stores.

This stage deliberately does not claim that every historical manager has been
merged. Remaining manager classes are private implementation units and may be
consolidated incrementally without changing the WPF-facing SDK.

## Deliberate exceptions

`RenderManager` remains in the private render boundary.
`VisualStateManager` remains public because it maps to WPF semantics.
`FontManager` remains public pending the text-provider SDK review.
