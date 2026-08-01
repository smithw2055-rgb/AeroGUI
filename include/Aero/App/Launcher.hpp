#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Integration/View.hpp>

#include <cstdint>

namespace Aero {
class Application;
class Window;
struct ModuleRegistration;
namespace Core { class IDiagnosticSink; }
}

namespace Aero::App {

enum class GraphicsBackend : std::uint8_t { Automatic = 0U, D3D11, OpenGL33 };

struct LaunchOptions final {
    Base::StringView applicationFile = "App.xaml";
    GraphicsBackend graphicsBackend = GraphicsBackend::Automatic;
    std::uint32_t defaultWidth = 0U;
    std::uint32_t defaultHeight = 0U;
    bool visible = true;
    bool resizable = true;
    bool automaticAnimationClock = true;
    bool loadBuiltInTheme = true;
    BuiltInTheme builtInTheme = BuiltInTheme::Light;
    Core::IDiagnosticSink* diagnostics = nullptr;
};

class AERO_API Launcher final {
public:
    explicit Launcher(const LaunchOptions& options = {}, Base::IAllocator* allocator = nullptr) noexcept;
    ~Launcher() noexcept;

    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    Base::Result<int> Run() noexcept;
    Base::Result<void> AddModule(const ModuleRegistration& registration) noexcept;
    void RequestExit(int exitCode = 0) noexcept;
    Application* GetApplication() const noexcept;
    Window* GetMainWindow() const noexcept;

private:
    struct Impl;
    static void AttachRuntime(Application& application, Window& window, void* applicationRuntime, void* windowRuntime) noexcept;
    static void DetachRuntime(Application* application, Window* window) noexcept;
    Impl* impl_ = nullptr;
};

AERO_API int Run(const LaunchOptions& options = {}, Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::App
