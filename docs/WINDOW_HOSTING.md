# Native window hosting

AeroGUI separates WPF objects, the operating-system window, and native GPU
presentation. The public host frame entry is `View::Update(elapsedMs)`; XAML
loading is performed through `Markup::XamlReader`.

## Default desktop application

`Aero::App` privately owns the native event loop. Every visible `Aero::Window`
receives one host record containing:

```text
Window
├─ native top-level window
├─ View
├─ App::Detail::RenderContext
│  ├─ RenderDevice
│  └─ window RenderTarget
├─ clipboard / IME services
└─ activation, visibility and close state
```

`RenderContext` is source-private. It owns backend/window target creation,
resize, final render handoff and shutdown. `DesktopHost` does not directly own a
swap chain or surface lifecycle.

`Application::Windows` enumerates all live top-level windows. `MainWindow` is a
normal entry in that collection, while `ShutdownMode` independently implements
last-window, main-window and explicit shutdown policies.

## Embedded host sequence

An engine/editor host uses `Aero::Gui` plus an explicit backend device and
RenderTarget. Window/present policy is not part of the embedded Render API.

```cpp
Aero::Gui gui;
gui.AddXamlProvider(sourceProvider, "app");
gui.Initialize();

auto device = Aero::Render::CreateD3D11Device(deviceOptions).Value();
auto target = Aero::Render::CreateD3D11RenderTarget(
    device, targetOptions).Value();

auto view = gui.CreateView().Value();
view->GetRenderer().Init(device);

Aero::Markup::XamlReader reader(*view);
auto document = reader.Load("app:///MainView.xaml").Value();
view->SetContent(std::move(document), logicalSize);
view->SetViewport({logicalSize, pixelWidth, pixelHeight, dpiScale});

view->Update(elapsedMilliseconds);
view->GetRenderer().UpdateRenderTree();
view->GetRenderer().RenderOffscreen();
view->GetRenderer().Render(*target);
```

The host owns frame scheduling. AeroGUI creates no hidden render thread, pending
frame queue or presentation policy. Mutable UI objects remain View-thread
bound; the committed RenderFrame is immutable.

## Platform boundary

Default App platform adapters use `Aero::Platform::NativeWindowHandle` and the
input contracts under `Aero::Input`. Win32/X11 windows, clipboard adapters, IME,
WGL/GLX context carriers and swap-chain details remain source-private.

Public D3D11/OpenGL embedded headers do not include `NativeWindow` and expose no
window-surface factory. The App reaches those factories through the private
backend contract used by `RenderContext`.

## Device and target loss

`RenderDevice` owns device-generation loss/restore. Target-specific recovery is
coordinated by the backend/App presentation path rather than exposed as a second
public Surface product.

Optional diagnostics are available separately:

```cpp
#include <Aero/Diagnostics/Rendering.hpp>
auto deviceStats = Aero::Diagnostics::GetRenderDeviceStatistics(*device);
auto frameStats = Aero::Diagnostics::GetLastRenderFrameStatistics(*device);
```
