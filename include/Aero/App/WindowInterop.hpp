#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Platform/NativeWindow.hpp>

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
