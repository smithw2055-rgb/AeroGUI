#include "DesktopHost.hpp"

#include <Aero/Application.hpp>
#include <Aero/Window.hpp>
#include "ApplicationState.hpp"
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Integration/OpenGL33.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Integration/ViewOptions.hpp>
#include <Aero/View.hpp>


#if defined(_WIN32)
#include <Aero/Integration/D3D11.hpp>
#include "platform/win32/InputRouters.hpp"
#include "platform/win32/Window.hpp"
#else
#include "platform/x11/Window.hpp"
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

Input::MouseButton MapButton(
    Platform::WindowPointerButton button) noexcept {
    switch (button) {
    case Platform::WindowPointerButton::Right:
        return Input::MouseButton::Right;
    case Platform::WindowPointerButton::Middle:
        return Input::MouseButton::Middle;
    case Platform::WindowPointerButton::XButton1:
        return Input::MouseButton::XButton1;
    case Platform::WindowPointerButton::XButton2:
        return Input::MouseButton::XButton2;
    case Platform::WindowPointerButton::Unknown:
    case Platform::WindowPointerButton::Left:
    default:
        return Input::MouseButton::Left;
    }
}

std::uint32_t WindowExtent(
    double value,
    std::uint32_t fallback) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return fallback;
    }
    if (value >= static_cast<double>(UINT32_MAX)) {
        return UINT32_MAX;
    }
    return static_cast<std::uint32_t>(value);
}

} // namespace

struct Detail::DesktopHost::Impl final {
    struct WindowHost final {
        explicit WindowHost(Impl& applicationHost) noexcept
            : owner(&applicationHost),
              runtime{
                  this,
                  &WindowHost::ShowThunk,
                  &WindowHost::CloseThunk,
                  &WindowHost::IsOpenThunk,
                  &WindowHost::NativeHandleThunk,
                  &WindowHost::HostedViewThunk} {}

        ~WindowHost() noexcept {
            Shutdown();
        }

        WindowHost(const WindowHost&) = delete;
        WindowHost& operator=(const WindowHost&) = delete;

        Base::Result<void> CreateView() noexcept {
            Integration::ViewOptions options;
            options.text.fontSearchRoot = owner->assetRoot.View();
#if defined(_WIN32)
            options.clipboard = &clipboard;
            options.textInputMethodHost = &inputMethod;
#endif
            Base::Result<Base::Ref<View>> created =
                owner->environment.CreateView(options, owner->allocator);
            if (!created) return created.GetStatus();
            view = std::move(created).Value();
            Markup::XamlReader reader(*view);
            if (owner->loadBuiltInTheme) {
                Base::Result<void> themed = reader.LoadTheme(
                    owner->builtInTheme);
                if (!themed) return themed.GetStatus();
            }
            Base::Ref<ResourceDictionary> resources =
                owner->application != nullptr
                ? owner->application->GetResources()
                : Base::Ref<ResourceDictionary>{};
            if (resources) {
                Base::Result<void> installed =
                    reader.SetResources(
                        ResourceLayer::Application,
                        *resources,
                        ResourceLoadMode::Replace);
                if (!installed) return installed.GetStatus();
            }
            return {};
        }

        Base::Result<void> LoadFromUri(
            const Base::ResourceUri& uri) noexcept {
            Base::Result<void> created = CreateView();
            if (!created) return created.GetStatus();
            Markup::XamlReader reader(*view);
            Base::Result<UiDocument> loaded = reader.Load(
                uri.Canonical(), owner->diagnostics);
            if (!loaded) return loaded.GetStatus();
            const Base::Ref<Base::Object>& root = loaded.Value().Root();
            if (!root ||
                !view->IsInstanceOf(*root, Window::StaticTypeId())) {
                return HostFailure(
                    Base::ErrorCode::InvalidArgument,
                    "StartupUri XAML root must be Window");
            }
            windowOwner = root;
            window = static_cast<Window*>(windowOwner.Get());
            loadedDocument = std::move(loaded).Value();
            return FinishInitialization(false);
        }

        Base::Result<void> LoadProgrammatic(
            Base::Ref<Window> value) noexcept {
            if (!value) {
                return HostFailure(
                    Base::ErrorCode::InvalidArgument,
                    "Application window must not be null");
            }
            Base::Result<void> created = CreateView();
            if (!created) return created.GetStatus();
            windowOwner = Base::Ref<Base::Object>(value);
            window = value.Get();
            return FinishInitialization(true);
        }

        Base::Result<void> FinishInitialization(
            bool programmaticRoot) noexcept {
            if (window == nullptr || !view) {
                return HostFailure(
                    Base::ErrorCode::InvalidState,
                    "Window host is missing its Window or View");
            }
            std::uint32_t width = WindowExtent(
                window->GetWidth(), owner->defaultWidth);
            std::uint32_t height = WindowExtent(
                window->GetHeight(), owner->defaultHeight);
            Base::Result<void> native = CreateNativeWindow(width, height);
            if (!native) return native.GetStatus();
            width = nativeWindow->ClientWidth();
            height = nativeWindow->ClientHeight();
            if (width == 0U || height == 0U) {
                return HostFailure(
                    Base::ErrorCode::InvalidState,
                    "Application native window has an empty client area");
            }
            Base::Result<void> graphics = CreateRenderDevice(width, height);
            if (!graphics) return graphics.GetStatus();
            Base::Result<void> attached = view->SetRenderDevice(
                renderDevice, owner->automaticAnimationClock);
            if (!attached) return attached.GetStatus();
            const Size size{
                static_cast<double>(width),
                static_cast<double>(height)};
            Base::Result<void> mounted = programmaticRoot
                ? view->SetContent(
                      Base::Ref<FrameworkElement>::FromBorrowed(*window),
                      size)
                : view->SetContent(std::move(loadedDocument), size);
            if (!mounted) return mounted.GetStatus();
            Detail::DesktopPrivate::Attach(*window, &runtime);
            Detail::DesktopPrivate::NotifySourceInitialized(*window);
            return {};
        }

        Base::Result<void> CreateNativeWindow(
            std::uint32_t width,
            std::uint32_t height) noexcept {
#if defined(_WIN32)
            nativeWindow.reset(new (std::nothrow)
                Platform::Win32Window(owner->allocator));
#else
            nativeWindow.reset(new (std::nothrow)
                Platform::X11Window(owner->allocator));
#endif
            if (!nativeWindow) {
                return HostFailure(
                    Base::ErrorCode::OutOfMemory,
                    "Application native window allocation failed");
            }
            Platform::WindowDescriptor descriptor;
            descriptor.title = window->GetTitle().Empty()
                ? Base::StringView("AeroGUI")
                : window->GetTitle();
            descriptor.width = width;
            descriptor.height = height;
            descriptor.visible = false;
            descriptor.resizable = owner->resizable;
            Base::Result<void> created =
                nativeWindow->Create(descriptor);
            if (!created) return created.GetStatus();
#if defined(_WIN32)
            const Integration::NativeWindowHandle handle =
                nativeWindow->NativeHandle();
            void* nativeHandle = reinterpret_cast<void*>(handle.window);
            clipboard.SetOwnerWindow(nativeHandle);
            Base::Result<void> inputAttached =
                inputMethod.Attach(nativeHandle);
            if (!inputAttached) {
                nativeWindow->Close();
                return inputAttached.GetStatus();
            }
#endif
            return {};
        }

        Base::Result<void> CreateRenderDevice(
            std::uint32_t width,
            std::uint32_t height) noexcept {
#if !AERO_APP_HAS_D3D11 && !AERO_APP_HAS_OPENGL_WINDOW
            static_cast<void>(width);
            static_cast<void>(height);
#endif
            GraphicsBackend selected = owner->backend;
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
                Integration::D3D11WindowDeviceOptions options;
                options.window = nativeWindow->NativeHandle();
                options.width = width;
                options.height = height;
                options.presentMode = Integration::RenderPresentMode::Fifo;
                options.allowWarpFallback = true;
                Base::Result<Base::Ref<Integration::RenderDevice>> created =
                    Integration::CreateD3D11WindowDevice(
                        options, owner->allocator);
                if (!created) return created.GetStatus();
                renderDevice = std::move(created).Value();
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
                Integration::OpenGL33WindowDeviceOptions options;
                options.window = nativeWindow->NativeHandle();
                options.width = width;
                options.height = height;
                options.presentMode = Integration::RenderPresentMode::Fifo;
                Base::Result<Base::Ref<Integration::RenderDevice>> created =
                    Integration::CreateOpenGL33WindowDevice(
                        options, owner->allocator);
                if (!created) return created.GetStatus();
                renderDevice = std::move(created).Value();
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
                if (window != nullptr) window->Close();
                else Close();
                return {};
            case Platform::WindowEventType::Closed:
                closeRequested = true;
                if (window != nullptr) {
                    Detail::DesktopPrivate::NotifyClosed(*window);
                }
                return {};
            case Platform::WindowEventType::Resized:
            case Platform::WindowEventType::ScaleChanged:
                if (event.width != 0U && event.height != 0U) {
                    pendingResizeWidth = event.width;
                    pendingResizeHeight = event.height;
                    hasPendingResize = true;
                }
                return {};
            case Platform::WindowEventType::PointerMove:
            case Platform::WindowEventType::PointerDown:
            case Platform::WindowEventType::PointerUp:
            case Platform::WindowEventType::PointerWheel: {
                Base::Result<void> resized = ApplyPendingResize();
                if (!resized) return resized.GetStatus();
                Input::PointerInput input;
                input.pointerId = 1U;
                input.position = {event.x, event.y};
                input.changedButton = MapButton(event.button);
                input.wheelDeltaX = event.wheelDeltaX / 120.0;
                input.wheelDeltaY = event.wheelDeltaY / 120.0;
                if (event.type == Platform::WindowEventType::PointerDown) {
                    input.action = Input::PointerAction::Down;
                } else if (event.type == Platform::WindowEventType::PointerUp) {
                    input.action = Input::PointerAction::Up;
                } else if (event.type == Platform::WindowEventType::PointerWheel) {
                    input.action = Input::PointerAction::Wheel;
                } else {
                    input.action = Input::PointerAction::Move;
                }
                Base::Result<Input::PointerDispatchResult> dispatched =
                    view->DispatchPointer(input);
                return dispatched
                    ? Base::Result<void>()
                    : Base::Result<void>(dispatched.GetStatus());
            }
            case Platform::WindowEventType::KeyDown:
            case Platform::WindowEventType::KeyUp: {
                if (event.key == 0U) return {};
                Input::KeyboardInput input;
                input.action = event.type == Platform::WindowEventType::KeyDown
                    ? Input::KeyboardAction::Down
                    : Input::KeyboardAction::Up;
                input.key = event.key;
                input.modifiers = event.modifiers;
                input.isRepeat = event.repeat;
                Base::Result<Input::KeyboardDispatchResult> dispatched =
                    view->DispatchKeyboard(input);
                return dispatched
                    ? Base::Result<void>()
                    : Base::Result<void>(dispatched.GetStatus());
            }
            case Platform::WindowEventType::TextInput: {
                if (event.textSize == 0U) return {};
                Base::Result<Input::TextInputDispatchResult> dispatched =
                    view->DispatchText({event.Text()});
                return dispatched
                    ? Base::Result<void>()
                    : Base::Result<void>(dispatched.GetStatus());
            }
            case Platform::WindowEventType::Exposed:
            case Platform::WindowEventType::Invalid:
            default:
                return {};
            }
        }

        Base::Result<void> ApplyPendingResize() noexcept {
            if (!hasPendingResize) return {};
            const std::uint32_t width = pendingResizeWidth;
            const std::uint32_t height = pendingResizeHeight;
            hasPendingResize = false;
            Base::Result<void> resized = renderDevice->Resize(width, height);
            if (!resized) return resized.GetStatus();
            return view->SetSize({
                static_cast<double>(width),
                static_cast<double>(height)});
        }

        Base::Result<bool> PumpEvents() noexcept {
            if (!nativeWindow || !nativeWindow->IsOpen() || closeRequested) {
                return false;
            }
            bool handled = false;
            for (;;) {
                Platform::WindowEvent event;
                Base::Result<bool> received = nativeWindow->PollEvent(event);
                if (!received) return received.GetStatus();
                if (!received.Value()) break;
                handled = true;
                Base::Result<void> status = HandleEvent(event);
                if (!status) return status.GetStatus();
                if (closeRequested) break;
            }
            Base::Result<void> resized = ApplyPendingResize();
            if (!resized) return resized.GetStatus();
            return handled;
        }

        Base::Result<void> RenderFrame() noexcept {
            if (closeRequested || !view || !nativeWindow ||
                !nativeWindow->IsOpen()) {
                return {};
            }
            const auto now = std::chrono::steady_clock::now();
            std::uint32_t elapsedMilliseconds = 0U;
            if (updateClockInitialized) {
                const auto elapsed = std::chrono::duration_cast<
                    std::chrono::milliseconds>(now - lastUpdate);
                const auto clamped = elapsed.count() < 0
                    ? 0LL
                    : (elapsed.count() > 1000 ? 1000LL : elapsed.count());
                elapsedMilliseconds = static_cast<std::uint32_t>(clamped);
            }
            lastUpdate = now;
            updateClockInitialized = true;
            Base::Result<void> frame =
                view->Update(elapsedMilliseconds);
            if (!frame) return frame.GetStatus();
            firstFrameRendered = true;
            return {};
        }

        Base::Result<void> Show() noexcept {
            if (closeRequested || !nativeWindow) {
                return HostFailure(
                    Base::ErrorCode::InvalidState,
                    "Window native host is unavailable");
            }
            if (!firstFrameRendered) {
                Base::Result<void> rendered = RenderFrame();
                if (!rendered) return rendered.GetStatus();
            }
            Base::Result<void> shown = nativeWindow->Show();
            if (shown && window != nullptr) {
                Detail::DesktopPrivate::NotifyContentRendered(*window);
            }
            return shown;
        }

        void Close() noexcept {
            closeRequested = true;
            if (nativeWindow != nullptr && nativeWindow->IsOpen()) {
                nativeWindow->Close();
            }
        }

        bool IsOpen() const noexcept {
            return !closeRequested && nativeWindow && nativeWindow->IsOpen();
        }

        Integration::NativeWindowHandle NativeHandle() const noexcept {
            return nativeWindow
                ? nativeWindow->NativeHandle()
                : Integration::NativeWindowHandle{};
        }

        View* HostedView() noexcept { return view.Get(); }

        void Shutdown() noexcept {
            if (shutdown) return;
            shutdown = true;
            if (renderDevice) static_cast<void>(renderDevice->WaitIdle());
            if (view) static_cast<void>(view->Unmount());
            if (window != nullptr) {
                Detail::DesktopPrivate::NotifyClosed(*window);
                Detail::DesktopPrivate::Detach(*window);
            }
#if defined(_WIN32)
            static_cast<void>(inputMethod.Detach());
#endif
            if (nativeWindow) nativeWindow->Close();
            loadedDocument = {};
            renderDevice.Reset();
            view.Reset();
            windowOwner.Reset();
            window = nullptr;
            nativeWindow.reset();
        }

        static Base::Result<void> ShowThunk(void* context) noexcept {
            return static_cast<WindowHost*>(context)->Show();
        }
        static void CloseThunk(void* context) noexcept {
            static_cast<WindowHost*>(context)->Close();
        }
        static bool IsOpenThunk(const void* context) noexcept {
            return static_cast<const WindowHost*>(context)->IsOpen();
        }
        static Integration::NativeWindowHandle NativeHandleThunk(
            const void* context) noexcept {
            return static_cast<const WindowHost*>(context)->NativeHandle();
        }
        static View* HostedViewThunk(void* context) noexcept {
            return static_cast<WindowHost*>(context)->HostedView();
        }

        Impl* owner = nullptr;
        Detail::WindowHostState runtime;
        Base::Ref<View> view;
        Base::Ref<Integration::RenderDevice> renderDevice;
        Base::Ref<Base::Object> windowOwner;
        UiDocument loadedDocument;
        Window* window = nullptr;
#if defined(_WIN32)
        Platform::Win32Clipboard clipboard;
        Platform::Win32ImeAdapter inputMethod;
#endif
        std::unique_ptr<Platform::IWindow> nativeWindow;
        bool closeRequested = false;
        bool hasPendingResize = false;
        bool firstFrameRendered = false;
        bool updateClockInitialized = false;
        std::chrono::steady_clock::time_point lastUpdate;
        bool shutdown = false;
        std::uint32_t pendingResizeWidth = 0U;
        std::uint32_t pendingResizeHeight = 0U;
    };

    Impl(
        const RunOptions& source,
        Application* providedApplication,
        Base::Ref<Window> providedWindow) noexcept
        : applicationRuntime{
              this,
              &Impl::RequestExitThunk,
              &Impl::ShowWindowThunk,
              &Impl::WindowCountThunk,
              &Impl::WindowAtThunk,
              &Impl::SetMainWindowThunk},
          allocator(source.allocator != nullptr
              ? source.allocator
              : &Base::GetDefaultAllocator()),
          environment(allocator),
          windows(allocator),
          backend(source.graphicsBackend),
          defaultWidth(source.defaultWidth),
          defaultHeight(source.defaultHeight),
          visible(source.visible),
          resizable(source.resizable),
          automaticAnimationClock(source.automaticAnimationClock),
          loadBuiltInTheme(source.loadBuiltInTheme),
          builtInTheme(source.builtInTheme),
          modules(source.modules),
          diagnostics(source.diagnostics),
          suppliedApplication(providedApplication),
          suppliedWindow(std::move(providedWindow)) {
        optionsStatus = applicationFile.TryAssign(source.applicationFile);
        if (optionsStatus) {
            const Base::StringView path = applicationFile.View();
            std::uint32_t separator = path.SizeBytes();
            for (std::uint32_t index = 0U; index < path.SizeBytes(); ++index) {
                if (path[index] == '/' || path[index] == '\\') {
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
        ShutdownWindows();
        if (application != nullptr) {
            Detail::DesktopPrivate::Detach(*application);
        }
        application = nullptr;
        applicationOwner.Reset();
        loaderView.Reset();
    }

    Base::Result<void> CreateRuntime() noexcept {
        if (!optionsStatus) return optionsStatus.GetStatus();
        for (const ModuleRegistration& module : modules) {
            Base::Result<void> added = environment.AddModule(module);
            if (!added) return added.GetStatus();
        }
        return environment.Initialize();
    }

    Base::Result<Base::Ref<View>> CreateLoaderView() noexcept {
        Integration::ViewOptions options;
        options.text.fontSearchRoot = assetRoot.View();
        return environment.CreateView(options, allocator);
    }

    Base::Result<void> LoadApplication() noexcept {
        if (suppliedApplication != nullptr) {
            application = suppliedApplication;
            if (!application->GetStartupUri().Empty()) {
                Base::Result<Base::ResourceUri> baseUri =
                    Base::ResourceUri::Parse(applicationFile.View());
                if (!baseUri) return baseUri.GetStatus();
                Base::Result<Base::ResourceUri> resolved =
                    Base::ResourceUri::Resolve(
                        baseUri.Value(), application->GetStartupUri());
                if (!resolved) return resolved.GetStatus();
                startupUri = std::move(resolved).Value();
            }
            return {};
        }

        Base::Result<Base::Ref<View>> created = CreateLoaderView();
        if (!created) return created.GetStatus();
        loaderView = std::move(created).Value();
        Markup::XamlReader reader(*loaderView);
        Base::Result<UiDocument> loaded = reader.Load(
            applicationFile.View(), diagnostics);
        if (!loaded) return loaded.GetStatus();
        const Base::Ref<Base::Object>& root = loaded.Value().Root();
        if (!root ||
            !loaderView->IsInstanceOf(
                *root, Application::StaticTypeId())) {
            return HostFailure(
                Base::ErrorCode::InvalidArgument,
                "Application XAML root must be Application");
        }
        applicationOwner = root;
        application = static_cast<Application*>(applicationOwner.Get());
        if (!application->GetStartupUri().Empty()) {
            Base::Result<Base::ResourceUri> resolved =
                Base::ResourceUri::Resolve(
                    loaded.Value().CanonicalUri(),
                    application->GetStartupUri());
            if (!resolved) return resolved.GetStatus();
            startupUri = std::move(resolved).Value();
        }
        // Retain the loader View for the Application object's XAML lifetime.
        // Resource dictionaries, deferred content and markup effects may still
        // refer to the loader-owned runtime state until application shutdown.
        return {};
    }

    Base::Result<WindowHost*> CreateWindowHost() noexcept {
        auto* host = new (std::nothrow) WindowHost(*this);
        if (host == nullptr) {
            return HostFailure(
                Base::ErrorCode::OutOfMemory,
                "Unable to allocate application Window host");
        }
        Base::Result<void> appended = windows.TryPushBack(host);
        if (!appended) {
            delete host;
            return appended.GetStatus();
        }
        return host;
    }

    void RemoveWindowAt(std::uint32_t index) noexcept {
        WindowHost* host = windows[index];
        for (std::uint32_t move = index + 1U;
             move < windows.Size(); ++move) {
            windows[move - 1U] = windows[move];
        }
        windows.PopBack();
        delete host;
    }

    WindowHost* FindWindow(const Window& target) const noexcept {
        for (WindowHost* host : windows) {
            if (host != nullptr && host->window == &target) return host;
        }
        return nullptr;
    }

    Base::Result<void> StartApplication() noexcept {
        Detail::DesktopPrivate::Attach(
            *application, &applicationRuntime, nullptr);

        if (suppliedWindow || !startupUri.Empty()) {
            Base::Result<WindowHost*> allocated = CreateWindowHost();
            if (!allocated) return allocated.GetStatus();
            WindowHost* host = allocated.Value();
            Base::Result<void> loaded = suppliedWindow
                ? host->LoadProgrammatic(suppliedWindow)
                : host->LoadFromUri(startupUri);
            if (!loaded) {
                RemoveWindowAt(windows.Size() - 1U);
                return loaded.GetStatus();
            }
            mainWindow = host->window;
            if (application->GetMainWindow() == nullptr) {
                application->SetMainWindow(mainWindow);
            }
        }

        // WPF allows Application.Run() without StartupUri. In that form the
        // application creates and shows one or more windows from OnStartup().
        Detail::DesktopPrivate::RaiseStartup(*application);
        if (application->GetMainWindow() == nullptr && !windows.Empty()) {
            application->SetMainWindow(windows[0]->window);
        }
        return {};
    }

    Base::Result<void> ShowWindow(Window& value) noexcept {
        WindowHost* existing = FindWindow(value);
        if (existing != nullptr) return {};
        Base::Ref<Window> owner = Base::Ref<Window>::TryFromBorrowed(value);
        if (!owner) {
            return HostFailure(
                Base::ErrorCode::InvalidArgument,
                "Window.Show requires a Window created with Base::MakeRef");
        }
        Base::Result<WindowHost*> allocated = CreateWindowHost();
        if (!allocated) return allocated.GetStatus();
        WindowHost* host = allocated.Value();
        Base::Result<void> loaded = host->LoadProgrammatic(std::move(owner));
        if (!loaded) {
            RemoveWindowAt(windows.Size() - 1U);
            return loaded.GetStatus();
        }
        if (application->GetMainWindow() == nullptr) {
            application->SetMainWindow(host->window);
        }
        Base::Result<void> rendered = host->RenderFrame();
        if (!rendered) {
            RemoveWindowAt(windows.Size() - 1U);
            return rendered.GetStatus();
        }
        return {};
    }

    std::uint32_t WindowCount() const noexcept {
        return windows.Size();
    }

    Window* WindowAt(std::uint32_t index) const noexcept {
        return index < windows.Size() && windows[index] != nullptr
            ? windows[index]->window
            : nullptr;
    }

    void SetMainWindow(Window* value) noexcept {
        mainWindow = value;
    }

    Base::Result<void> RemoveClosedWindows() noexcept {
        for (std::uint32_t index = 0U; index < windows.Size();) {
            WindowHost* host = windows[index];
            if (host != nullptr && host->IsOpen()) {
                ++index;
                continue;
            }
            Window* closingWindow = host != nullptr ? host->window : nullptr;
            const bool mainClosed = closingWindow != nullptr &&
                closingWindow == application->GetMainWindow();
            if (host != nullptr && closingWindow != nullptr) {
                Detail::DesktopPrivate::NotifyClosed(*closingWindow);
            }
            RemoveWindowAt(index);
            const ShutdownMode mode = application->GetShutdownMode();
            if ((mode == ShutdownMode::OnMainWindowClose && mainClosed) ||
                (mode == ShutdownMode::OnLastWindowClose && windows.Empty())) {
                RequestExit(0);
                return {};
            }
        }
        return {};
    }

    Base::Result<int> Run() noexcept {
        Base::Result<void> status = CreateRuntime();
        if (status) status = LoadApplication();
        if (status) status = StartApplication();
        if (!status) return status.GetStatus();

        for (WindowHost* host : windows) {
            status = host->RenderFrame();
            if (!status) return status.GetStatus();
        }
        if (visible && !windows.Empty()) {
            status = windows[0]->Show();
            if (!status) return status.GetStatus();
        }

        while (!exitRequested) {
            bool handledEvent = false;
            for (std::uint32_t index = 0U;
                 index < windows.Size() && !exitRequested;
                 ++index) {
                WindowHost* host = windows[index];
                if (host == nullptr) continue;
                Base::Result<bool> pumped = host->PumpEvents();
                if (!pumped) return pumped.GetStatus();
                handledEvent = handledEvent || pumped.Value();
            }
            status = RemoveClosedWindows();
            if (!status) return status.GetStatus();
            if (exitRequested) break;
            for (WindowHost* host : windows) {
                status = host->RenderFrame();
                if (!status) return status.GetStatus();
            }
            if (!handledEvent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        const int result = exitCode;
        ShutdownWindows();
        if (application != nullptr) {
            Detail::DesktopPrivate::RaiseExit(*application, result);
            Detail::DesktopPrivate::Detach(*application);
            application = nullptr;
        }
        applicationOwner.Reset();
        return result;
    }

    void ShutdownWindows() noexcept {
        while (!windows.Empty()) {
            RemoveWindowAt(windows.Size() - 1U);
        }
    }

    void RequestExit(int requestedExitCode) noexcept {
        exitCode = requestedExitCode;
        exitRequested = true;
        for (WindowHost* host : windows) {
            if (host != nullptr) host->Close();
        }
    }

    static void RequestExitThunk(
        void* context, int exitCode) noexcept {
        static_cast<Impl*>(context)->RequestExit(exitCode);
    }
    static Base::Result<void> ShowWindowThunk(
        void* context, Window& window) noexcept {
        return static_cast<Impl*>(context)->ShowWindow(window);
    }
    static std::uint32_t WindowCountThunk(
        const void* context) noexcept {
        return static_cast<const Impl*>(context)->WindowCount();
    }
    static Window* WindowAtThunk(
        const void* context, std::uint32_t index) noexcept {
        return static_cast<const Impl*>(context)->WindowAt(index);
    }
    static void SetMainWindowThunk(
        void* context, Window* window) noexcept {
        static_cast<Impl*>(context)->SetMainWindow(window);
    }

    Detail::ApplicationHostState applicationRuntime;
    Base::IAllocator* allocator = nullptr;
    Gui environment;
    Base::Vector<WindowHost*> windows;
    Base::String applicationFile;
    Base::String assetRoot;
    Base::Result<void> optionsStatus;
    Base::ResourceUri startupUri;
    GraphicsBackend backend = GraphicsBackend::Automatic;
    std::uint32_t defaultWidth = 0U;
    std::uint32_t defaultHeight = 0U;
    bool visible = true;
    bool resizable = true;
    bool automaticAnimationClock = true;
    bool loadBuiltInTheme = true;
    BuiltInTheme builtInTheme = BuiltInTheme::Light;
    Base::Span<const ModuleRegistration> modules;
    Core::IDiagnosticSink* diagnostics = nullptr;
    Application* suppliedApplication = nullptr;
    Base::Ref<Window> suppliedWindow;
    Base::Ref<View> loaderView;
    Base::Ref<Base::Object> applicationOwner;
    Application* application = nullptr;
    Window* mainWindow = nullptr;
    bool exitRequested = false;
    int exitCode = 0;
};

Detail::DesktopHost::DesktopHost(
    const RunOptions& options) noexcept
    : impl_(new (std::nothrow) Impl(options, nullptr, {})) {}

Detail::DesktopHost::DesktopHost(
    Application& application,
    Base::Ref<Window> window,
    const RunOptions& options) noexcept
    : impl_(new (std::nothrow) Impl(
          options, &application, std::move(window))) {}

Detail::DesktopHost::~DesktopHost() noexcept {
    delete impl_;
    impl_ = nullptr;
}

Base::Result<int> Detail::DesktopHost::Run() noexcept {
    if (impl_ == nullptr) {
        return HostFailure(
            Base::ErrorCode::OutOfMemory,
            "Application host allocation failed");
    }
    return impl_->Run();
}

void Detail::DesktopPrivate::Attach(
    Application& application,
    void* hostState,
    Window* mainWindow) noexcept {
    application.Attach(hostState, mainWindow);
}

void Detail::DesktopPrivate::Detach(
    Application& application) noexcept {
    application.Detach();
}

void Detail::DesktopPrivate::RaiseStartup(
    Application& application) noexcept {
    application.RaiseStartup();
}

void Detail::DesktopPrivate::RaiseExit(
    Application& application,
    int exitCode) noexcept {
    application.RaiseExit(exitCode);
}

void Detail::DesktopPrivate::Attach(
    Window& window,
    void* hostState) noexcept {
    window.Attach(hostState);
}

void Detail::DesktopPrivate::Detach(Window& window) noexcept {
    window.Detach();
}

void Detail::DesktopPrivate::NotifySourceInitialized(
    Window& window) noexcept {
    window.NotifySourceInitialized();
}

void Detail::DesktopPrivate::NotifyContentRendered(
    Window& window) noexcept {
    window.NotifyContentRendered();
}

void Detail::DesktopPrivate::NotifyClosed(
    Window& window) noexcept {
    window.NotifyClosed();
}

int Run(const RunOptions& options) noexcept {
    Detail::DesktopHost host(options);
    Base::Result<int> result = host.Run();
    return result ? result.Value() : -1;
}

} // namespace Aero::App
