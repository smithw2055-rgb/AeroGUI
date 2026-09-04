# Runtime Access Ownership Model

## Purpose

WPF/XAML authoring headers expose controls, dependency properties, routed
commands, styles, templates, drawing operations, and value types. Runtime
coordination is owned by view composition and must not become a second public
framework of managers, services, or access objects.

## Current kernel

Installed headers keep the WPF surface. Kernel-private operations live in
`src/gui/internal/` and are reached through one friend,
`friend class ::Aero::AeroGuiInternal`.

`View` / `ElementTree` is the service hub: named pointers to layout, bindings,
styles, events, input, animations, visual states, templates, text layout,
control behaviors, mesh resources, and name scope. Visual/UIElement reach that
hub through `GetTree()`.

Hot layout/visual fields stay on the object. Cold data uses a lazy rare
pointer. The dependency-property store is an opaque hashmap handle on
`DependencyObject`.

The ECS-style `Core::Facet` / `GetFacet` / `ElementFacet` bags and the older
`Access` / `GuiPrivate` facades are deleted. They are not the product
architecture.

XAML metadata type-capability tables (`XamlFacets`) are a different system
used by the markup runtime. They are not element or engine facets.

## Historical note

Earlier drafts described `GuiPrivate.hpp`, per-domain `Access` types, and a
typed `ElementHost` facet matrix. Those documents are not the current
contract; see `docs/SOURCE_ARCHITECTURE.md`.
