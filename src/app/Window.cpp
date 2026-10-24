#include <AeroApp/Window.hpp>
#include <AeroApp/Application.hpp>

#include <AeroApp/WindowInterop.hpp>
#include "ApplicationHost.hpp"
#include "DesktopHost.hpp"

namespace Aero::App {

void Window::InitializeComponent() noexcept {
    componentRequested_ = true;
    componentUri_.Clear();
}

void Window::InitializeComponent(
    Base::StringView componentUri) noexcept {
    componentRequested_ = true;
    static_cast<void>(componentUri_.Assign(componentUri));
}

void Window::Show() noexcept {
    if (host_ == nullptr) {
        Application* application = Application::Current();
        if (application != nullptr && application->host_ != nullptr) {
            static_cast<void>(application->host_->ShowWindow(*this));
        }
    }
    if (host_ == nullptr) {
        return;
    }
    Base::Result<void> shown = host_->Show();
    if (shown) {
        NotifySourceInitialized();
        NotifyActivated();
    }
}

bool Window::ShowDialog() noexcept {
    if (dialogActive_ || GetIsOpen()) {
        return false;
    }
    dialogActive_ = true;
    Show();
    Application* application = Application::Current();
    if (application == nullptr || application->host_ == nullptr) {
        dialogActive_ = false;
        return false;
    }
    Base::Result<bool> result = application->host_->RunDialog(*this);
    dialogActive_ = false;
    return result.HasValue() ? result.Value() : false;
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
    return host_ != nullptr && host_->IsOpen();
}

void Window::Close() noexcept {
    if (closed_) return;
    CancelEventArgs closing;
    OnClosing(closing);
    if (closing.GetCancel()) return;
    if (host_ != nullptr) host_->Close();
    NotifyClosed();
}

void Window::Attach(IWindowHost* host) noexcept {
    host_ = host;
    sourceInitialized_ = false;
    contentRendered_ = false;
    closed_ = false;
}

void Window::Detach() noexcept {
    host_ = nullptr;
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

bool Window::ComponentRequested() const noexcept {
    return componentRequested_;
}

Base::StringView Window::ComponentUri() const noexcept {
    return componentUri_.View();
}

Platform::NativeWindowHandle WindowInterop::NativeHandle(const Window& window) noexcept {
    return window.host_ != nullptr ? window.host_->NativeHandle() : Platform::NativeWindowHandle{};
}

::Aero::View* WindowInterop::HostedView(Window& window) noexcept {
    return window.host_ != nullptr ? window.host_->HostedView() : nullptr;
}

} // namespace Aero::App
