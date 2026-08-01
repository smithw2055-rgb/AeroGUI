#include <Aero/Window.hpp>
#include <Aero/Application.hpp>

#include <Aero/App/WindowInterop.hpp>
#include "ApplicationRuntime.hpp"

namespace Aero {


Base::Result<void> Window::Show() noexcept {
    auto* state = static_cast<App::Detail::WindowRuntimeState*>(runtimeState_);
    if (state == nullptr) {
        Application* application = Application::Current();
        auto* applicationState = application != nullptr
            ? static_cast<App::Detail::ApplicationRuntimeState*>(
                  application->runtimeState_)
            : nullptr;
        if (applicationState == nullptr ||
            applicationState->showWindow == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Window.Show requires a running Application host");
        }
        Base::Result<void> attached =
            applicationState->showWindow(applicationState->context, *this);
        if (!attached) return attached.GetStatus();
        state = static_cast<App::Detail::WindowRuntimeState*>(runtimeState_);
    }
    if (state == nullptr || state->show == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Window is not attached to an application host");
    }
    Base::Result<void> shown = state->show(state->context);
    if (shown) {
        NotifySourceInitialized();
        NotifyActivated();
    }
    return shown;
}

Base::Result<void> Window::SetWindowState(WindowState value) noexcept {
    const WindowState previous = GetWindowState();
    Base::Result<void> changed = SetValue(WindowStateProperty, value);
    if (changed && previous != value) {
        RoutedEventArgs args;
        OnStateChanged(args);
    }
    return changed;
}

bool Window::IsOpen() const noexcept {
    const auto* state = static_cast<const App::Detail::WindowRuntimeState*>(runtimeState_);
    return state != nullptr && state->isOpen != nullptr && state->isOpen(state->context);
}

void Window::Close() noexcept {
    if (closed_) return;
    CancelEventArgs closing;
    OnClosing(closing);
    if (closing.GetCancel()) return;
    auto* state = static_cast<App::Detail::WindowRuntimeState*>(runtimeState_);
    if (state != nullptr && state->close != nullptr) state->close(state->context);
    NotifyClosed();
}

void Window::NotifySourceInitialized() noexcept {
    if (sourceInitialized_) return;
    sourceInitialized_ = true;
    RoutedEventArgs args;
    OnSourceInitialized(args);
}

void Window::NotifyActivated() noexcept {
    RoutedEventArgs args;
    OnActivated(args);
    if (Application::Current() != nullptr) Application::Current()->RaiseActivated();
}

void Window::NotifyDeactivated() noexcept {
    RoutedEventArgs args;
    OnDeactivated(args);
    if (Application::Current() != nullptr) Application::Current()->RaiseDeactivated();
}

void Window::NotifyContentRendered() noexcept {
    if (contentRendered_) return;
    contentRendered_ = true;
    RoutedEventArgs args;
    OnContentRendered(args);
}

void Window::NotifyClosed() noexcept {
    if (closed_) return;
    closed_ = true;
    NotifyDeactivated();
    RoutedEventArgs args;
    OnClosed(args);
}

} // namespace Aero

namespace Aero::App {

Integration::NativeWindowHandle WindowInterop::NativeHandle(const ::Aero::Window& window) noexcept {
    const auto* state = static_cast<const App::Detail::WindowRuntimeState*>(window.runtimeState_);
    return state != nullptr && state->nativeHandle != nullptr ? state->nativeHandle(state->context) : Integration::NativeWindowHandle{};
}

::Aero::View* WindowInterop::HostedView(::Aero::Window& window) noexcept {
    auto* state = static_cast<App::Detail::WindowRuntimeState*>(window.runtimeState_);
    return state != nullptr && state->hostedView != nullptr ? state->hostedView(state->context) : nullptr;
}

} // namespace Aero::App
