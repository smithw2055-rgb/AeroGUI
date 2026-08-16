#include <AeroRender/RenderTarget.hpp>
#include <AeroRender/RenderDevice.hpp>

namespace Aero {

namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

} // namespace

RenderTarget::~RenderTarget() noexcept = default;

Result<void> RenderTarget::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (!device_) return NotInitialized("Render target is not initialized");
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target dimensions must be nonzero");
    }
    if (device_->State() != RenderDeviceState::Ready ||
        state_ != RenderTargetState::Ready) {
        return InvalidState("Render target cannot resize in its current state");
    }
    Result<void> idle = device_->WaitIdle();
    if (!idle) return idle.GetStatus();
    return ResizeBackend(width, height);
}

void RenderTarget::NotifyLost() noexcept {
    if (!device_ || device_->State() != RenderDeviceState::Ready ||
        state_ != RenderTargetState::Ready) {
        return;
    }
    state_ = RenderTargetState::Lost;
    NotifyBackendLost();
}

Result<void> RenderTarget::Restore() noexcept {
    if (!device_) return NotInitialized("Render target is not initialized");
    if (device_->State() != RenderDeviceState::Ready ||
        state_ != RenderTargetState::Lost) {
        return InvalidState("Only a lost render target can be restored");
    }
    Result<void> restored = RestoreBackend();
    if (restored) {
        state_ = RenderTargetState::Ready;
    }
    return restored;
}

} // namespace Aero
