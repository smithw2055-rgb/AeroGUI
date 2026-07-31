#pragma once

#include <Aero/Base/Result.hpp>

namespace Aero {
class View;
}

namespace Aero::Platform {
class IWindow;
}

namespace Aero::App::Detail {

// Internal bridge implemented by the default application host. WPF-facing
// Application and Window objects retain their semantic API without depending
// on native platform or View implementation headers.
class IApplicationPeer {
public:
    virtual ~IApplicationPeer() = default;
    virtual void RequestExit(int exitCode) noexcept = 0;
};

class IWindowPeer {
public:
    virtual ~IWindowPeer() = default;
    virtual Base::Result<void> Show() noexcept = 0;
    virtual void Close() noexcept = 0;
    virtual bool IsOpen() const noexcept = 0;
    virtual Platform::IWindow* NativeWindow() noexcept = 0;
    virtual View* HostedView() noexcept = 0;
};

} // namespace Aero::App::Detail
