#include <Aero/Gui/Window.hpp>
#include <Aero/Gui/Application.hpp>

#include <Aero/App/WindowInterop.hpp>
#include "ApplicationState.hpp"
#include "DesktopHost.hpp"

namespace Aero {

void Window::InitializeComponent() noexcept {
    componentRequested_ = true;
    componentUri_.Clear();
}

void Window::InitializeComponent(
    Base::StringView componentUri) noexcept {
    componentRequested_ = true;
    static_cast<void>(componentUri_.Assign(componentUri));
}

Base::Result<void> Window::Show() noexcept {
    auto* state = static_cast<::Aero::App::WindowHostState*>(
        hostState_);
    if (state == nullptr) {
        Application* application = Application::Current();
        auto* applicationState = application != nullptr
            ? static_cast<::Aero::App::ApplicationHostState*>(
                  application->hostState_)
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
        state = static_cast<::Aero::App::WindowHostState*>(
            hostState_);
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
    const auto* state =
        static_cast<const ::Aero::App::WindowHostState*>(hostState_);
    return state != nullptr && state->isOpen != nullptr && state->isOpen(state->context);
}

Base::Result<void> Window::Close() noexcept {
    if (closed_) return {};
    CancelEventArgs closing;
    OnClosing(closing);
    if (closing.GetCancel()) return {};
    auto* state = static_cast<::Aero::App::WindowHostState*>(
        hostState_);
    if (state != nullptr && state->close != nullptr) state->close(state->context);
    NotifyClosed();
    return {};
}

void Window::Attach(void* hostState) noexcept {
    hostState_ = hostState;
    sourceInitialized_ = false;
    contentRendered_ = false;
    closed_ = false;
}

void Window::Detach() noexcept {
    hostState_ = nullptr;
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

Platform::NativeWindowHandle WindowInterop::NativeHandle(const ::Aero::Window& window) noexcept {
    const auto* state =
        static_cast<const ::Aero::App::WindowHostState*>(
            window.hostState_);
    return state != nullptr && state->nativeHandle != nullptr ? state->nativeHandle(state->context) : Platform::NativeWindowHandle{};
}

::Aero::View* WindowInterop::HostedView(::Aero::Window& window) noexcept {
    auto* state = static_cast<::Aero::App::WindowHostState*>(
        window.hostState_);
    return state != nullptr && state->hostedView != nullptr ? state->hostedView(state->context) : nullptr;
}

} // namespace Aero::App
