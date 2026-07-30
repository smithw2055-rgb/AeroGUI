#include <Aero/Window.hpp>

namespace Aero {

Base::Result<void> Window::Show() noexcept {
    if (peer_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Window is not attached to an application host");
    }
    return peer_->Show();
}

void Window::Close() noexcept {
    if (peer_ != nullptr) {
        peer_->Close();
    }
}

bool Window::IsOpen() const noexcept {
    return peer_ != nullptr && peer_->IsOpen();
}

Platform::IWindow* Window::NativeWindow() noexcept {
    return peer_ != nullptr
        ? peer_->NativeWindow()
        : nullptr;
}

View* Window::HostedView() noexcept {
    return peer_ != nullptr
        ? peer_->HostedView()
        : nullptr;
}

} // namespace Aero
