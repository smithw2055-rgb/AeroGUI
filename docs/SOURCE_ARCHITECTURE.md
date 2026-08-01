# AeroGUI source architecture

This document defines ownership inside `src/`. It is an implementation guide,
not an additional public SDK layer. Public products remain `Aero::Gui`,
`Aero::Meta`, `Aero::Integration`, `Aero::App`, `Aero::Base`, and the optional
`Aero::Audio` module.

## Design rules

1. A source directory owns a product responsibility, not an interface tier.
2. A translation unit belongs to one top-level source domain.
3. Internal headers exist only when two or more translation units share a
   contract; single-use helpers stay in the owning `.cpp` file.
4. `Access`, `Manager`, `Provider`, and `Runtime` are not automatic layering
   patterns. Add one only when it represents a real lifetime or shared state.
5. UI objects do not cross the immutable `RenderFrame` boundary.
6. Platform code owns native windows, contexts, and surfaces. Graphics code owns
   GPU resources and command submission. Render code owns conversion from the UI
   frame to graphics commands.
7. Public DependencyProperty and RoutedEvent static declarations remain on one
   physical line.

## Directory ownership

```text
src/
├─ base/          allocation, strings, object ownership and C ABI
├─ gui/           WPF semantic kernel
│  ├─ metadata/   type/member metadata and registration storage
│  ├─ property/   dependency properties and effective values
│  ├─ tree/       logical/visual tree and mount transactions
│  ├─ events/     routed-event route construction and dispatch
│  ├─ input/      input state, focus, capture and commands
│  ├─ layout/     measure/arrange queues and layout execution
│  ├─ binding/    binding expressions and scheduling
│  ├─ resources/  resource dictionaries and lookup
│  ├─ styling/    style compilation and application
│  ├─ animation/  animation scheduling against dependency properties
│  ├─ threading/  dispatcher implementation
│  └─ registration/ built-in GUI metadata registration
├─ controls/      standard controls, templates and control behavior
├─ markup/        XAML schema, object writer, compiled XAML and document cache
├─ text/          shaping, glyph atlas and text-provider adapters
├─ media/         brushes, images, transforms, effects and animation objects
├─ runtime/       View composition, frame lifecycle and built-in modules
├─ render/        RenderTree, immutable RenderFrame and generic Renderer
├─ graphics/      GraphicsDevice, command list and API backends
├─ platform/      native window, clipboard, IME and native surfaces
├─ integration/   public host factories and RenderEndpoint implementations
├─ app/           default Application/Window desktop lifetime
├─ diagnostics/   inspector and diagnostics implementation
└─ audio/         optional audio module
```

The root of `src/` and the root of `src/gui/` intentionally contain no source
files. New files must be placed in the domain that owns their behavior.

## Runtime composition

`ViewRuntime::Impl` is the composition root for a view. Presentation services
are inherited from one private aggregate rather than exposed as a service locator:

```text
ViewRuntime::Impl
├─ schema and document cache
├─ PresentationRuntime
│  ├─ dependency-property engine
│  ├─ element tree
│  ├─ routed-event router
│  ├─ input service
│  ├─ layout queue
│  ├─ binding service
│  ├─ style service
│  └─ animation service
├─ control behavior service
├─ RenderTree
└─ RenderEndpoint
```

`InputService` is the only view-facing entry for pointer, keyboard, text, focus,
capture and routed commands. Its state classes are private implementation units;
callers must not reach through it to obtain manager objects.

Control-specific default behavior is coordinated by one private
`ControlBehaviorService`. Controls and XAML authoring APIs do not expose its
state or lifetime.

## Event and command routing

There is one route implementation under `src/gui/events`.

```text
input or command source
→ EventRouter builds one route
→ preview/tunnel traversal
→ bubble traversal
→ class handlers
→ instance handlers
```

Commands visit the same route through `EventRouter`; command code must not walk
visual or logical parents directly. `Handled` and `handledEventsToo` are applied
only by the event router.

## Template ownership

Template implementation is split by responsibility:

- `TemplateProgram.hpp`: immutable compiled instructions and deferred factories;
- `TemplateInstance.hpp`: mounted parts, projections and rollback state;
- `TemplateAccess.hpp`: the narrow cross-translation-unit private bridge;
- `VisualStateRuntime.hpp`: current-state and transition bookkeeping.

Do not recreate a catch-all `TemplateRuntime.hpp` or per-type Access headers.

## Render and graphics path

```text
retained UI tree
→ RenderTree::Commit() creates an immutable RenderFrame
→ RenderEndpoint::Submit(RenderFrame)
→ generic Renderer records a Graphics::CommandList
→ GraphicsDevice submits to GraphicsBackend
→ native RenderSurface presents
```

`RenderTree` does not own an endpoint or backend. `RenderEndpoint` owns queueing,
device/surface loss and presentation policy. Backend-specific endpoint adapters provide GPU functions, shader packages and a
native surface while reusing the common `Renderer` for frame recording.

Render resources are passed through explicit image, mesh and glyph registries.
Magic service identifiers and `QueryInternalService()` style lookup are
forbidden.

## CMake ownership

- `AeroGuiTargets.cmake` owns the GUI kernel, controls, markup and module catalog.
- `AeroRuntimeTargets.cmake` owns View/runtime composition.
- `AeroGraphicsTargets.cmake` owns render, graphics and native backend targets.
- `AeroProductTargets.cmake` owns Integration and App products.
- `AeroToolsTargets.cmake` owns `aero-schema-gen` and `aero-xamlc`.
- `AeroInstall.cmake` owns package exports and the explicit public-header list.

The root `CMakeLists.txt` contains global options, third-party setup, generated
assets and module includes. It must not regain target source lists from the
module files.

## Architecture gates

`cmake/CheckArchitecture.cmake` enforces the following invariants:

- the physical public header tree equals the install whitelist;
- no public `Core`, `Platform` or `Detail` directory is recreated;
- no source file lives directly in `src/` or `src/gui/`;
- retired catch-all runtime headers and old input-manager names stay removed;
- command routing does not construct its own event route;
- RenderTree does not submit to endpoints;
- render service-locator IDs stay removed;
- private Access headers and public headers remain within explicit budgets;
- DependencyProperty and RoutedEvent static declarations are single-line.
