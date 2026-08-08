# AeroGUI source ownership

This document maps implementation responsibilities inside `src`. It does not
define another SDK layer; the installed contract is under `include/Aero`.

## Directory owners

```text
src/base/       allocation, strings, object lifetime, streams and C ABI
src/gui/        WPF semantic kernel, Gui/View composition and ViewRenderer
src/controls/   controls, templates, item generation and default behavior
src/markup/     XAML schema, parser, writer, compiled documents and cache
src/input/      platform-neutral input services
src/text/       shaping, glyph atlas, editing and font adapters
src/media/      brushes, images, transforms, effects and animation
src/render/     immutable-frame encoding, GPU resources and native backends
src/app/        Application, Window, DesktopHost and desktop presentation
src/diagnostics opt-in inspection and rendering diagnostics
src/audio/      optional audio product
```

App-owned XAML behavior is supplied to the Gui schema through copied module
descriptors (`Markup::ResourceScopeRegistration`). This keeps callbacks close
to the owning product while preserving the one-way `AeroApp -> AeroGui`
binary dependency.

The retired `src/integration`, `src/runtime`, `src/providers`, root
`src/platform`, and domain `private`/`detail` directories must not return.
Files use responsibility names; `*Internal*` and `*Private*` filenames are
forbidden. Helpers needed by one translation unit stay in an anonymous
namespace.

## View composition

`ViewState` owns the view-affine object factory, effective values, animation,
element tree, layout, render tree, image cache, text pipeline, binding, events,
input, control behavior, templates, styles, and the one `ViewRenderer`.

The public `View` surface stays small and WPF/Noesis shaped. `Gui` and
`XamlReader` are the only trusted construction/loading peers. `DesktopHost`
uses public `View` methods and does not operate on `ViewState`.

No source-only object uses a heap-allocated `Impl`/`Access` Pimpl. Where a
large implementation type must remain out of a source header, the owner keeps
fixed aligned storage and constructs the data state in place. There is no
second ownership or forwarding object.

## Rendering ownership

```text
View
  -> ViewRenderer : IRenderer
       -> immutable RenderFrame
       -> UiFrameEncoder
       -> direct buffer/texture uploads
       -> one RenderBatch per UI draw
       -> shared RenderDevice
```

Only `ViewRenderer` is a concrete renderer. `RenderDevice` has no renderer
pointer or token. `UiFrameEncoder` is a helper directly owned by
`ViewRenderer`, not a peer renderer or command-list product.

D3D11 and OpenGL device implementations execute `RenderBatch` directly and
own their native resource/pipeline caches. A batch is one draw, not a list of
begin-pass, bind, upload, draw, and end-pass commands. Native pipeline values
are selected from the UI shader/render-state key inside the backend.

`RenderTarget` identifies a drawable embedded or desktop target and delegates
only target acquisition, resize, loss, restore, and drawing. Desktop frame
state and presentation belong exclusively to `src/app/RenderContext.*` and
the concrete App D3D11/OpenGL contexts.

## Markup ownership

`Schema`, `GuiSchema`, `Loader`, `DocumentCache`, `DependencyGraph`, UI object
model facets, and template facets are ordinary source-only implementations.
They use direct or in-place state and do not publish implementation classes in
the SDK. `XamlDocument` is the ABI-heavy public value that deliberately keeps
an opaque state pointer.

## CMake ownership

Gui, Controls, Markup, View composition, and rendering sources compile directly
into `AeroGui`; App sources compile into `AeroApp`. Object targets exist only
for build reuse and header-consumer checks and are never installed products.
Every `.cpp` has one compile owner.

The permanent gate is `cmake/CheckArchitecture.cmake`. It checks final
invariants rather than migration stage names.
