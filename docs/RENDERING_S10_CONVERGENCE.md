# Rendering S10-S12 convergence

> Historical milestone record. See `ARCHITECTURE.md` and
> `WINDOW_HOSTING.md` for the current rendering and presentation model.

S10 finishes the host-visible Render target vocabulary and moves default desktop
presentation behind App.

## Installed rendering contract

```text
View
  -> IRenderer
       -> RenderDevice
            -> source-private Render::Renderer
       -> RenderTarget
```

`RenderTarget` is the only installed target object. `RenderSurface` is retired
from the public header tree.

Embedded D3D11/OpenGL hosts use explicit device + target construction:

```cpp
auto device = Aero::Render::CreateD3D11Device(options);
auto target = Aero::Render::CreateD3D11RenderTarget(
    device.Value(), targetOptions);

view->GetRenderer().Init(device.Value());
view->GetRenderer().Render(*target.Value());
```

The embedded target factory always receives an explicit RenderDevice and never
creates a second implicit device lifetime. Public backend headers contain no
native-window or present-mode API.

## Desktop presentation

The default App no longer creates/resizes/renders a backend target directly from
`DesktopHost::WindowHost`. One source-private `App::Detail::RenderContext` owns:

- backend/window target construction through source-private backend contracts;
- selected RenderDevice access;
- target resize;
- the final `IRenderer::Render(RenderTarget&)` handoff;
- WaitIdle and target shutdown.

Window event routing, application lifetime and View updates remain on
`DesktopHost`; GPU target/presentation ownership does not.

## Target and native surface ownership

Backend target classes now derive directly from the source-private
`RenderTarget::Impl`. The old `NativeRenderTarget` name is only a type alias for
that Impl and has no independent allocation, pointer hop or lifecycle.

Native acquire/present mechanics remain in `Graphics::SurfaceSession` plus
concrete swap-chain/context adapters:

```text
SDK RenderTarget
  -> backend RenderTarget::Impl
       -> Graphics::SurfaceSession
            -> native swap chain / external target / GL context
```

This keeps one native surface state machine while removing the extra target
wrapper object.

## Renderer ownership

`Render::Renderer` is the single semantic device renderer. It owns the low-level
command encoder and device-generation-scoped image/mesh/glyph resource tables.
The old `DeviceRenderer` spelling is a source-only alias; it no longer represents
a separate class or lifetime. `FrameEncoder.cpp` remains the low-level encoding
implementation rather than another semantic renderer.

## Diagnostics

Render device/frame statistics moved out of the normal RenderDevice API into:

```cpp
#include <Aero/Diagnostics/Rendering.hpp>

Aero::Diagnostics::GetRenderDeviceStatistics(device);
Aero::Diagnostics::GetLastRenderFrameStatistics(device);
```

The repository conformance executable has an internal compile-time bridge for
its legacy statistics checks; normal SDK consumers do not see that member API.

## Architecture gates

S9 replaces cumulative H/J/K/R/S migration checks with final invariants.
`CheckArchitecture.cmake` now validates installed product ownership, removal of
Integration/RenderSurface, RenderTarget/Renderer/RenderContext boundaries and
immutable RenderTree submission separation. `CheckConventions.cmake` keeps only
stable public conventions such as duplicate includes and one-line property/event
declarations.

Neither gate freezes historical file counts, private aggregate names, View arena
markers, internal Object Library names or past migration aliases.


## S11 frame orchestration

S11 makes `Render::Renderer` the only semantic command-submission boundary.
Native backends still acquire/import platform targets, but they no longer own a
second record/submit/present sequence. The flow is now:

```text
backend target acquisition/import
  -> Render::Renderer
       -> encode immutable RenderFrame
       -> collect retired GPU resources
       -> submit exactly once
       -> SurfaceSession::CompleteFrame
            -> present or release borrowed target
```

`SurfaceSession` no longer has `SubmitFrame`. This removes the former OpenGL
embedded double-submit path and makes capture/present bookkeeping consume the
fence produced by the one renderer submission. `Render::Renderer` captures its
owning render thread at initialization and rejects GPU frame execution from a
different thread. Device-lost state is checked before recording/submission.

The public `IRenderer::UpdateRenderTree()` return value is the dirty-frame gate:
`true` means a new immutable frame was committed. Default App avoids redundant
GPU work for unchanged frames, while native expose/resize can force a re-present.

## S12 desktop, compiled-XAML and SDK closure

The default desktop loop no longer sleeps/polls every millisecond. The platform
window contract provides `WaitEventFor` in addition to the indefinite wait. A
single non-animated window blocks on native events; animation and multi-window
hosting use a 16 ms timed native wait so frame clocks and other windows stay
responsive without a busy loop.

Built-in theme bootstrap has two physically distinct outputs. The runtime source
fallback is generated under `runtime-generated/Aero`; post-Gui compiled assets
remain under `generated/Aero`. Therefore an in-tree `aero-xamlc` can link the
finished Gui product without Ninja rebinding View's generated-header dependency
back to xamlc and forming `AeroGui -> aero-xamlc -> AeroGui`. Independent host
tools may still provide precompiled built-ins for cross/production builds.

Schema generation also has one deterministic metadata order: foundational
`Point` value metadata is registered before geometry properties author Point
defaults, and `UserControl` has one Controls registration owner. The resulting
manifest is consumed by the same `aero-xamlc --schema` path installed with the
SDK.
