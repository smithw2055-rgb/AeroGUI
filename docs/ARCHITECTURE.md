# AeroGUI-R Architecture

This is the normative description of the current repository structure. ADRs
and documents under `docs/spec` preserve rationale and historical detail; when
they disagree with this file, this file describes the current implementation.

## Product boundary

The installed products are `Aero::Base`, `Aero::Gui`, `Aero::Meta`,
`Aero::App`, and optional `Aero::Audio`. `Aero::Gui` is the embeddable runtime;
`Aero::App` adds the default desktop lifetime/window policy. Repository object
targets are build components only and are never exported as SDK products.

`cmake/AeroPublicHeaders.cmake` is the complete installation whitelist. Public
headers contain no `Aero::Internal`, source-private type, native host adapter,
or render-backend state. `View`, public `Renderer`, and `RenderDevice` are final
opaque objects whose implementation state lives under `src`.

The event declaration paths are exactly:

- `<Aero/Events.hpp>` for the event umbrella;
- `<Aero/Events/RoutedEvent.hpp>` for routed-event declarations.

`<Aero/RoutedEvent.hpp>` and `<Aero/Events/Events.hpp>` are retired and are not
forwarded or installed.

## Source ownership

Private implementation namespaces use `Aero::<Domain>::Detail`. The public
template-support namespaces `Aero::Base::Detail` and `Aero::Meta::Detail` are
the only installed Detail surfaces. Class-local `Impl` names are opaque state,
not a namespace convention.

Cross-translation-unit private declarations enter through the domain headers
`GuiPrivate.hpp`, `ControlsPrivate.hpp`, `MarkupPrivate.hpp`, and domain-private
render/platform headers under `src`. Single-translation-unit helpers stay in
anonymous namespaces; there is no Integration private aggregate.

Every `.cpp` has one compile owner. In particular, `MarkupSchema.cpp` and
`MarkupLoader.cpp` own the reusable Markup implementation, while
`GuiSchema.cpp` and `XamlReader.cpp` own product-composition seams. No source is
recompiled under a macro-selected identity.

## Object and metadata model

`DependencyObject` owns effective values and exposes checked mutation
companions. `Freezable : DependencyObject` owns instance-level freeze state,
revision, Changed subscriptions, and weak consumer records. Freeze checks the
entire object graph before a child-first commit, rejects expressions,
animations and cycles, and leaves a rejected graph unchanged.

Freeze is deliberately not a Meta facet. Meta and Markup facets are sealed
type-level programs indexed by `TypeId`; Freezable is mutable per-object state
with graph traversal and dependency-property write constraints.

The first shareable media families are Brush, Transform, Effect, Geometry,
GradientStop/GradientStopCollection, and Timeline. Style, Schema, Registry and
their existing Seal/Freeze operations remain separate type/program lifetimes.

## Runtime and rendering

`Gui` receives default XAML, texture, and font providers before `Initialize`.
`ViewOptions` may override them per View; routes are copied at View creation,
while provider object lifetime remains host-owned.

The frame path is:

```text
retained UI -> immutable RenderFrame -> RenderDevice -> Render::Renderer -> backend
```

`View::Viewport` carries logical size, exact pixel size, and DPI. Offscreen
targets use pixel dimensions. Embedded final targets load host content by
default; the desktop App explicitly requests a clear. Final composition is a
top-left 1:1 copy clipped to the target intersection.

Ordinary frame failures increment failure statistics without poisoning a ready
device. Only explicit surface loss, device loss, or a fatal backend health
state changes the device lifecycle state.

Gradient brushes upload a ramp texture cached by Freezable revision. ImageBrush
honors Viewbox, Viewport, Stretch, TileMode, Alignment, RelativeTransform and
MappingMode in the dedicated mask path. Effects allocate local bounded surfaces;
blur and drop shadow use separable horizontal/vertical passes. Nested groups keep
the fixed order `child content -> effect -> opacity mask -> parent composite`
without introducing a RenderGraph layer.

## Verification

`cmake/CheckArchitecture.cmake` enforces the installed whitelist, namespace and
private-type boundary, product dependency direction, retired paths, unique
source ownership, and installation model. Formatting and general naming style
belong to separate lint, not the architecture gate.

`AERO_BUILD_CONFORMANCE=ON` builds the non-installed `aero-conformance`
executable. It is independent of repository `tests` and `samples` and covers
the stable runtime contracts used by SDK consumers. On Windows it also compares
D3D11 WARP and OpenGL 3.3 pixel readback with a defined tolerance.
