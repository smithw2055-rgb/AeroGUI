# Native window hosting

AeroGUI separates WPF objects, the operating-system window, and the private GPU
implementation. The public host frame entry is `View::Update(elapsedMs)`.
XAML loading is performed through `Markup::XamlReader`; it is not a View
responsibility.

## Default desktop application

`Aero::App` privately owns the native event loop. Every visible `Aero::Window`
receives an independent host record containing:

```text
Window
├─ native top-level window
├─ View
├─ RenderDevice
├─ clipboard / IME services
└─ activation, visibility and close state
```

`Application::Windows` enumerates all live top-level windows. `MainWindow` is a
normal entry in that collection, while `ShutdownMode` independently implements
last-window, main-window and explicit shutdown policies. No public Launcher,
WindowPeer or application service locator is required.

The default application is process-wide rather than thread-local. Main-window
ownership is explicit: `SetMainWindow(Ref<Window>)` retains the window and
`SetMainWindowBorrowed(Window*)` does not. Application resources are always
available by reference. Diagnostic hosts can use the `RunChecked`,
`ShowChecked` and `CloseChecked` variants without complicating ordinary WPF-style
call sites.

## Embedded host sequence

An engine or editor host uses the explicit Integration surface:

```cpp
Aero::Gui environment;
environment.AddModule(module);
environment.Initialize();

Aero::Integration::ViewOptions options;
options.renderDevice = renderDevice;
auto view = environment.CreateView(options).Value();

Aero::Markup::XamlReader reader(*view);
gui.AddXamlProvider(sourceProvider, "app");
auto document = reader.Load("app:///MainView.xaml").Value();
view->SetContent(std::move(document), logicalSize);

view->SetSize(logicalSize);
renderDevice->Resize(pixelWidth, pixelHeight);
view->Update(elapsedMilliseconds);
```

`View::Update()` advances host-driven clocks, processes property/binding/input
work, commits an immutable `RenderFrame`, and submits it synchronously to the
bound RenderDevice. AeroGUI creates no hidden render thread or frame queue. Engines
may call Update from their chosen scheduling point, but mutable UI objects must
remain on the View owner thread.

## Platform boundary

Hosts exchange only `Integration::NativeWindowHandle` plus the narrow contracts
from `Integration/Platform.hpp`. Win32/X11 windows, clipboard adapters,
IME message handling, WGL and GLX context carriers live under
`src/platform/<os>` and are not installed SDK types.

## Device and surface loss

```cpp
renderDevice->NotifySurfaceLost(); // or NotifyDeviceLost()
renderDevice->Restore();
view->Update(elapsedMilliseconds);
```

Loss advances the device generation and invalidates backend image, mesh and
glyph resources. The next successful update submits a complete immutable
snapshot. Renderer, native surface and GPU resource caches remain private.
