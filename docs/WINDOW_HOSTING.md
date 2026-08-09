# Native window hosting

AeroGUI separates the WPF object model, operating-system windows, and GPU
presentation. The host owns scheduling; AeroGUI creates no hidden render
thread or pending frame queue.

## Default desktop application

Every visible `Aero::Window` is managed by `DesktopHost` with one native window,
one `View`, and one source-only `Aero::App::RenderContext`:

```text
Window host
  -> native Win32/X11 window
  -> View + ViewRenderer
  -> App::RenderContext
       -> RenderDevice
       -> current RenderTarget
       -> BeginFrame / EndFrame / Present
       -> D3D11 swap chain or WGL/GLX swap
  -> clipboard and IME services
```

`RenderContext` is the sole desktop presentation owner. The base class stores
frame-open/rendered/ended state and orders begin, render, end, present, resize,
cancel, and shutdown. `D3D11RenderContext` owns the D3D device, immediate
context, swap chain, and current back buffer. `OpenGLRenderContext` owns the
platform OpenGL window/context and swaps it directly.

A `RenderTarget` is drawable state only. It never presents and never stores a
desktop frame protocol.

## Embedded host sequence

An engine/editor host supplies a device and target through the backend-specific
public callbacks; window and present policy remain the host's responsibility.

```cpp
Aero::Gui gui;
gui.SetXamlProvider(sourceProvider, "app");
gui.Initialize();

auto device = Aero::Render::D3D11::CreateDevice(deviceOptions).Value();
auto target = Aero::Render::D3D11::CreateTarget(
    device, targetOptions).Value();

auto root = gui.LoadXaml<Aero::FrameworkElement>(
    "app:///MainView.xaml").Value();
auto view = gui.CreateView(root).Value();
view->GetRenderer().Init(device);

view->SetSize(logicalSize);
view->SetViewport({logicalSize, pixelWidth, pixelHeight, dpiScale});

view->Update(totalTimeSeconds);
auto& renderer = view->GetRenderer();
if (renderer.UpdateRenderTree() && renderer.RenderOffscreen()) {
    renderer.Render(*target);
}
```

`UpdateRenderTree()` reports whether a new immutable frame was committed, so
an unchanged frame can skip GPU work. Mutable UI state remains on the View
thread; renderer/device calls remain on the host's render thread.

## Loss and recovery

`RenderDevice` owns device loss and generation recovery. `RenderTarget` owns
only target-specific lost/restore state. App coordinates native swap-chain or
OpenGL context recreation through `RenderContext`; embedded hosts coordinate
their own callback resources.

Optional statistics are available from
`<Aero/Diagnostics/Rendering.hpp>`.
