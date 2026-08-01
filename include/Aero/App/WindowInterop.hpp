#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Integration/NativeWindow.hpp>

namespace Aero {
class View;
class Window;
}

namespace Aero::App {

// Explicit escape hatch for native hosts. Normal WPF-facing code should use
// Window lifecycle and dependency properties instead.
class AERO_API WindowInterop final {
public:
    static Integration::NativeWindowHandle NativeHandle(
        const ::Aero::Window& window) noexcept;
    static ::Aero::View* HostedView(
        ::Aero::Window& window) noexcept;
};

} // namespace Aero::App
