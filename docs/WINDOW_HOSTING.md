# Native window hosting

AeroGUI separates the operating-system window from the product runtime and the
private graphics implementation. The only public product frame entry is
`View::RunFrame()`.

## Ownership boundary

`Aero::Platform::IWindow` owns the native top-level window and event pump:

- create, show and close;
- client size and DPI scale;
- expose, resize and close events;
- normalized pointer, keyboard and UTF-8 text events;
- an opaque `NativeWindowHandle`.

`Aero::Integration::RenderEndpoint` is the only rendering object held by a
`View`. It is reference counted and opaque: it does not expose a renderer,
device, surface, command list, resource registry or the internal immutable
render snapshot.

An endpoint has one of three modes:

- `Headless`: Runtime-owned CPU diagnostics and text-resource sink; no GPU
  device is created.
- `Embedded`: the host lends a native device/context and supplies the current
  target callback. AeroGUI records and submits UI work but never calls Present.
- `Window`: the endpoint owns the surface/swapchain/context and presents each
  accepted GPU frame exactly once.

One endpoint can be bound to one `View`. The `View` keeps a strong reference,
so the caller may release its reference after `ViewHost::CreateView()` returns.
Multiple endpoints may borrow the same host native device.

## Public factories

The default `Aero/Integration.hpp` exposes `ViewHost`, `RenderEndpoint`,
source-provider registration and reload coordination. Backend factories are
explicit opt-in headers:

- `Aero/Integration/D3D11.hpp`;
- `Aero/Integration/OpenGL33.hpp`;
- `Aero/Integration/HostedGraphics.hpp` for third-party backends.

These headers use `std::uintptr_t`, function pointers and versioned POD
contracts. They do not include Windows, D3D11, OpenGL or X11 system headers.
`HostedGraphics.hpp` exposes a tagged read-only graphics command ABI, not the
internal render snapshot or RHI classes.

## Resize and frame flow

Logical and physical resize are separate operations:

```cpp
view.Resize({logicalWidth, logicalHeight});
endpoint.Resize(physicalWidth, physicalHeight);
```

The host then calls `view.RunFrame()`. That call performs property, binding,
lifecycle and layout phases, builds a candidate immutable snapshot and offers
it to the bound endpoint. The current snapshot version advances only after the
endpoint accepts the candidate. `ViewFrameResult` contains safe layout/render
statistics only.

`RenderSubmissionMode::Immediate` preserves synchronous submission.
`DedicatedThread` uses the endpoint's private worker and two frame slots: an
executing snapshot is never replaced, while a pending snapshot may be replaced
by the newest accepted one. UI and `Visual` pointers never cross this boundary.
HostedGraphics requires the `ThreadSafe` capability for this mode. The current
D3D11 and OpenGL 3.3 factories reject it with `Unsupported` because their
text/resource preparation and native contexts remain owner-thread affine.

## Device and surface loss

Hosts report loss through the endpoint:

```cpp
endpoint.NotifySurfaceLost(); // or NotifyDeviceLost()
endpoint.Restore();
view.RunFrame();
```

Reporting loss stops acceptance, discards pending frames and advances the
endpoint generation. Backend image, mesh and glyph resources are invalidated
inside the endpoint. A successful `Restore()` rebuilds the native surface or
device, and the next `RunFrame()` submits a complete snapshot. Hosts do not
manually shut down or reinitialize renderer, surface, presenter and device
objects.

## ControlGallery GUI mode

```text
AeroControlGallery --backend=d3d11 --xaml=compiled --theme=light --interactive
AeroControlGallery --backend=opengl --xaml=compiled --theme=dark --interactive
```

The sample follows the public sequence:

1. create the OS window;
2. create a Window endpoint from its opaque native handle;
3. create the `View` through `Integration::ViewHost`;
4. load content and dispatch normalized input;
5. resize the `View` logically and the endpoint physically;
6. call `View::RunFrame()`;
7. use endpoint loss/restore operations for recovery.

Win32 clipboard and IME stay platform services supplied in `ViewHostOptions`;
they do not become renderer or text-layout extension points.
