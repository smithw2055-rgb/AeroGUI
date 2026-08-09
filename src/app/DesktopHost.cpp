#include "DesktopHost.hpp"
#include "Metadata.hpp"
#include "RenderContext.hpp"

#include <AeroApp/Application.hpp>
#include <AeroApp/Window.hpp>
#include "ApplicationState.hpp"
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/View.hpp>

#if defined(_WIN32)
#include "app/platform/win32/InputRouters.hpp"
#include "app/platform/win32/Window.hpp"
#else
#include "app/platform/x11/Window.hpp"
#endif

#include <chrono>
#include <cmath>
#include <memory>
#include <new>
#include <utility>

namespace Aero::App {

using namespace ::Aero::App;
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

std::uint32_t DecodeWindowCodePoint(
    Base::StringView text) noexcept {
    if (text.Empty()) return 0U;
    const auto lead = static_cast<std::uint8_t>(text[0]);
    std::uint32_t value = 0U;
    std::uint32_t count = 1U;
    if (lead < 0x80U) return lead;
    if ((lead & 0xE0U) == 0xC0U) {
        value = lead & 0x1FU;
        count = 2U;
    } else if ((lead & 0xF0U) == 0xE0U) {
        value = lead & 0x0FU;
        count = 3U;
    } else if ((lead & 0xF8U) == 0xF0U) {
        value = lead & 0x07U;
        count = 4U;
    } else {
        return 0xFFFDU;
    }
    if (text.SizeBytes() < count) return 0xFFFDU;
    for (std::uint32_t index = 1U; index < count; ++index) {
        const auto byte = static_cast<std::uint8_t>(text[index]);
        if ((byte & 0xC0U) != 0x80U) return 0xFFFDU;
        value = (value << 6U) | (byte & 0x3FU);
    }
    return value;
}

} // namespace

struct DesktopHostState {
    struct WindowHost {
        explicit WindowHost(DesktopHostState& applicationHost) noexcept
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
            ViewOptions options;
            options.text.fontSearchRoot = owner->assetRoot.View();
            options.automaticAnimationClock =
                owner->automaticAnimationClock;
            options.loadBuiltInTheme = owner->loadBuiltInTheme;
            options.builtInTheme = owner->builtInTheme;
            options.applicationResources = owner->application != nullptr
                ? &owner->application->GetResources()
                : nullptr;
            options.diagnostics = owner->diagnostics;
#if defined(_WIN32)
            options.clipboard = &clipboard;
            options.textInputMethodHost = &inputMethod;
#endif
            Base::Result<Base::Ref<View>> created =
                owner->environment.CreateView(options, owner->allocator);
            if (!created) return created.GetStatus();
            view = std::move(created).Value();
            return {};
        }

        Base::Result<void> LoadFromUri(
            const Base::ResourceUri& uri) noexcept {
            Base::Result<void> created = CreateView();
            if (!created) return created.GetStatus();
            Markup::XamlReader reader(owner->environment);
            Base::Result<Markup::XamlDocument> loaded =
                reader.LoadComponent<Window>(
                    uri.Canonical(), {}, owner->diagnostics);
            if (!loaded) return loaded.GetStatus();
            const Base::Ref<Base::Object>& root = loaded.Value().Root();
            if (!root) {
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
            if (DesktopHost::WindowComponentRequested(*value)) {
                Base::String componentPath;
                const Base::StringView authored =
                    DesktopHost::WindowComponentUri(*value);
                Base::Result<void> assigned;
                if (!authored.Empty()) {
                    const bool absolute = authored.SizeBytes() > 2U &&
                        (authored[1] == ':' || authored[0] == '/' ||
                         authored[0] == '\\');
                    if (!absolute) {
                        assigned = componentPath.Assign(
                            owner->assetRoot.View());
                        if (assigned && !componentPath.Empty()) {
                            assigned = componentPath.Append("/");
                        }
                    }
                    if (assigned) {
                        assigned = componentPath.Append(authored);
                    }
                } else {
                    const Meta::RuntimeTypeInfo type =
                        Meta::ResolveRuntimeTypeInfo(
                            value->RuntimeType());
                    if (type.id == Meta::InvalidTypeId ||
                        type.name.Empty()) {
                        return HostFailure(
                            Base::ErrorCode::NotFound,
                            "InitializeComponent requires a registered Window type");
                    }
                    assigned = componentPath.Assign(
                        owner->assetRoot.View());
                    if (assigned && !componentPath.Empty()) {
                        assigned = componentPath.Append("/");
                    }
                    if (assigned) {
                        assigned = componentPath.Append(type.name);
                    }
                    if (assigned) {
                        assigned = componentPath.Append(".xaml");
                    }
                }
                if (!assigned) return assigned.GetStatus();
                Markup::XamlReader reader(owner->environment);
                Base::Ref<Base::Object> existingRoot(value);
                Base::Result<Markup::XamlDocument> loaded =
                    reader.LoadComponentInto(
                        existingRoot,
                        componentPath.View(),
                        {},
                        owner->diagnostics);
                if (!loaded) return loaded.GetStatus();
                const Base::Ref<Base::Object>& root = loaded.Value().Root();
                if (!root || root.Get() != value.Get()) {
                    return HostFailure(
                        Base::ErrorCode::InvalidArgument,
                        "InitializeComponent must populate the existing Window");
                }
                windowOwner = std::move(existingRoot);
                window = value.Get();
                loadedDocument = std::move(loaded).Value();
                return FinishInitialization(false);
            }
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
            Base::Result<RenderContext*> graphics = CreateRenderContext(
                owner->backend,
                nativeWindow->NativeHandle(),
                width,
                height,
                owner->allocator);
            if (!graphics) return graphics.GetStatus();
            renderContext.reset(graphics.Value());
            Base::Ref<RenderDevice> renderDevice = renderContext->Device();
            if (!renderDevice) {
                return HostFailure(
                    Base::ErrorCode::InvalidState,
                    "Application render context has no render device");
            }
            Base::Result<void> renderer =
                view->GetRenderer().Init(std::move(renderDevice));
            if (!renderer) return renderer.GetStatus();
            dpiScale = nativeWindow->DpiScale();
            if (!std::isfinite(dpiScale) || dpiScale <= 0.0) {
                dpiScale = 1.0;
            }
            pendingDpiScale = dpiScale;
            const Size size{
                static_cast<double>(width) / dpiScale,
                static_cast<double>(height) / dpiScale};
            view->SetScale(dpiScale);
            view->SetSize(size);
            if (programmaticRoot) {
                view->SetContent(
                    Base::Ref<FrameworkElement>::FromBorrowed(*window),
                    size);
            } else {
                view->SetContent(std::move(loadedDocument), size);
            }
            DesktopHost::AttachWindow(*window, &runtime);
            DesktopHost::NotifyWindowSourceInitialized(*window);
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
            const Platform::NativeWindowHandle handle =
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
                    DesktopHost::NotifyWindowClosed(*window);
                }
                return {};
            case Platform::WindowEventType::Resized:
            case Platform::WindowEventType::ScaleChanged:
                frameRequested = true;
                pendingResizeWidth = event.width;
                pendingResizeHeight = event.height;
                if (std::isfinite(event.dpiScale) &&
                    event.dpiScale > 0.0) {
                    pendingDpiScale = event.dpiScale;
                }
                hasPendingResize = true;
                return {};
            case Platform::WindowEventType::PointerMove:
            case Platform::WindowEventType::PointerDown:
            case Platform::WindowEventType::PointerUp:
            case Platform::WindowEventType::PointerWheel: {
                Base::Result<void> resized = ApplyPendingResize();
                if (!resized) return resized.GetStatus();
                const int x = static_cast<int>(event.x / dpiScale);
                const int y = static_cast<int>(event.y / dpiScale);
                bool handled = false;
                if (event.type == Platform::WindowEventType::PointerDown) {
                    handled = view->MouseButtonDown(
                        x, y, MapButton(event.button));
                } else if (event.type == Platform::WindowEventType::PointerUp) {
                    handled = view->MouseButtonUp(
                        x, y, MapButton(event.button));
                } else if (event.type == Platform::WindowEventType::PointerWheel) {
                    handled = view->MouseWheel(
                        x, y, static_cast<int>(event.wheelDeltaY / 120.0));
                } else {
                    handled = view->MouseMove(x, y);
                }
                (void)handled;
                frameRequested = true;
                return {};
            }
            case Platform::WindowEventType::KeyDown:
            case Platform::WindowEventType::KeyUp: {
                if (event.key == 0U) return {};
                const Input::Key key = static_cast<Input::Key>(event.key);
                const bool handled =
                    event.type == Platform::WindowEventType::KeyDown
                    ? view->KeyDown(key)
                    : view->KeyUp(key);
                (void)handled;
                frameRequested = true;
                return {};
            }
            case Platform::WindowEventType::TextInput: {
                if (event.textSize == 0U) return {};
                const bool handled = view->Char(
                    DecodeWindowCodePoint(event.Text()));
                (void)handled;
                frameRequested = true;
                return {};
            }
            case Platform::WindowEventType::Exposed:
                frameRequested = true;
                return {};
            case Platform::WindowEventType::Invalid:
            default:
                return {};
            }
        }

        Base::Result<void> ApplyPendingResize() noexcept {
            if (!hasPendingResize) return {};
            const std::uint32_t width = pendingResizeWidth;
            const std::uint32_t height = pendingResizeHeight;
            const double nextDpiScale = pendingDpiScale;
            hasPendingResize = false;
            if (width != 0U && height != 0U) {
                Base::Result<void> resized =
                    renderContext->Resize(width, height);
                if (!resized) return resized.GetStatus();
            }
            const Size logicalSize{
                static_cast<double>(width) / nextDpiScale,
                static_cast<double>(height) / nextDpiScale};
            view->SetScale(nextDpiScale);
            view->SetSize(logicalSize);
            dpiScale = nextDpiScale;
            frameRequested = true;
            return {};
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

        Base::Result<bool> WaitForActivity(
            std::uint32_t timeoutMilliseconds,
            bool blockUntilEvent) noexcept {
            if (!nativeWindow || !nativeWindow->IsOpen() || closeRequested) {
                return false;
            }
            Platform::WindowEvent event;
            Base::Result<bool> received = blockUntilEvent
                ? nativeWindow->WaitEvent(event)
                : nativeWindow->WaitEventFor(event, timeoutMilliseconds);
            if (!received) return received.GetStatus();
            bool handled = received.Value();
            if (handled) {
                Base::Result<void> status = HandleEvent(event);
                if (!status) return status.GetStatus();
            }
            Base::Result<bool> drained = PumpEvents();
            if (!drained) return drained.GetStatus();
            return handled || drained.Value();
        }

        Base::Result<void> RenderFrame(bool force = false) noexcept {
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
            updateTimeSeconds +=
                static_cast<double>(elapsedMilliseconds) / 1000.0;
            view->Update(updateTimeSeconds);

            if (!renderContext || !renderContext->IsReady()) {
                return HostFailure(
                    Base::ErrorCode::NotInitialized,
                    "Application render context is unavailable");
            }
            IRenderer& renderer = view->GetRenderer();
            const bool synchronized = renderer.UpdateRenderTree();
            Base::Ref<RenderDevice> renderDevice = renderContext->Device();
            if (!renderDevice ||
                renderDevice->State() != RenderDeviceState::Ready) {
                return HostFailure(
                    Base::ErrorCode::InvalidState,
                    "Application render device is unavailable");
            }
            const bool needsRender = force || frameRequested ||
                synchronized || !firstFrameRendered;
            if (!needsRender) return {};

            if (!renderer.RenderOffscreen()) {
                return HostFailure(
                    Base::ErrorCode::InvalidState,
                    "Application offscreen rendering failed");
            }
            Base::Result<void> rendered = renderContext->Render(renderer);
            if (!rendered) return rendered.GetStatus();

            frameRequested = false;
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
                Base::Result<void> rendered = RenderFrame(true);
                if (!rendered) return rendered.GetStatus();
            }
            Base::Result<void> shown = nativeWindow->Show();
            if (shown && window != nullptr) {
                DesktopHost::NotifyWindowContentRendered(*window);
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

        Platform::NativeWindowHandle NativeHandle() const noexcept {
            return nativeWindow
                ? nativeWindow->NativeHandle()
                : Platform::NativeWindowHandle{};
        }

        View* HostedView() noexcept { return view.Get(); }

        void Shutdown() noexcept {
            if (shutdown) return;
            shutdown = true;
            if (renderContext) renderContext->Shutdown();
            if (window != nullptr) {
                DesktopHost::NotifyWindowClosed(*window);
                DesktopHost::DetachWindow(*window);
            }
#if defined(_WIN32)
            static_cast<void>(inputMethod.Detach());
#endif
            if (nativeWindow) nativeWindow->Close();
            loadedDocument = {};
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
        static Platform::NativeWindowHandle NativeHandleThunk(
            const void* context) noexcept {
            return static_cast<const WindowHost*>(context)->NativeHandle();
        }
        static View* HostedViewThunk(void* context) noexcept {
            return static_cast<WindowHost*>(context)->HostedView();
        }

        DesktopHostState* owner = nullptr;
        WindowHostState runtime;
        Base::Ref<View> view;
        std::unique_ptr<RenderContext> renderContext;
        Base::Ref<Base::Object> windowOwner;
        Markup::XamlDocument loadedDocument;
        Window* window = nullptr;
#if defined(_WIN32)
        Platform::Win32Clipboard clipboard;
        Platform::Win32ImeAdapter inputMethod;
#endif
        std::unique_ptr<Platform::IWindow> nativeWindow;
        bool closeRequested = false;
        bool hasPendingResize = false;
        bool firstFrameRendered = false;
        bool frameRequested = true;
        bool updateClockInitialized = false;
        std::chrono::steady_clock::time_point lastUpdate;
        double updateTimeSeconds = 0.0;
        bool shutdown = false;
        std::uint32_t pendingResizeWidth = 0U;
        std::uint32_t pendingResizeHeight = 0U;
        double dpiScale = 1.0;
        double pendingDpiScale = 1.0;
    };

    DesktopHostState(
        const RunOptions& source,
        Application* providedApplication,
        Base::Ref<Window> providedWindow) noexcept
        : applicationRuntime{
              this,
              &DesktopHostState::RequestExitThunk,
              &DesktopHostState::ShowWindowThunk,
              &DesktopHostState::WindowCountThunk,
              &DesktopHostState::WindowAtThunk,
              &DesktopHostState::SetMainWindowThunk},
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
        optionsStatus = applicationFile.Assign(source.applicationFile);
        if (optionsStatus) {
            const Base::StringView path = applicationFile.View();
            std::uint32_t separator = path.SizeBytes();
            for (std::uint32_t index = 0U; index < path.SizeBytes(); ++index) {
                if (path[index] == '/' || path[index] == '\\') {
                    separator = index;
                }
            }
            optionsStatus = assetRoot.Assign(
                separator < path.SizeBytes()
                ? path.Substr(0U, separator)
                : Base::StringView("."));
        }
    }

    ~DesktopHostState() noexcept {
        ShutdownWindows();
        if (application != nullptr) {
            DesktopHost::DetachApplication(*application);
        }
        application = nullptr;
        applicationOwner.Reset();
        loaderView.Reset();
    }

    Base::Result<void> CreateRuntime() noexcept {
        if (!optionsStatus) return optionsStatus.GetStatus();
        Base::Result<void> appModule =
            environment.AddModule(AppMetadataModule());
        if (!appModule) return appModule.GetStatus();
        for (const ModuleRegistration& module : modules) {
            Base::Result<void> added = environment.AddModule(module);
            if (!added) return added.GetStatus();
        }
        return environment.Initialize();
    }

    Base::Result<Base::Ref<View>> CreateLoaderView() noexcept {
        ViewOptions options;
        options.text.fontSearchRoot = assetRoot.View();
        options.diagnostics = diagnostics;
        return environment.CreateView(options, allocator);
    }

    Base::Result<void> LoadApplication() noexcept {
        if (suppliedApplication != nullptr) {
            application = suppliedApplication;
            // A C++ Application supplies lifecycle callbacks, but App.xaml is
            // still its authored resource and StartupUri contract. Loading it
            // here keeps source XAML resources in their Application layer
            // instead of forcing samples to duplicate their theme in code.
            Base::Result<Base::Ref<View>> created = CreateLoaderView();
            if (!created) return created.GetStatus();
            loaderView = std::move(created).Value();
            Markup::XamlReader reader(environment);
            Base::Result<Markup::XamlDocument> loaded =
                reader.LoadComponent<Application>(
                    applicationFile.View(), {}, diagnostics);
            if (!loaded) return loaded.GetStatus();
            const Base::Ref<Base::Object>& root = loaded.Value().Root();
            if (!root) {
                return HostFailure(
                    Base::ErrorCode::InvalidArgument,
                    "Application XAML root must be Application");
            }
            applicationOwner = root;
            Application& authored = static_cast<Application&>(*root);
            Base::Result<ResourceDictionary> sharedResources =
                authored.GetResources().Share();
            if (!sharedResources) {
                return sharedResources.GetStatus();
            }
            DesktopHost::AdoptApplicationResources(
                *application, std::move(sharedResources).Value());
            const Base::StringView startup =
                application->GetStartupUri().Empty()
                ? authored.GetStartupUri()
                : application->GetStartupUri();
            if (!startup.Empty()) {
                Base::Result<Base::ResourceUri> resolved =
                    Base::ResourceUri::Resolve(
                        loaded.Value().CanonicalUri(), startup);
                if (!resolved) return resolved.GetStatus();
                startupUri = std::move(resolved).Value();
            }
            return {};
        }

        Base::Result<Base::Ref<View>> created = CreateLoaderView();
        if (!created) return created.GetStatus();
        loaderView = std::move(created).Value();
        Markup::XamlReader reader(environment);
        Base::Result<Markup::XamlDocument> loaded =
            reader.LoadComponent<Application>(
                applicationFile.View(), {}, diagnostics);
        if (!loaded) return loaded.GetStatus();
        const Base::Ref<Base::Object>& root = loaded.Value().Root();
        if (!root) {
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
        Base::Result<void> appended = windows.PushBack(host);
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
        Base::Result<void> attached = DesktopHost::AttachApplication(
            *application, &applicationRuntime, nullptr);
        if (!attached) return attached.GetStatus();

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
                DesktopHost::AttachMainWindow(*application, mainWindow);
            }
        }

        // WPF allows Application.Run() without StartupUri. In that form the
        // application creates and shows one or more windows from OnStartup().
        DesktopHost::RaiseApplicationStartup(*application);
        if (application->GetMainWindow() == nullptr && !windows.Empty()) {
            DesktopHost::AttachMainWindow(*application, windows[0]->window);
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
            DesktopHost::AttachMainWindow(*application, host->window);
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
                DesktopHost::NotifyWindowClosed(*closingWindow);
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
            status = host->RenderFrame(true);
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

            if (!handledEvent && !windows.Empty()) {
                WindowHost* waiter = windows[0];
                if (waiter != nullptr) {
                    // A single non-animated window can block fully on native
                    // events. Animation or multi-window hosting uses a 16 ms
                    // timed native wait so other windows and the animation
                    // clock remain responsive without a 1 ms polling loop.
                    const bool blockUntilEvent =
                        !automaticAnimationClock && windows.Size() == 1U;
                    Base::Result<bool> waited = waiter->WaitForActivity(
                        16U, blockUntilEvent);
                    if (!waited) return waited.GetStatus();
                }
            }
        }

        const int result = exitCode;
        ShutdownWindows();
        if (application != nullptr) {
            DesktopHost::RaiseApplicationExit(*application, result);
            DesktopHost::DetachApplication(*application);
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
        static_cast<DesktopHostState*>(context)->RequestExit(exitCode);
    }
    static Base::Result<void> ShowWindowThunk(
        void* context, Window& window) noexcept {
        return static_cast<DesktopHostState*>(context)->ShowWindow(window);
    }
    static std::uint32_t WindowCountThunk(
        const void* context) noexcept {
        return static_cast<const DesktopHostState*>(context)->WindowCount();
    }
    static Window* WindowAtThunk(
        const void* context, std::uint32_t index) noexcept {
        return static_cast<const DesktopHostState*>(context)->WindowAt(index);
    }
    static void SetMainWindowThunk(
        void* context, Window* window) noexcept {
        static_cast<DesktopHostState*>(context)->SetMainWindow(window);
    }

    ApplicationHostState applicationRuntime;
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
    Diagnostics::IDiagnosticSink* diagnostics = nullptr;
    Application* suppliedApplication = nullptr;
    Base::Ref<Window> suppliedWindow;
    Base::Ref<View> loaderView;
    Base::Ref<Base::Object> applicationOwner;
    Application* application = nullptr;
    Window* mainWindow = nullptr;
    bool exitRequested = false;
    int exitCode = 0;
};

static_assert(sizeof(DesktopHostState) <= 131072U,
    "DesktopHost inline state storage is too small");
static_assert(alignof(DesktopHostState) <= alignof(std::max_align_t),
    "DesktopHost inline state alignment is insufficient");

DesktopHost::DesktopHost(
    const RunOptions& options) noexcept
    : state_(new (stateStorage_)
          DesktopHostState(options, nullptr, {})) {}

DesktopHost::DesktopHost(
    Application& application,
    Base::Ref<Window> window,
    const RunOptions& options) noexcept
    : state_(new (stateStorage_) DesktopHostState(
          options, &application, std::move(window))) {}

DesktopHost::~DesktopHost() noexcept {
    if (state_ != nullptr) state_->~DesktopHostState();
    state_ = nullptr;
}

Base::Result<int> DesktopHost::Run() noexcept {
    return state_->Run();
}

Base::Result<void> DesktopHost::AttachApplication(
    Application& application,
    void* hostState,
    Window* mainWindow) noexcept {
    return application.Attach(hostState, mainWindow);
}

void DesktopHost::DetachApplication(Application& application) noexcept {
    application.Detach();
}

void DesktopHost::RaiseApplicationStartup(Application& application) noexcept {
    application.RaiseStartup();
}

void DesktopHost::RaiseApplicationExit(
    Application& application,
    int exitCode) noexcept {
    application.RaiseExit(exitCode);
}

void DesktopHost::AttachMainWindow(
    Application& application,
    Window* window) noexcept {
    application.AttachMainWindow(window);
}

void DesktopHost::AdoptApplicationResources(
    Application& application,
    ResourceDictionary&& resources) noexcept {
    application.AdoptResources(std::move(resources));
}

void DesktopHost::AttachWindow(Window& window, void* hostState) noexcept {
    window.Attach(hostState);
}

void DesktopHost::DetachWindow(Window& window) noexcept {
    window.Detach();
}

void DesktopHost::NotifyWindowSourceInitialized(Window& window) noexcept {
    window.NotifySourceInitialized();
}

void DesktopHost::NotifyWindowContentRendered(Window& window) noexcept {
    window.NotifyContentRendered();
}

void DesktopHost::NotifyWindowClosed(Window& window) noexcept {
    window.NotifyClosed();
}

bool DesktopHost::WindowComponentRequested(
    const Window& window) noexcept {
    return window.ComponentRequested();
}

Base::StringView DesktopHost::WindowComponentUri(
    const Window& window) noexcept {
    return window.ComponentUri();
}

} // namespace Aero::App

namespace Aero {

void Application::AdoptResources(
    ResourceDictionary&& resources) noexcept {
    resources_ = std::move(resources);
}

bool Window::ComponentRequested() const noexcept {
    return componentRequested_;
}

Base::StringView Window::ComponentUri() const noexcept {
    return componentUri_.View();
}

} // namespace Aero

namespace Aero::App {

int Run(const RunOptions& options) noexcept {
    ::Aero::App::DesktopHost host(options);
    Base::Result<int> result = host.Run();
    return result ? result.Value() : -1;
}

} // namespace Aero::App
