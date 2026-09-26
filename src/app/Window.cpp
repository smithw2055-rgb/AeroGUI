#include <AeroApp/Window.hpp>
#include <AeroApp/Application.hpp>

#include <AeroApp/WindowInterop.hpp>
#include "ApplicationHost.hpp"
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
    auto* state = static_cast<::Aero::App::WindowHostBridge*>(
        hostState_);
    if (state == nullptr) {
        Application* application = Application::Current();
        auto* applicationState = application != nullptr
            ? static_cast<::Aero::App::ApplicationHost*>(
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
        state = static_cast<::Aero::App::WindowHostBridge*>(
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

Base::Result<bool> Window::ShowDialog() noexcept {
    if (dialogActive_ || GetIsOpen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Window.ShowDialog requires a window that is not already open");
    }
    dialogActive_ = true;
    Base::Result<void> shown = Show();
    if (!shown) {
        dialogActive_ = false;
        return shown.GetStatus();
    }
    Application* application = Application::Current();
    auto* applicationState = application != nullptr
        ? static_cast<::Aero::App::ApplicationHost*>(
              application->hostState_)
        : nullptr;
    if (applicationState == nullptr ||
        applicationState->runDialog == nullptr) {
        dialogActive_ = false;
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Window.ShowDialog requires a running Application host");
    }
    Base::Result<bool> result = applicationState->runDialog(
        applicationState->context, *this);
    dialogActive_ = false;
    return result;
}

Nullable<bool> Window::GetDialogResult() const noexcept {
    return GetValue(DialogResultProperty);
}

void Window::SetDialogResult(Nullable<bool> value) noexcept {
    SetValue(DialogResultProperty, value);
}

Base::Ref<Window> Window::GetOwner() const noexcept {
    return GetValue(OwnerProperty);
}

void Window::SetOwner(Window* owner) noexcept {
    if (owner == nullptr || owner == this) {
        SetValue(OwnerProperty, Base::Ref<Window>{});
        return;
    }
    SetValue(OwnerProperty, Base::Ref<Window>::TryFromBorrowed(*owner));
}

void Window::SetOwner(Base::Ref<Window> owner) noexcept {
    if (owner.Get() == this) owner.Reset();
    SetValue(OwnerProperty, std::move(owner));
}

void Window::OnPropertyChanged(
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    Controls::ContentControl::OnPropertyChanged(args);
    if (!dialogActive_ || closed_ ||
        args.GetProperty() != DialogResultProperty.Handle()) {
        return;
    }
    const Nullable<bool> result = GetDialogResult();
    if (result.GetHasValue()) Close();
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
        static_cast<const ::Aero::App::WindowHostBridge*>(hostState_);
    return state != nullptr && state->isOpen != nullptr && state->isOpen(state->context);
}

void Window::Close() noexcept {
    if (closed_) return;
    CancelEventArgs closing;
    OnClosing(closing);
    if (closing.GetCancel()) return;
    auto* state = static_cast<::Aero::App::WindowHostBridge*>(
        hostState_);
    if (state != nullptr && state->close != nullptr) state->close(state->context);
    NotifyClosed();
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
        static_cast<const ::Aero::App::WindowHostBridge*>(
            window.hostState_);
    return state != nullptr && state->nativeHandle != nullptr ? state->nativeHandle(state->context) : Platform::NativeWindowHandle{};
}

::Aero::View* WindowInterop::HostedView(::Aero::Window& window) noexcept {
    auto* state = static_cast<::Aero::App::WindowHostBridge*>(
        window.hostState_);
    return state != nullptr && state->hostedView != nullptr ? state->hostedView(state->context) : nullptr;
}

} // namespace Aero::App
