# Native window hosting

AeroGUI separates the operating-system window from the GPU surface.

## Ownership boundary

`Aero::Platform::IWindow` owns the native top-level window and its event pump:

- create/show/close lifecycle;
- client size and DPI scale;
- expose, resize and close events;
- normalized pointer, keyboard and UTF-8 text events;
- an opaque `NativeWindowHandle` for graphics integration.

`Aero::Rhi::SurfaceSession` continues to own only graphics presentation state:

- D3D11 swap chains;
- WGL/GLX/EGL contexts and drawable bindings;
- surface resize, acquire, present and context-loss recovery.

The platform layer must not create a renderer or traverse a `RenderPlan`. The
RHI layer must not run a Win32/X11 application event loop.

## Operating-system carriers

- `Aero/Platform/Win32Window.hpp` declares the Windows carrier. Its PImpl
  implementation owns an HWND and translates Win32 messages into `WindowEvent`
  values.
- `Aero/Platform/X11Window.hpp` declares the X11 carrier. Its PImpl
  implementation can create a default-visual window or attach to an existing
  drawable. ControlGallery uses attachment because the GLX backend must choose
  a compatible visual before creating its OpenGL context.

Neither public header includes `windows.h` or Xlib headers. Native SDK types stay
inside `AeroPlatform`, preserving public-header self-containment and allowing
non-Windows or X11-disabled builds to compile the same declarations. The X11
implementation becomes a linkable Unsupported adapter when GLX/X11 is disabled.

## Runtime resize

`RuntimeHost::Resize()` updates the mounted root's logical available size. The
XAML visual-tree host invalidates both layout and the root render node, so the
next `RunFrame()` publishes a new immutable `RenderPlan` containing the updated
layout slots and render sizes.

The OS window reports physical client pixels. Application hosts divide pointer
positions and client dimensions by the event DPI scale before dispatching input
or resizing the runtime. The RHI surface continues to use physical pixels.

## Win32 text services

The visible Windows sample uses the existing platform contracts rather than
adding text-specific behavior to the renderer:

- `Win32Clipboard` reads and writes `CF_UNICODETEXT`, converting at the boundary
  between UTF-8 runtime strings and UTF-16 Win32 storage;
- `Win32ImeAdapter` subclasses the active HWND and consumes
  `WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION` and
  `WM_IME_ENDCOMPOSITION`;
- composition updates are delivered through `ITextCompositionClient` to the
  focused TextBox model;
- the candidate and composition windows are positioned from the TextBox logical
  caret rectangle using the HWND DPI;
- the retained renderer continues to draw the normal TextBox caret, while the
  adapter creates a Win32 native caret during active IME composition;
- an IME update invalidates the HWND, causing the next expose event to run the
  full RuntimeHost frame and publish a fresh RenderPlan.

The IME adapter accepts its TextBox client before an HWND exists. ControlGallery
attaches it to the active HWND on the first native event, which preserves the
existing runtime-before-surface startup order.

## ControlGallery GUI mode

The existing non-interactive smoke path is unchanged. A visible GUI instance is
started explicitly:

```text
AeroControlGallery --backend=d3d11 --xaml=compiled --theme=light --interactive
AeroControlGallery --backend=opengl --xaml=compiled --theme=dark --interactive
```

In GUI mode the sample:

1. creates the OS window carrier;
2. passes only its opaque native handle to the selected RHI surface;
3. submits the current immutable `RenderPlan`;
4. translates pointer, keyboard and text events into `RuntimeHost` input;
5. converts physical resize events to logical runtime dimensions;
6. runs the complete property/binding/layout/render frame pipeline;
7. resizes the physical GPU surface;
8. submits the newly published `runtime.Plan()`;
9. routes Win32 IME and native clipboard operations through `AeroPlatform`;
10. preserves the existing context-loss recovery path.

Non-Windows builds keep the memory clipboard and do not attach the Win32 IME
host. The portable runtime and headless smoke paths therefore remain independent
of native Windows APIs.
