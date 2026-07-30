#include <Aero/App/ApplicationHost.hpp>

#include <Aero/App/Application.hpp>
#include <Aero/App/Window.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Integration/OpenGL33.hpp>
#include <Aero/Integration/ViewHost.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#if defined(_WIN32)
#include <Aero/Integration/D3D11.hpp>
#include <Aero/Platform/Win32Window.hpp>
#else
#include <Aero/Platform/X11Window.hpp>
#endif

#include <chrono>
#include <cmath>
#include <memory>
#include <new>
#include <thread>
#include <utility>

namespace Aero::App {
namespace {

Base::Status HostFailure(
    Base::ErrorCode code,
    const char* message) noexcept {
    return Base::Status::Failure(code, message);
}

Presentation::MouseButton MapButton(
    Platform::WindowPointerButton button) noexcept {
    switch (button) {
    case Platform::WindowPointerButton::Right:
        return Presentation::MouseButton::Right;
    case Platform::WindowPointerButton::Middle:
        return Presentation::MouseButton::Middle;
    case Platform::WindowPointerButton::XButton1:
        return Presentation::MouseButton::XButton1;
    case Platform::WindowPointerButton::XButton2:
        return Presentation::MouseButton::XButton2;
    case Platform::WindowPointerButton::Unknown:
    case Platform::WindowPointerButton::Left:
    default:
        return Presentation::MouseButton::Left;
    }
}

std::uint32_t WindowExtent(
    double value,
    std::uint32_t fallback) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return fallback;
    }
    if (value >=
        static_cast<double>(UINT32_MAX)) {
        return UINT32_MAX;
    }
    return static_cast<std::uint32_t>(value);
}

} // namespace

struct ApplicationHost::Impl final
    : Detail::IApplicationPeer,
      Detail::IWindowPeer {
    Impl(
        const ApplicationHostOptions& source,
        Base::IAllocator* selectedAllocator) noexcept
        : allocator(selectedAllocator != nullptr
              ? selectedAllocator
              : &Base::GetDefaultAllocator()),
          environment(allocator),
          backend(source.graphicsBackend),
          defaultWidth(source.defaultWidth),
          defaultHeight(source.defaultHeight),
          visible(source.visible),
          resizable(source.resizable),
          automaticAnimationClock(
              source.automaticAnimationClock),
          loadBuiltInTheme(source.loadBuiltInTheme),
          builtInTheme(source.builtInTheme),
          diagnostics(source.diagnostics),
          startup(source.startup),
          startupContext(source.startupContext),
          frame(source.frame),
          frameContext(source.frameContext) {
        optionsStatus =
            applicationFile.TryAssign(
                source.applicationFile);
        if (optionsStatus) {
            const Base::StringView path =
                applicationFile.View();
            std::uint32_t separator =
                path.SizeBytes();
            for (std::uint32_t index = 0U;
                 index < path.SizeBytes();
                 ++index) {
                if (path[index] == '/' ||
                    path[index] == '\\') {
                    separator = index;
                }
            }
            optionsStatus = assetRoot.TryAssign(
                separator < path.SizeBytes()
                ? path.Substr(0U, separator)
                : Base::StringView("."));
        }
    }

    ~Impl() noexcept {
        if (endpoint) {
            static_cast<void>(endpoint->WaitIdle());
        }
        if (view) {
            static_cast<void>(view->Unmount());
        }
        DetachObjects();
        windowOwner.Reset();
        applicationOwner.Reset();
        view.Reset();
        endpoint.Reset();
        if (nativeWindow) {
            nativeWindow->Close();
        }
    }

    Base::Result<void> CreateRuntime() noexcept {
        if (!optionsStatus) {
            return optionsStatus.GetStatus();
        }
        Base::Result<void> initialized =
            environment.Initialize();
        if (!initialized) return initialized.GetStatus();
        Integration::ViewHostOptions hostOptions;
        hostOptions.text.fontSearchRoot =
            assetRoot.View();
        Base::Result<Base::Ref<View>> created =
            Integration::ViewHost::CreateView(
                environment, hostOptions, allocator);
        if (!created) return created.GetStatus();
        view = std::move(created).Value();
        if (loadBuiltInTheme) {
            Base::Result<void> themed =
                view->LoadBuiltInTheme(
                    builtInTheme);
            if (!themed) {
                view.Reset();
                return themed.GetStatus();
            }
        }
        return {};
    }

    Base::Result<void> LoadApplication() noexcept {
        Base::Result<UiDocument> loaded =
            view->Load(
                applicationFile.View(),
                diagnostics);
        if (!loaded) return loaded.GetStatus();
        const Base::Ref<Base::Object>& root =
            loaded.Value().Root();
        if (!root ||
            root->RuntimeType() !=
                Application::StaticTypeId()) {
            return HostFailure(
                Base::ErrorCode::InvalidArgument,
                "Application XAML root must be Application");
        }
        applicationOwner = root;
        application = static_cast<Application*>(
            applicationOwner.Get());
        if (application->StartupUri().Empty()) {
            return HostFailure(
                Base::ErrorCode::InvalidArgument,
                "Application StartupUri is empty");
        }
        Base::Result<Base::ResourceUri> resolvedStartupUri =
            Base::ResourceUri::Resolve(
                loaded.Value().CanonicalUri(),
                application->StartupUri());
        if (!resolvedStartupUri) {
            return resolvedStartupUri.GetStatus();
        }
        startupUri =
            std::move(resolvedStartupUri).Value();

        Base::Ref<Presentation::ResourceDictionary>
            resources = application->Resources();
        if (resources) {
            Base::Result<void> installed =
                view->SetResourceDictionary(
                    RuntimeResourceLayer::Application,
                    *resources);
            if (!installed) return installed.GetStatus();
        }
        return {};
    }

    Base::Result<void> LoadMainWindow() noexcept {
        Base::Result<UiDocument> loaded =
            view->Load(
                startupUri.Canonical(),
                diagnostics);
        if (!loaded) return loaded.GetStatus();
        const Base::Ref<Base::Object>& root =
            loaded.Value().Root();
        if (!root ||
            root->RuntimeType() !=
                Window::StaticTypeId()) {
            return HostFailure(
                Base::ErrorCode::InvalidArgument,
                "StartupUri XAML root must be Window");
        }
        windowOwner = root;
        window = static_cast<Window*>(windowOwner.Get());

        std::uint32_t width =
            WindowExtent(window->Width(), defaultWidth);
        std::uint32_t height =
            WindowExtent(window->Height(), defaultHeight);
        Base::Result<void> native =
            CreateNativeWindow(width, height);
        if (!native) return native.GetStatus();
        width = nativeWindow->ClientWidth();
        height = nativeWindow->ClientHeight();
        if (width == 0U || height == 0U) {
            return HostFailure(
                Base::ErrorCode::InvalidState,
                "Application native window has an empty client area");
        }
        Base::Result<void> graphics =
            CreateEndpoint(width, height);
        if (!graphics) return graphics.GetStatus();
        Base::Result<void> attached =
            view->SetRenderEndpoint(
                endpoint, automaticAnimationClock);
        if (!attached) return attached.GetStatus();
        Base::Result<void> mounted =
            view->SetContent(
                std::move(loaded).Value(),
                {static_cast<double>(width),
                 static_cast<double>(height)});
        if (!mounted) return mounted.GetStatus();

        window->Attach(this);
        application->Attach(this, window);
        if (startup != nullptr) {
            Base::Result<void> initialized =
                startup(
                    *application,
                    *window,
                    startupContext);
            if (!initialized) {
                return initialized.GetStatus();
            }
        }
        return {};
    }

    Base::Result<void> CreateNativeWindow(
        std::uint32_t width,
        std::uint32_t height) noexcept {
#if defined(_WIN32)
        nativeWindow.reset(
            new (std::nothrow)
                Platform::Win32Window(allocator));
#else
        nativeWindow.reset(
            new (std::nothrow)
                Platform::X11Window(allocator));
#endif
        if (!nativeWindow) {
            return HostFailure(
                Base::ErrorCode::OutOfMemory,
                "Application native window allocation failed");
        }
        Platform::WindowDescriptor descriptor;
        descriptor.title = window->Title().Empty()
            ? Base::StringView("AeroGUI")
            : window->Title();
        descriptor.width = width;
        descriptor.height = height;
        descriptor.visible = false;
        descriptor.resizable = resizable;
        return nativeWindow->Create(descriptor);
    }

    Base::Result<void> CreateEndpoint(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        GraphicsBackend selected = backend;
        if (selected == GraphicsBackend::Automatic) {
#if defined(_WIN32)
            selected = GraphicsBackend::D3D11;
#else
            selected = GraphicsBackend::OpenGL33;
#endif
        }
#if defined(_WIN32)
        if (selected == GraphicsBackend::D3D11) {
#if AERO_APP_HAS_D3D11
            Integration::D3D11WindowEndpointOptions options;
            options.window = nativeWindow->NativeHandle();
            options.width = width;
            options.height = height;
            options.presentMode =
                Integration::RenderPresentMode::Fifo;
            options.allowWarpFallback = true;
            Base::Result<Base::Ref<
                Integration::RenderEndpoint>> created =
                Integration::CreateD3D11WindowEndpoint(
                    options, allocator);
            if (!created) return created.GetStatus();
            endpoint = std::move(created).Value();
            return {};
#else
            return HostFailure(
                Base::ErrorCode::Unsupported,
                "D3D11 application backend is not enabled");
#endif
        }
#endif
        if (selected == GraphicsBackend::OpenGL33) {
#if AERO_APP_HAS_OPENGL_WINDOW
            Integration::OpenGL33WindowEndpointOptions options;
            options.window = nativeWindow->NativeHandle();
            options.width = width;
            options.height = height;
            options.presentMode =
                Integration::RenderPresentMode::Fifo;
            Base::Result<Base::Ref<
                Integration::RenderEndpoint>> created =
                Integration::CreateOpenGL33WindowEndpoint(
                    options, allocator);
            if (!created) return created.GetStatus();
            endpoint = std::move(created).Value();
            return {};
#else
            return HostFailure(
                Base::ErrorCode::Unsupported,
                "OpenGL window application backend is not enabled");
#endif
        }
        return HostFailure(
            Base::ErrorCode::Unsupported,
            "Requested application graphics backend is unavailable");
    }

    Base::Result<void> HandleEvent(
        const Platform::WindowEvent& event) noexcept {
        switch (event.type) {
        case Platform::WindowEventType::CloseRequested:
        case Platform::WindowEventType::Closed:
            RequestExit(0);
            return {};
        case Platform::WindowEventType::Resized:
        case Platform::WindowEventType::ScaleChanged:
            if (event.width == 0U ||
                event.height == 0U) {
                return {};
            }
            // WM_SIZE can arrive much faster than a D3D11 swap chain can be
            // recreated. Keep only the newest extent and apply it once after
            // the native queue has drained for this host tick.
            pendingResizeWidth = event.width;
            pendingResizeHeight = event.height;
            hasPendingResize = true;
            return {};
        case Platform::WindowEventType::PointerMove:
        case Platform::WindowEventType::PointerDown:
        case Platform::WindowEventType::PointerUp:
        case Platform::WindowEventType::PointerWheel: {
            // Preserve the coordinate contract for a pointer message that
            // follows WM_SIZE in the same native queue.
            Base::Result<void> appliedResize = ApplyPendingResize();
            if (!appliedResize) return appliedResize.GetStatus();
            Presentation::PointerInput input;
            input.pointerId = 1U;
            input.position = {event.x, event.y};
            input.changedButton = MapButton(event.button);
            input.wheelDeltaX = event.wheelDeltaX / 120.0;
            input.wheelDeltaY = event.wheelDeltaY / 120.0;
            if (event.type ==
                Platform::WindowEventType::PointerDown) {
                input.action =
                    Presentation::PointerAction::Down;
            } else if (event.type ==
                Platform::WindowEventType::PointerUp) {
                input.action =
                    Presentation::PointerAction::Up;
            } else if (event.type ==
                Platform::WindowEventType::PointerWheel) {
                input.action =
                    Presentation::PointerAction::Wheel;
            } else {
                input.action =
                    Presentation::PointerAction::Move;
            }
            Base::Result<
                Presentation::PointerDispatchResult>
                dispatched =
                    view->DispatchPointer(input);
            return dispatched
                ? Base::Result<void>()
                : Base::Result<void>(
                      dispatched.GetStatus());
        }
        case Platform::WindowEventType::KeyDown:
        case Platform::WindowEventType::KeyUp: {
            if (event.key == 0U) return {};
            Presentation::KeyboardInput input;
            input.action =
                event.type ==
                    Platform::WindowEventType::KeyDown
                ? Presentation::KeyboardAction::Down
                : Presentation::KeyboardAction::Up;
            input.key = event.key;
            input.modifiers = event.modifiers;
            input.isRepeat = event.repeat;
            Base::Result<
                Presentation::KeyboardDispatchResult>
                dispatched =
                    view->DispatchKeyboard(input);
            return dispatched
                ? Base::Result<void>()
                : Base::Result<void>(
                      dispatched.GetStatus());
        }
        case Platform::WindowEventType::TextInput: {
            if (event.textSize == 0U) return {};
            Base::Result<
                Presentation::TextInputDispatchResult>
                dispatched = view->DispatchText(
                    {event.Text()});
            return dispatched
                ? Base::Result<void>()
                : Base::Result<void>(
                      dispatched.GetStatus());
        }
        case Platform::WindowEventType::Exposed:
        case Platform::WindowEventType::Invalid:
        default:
            return {};
        }
    }

    Base::Result<void> ApplyPendingResize() noexcept {
        if (!hasPendingResize) {
            return {};
        }

        const std::uint32_t width = pendingResizeWidth;
        const std::uint32_t height = pendingResizeHeight;
        hasPendingResize = false;
        Base::Result<void> resized = endpoint->Resize(width, height);
        if (!resized) return resized.GetStatus();

        resized = view->Resize({
            static_cast<double>(width),
            static_cast<double>(height)});
        if (!resized) return resized.GetStatus();
        return {};
    }

    Base::Result<int> Run() noexcept {
        Base::Result<void> status = CreateRuntime();
        if (status) status = LoadApplication();
        if (status) status = LoadMainWindow();
        if (!status) return status.GetStatus();

        Base::Result<ViewFrameResult> firstFrame =
            view->RunFrame();
        if (!firstFrame) return firstFrame.GetStatus();
        std::uint64_t frameIndex = 0U;
        if (frame != nullptr) {
            status = frame(
                *application, *window,
                frameIndex, frameContext);
            if (!status) return status.GetStatus();
        }
        if (visible) {
            status = Show();
            if (!status) return status.GetStatus();
        }

        while (!exitRequested &&
               nativeWindow->IsOpen()) {
            for (;;) {
                Platform::WindowEvent event;
                Base::Result<bool> received =
                    nativeWindow->PollEvent(event);
                if (!received) {
                    return received.GetStatus();
                }
                if (!received.Value()) break;
                status = HandleEvent(event);
                if (!status) return status.GetStatus();
                if (exitRequested) break;
            }
            if (exitRequested) break;
            status = ApplyPendingResize();
            if (!status) return status.GetStatus();
            Base::Result<ViewFrameResult> renderedFrame =
                view->RunFrame();
            if (!renderedFrame) {
                return renderedFrame.GetStatus();
            }
            ++frameIndex;
            if (this->frame != nullptr) {
                status = this->frame(
                    *application, *window,
                    frameIndex, frameContext);
                if (!status) return status.GetStatus();
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1));
        }
        if (endpoint) {
            status = endpoint->WaitIdle();
            if (!status) return status.GetStatus();
        }
        const int result = exitCode;
        if (view) {
            status = view->Unmount();
            if (!status) return status.GetStatus();
        }
        DetachObjects();
        windowOwner.Reset();
        applicationOwner.Reset();
        view.Reset();
        endpoint.Reset();
        return result;
    }

    void DetachObjects() noexcept {
        if (application != nullptr) {
            application->Detach();
        }
        if (window != nullptr) {
            window->Detach();
        }
        application = nullptr;
        window = nullptr;
    }

    void RequestExit(int requestedExitCode) noexcept override {
        exitCode = requestedExitCode;
        exitRequested = true;
        if (nativeWindow) nativeWindow->Close();
    }

    Base::Result<void> Show() noexcept override {
        return nativeWindow
            ? nativeWindow->Show()
            : Base::Result<void>(
                  HostFailure(
                      Base::ErrorCode::InvalidState,
                      "Application native window is unavailable"));
    }

    void Close() noexcept override {
        RequestExit(0);
    }

    bool IsOpen() const noexcept override {
        return nativeWindow &&
            nativeWindow->IsOpen() &&
            !exitRequested;
    }

    Platform::IWindow* NativeWindow() noexcept override {
        return nativeWindow.get();
    }

    View* HostedView() noexcept override {
        return view.Get();
    }

    Base::IAllocator* allocator = nullptr;
    RuntimeEnvironment environment;
    Base::String applicationFile;
    Base::String assetRoot;
    Base::Result<void> optionsStatus;
    Base::ResourceUri startupUri;
    GraphicsBackend backend =
        GraphicsBackend::Automatic;
    std::uint32_t defaultWidth = 0U;
    std::uint32_t defaultHeight = 0U;
    bool visible = true;
    bool resizable = true;
    bool automaticAnimationClock = true;
    bool loadBuiltInTheme = true;
    BuiltInTheme builtInTheme =
        BuiltInTheme::Light;
    Core::IDiagnosticSink* diagnostics = nullptr;
    ApplicationStartupCallback startup = nullptr;
    void* startupContext = nullptr;
    ApplicationFrameCallback frame = nullptr;
    void* frameContext = nullptr;
    bool exitRequested = false;
    bool hasPendingResize = false;
    int exitCode = 0;
    std::uint32_t pendingResizeWidth = 0U;
    std::uint32_t pendingResizeHeight = 0U;

    Base::Ref<View> view;
    Base::Ref<Base::Object> applicationOwner;
    Base::Ref<Base::Object> windowOwner;
    Application* application = nullptr;
    Window* window = nullptr;
    std::unique_ptr<Platform::IWindow> nativeWindow;
    Base::Ref<Integration::RenderEndpoint> endpoint;
};

ApplicationHost::ApplicationHost(
    const ApplicationHostOptions& options,
    Base::IAllocator* allocator) noexcept
    : impl_(new (std::nothrow) Impl(
          options, allocator)) {}

ApplicationHost::~ApplicationHost() noexcept {
    delete impl_;
    impl_ = nullptr;
}

Base::Result<int> ApplicationHost::Run() noexcept {
    if (impl_ == nullptr) {
        return HostFailure(
            Base::ErrorCode::OutOfMemory,
            "Application host allocation failed");
    }
    return impl_->Run();
}

Base::Result<void> ApplicationHost::AddModule(
    const ModuleRegistration& registration) noexcept {
    if (impl_ == nullptr) {
        return HostFailure(
            Base::ErrorCode::OutOfMemory,
            "Application host allocation failed");
    }
    return impl_->environment.AddModule(registration);
}

void ApplicationHost::RequestExit(int exitCode) noexcept {
    if (impl_ != nullptr) {
        impl_->RequestExit(exitCode);
    }
}

Application*
ApplicationHost::CurrentApplication() const noexcept {
    return impl_ != nullptr
        ? impl_->application
        : nullptr;
}

Window* ApplicationHost::MainWindow() const noexcept {
    return impl_ != nullptr
        ? impl_->window
        : nullptr;
}

} // namespace Aero::App
