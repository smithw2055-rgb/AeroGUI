# AeroGUI source architecture

This document defines ownership inside `src/`. It is an implementation guide,
not an additional SDK layer. Supported products remain `Aero::Gui`,
`Aero::Meta`, `Aero::Integration`, `Aero::App`, `Aero::Base`, and the optional
`Aero::Audio` module.

## Design rules

1. Public types follow WPF naming and semantics. Private implementation names do
   not create a second authoring model.
2. A directory owns a product responsibility, not an abstract interface tier.
3. A private header must describe a shared domain contract. Single-use helpers
   stay in the owning `.cpp` file.
4. `Access`, `Manager`, `Provider`, and `Runtime` are not default layering
   patterns. Add one only when it represents shared state or a real lifetime.
5. Logical and visual relationships are framework semantics. There is no public
   or private `ObjectTree` product abstraction.
6. UI objects never cross the immutable `RenderFrame` boundary.
7. Public DependencyProperty and RoutedEvent static declarations remain on one
   physical line.

## Directory ownership

```text
src/
├─ base/          allocation, strings, object ownership and C ABI
├─ gui/           flat WPF semantic kernel
├─ controls/      standard controls, templates and default behavior
├─ markup/        XAML schema, object writer, compiled XAML and document cache
├─ text/          shaping, glyph atlas and text-provider adapters
├─ media/         brushes, images, transforms, effects and animation objects
├─ runtime/       View composition, frame lifecycle and built-in modules
├─ render/        RenderTree, RenderFrame, Renderer, RenderDevice and GPU backends
├─ platform/      OS-private windows, clipboard, IME and context adapters
├─ integration/   host factories and RenderEndpoint implementations
├─ app/           default Application/Window desktop lifetime
├─ diagnostics/   inspector and diagnostics implementation
└─ audio/         optional audio module
```

`src/gui` is intentionally flat. Its implementation files are grouped by file
name rather than by one-directory-per-concept. Shared private contracts are
limited to:

```text
AnimationInternal.hpp
BindingInternal.hpp
ElementInternal.hpp
InputInternal.hpp
LayoutInternal.hpp
MetadataInternal.hpp
PropertyInternal.hpp
RoutedEventInternal.hpp
StyleInternal.hpp
```

The larger metadata stores remain separate only because they have independent
storage and freeze lifetimes.

## GUI context and element relationships

`GuiContext` is a private per-View context. It owns Dispatcher-affine lifecycle,
property inheritance integration, event/input/layout coordination and stable
runtime identity. It is not a public tree API and it does not replace WPF's
logical-tree and visual-tree semantics.

Public traversal remains:

```text
VisualTreeHelper  -> visual parent and visual children
LogicalTreeHelper -> logical parent and logical children
```

`ContentElement` and `FrameworkContentElement` participate in logical content
and routed events without becoming Visual objects. `TextElement` and `Inline`
therefore remain nonvisual; `TextBlock` is their layout and rendering host.

The retired `ObjectTree`, `MountService`, and `VisualTreeMount` layers must not
be recreated. Root and child attachment are coordinated directly by
`GuiContext` while controls remain the owners of Content, Items and Children.
See `TREE_MODEL.md` for the detailed contract.

## Runtime composition

```text
Aero::Detail::ViewState::Impl
├─ schema and document cache
├─ GUI services owned directly by the View
│  ├─ metadata and dependency properties
│  ├─ GuiContext
│  ├─ routed events and input
│  ├─ layout and binding
│  ├─ style, template and animation
│  └─ text and image resources
├─ control behavior service
├─ RenderTree
└─ RenderEndpoint
```

`Aero::Detail::ViewState` does not expose a service-locator surface. Repository-owned
inspection and reload code uses the narrow `ViewAccess` bridge implemented next
to `Aero::Detail::ViewState::Impl`.

Platform-neutral clipboard and text-input contracts are declared in
`Aero/Integration/PlatformServices.hpp`. The default App owns concrete adapters
from `src/platform/win32` or another OS directory. Controls and View state consume
only the interfaces; native message types and window procedures never cross the
installed SDK boundary.
There is no standalone `AeroPlatform` library. The reusable in-memory clipboard
implementation belongs to Integration, while the default OS window, clipboard
and IME adapters are compiled directly into App. Platform directories organize
source ownership; they are not an additional link-time product layer.

## Event and command routing

There is one route implementation in `RoutedEventInternal.hpp` and
`GuiContext.cpp`.

```text
input, command or content source
→ resolve one event parent chain
→ create one stable route snapshot
→ preview/tunnel traversal
→ bubble traversal
→ class handlers
→ instance handlers
```

Route nodes are `DependencyObject` instances, so both `UIElement` and
`ContentElement` participate. Commands visit the same route through
`EventRouter`; command code must not walk visual or logical parents directly.

Loaded `UIElement` objects carry one private View-services attachment. It
provides the canonical `EventRouter` and `InputService`; separate event-router
and command-router pointers are not stored on each element. Layout ownership
and routed-handler storage remain explicit element state.

## Templates and controls

Template implementation is split by real responsibility:

- `TemplateProgram.hpp`: immutable compiled instructions and deferred factories;
- `TemplateInstance.hpp`: mounted parts, projections and rollback state;
- `TemplateAccess.hpp`: narrow cross-translation-unit bridge;
- `VisualStateRuntime.hpp`: current state and transition bookkeeping.

Do not recreate a catch-all `TemplateRuntime.hpp` or per-control Access headers.
Default control behavior is coordinated by one private
`ControlBehaviorService` and routed class handlers.

## Render and graphics path

```text
retained UI
→ RenderTree::Commit() creates RenderFrame
→ RenderEndpoint::Submit(RenderFrame)
→ Renderer records the private RenderDevice command stream
→ the selected D3D11/OpenGL/hosted device submits and presents
```

Render resources use explicit image, mesh and glyph contracts. Magic service
identifiers and `QueryInternalService()` lookup are forbidden. `RenderEndpoint`
submits synchronously on the caller-selected thread; the core library owns no
render worker, pending-frame queue or coalescing policy.

## CMake ownership

- `AeroGuiTargets.cmake` owns `AeroGuiKernel`, controls, markup, `UiDocument` and schema;
- `AeroRuntimeTargets.cmake` owns View/runtime composition;
- `AeroRenderingTargets.cmake` owns the retained renderer, render device,
  native GPU backends and their private surface/context adapters;
- `AeroProductTargets.cmake` owns Integration, App and their platform implementations;
- `AeroToolsTargets.cmake` owns `aero-schema-gen` and `aero-xamlc`;
- `AeroInstall.cmake` owns package exports and public headers.

`AeroGuiKernel` and `AeroRendering` are internal binary identities. Product
consumers link `Aero::Gui` or `Aero::Integration`, not the underscore-prefixed
static support targets.

## Architecture gates

`cmake/CheckArchitecture.cmake` verifies that:

- the physical public tree equals the install whitelist;
- public `Core`, `Platform`, and `Detail` directories are not recreated;
- `src/gui` stays flat and within explicit file budgets;
- ObjectTree/MountService/VisualTreeMount and the old runtime forward layer stay
  removed;
- `Aero::Detail::ViewState` does not expose internal service accessors;
- commands and content use the single routed-event route;
- RenderTree does not own endpoint submission;
- DependencyProperty and RoutedEvent declarations remain single-line.
