#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/RuntimeTypes.hpp>

#include <cstdint>

namespace Aero::App {

class Application;
class Window;

using ApplicationStartupCallback =
    Base::Result<void> (*)(
        Application& application,
        Window& mainWindow,
        void* context) noexcept;
using ApplicationFrameCallback =
    Base::Result<void> (*)(
        Application& application,
        Window& mainWindow,
        std::uint64_t frameIndex,
        void* context) noexcept;
}

namespace Aero {
struct ModuleRegistration;
namespace Core {
class IDiagnosticSink;
}
}

namespace Aero::App {

enum class GraphicsBackend : std::uint8_t {
    Automatic = 0U,
    D3D11,
    OpenGL33
};

struct ApplicationHostOptions final {
    Base::StringView applicationFile = "App.xaml";
    GraphicsBackend graphicsBackend = GraphicsBackend::Automatic;
    // Zero preserves the platform's preferred initial window size when the
    // Window XAML does not declare Width/Height.
    std::uint32_t defaultWidth = 0U;
    std::uint32_t defaultHeight = 0U;
    bool visible = true;
    bool resizable = true;
    bool automaticAnimationClock = true;
    bool loadBuiltInTheme = true;
    BuiltInTheme builtInTheme = BuiltInTheme::Light;
    Core::IDiagnosticSink* diagnostics = nullptr;
    // Invoked after the XAML Window is mounted and both Application/Window
    // peers are attached, but before the first frame or native Show().
    ApplicationStartupCallback startup = nullptr;
    void* startupContext = nullptr;
    // Runs after each completed frame on the application thread. The first
    // callback uses frameIndex 0 and runs before the native window is shown.
    ApplicationFrameCallback frame = nullptr;
    void* frameContext = nullptr;
};

// Owns one complete application lifetime: runtime environment, XAML
// application, native window, graphics endpoint, View and event pump.
class AERO_API ApplicationHost final {
public:
    explicit ApplicationHost(
        const ApplicationHostOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~ApplicationHost() noexcept;

    ApplicationHost(const ApplicationHost&) = delete;
    ApplicationHost& operator=(const ApplicationHost&) = delete;

    Base::Result<int> Run() noexcept;
    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    void RequestExit(int exitCode = 0) noexcept;

    Application* CurrentApplication() const noexcept;
    Window* MainWindow() const noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Aero::App
