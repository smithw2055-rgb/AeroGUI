# S16-S17 rendering and XAML closure

> Historical milestone record. S18-S24 superseded the renderer, batch, device,
> and App context descriptions below; see `REFACTOR_CLOSURE_S18_S24.md`.

## One UI rendering path

The backend-neutral renderer now records one source-private `RenderBatch`.
The selected `RenderDevice::Impl` owns `DrawBatch()` and therefore owns the
actual GPU submission:

```text
View::Renderer
  -> Render::Renderer::BuildOffscreenBatch / BuildOnscreenBatch
  -> RenderBatch
  -> D3D11/OpenGL RenderDevice::Impl::DrawBatch
```

The former duplicate diagnostic batching pass has been removed.
Frame diagnostics are populated from the encoder that produced the submitted
batch, so diagnostics and rendering cannot disagree about the merge result.
Resource allocation, retirement, and command submission now live on the same
`RenderDevice::Impl`. Concrete D3D11/OpenGL command queues are compile-time
forwarded helpers; the former generic graphics-device and abstract-backend
objects and source files have been removed.

Desktop presentation is a separate source-private path:

```text
DesktopHost
  -> App::Detail::CreateRenderContext
       -> D3D11RenderContext / GLRenderContext
  -> App::Detail::RenderContext
       -> BeginFrame
       -> IRenderer::Render(current target)
       -> EndFrame
       -> Present
```

Window D3D11 and OpenGL targets defer native presentation while this lifecycle
is open. Embedded targets retain the explicit one-call
`IRenderer::Render(RenderTarget&)` contract.

## WPF-style code behind

`x:Class` activates a registered derived root. A component type is registered
with `DefineComponentModule<T...>()`; default construction and optional
`DescribeComponent()` metadata are supplied without Registry or XAML facet
callbacks. `TypeDescription::EventHandler()` describes code-behind handlers,
and the object writer connects attributes such as
`Click="OnHelloClick"` through the existing routed-event system.

`Window::InitializeComponent()` requests the conventional `<TypeName>.xaml`
next to `App.xaml`; generated code can pass an explicit component URI. Named
objects remain available through `FrameworkElement::FindName<T>()` and
`XamlDocument::FindName<T>()`.

The default C++ template under `templates/AeroApp` uses `App.xaml` and
`MainWindow.xaml`, includes an `x:Class`, a named button, and an automatically
connected `Click` handler.
