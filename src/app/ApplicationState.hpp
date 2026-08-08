#pragma once

#include <Aero/Base/Result.hpp>
#include "../app/platform/Window.hpp"

#include <cstdint>

namespace Aero {
class View;
class Window;
}

namespace Aero::App {

// Private bridge between the WPF-facing Application object and the optional
// desktop host. The callback table keeps native lifetime out of public class
// layouts while supporting the real multi-window Application.Windows model.
struct ApplicationHostState {
    void* context = nullptr;
    void (*requestExit)(void* context, int exitCode) noexcept = nullptr;
    Base::Result<void> (*showWindow)(
        void* context,
        Window& window) noexcept = nullptr;
    std::uint32_t (*windowCount)(
        const void* context) noexcept = nullptr;
    Window* (*windowAt)(
        const void* context,
        std::uint32_t index) noexcept = nullptr;
    void (*setMainWindow)(
        void* context,
        Window* window) noexcept = nullptr;
};

// One state record is created per hosted Window. This intentionally points at
// a window record rather than the application host so independent top-level
// windows can own distinct View, native surface and input-service lifetimes.
struct WindowHostState {
    void* context = nullptr;
    Base::Result<void> (*show)(void* context) noexcept = nullptr;
    void (*close)(void* context) noexcept = nullptr;
    bool (*isOpen)(const void* context) noexcept = nullptr;
    Platform::NativeWindowHandle (*nativeHandle)(
        const void* context) noexcept = nullptr;
    View* (*hostedView)(void* context) noexcept = nullptr;
};

} // namespace Aero::App
