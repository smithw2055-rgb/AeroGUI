#pragma once

#include <AeroRender/WindowInterop.hpp>

namespace Aero {
class View;
}

namespace Aero::App {

class Window;

// Explicit escape hatch for native hosts. Normal WPF-facing code should use
// Window lifecycle and dependency properties instead. The NativeWindowHandle
// value type is owned by AeroRender/WindowInterop.hpp; input services
// (clipboard/IME) are owned by Aero/InputInterop.hpp.
class AERO_APP_API WindowInterop  {
public:
    static Platform::NativeWindowHandle NativeHandle(const Window& window) noexcept;
    static ::Aero::View* HostedView(Window& window) noexcept;
};

} // namespace Aero::App
