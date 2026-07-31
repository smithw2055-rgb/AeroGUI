#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Platform/Window.hpp>

namespace Aero {
class View;
class Window;
}

namespace Aero::Integration {

// Explicit escape hatch for native hosts. Normal WPF-facing code should use
// Window lifecycle and dependency properties instead.
class AERO_API WindowInterop final {
public:
    static Platform::NativeWindowHandle NativeHandle(
        const ::Aero::Window& window) noexcept;
    static ::Aero::View* HostedView(
        ::Aero::Window& window) noexcept;
};

} // namespace Aero::Integration
