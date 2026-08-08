# C8-C12 structure closure

This closure finishes the post-S12 simplification pass without adding new framework layers.

## C8 - device and target lifetime

D3D11 and OpenGL now use the same ownership rule: a `RenderDevice` owns exactly one backend device implementation and every `RenderTarget` owns exactly one target implementation. The OpenGL desktop path no longer uses one object as both device and target. `DefaultTarget`, borrowed target implementations and target ownership flags are retired.

## C9 - one semantic renderer

`Aero::Render::Renderer` is the only semantic backend renderer. Its source-private `BatchComposer` produces a UI-specific `RenderBatch`; no generic command-list layer remains. Its lightweight attachment value is named `FrameTarget`, so it cannot be confused with the installed `Aero::RenderTarget`.

## C10 - rendering scope

The private surface contract represents window presentation only. Speculative EGL/WebGL/console surface kinds are removed from the current D3D11/OpenGL product line. Embedded targets continue to import host-owned render targets directly and do not fabricate a generic surface descriptor. The private graphics backend kind list contains only backends compiled by this tree.

## C11 - runtime ownership

Source-private runtime helpers live with their owners: the per-View renderer is a private nested View implementation, image cache lives under `Media::Detail`, and text pipeline under `Text::Detail`. Built-in generated themes belong to Gui-private state. Normal `XamlReader` authoring is load/parse oriented; View presentation remains `View::SetContent()`.

## C12 - headers and dependency visibility

The View composition root includes the private contracts it actually consumes instead of domain-wide private umbrellas. Stable WPF leaf headers begin owning real declarations while family headers remain aggregation points. Architecture documentation and gates describe final invariants rather than migration history.

## Final rendering path

```text
UI objects
  -> RenderTree
  -> immutable RenderFrame
  -> per-View IRenderer
  -> RenderDevice
       -> Render::Renderer
            -> BatchComposer
            -> one RenderBatch submission
  -> RenderTarget
       -> concrete embedded/window target
```

There is no `Integration` product, `RenderSurface` SDK type, `SurfaceSession`, second semantic renderer, or OpenGL device/target dual lifetime in the final model.
