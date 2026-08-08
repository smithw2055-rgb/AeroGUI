# AeroGUI source architecture

This document defines current implementation ownership inside `src/`. It is an
implementation guide, not another SDK layer.

## Product boundary

The installed product surface is intentionally small:

```text
Aero::Base
   ↑
Aero::Gui        Aero::Audio (optional)
   ↑
Aero::App (optional desktop lifetime)
```

`Aero::Meta` is a Gui authoring facade, not a separate runtime product.
`Aero::Integration` is retired. Embedded backend factories live under
`Aero::Render`; default desktop hosting belongs to `Aero::App`.
`Aero::Render` is a specialist C++ namespace shipped by the single embeddable
`Aero::Gui` product, not another CMake/ABI product layer.

## Design rules

1. Public types follow WPF naming and semantics.
2. A directory owns a real product responsibility, not an abstract interface tier.
3. Private helpers do not create a second authoring model.
4. `Access`, `Manager`, `Provider`, `Runtime`, and `Context` are not default
   layering patterns; each must represent real shared state or lifetime.
5. Logical and visual relationships remain framework semantics; there is no
   `ObjectTree` product abstraction.
6. UI objects never cross the immutable `RenderFrame` boundary.
7. The host owns frame scheduling; AeroGUI owns no hidden render worker.
8. Historical migration constraints are not permanent architecture gates.

## Current source ownership

```text
src/
├─ base/          allocation, strings, object ownership and C ABI
├─ gui/           WPF semantic kernel plus Gui/View composition
├─ controls/      standard controls, templates and default behavior
├─ markup/        XAML schema, providers, object writer, compiled XAML and cache
├─ input/         platform-neutral input services
├─ text/          shaping, glyph atlas and text adapters
├─ media/         brushes, images, transforms, effects, animation and image cache
├─ render/        RenderTree, Renderer, RenderDevice, targets and GPU backends
├─ app/           Application, Window, DesktopHost and private RenderContext
│  └─ platform/   OS-private window, IME and clipboard adapters
├─ diagnostics/   inspector and opt-in runtime/render diagnostics
└─ audio/         optional audio module
```

`src/integration`, `src/runtime`, `src/providers`, and the former root
`src/platform` are retired. Runtime helpers live with their real owners.

## Gui and View

`Aero::Gui` owns process-level schema/module/provider state and creates Views.
Each `Aero::View` owns presentation-affine state such as layout, input, binding,
style/template execution and an immutable retained render tree.

Stable per-View engines may use one packed allocation as an implementation
optimization, but the allocation strategy is not part of the architecture
contract and is not enforced by source-string gates.

Public traversal remains WPF-shaped:

```text
VisualTreeHelper  -> visual parent and children
LogicalTreeHelper -> logical parent and children
```

`ContentElement` and `FrameworkContentElement` participate in logical content and
routed events without becoming Visual objects.

## Rendering

The canonical host-visible rendering path is:

```text
retained UI
→ RenderTree::Commit() creates immutable RenderFrame
→ IRenderer synchronizes one View
→ RenderDevice owns shared GPU lifetime
→ source-private Render::Renderer records GPU commands
→ RenderTarget identifies the current embedded/window target
```

`RenderSurface` is not an installed SDK type. Public D3D11/OpenGL headers expose
explicit `Create*Device()` plus `Create*RenderTarget(device, options)` for
embedded hosts. They do not expose native-window or presentation policy.

Backend target state derives directly from `RenderTarget::Impl`; the former
native-target wrapper is gone. Every RenderTarget owns exactly one target Impl;
D3D11 and OpenGL use the same separate Device/Target lifetime model. Window
acquire/present mechanics remain source-private in concrete swap-chain/context
adapters.

`Render::Renderer` is the one semantic backend renderer. `FrameEncoder.cpp`
implements the source-private `CommandEncoder`; it records command lists but has
no peer renderer lifetime. Its target value is `FrameTarget`, avoiding collision
with the installed `Aero::RenderTarget` object. `Graphics::Device` is only the
source-private resource/command core used by these backends; its backend and
shader vocabulary is limited to implementations compiled by this tree.

For OpenGL desktop hosting, WGL/GLX context creation remains physically coupled
to the device, but presentation state (lost flag and frame serial) belongs to the
`RenderTarget` implementation. The target is therefore no longer a lifecycle-free
proxy around the device.

Rendering statistics are opt-in through `<Aero/Diagnostics/Rendering.hpp>` and
are not methods on the normal RenderDevice authoring surface.

For the default desktop product:

```text
DesktopHost
  ├─ NativeWindow
  ├─ View
  └─ App::Detail::RenderContext
       ├─ source-private backend/window target creation
       ├─ resize
       ├─ RenderDevice handoff
       ├─ IRenderer::Render(RenderTarget&)
       └─ idle/shutdown
```

This keeps swap-chain/context presentation policy out of View and out of the
installed backend factory headers.

## Markup

Schema, document cache, provider routing and XAML object construction are Gui-owned.
`XamlReader` is constructed from `Gui&` and creates an unmounted document without
requiring a View. Binding, MultiBinding and DynamicResource effects bind their
View-affine runtime services only when the document is mounted. `ViewAccess` has
been removed; View owns content/mount, resource-layer, layout, input and render
state rather than acting as a loader gateway.

## CMake ownership

Installed targets are product targets only. Gui, Controls, Markup, View
composition and rendering implementation sources compile directly into
`AeroGui`. Only narrow components that are genuinely reused by offline tools
(text, module/schema and App metadata) remain Object Libraries; they are not SDK
targets. A separate `AeroRender` binary is intentionally not created because the
private renderer consumes the UI-generated immutable `RenderFrame`; adding a
thin product solely around that boundary would increase SDK layering without an
independent lifetime or ABI.

The permanent build contract is limited to final SDK/dependency invariants:

- installed headers exist and do not include source-private contracts;
- `Aero::Integration` and `src/integration` cannot return;
- `RenderTarget` is the installed target object and window surface policy stays private;
- `RenderTree` never owns GPU submission;
- `Render::Renderer` is the semantic backend renderer;
- desktop presentation is coordinated by App's private `RenderContext`;
- rendering statistics remain an opt-in Diagnostics API;
- product targets remain `Aero::Base`, `Aero::Gui`, `Aero::App` and optional
  modules/facades.

Historical H/J/K/R/S migration markers belong in Git history and focused design
notes, not in `CheckArchitecture.cmake` or `CheckConventions.cmake`.
