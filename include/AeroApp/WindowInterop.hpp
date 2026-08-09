#pragma once

#include <Aero/Base/Config.hpp>

#include <cstdint>

namespace Aero::Platform {

enum class WindowSystem : std::uint8_t {
    Unknown = 0U,
    Win32,
    X11,
    Cocoa,
    Android,
    Web
};

struct NativeWindowHandle {
    WindowSystem system = WindowSystem::Unknown;
    std::uintptr_t display = 0U;
    std::uintptr_t window = 0U;
    std::uintptr_t instance = 0U;

    bool IsValid() const noexcept {
        return system != WindowSystem::Unknown && window != 0U;
    }
};

} // namespace Aero::Platform

namespace Aero {
class View;
class Window;
}

namespace Aero::App {

// Explicit escape hatch for native hosts. Normal WPF-facing code should use
// Window lifecycle and dependency properties instead.
class AERO_APP_API WindowInterop  {
public:
    static Platform::NativeWindowHandle NativeHandle(
        const ::Aero::Window& window) noexcept;
    static ::Aero::View* HostedView(
        ::Aero::Window& window) noexcept;
};

} // namespace Aero::App
