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
`Aero::Integration` is retired. Backend factories live under `Aero::Render`,
providers live with their owning domains, and default desktop hosting belongs to
`Aero::App`.

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
├─ gui/           flat WPF semantic kernel
├─ controls/      standard controls, templates and default behavior
├─ markup/        XAML schema, object writer, compiled XAML and document cache
├─ providers/     source-provider adapters pending final domain relocation
├─ text/          shaping, glyph atlas and text adapters
├─ media/         brushes, images, transforms, effects and animation objects
├─ runtime/       View composition pending final relocation into Gui/Media/Text
├─ render/        RenderTree, RenderFrame, RenderDevice and native GPU backends
├─ platform/      OS-private window/input adapters used by App
├─ app/           Application, Window, DesktopHost and private RenderContext
├─ diagnostics/   inspector and diagnostics implementation
└─ audio/         optional audio module
```

`src/integration` no longer exists. The remaining `runtime`, `providers`, and
`platform` directories describe physical ownership still scheduled for the SDK
source-layout closure; they are not installed products.

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
→ RenderDevice owns shared GPU resources
→ RenderTarget identifies the current onscreen/embedded target
```

`RenderSurface` is not an installed SDK type.

Backend-neutral native acquire/present mechanics remain source-private under
`src/render` (`Graphics::SurfaceSession` and backend adapters). They are allowed
to use the word *surface* because it describes the native presentation
mechanism, not a second public product object.

For the default desktop product:

```text
DesktopHost
  ├─ NativeWindow
  ├─ View
  └─ App::Detail::RenderContext
       ├─ backend/window target creation
       ├─ resize
       ├─ RenderDevice handoff
       ├─ IRenderer::Render(RenderTarget&)
       └─ idle/shutdown
```

This keeps swap-chain/context presentation policy out of View and prevents the
App host from duplicating RenderDevice/target lifecycles.

Embedded hosts create a RenderDevice and RenderTarget explicitly through the
D3D11/OpenGL factory headers and retain scheduling control.

## Markup

Schema, document cache and provider routing are Gui-owned. Object creation,
name-scope effects and mounted resource state remain View-affine where required.
`XamlReader` is still View-bound in the current source baseline; moving the
facade to Gui ownership and removing the remaining `ViewAccess` forwarding layer
is the next Gui/XAML convergence stage.

## CMake ownership

Installed targets are product targets only. Object libraries are internal build
components and may be merged, split or removed without changing architecture.
No architecture gate requires a particular internal Object Library name.

The permanent build contract is limited to dependency and SDK invariants:

- installed headers exist and do not include source-private contracts;
- `Aero::Integration` and `src/integration` cannot return;
- `RenderTarget` is the installed target object;
- `RenderTree` never owns GPU submission;
- desktop presentation is coordinated by App's private `RenderContext`;
- product targets remain `Aero::Base`, `Aero::Gui`, `Aero::App` and optional
  modules/facades;
- internal object components are never installed as SDK products.

Historical H/J/K/R/S migration markers belong in Git history and focused design
notes, not in `CheckArchitecture.cmake`.
