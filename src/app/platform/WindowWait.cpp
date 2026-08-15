#include "Window.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif AERO_PLATFORM_HAS_X11_WINDOW
#include <X11/Xlib.h>
#undef Status
#include <cerrno>
#include <poll.h>
#endif

namespace Aero::Platform {
#if defined(_WIN32) || AERO_PLATFORM_HAS_X11_WINDOW
namespace {

Base::Status WaitFailure(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InternalError, message);
}

} // namespace
#endif

Base::Result<bool> IWindow::WaitEventFor(
    WindowEvent& event,
    std::uint32_t timeoutMilliseconds) noexcept {
    Base::Result<bool> ready = PollEvent(event);
    if (!ready || ready.Value() || !IsOpen()) return ready;

#if defined(_WIN32)
    const NativeWindowHandle native = NativeHandle();
    if (native.system != WindowSystem::Win32) return false;
    const DWORD waited = MsgWaitForMultipleObjectsEx(
        0U, nullptr, timeoutMilliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    if (waited == WAIT_TIMEOUT) return false;
    if (waited == WAIT_FAILED) return WaitFailure("Win32 message wait failed");
    return PollEvent(event);
#elif AERO_PLATFORM_HAS_X11_WINDOW
    const NativeWindowHandle native = NativeHandle();
    if (native.system != WindowSystem::X11 || native.display == 0U) return false;
    auto* display = reinterpret_cast<Display*>(native.display);
    if (XPending(display) > 0) return PollEvent(event);

    pollfd descriptor{};
    descriptor.fd = ConnectionNumber(display);
    descriptor.events = POLLIN;
    const int timeout = timeoutMilliseconds >
            static_cast<std::uint32_t>(INT32_MAX)
        ? INT32_MAX
        : static_cast<int>(timeoutMilliseconds);
    int waited = 0;
    do {
        waited = ::poll(&descriptor, 1U, timeout);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) return WaitFailure("X11 event wait failed");
    if (waited == 0) return false;
    return PollEvent(event);
#else
    static_cast<void>(timeoutMilliseconds);
    return false;
#endif
}

} // namespace Aero::Platform
