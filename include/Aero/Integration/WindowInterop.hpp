#pragma once

#include <Aero/Window.hpp>

namespace Aero::Integration {

// Explicit escape hatch for native hosts and engine integrations. WPF-facing
// application code should use Aero::Window lifecycle and dependency properties
// rather than depending on the platform peer or hosted View.
class WindowInterop final {
public:
    static Platform::IWindow* NativeWindow(
        ::Aero::Window& window) noexcept {
        return window.NativeWindow();
    }

    static ::Aero::View* HostedView(
        ::Aero::Window& window) noexcept {
        return window.HostedView();
    }
};

} // namespace Aero::Integration
