#include <Aero/Window.hpp>
#include <Aero/Application.hpp>

#include <Aero/App/WindowInterop.hpp>
#include "ApplicationState.hpp"
#include "DesktopHost.hpp"

namespace Aero {

Base::Result<void> Window::Show() noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Window implementation allocation failed");
    }
    auto* state = static_cast<::Aero::App::Detail::WindowHostState*>(
        impl_->hostState);
    if (state == nullptr) {
        Application* application = Application::Current();
        auto* applicationState = application != nullptr &&
                application->impl_ != nullptr
            ? static_cast<::Aero::App::Detail::ApplicationHostState*>(
                  application->impl_->hostState)
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
        state = static_cast<::Aero::App::Detail::WindowHostState*>(
            impl_->hostState);
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

void Window::SetWindowState(WindowState value) noexcept {
    const WindowState previous = GetWindowState();
    SetValue(WindowStateProperty, value);
    if (previous != value) {
        RoutedEventArgs args;
        OnStateChanged(args);
    }
}

bool Window::GetIsOpen() const noexcept {
    const auto* state = impl_ != nullptr
        ? static_cast<const ::Aero::App::Detail::WindowHostState*>(
              impl_->hostState)
        : nullptr;
    return state != nullptr && state->isOpen != nullptr && state->isOpen(state->context);
}

Base::Result<void> Window::Close() noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Window implementation allocation failed");
    }
    if (impl_->closed) return {};
    CancelEventArgs closing;
    OnClosing(closing);
    if (closing.GetCancel()) return {};
    auto* state = static_cast<::Aero::App::Detail::WindowHostState*>(
        impl_->hostState);
    if (state != nullptr && state->close != nullptr) state->close(state->context);
    NotifyClosed();
    return {};
}

void Window::Attach(void* hostState) noexcept {
    if (impl_ == nullptr) return;
    impl_->hostState = hostState;
    impl_->sourceInitialized = false;
    impl_->contentRendered = false;
    impl_->closed = false;
}

void Window::Detach() noexcept {
    if (impl_ != nullptr) impl_->hostState = nullptr;
}

void Window::NotifySourceInitialized() noexcept {
    if (impl_ == nullptr || impl_->sourceInitialized) return;
    impl_->sourceInitialized = true;
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
    if (impl_ == nullptr || impl_->contentRendered) return;
    impl_->contentRendered = true;
    RoutedEventArgs args;
    OnContentRendered(args);
}

void Window::NotifyClosed() noexcept {
    if (impl_ == nullptr || impl_->closed) return;
    impl_->closed = true;
    NotifyDeactivated();
    RoutedEventArgs args;
    OnClosed(args);
}

} // namespace Aero

namespace Aero::App {

Platform::NativeWindowHandle WindowInterop::NativeHandle(const ::Aero::Window& window) noexcept {
    const auto* state = window.impl_ != nullptr
        ? static_cast<const ::Aero::App::Detail::WindowHostState*>(
              window.impl_->hostState)
        : nullptr;
    return state != nullptr && state->nativeHandle != nullptr ? state->nativeHandle(state->context) : Platform::NativeWindowHandle{};
}

::Aero::View* WindowInterop::HostedView(::Aero::Window& window) noexcept {
    auto* state = window.impl_ != nullptr
        ? static_cast<::Aero::App::Detail::WindowHostState*>(
              window.impl_->hostState)
        : nullptr;
    return state != nullptr && state->hostedView != nullptr ? state->hostedView(state->context) : nullptr;
}

} // namespace Aero::App
