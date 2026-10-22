#pragma once

#include <Aero/Base/Result.hpp>
#include "../app/platform/Window.hpp"

#include <cstdint>

namespace Aero {
class View;
}

namespace Aero::App {

class Window;

// Private bridge between the WPF-facing Application object and the optional
// desktop host.
class IApplicationHost {
public:
    virtual ~IApplicationHost() = default;
    virtual void RequestExit(int exitCode) noexcept = 0;
    virtual Base::Result<void> ShowWindow(Window& window) noexcept = 0;
    virtual std::uint32_t WindowCount() const noexcept = 0;
    virtual Window* WindowAt(std::uint32_t index) const noexcept = 0;
    virtual void SetMainWindow(Window* window) noexcept = 0;
    virtual Base::Result<bool> RunDialog(Window& window) noexcept = 0;
};

// One host bridge is created per hosted Window. This intentionally points at
// a window record rather than the application host so independent top-level
// windows can own distinct View, native surface and input-service lifetimes.
class IWindowHost {
public:
    virtual ~IWindowHost() = default;
    virtual Base::Result<void> Show() noexcept = 0;
    virtual void Close() noexcept = 0;
    virtual bool IsOpen() const noexcept = 0;
    virtual Platform::NativeWindowHandle NativeHandle() const noexcept = 0;
    virtual View* HostedView() noexcept = 0;
};

} // namespace Aero::App
