# Rendering S10 convergence

S10 finishes the host-visible Render target vocabulary and moves the default
desktop presentation lifetime behind App.

## Installed rendering contract

```text
View
  -> IRenderer
       -> RenderDevice
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

The embedded factory no longer creates an implicit second device lifetime.

## Desktop presentation

The default App no longer creates/resizes/renders a backend target directly from
`DesktopHost::WindowHost`. One source-private `App::Detail::RenderContext` owns:

- backend/window target construction;
- selected RenderDevice access;
- target resize;
- the final `IRenderer::Render(RenderTarget&)` handoff;
- WaitIdle and target shutdown.

Window event routing, application lifetime and View updates remain on
`DesktopHost`; GPU target/presentation ownership does not.

## Native surface vocabulary

`Graphics::SurfaceSession`, `ISurfaceBackend`, swap-chain surfaces and context
surfaces remain source-private implementation types. They manage native acquire,
submit, present and recovery and are not SDK product objects.

This distinction is intentional:

```text
SDK:              RenderTarget
native backend:   SurfaceSession / swap-chain or context surface
```

## Architecture gates

S9 replaced cumulative H/J/K/R/S migration checks with final invariants. The
permanent gate now checks the installed product boundary, removal of Integration,
RenderTarget ownership, immutable RenderTree submission separation and App
RenderContext ownership. It does not freeze historical file counts, private
aggregate names, View arena markers or Object Library names.
