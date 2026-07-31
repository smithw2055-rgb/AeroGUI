#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/RuntimeTypes.hpp>

#include <cstdint>

namespace Aero {
class Application;
class Window;
struct ModuleRegistration;
namespace Core { class IDiagnosticSink; }
}

namespace Aero::App {

using StartupCallback =
    Base::Result<void> (*)(
        Application& application,
        Window& mainWindow,
        void* context) noexcept;
using FrameCallback =
    Base::Result<void> (*)(
        Application& application,
        Window& mainWindow,
        std::uint64_t frameIndex,
        void* context) noexcept;

enum class GraphicsBackend : std::uint8_t {
    Automatic = 0U,
    D3D11,
    OpenGL33
};

struct LaunchOptions final {
    Base::StringView applicationFile = "App.xaml";
    GraphicsBackend graphicsBackend = GraphicsBackend::Automatic;
    // Zero asks the platform for its preferred initial size when XAML does not
    // declare Width/Height.
    std::uint32_t defaultWidth = 0U;
    std::uint32_t defaultHeight = 0U;
    bool visible = true;
    bool resizable = true;
    bool automaticAnimationClock = true;
    bool loadBuiltInTheme = true;
    BuiltInTheme builtInTheme = BuiltInTheme::Light;
    Core::IDiagnosticSink* diagnostics = nullptr;
    StartupCallback startup = nullptr;
    void* startupContext = nullptr;
    FrameCallback frame = nullptr;
    void* frameContext = nullptr;
};

// Optional default desktop lifetime. Application and Window remain ordinary
// WPF-facing objects; Launcher owns native windows, event pumping and endpoint
// selection behind an opaque implementation.
class AERO_API Launcher final {
public:
    explicit Launcher(
        const LaunchOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Launcher() noexcept;

    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    Base::Result<int> Run() noexcept;
    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    void RequestExit(int exitCode = 0) noexcept;

    Application* CurrentApplication() const noexcept;
    Window* MainWindow() const noexcept;

private:
    struct Impl;

    static void AttachRuntime(
        Application& application,
        Window& window,
        void* applicationRuntime,
        void* windowRuntime) noexcept;
    static void DetachRuntime(
        Application* application,
        Window* window) noexcept;

    Impl* impl_ = nullptr;
};

} // namespace Aero::App
