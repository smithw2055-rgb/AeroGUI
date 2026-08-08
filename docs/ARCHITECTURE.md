# AeroGUI-R architecture

This file is the normative description of the current tree. Older milestone
documents record the path taken; when they disagree with this file, this file
wins.

## Product and SDK boundary

The shipped CMake products are `Aero::Base`, `Aero::Gui`, `Aero::App`, and the
optional `Aero::Audio`. `Aero::Gui` is the embeddable UI runtime. `Aero::App`
adds the default desktop window and application lifetime.

`include/Aero` is the complete installed SDK and `src` is hidden
implementation. `cmake/AeroPublicHeaders.cmake` is the exact installation
whitelist. Each binary has its own export macro (`AERO_BASE_API`,
`AERO_AUDIO_API`, `AERO_GUI_API`, or `AERO_APP_API`), automatic Windows symbol
export is disabled, and source files may not use an API macro.

Product-specific XAML capabilities cross this boundary as module data. In
particular, `AeroApp` contributes the `Application` resource scope through
`Markup::ResourceScopeRegistration`; `AeroGui` never includes or links back to
the App object model.

WPF-shaped UI headers live under `include/Aero/Gui`. The advanced rendering
surface is split by responsibility:

```text
include/Aero/Gui/IRenderer.hpp
include/Aero/Render/RenderDevice.hpp
include/Aero/Render/RenderTarget.hpp
include/Aero/Render/D3D11.hpp
include/Aero/Render/OpenGL33.hpp
```

The C++ type names remain `Aero::IRenderer`, `Aero::RenderDevice`, and
`Aero::RenderTarget`; only their physical SDK ownership is split.

Public headers contain no `Impl` declaration, internal conformance macro, or
source include. XAML document ownership is declared by
`<Aero/Gui/XamlDocument.hpp>`; loading is declared by
`<Aero/Gui/XamlReader.hpp>`; provider and service contracts live under
`<Aero/Markup>`.

## Source vocabulary and ownership

Source location expresses visibility. Implementation types use their business
namespace (`Aero`, `Aero::Controls`, `Aero::Markup`, `Aero::Media`,
`Aero::Render`, or `Aero::App`) and single-translation-unit helpers use an
anonymous namespace. There are no `private`/`detail` directories,
`*Internal*`/`*Private*` filenames, domain `Detail` namespaces, or
`View::Operations` bridge.

Heavy source-only objects own their state directly. Delayed states use inline
storage owned by the object, not a second heap allocation or virtual Pimpl
lifetime.

## View and rendering

Each `View` owns one `ViewState` and one concrete `ViewRenderer`. The device is
shareable across views and does not own a renderer.

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

`LoadComponentInto()` preserves the identity of the existing C++ code-behind
root. `GuiSchema`, XAML schema facets, compiled documents, provider routing,
document caching, and object writing are Gui-owned implementation. A desktop
host interacts with `View` through its public size, scale, content, update,
input, and renderer API.

## Verification

`cmake/CheckArchitecture.cmake` enforces the final file, namespace, SDK,
rendering-ownership, and presentation invariants. SDK consumer object targets
compile the installed headers without source includes. Static and shared
builds cover the product libraries; the shared build additionally verifies
that export-all remains disabled and runs `dumpbin` through
`cmake/CheckWindowsExports.cmake` for every product DLL. The check requires a
known public symbol and rejects source-only owners and retired vocabulary.
