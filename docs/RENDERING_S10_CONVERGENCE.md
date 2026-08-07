# Rendering S10 convergence

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
