# Rendering S1 convergence (historical)

> Historical milestone record. See `ARCHITECTURE.md` and
> `WINDOW_HOSTING.md` for the current rendering and presentation model.

S1 removed the former `void* + RenderDeviceFunctions/RenderSurfaceFunctions`
dispatch system and replaced it with ordinary source-private C++ backend
contracts. That work is retained in Git history.

The current architecture has moved beyond the S1 public model:

```text
S1 public model
RenderDevice + RenderSurface

Current model after S10
RenderDevice + RenderTarget
```

`RenderSurface` is no longer an installed SDK type. Native window/context
surfaces remain implementation vocabulary under `src/render`, where
`Graphics::SurfaceSession` owns acquire/present completion. The default desktop
product coordinates target creation, resize, rendering and shutdown through the
source-private `App::Detail::RenderContext`.

See `SOURCE_ARCHITECTURE.md` and `RENDERING_S10_CONVERGENCE.md` for the current
contract. Permanent architecture checks validate final invariants rather than
replaying S1 migration markers.
