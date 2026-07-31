#include <Aero/Window.hpp>

#include <Aero/Integration/WindowInterop.hpp>
#include "ApplicationRuntime.hpp"

namespace Aero {

Base::Result<void> Window::Show() noexcept {
    auto* state = static_cast<App::Detail::WindowRuntimeState*>(
        runtimeState_);
    if (state == nullptr || state->show == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Window is not attached to an application host");
    }
    return state->show(state->context);
}

void Window::Close() noexcept {
    auto* state = static_cast<App::Detail::WindowRuntimeState*>(
        runtimeState_);
    if (state != nullptr && state->close != nullptr) {
        state->close(state->context);
    }
}

bool Window::IsOpen() const noexcept {
    const auto* state = static_cast<const App::Detail::WindowRuntimeState*>(
        runtimeState_);
    return state != nullptr && state->isOpen != nullptr &&
        state->isOpen(state->context);
}

} // namespace Aero

namespace Aero::Integration {

Platform::NativeWindowHandle WindowInterop::NativeHandle(
    const ::Aero::Window& window) noexcept {
    const auto* state =
        static_cast<const App::Detail::WindowRuntimeState*>(
            window.runtimeState_);
    return state != nullptr && state->nativeHandle != nullptr
        ? state->nativeHandle(state->context)
        : Platform::NativeWindowHandle{};
}

::Aero::View* WindowInterop::HostedView(
    ::Aero::Window& window) noexcept {
    auto* state = static_cast<App::Detail::WindowRuntimeState*>(
        window.runtimeState_);
    return state != nullptr && state->hostedView != nullptr
        ? state->hostedView(state->context)
        : nullptr;
}

} // namespace Aero::Integration
