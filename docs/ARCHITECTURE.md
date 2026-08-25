# AeroGUI-R architecture

This file is the normative description of the current tree. The migration path
is retained in Git history rather than as parallel milestone documents.

## Product and SDK boundary

The shipped CMake products are `Aero::Base`, backend-neutral `Aero::Gui`, the
backend-neutral `Aero::Render` contract target, `Aero::RenderD3D11`,
`Aero::RenderOpenGL33`, `Aero::App`, and optional `Aero::Audio`. `Aero::Render`
is an interface target over contracts implemented by `AeroGui`, not another
DLL. Embedded hosts link one backend; `Aero::App` composes the enabled desktop
defaults behind the window and application lifetime.

The installed SDK is split across `include/Aero`, `include/AeroRender`,
`include/AeroApp`, and `include/AeroAudio`; `src` is hidden implementation.
`cmake/AeroPublicHeaders.cmake` is the exact installation whitelist. Each
binary has its own export macro (`AERO_BASE_API`,
`AERO_AUDIO_API`, `AERO_GUI_API`, `AERO_RENDER_D3D11_API`,
`AERO_RENDER_OPENGL33_API`, or `AERO_APP_API`), automatic Windows symbol export
is disabled, and source files may not use an API macro.

Product-specific XAML capabilities cross this boundary as module data. In
particular, `AeroApp` contributes the `Application` resource scope through
`Markup::ResourceScopeRegistration`; `AeroGui` never includes or links back to
the App object model.

WPF-shaped UI headers follow their public semantics: the object-model spine is
owned directly under `include/Aero`, controls live under `include/Aero/Controls`,
bindings under `include/Aero/Data`, media under `include/Aero/Media`, and
advanced XAML types under `include/Aero/Markup`. There are no installed
`include/Aero/Gui/*` forwarding headers. Audio and App contracts enter through
`AeroAudio/Audio.hpp` and `AeroApp`; GUI input interop remains
`Aero/InputInterop.hpp`. Native-window handle declarations are part of
`AeroApp/WindowInterop.hpp`, while the host font provider lives beside the
other media contracts in `Aero/Media/FontProvider.hpp`.
The advanced rendering surface is split by responsibility:

```text
include/Aero/IRenderer.hpp
include/AeroRender/Render.hpp
include/AeroRender/RenderDevice.hpp
include/AeroRender/RenderTarget.hpp
include/AeroRender/D3D11.hpp
include/AeroRender/OpenGL33.hpp
```

The C++ type names remain `Aero::IRenderer`, `Aero::RenderDevice`, and
`Aero::RenderTarget`; the backend factories live in separate linker products.

Public headers contain no `Impl` declaration, internal conformance macro, or
source include. Ordinary embedding loads object trees through
`Gui::LoadXaml<T>()` and `Gui::LoadComponent()`. `XamlDocument` and
`XamlReader` remain advanced parsing, compiled-XAML, reload, and tooling APIs
under `<Aero/Markup>`.

## Source vocabulary and ownership

Source location expresses visibility. Implementation types use their business
namespace (`Aero`, `Aero::Controls`, `Aero::Markup`, `Aero::Media`,
`Aero::Render`, or `Aero::App`) and single-translation-unit helpers use an
anonymous namespace. There are no `private`/`detail` directories,
`*Private*` filenames, domain `Detail` namespaces, or `View::Operations`
bridge. Kernel-private operations that are not installed live in
`src/gui/internal/` and are reached through one friend, `AeroGuiInternal`.

Heavy source-only objects own their state directly. Delayed states use inline
storage owned by the object, not a second heap allocation or virtual Pimpl
lifetime.

`src/gui` is divided into the stable implementation domains `base`,
`metadata`, `property`, `binding`, `resources`, `layout`, `input`,
`interactivity`, `controls`, `markup`, `media`, `text`, `diagnostics`, and
`modules`. Its root is reserved for the `Gui`, `View`, `ViewState`, and
`ViewRenderer` composition files. `View.cpp` is the composition root
(construct, mount, viewport, `Update`). Clock slices live beside it
(`ViewFrame.cpp`, `ViewInput.cpp`, `ViewFocus.cpp`, `ViewRender.cpp`).
Storyboard sessions live next to `AnimationEngine`, trigger evaluation in
`interactivity/`, and XamlReader fragment mounts in `markup/`. View remains
the host; layout, input, and media stay separate collaborators.

## View and rendering

Each `View` owns one `ViewState` and one concrete `ViewRenderer`. The device is
shareable across views and does not own a renderer.

Hosts may submit a complete logical/pixel/DPI viewport transaction through
`View::SetViewport()`, while `SetSize()` and `SetScale()` remain the familiar
Noesis-shaped conveniences. `View::Update()` returns true only when the update
commits a new immutable `RenderFrame`. Double-click input preserves the normal
mouse-down route with `ClickCount == 2` and then raises WPF-shaped Control
double-click events; horizontal wheel input uses `MouseWheelEventArgs::DeltaX`.

```text
retained UI
  -> immutable RenderFrame
  -> ViewRenderer
  -> UiFrameEncoder
  -> one-draw RenderBatch values
  -> D3D11RenderDevice or OpenGL33RenderDevice
```

`RenderBatch` represents one UI draw: pipeline key, target/pass values,
geometry ranges, resources, uniforms, and scissor. Uploads are performed
directly through device services. It is not a frame command list and has no
`RenderStep`, builder, begin/end-pass commands, or upload commands.

The concrete backends own GPU device/context, resource and pipeline caches,
dynamic buffers, and direct draw submission. There is no command-queue object,
device-owned renderer, renderer token, or release-renderer protocol.

## Window presentation

`Aero::App::RenderContext` is the only desktop frame lifecycle owner. Its base
implementation owns `BeginFrame`, `EndFrame`, `Present`, resize ordering, and
the frame flags. Concrete D3D11 and OpenGL contexts own the swap chain or
WGL/GLX window context and implement the native presentation hooks.

`RenderTarget` remains a drawable target only. Embedded hosts supply a texture,
view, or framebuffer through the public backend callbacks. A target does not
own `frameOpen`, `frameEnded`, present, or discard state.

## Markup and code-behind

`Gui::LoadComponent()` preserves the identity of the existing managed C++
code-behind root. `Gui::LoadXaml<T>()` returns the typed root while Gui retains
the complete pending document until `CreateView(root)` or `View::SetContent(root)`
mounts it; NameScope, resources, visual edges, and deferred effects are not
discarded. `GuiSchema`, XAML schema facets, compiled documents, provider
routing, document caching, and object writing remain Gui-owned implementation.

Gui strongly owns XAML, texture, and font providers through `Ref<>` values.
`SetXamlProvider()` replaces an existing route before initialization, and
resolution order is `scheme+assembly`, `scheme`, `assembly`, then default;
built-in XAML providers form only the fallback registry. Provider identities
are registry-private. Provider change notifications are raised on the Gui
dispatcher thread: XAML notifications invalidate dependent documents for
`ReloadCoordinator::Poll()`, while texture and font notifications are consumed
by each View on its next update.

## Verification

`cmake/CheckArchitecture.cmake` enforces the final file, namespace, SDK,
rendering-ownership, and presentation invariants. SDK consumer object targets
compile the installed headers without source includes. Static and shared
builds cover the product libraries; the shared build additionally verifies
that export-all remains disabled and runs `dumpbin` through
`cmake/CheckWindowsExports.cmake` for every product DLL. The check requires a
known public symbol and rejects source-only owners and retired vocabulary.
