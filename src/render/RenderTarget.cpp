#include "render/RenderTargetState.hpp"

#include <utility>

namespace Aero {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

} // namespace

RenderTarget::RenderTarget(
    ConstructionToken,
    Base::Ref<Aero::RenderDevice> device,
    Access* implementation) noexcept
    : device_(std::move(device)),
      impl_(implementation) {}

RenderTarget::~RenderTarget() noexcept {
    delete impl_;
    impl_ = nullptr;
    device_.Reset();
}

RenderTargetKind RenderTarget::Kind() const noexcept {
    return impl_ != nullptr ? impl_->kind : RenderTargetKind::Embedded;
}

RenderTargetState RenderTarget::State() const noexcept {
    if (impl_ == nullptr || !device_) {
        return RenderTargetState::Shutdown;
    }
    switch (device_->State()) {
    case Aero::RenderDeviceState::Ready:
        break;
    case Aero::RenderDeviceState::DeviceLost:
        return RenderTargetState::DeviceLost;
    case Aero::RenderDeviceState::Failed:
        return RenderTargetState::Failed;
    case Aero::RenderDeviceState::Shutdown:
        return RenderTargetState::Shutdown;
    }
    switch (impl_->GetSurfaceHealth()) {
    case ::Aero::Render::SurfaceHealth::Ready:
        return RenderTargetState::Ready;
    case ::Aero::Render::SurfaceHealth::Lost:
        return RenderTargetState::Lost;
    case ::Aero::Render::SurfaceHealth::Failed:
        return RenderTargetState::Failed;
    case ::Aero::Render::SurfaceHealth::Shutdown:
        return RenderTargetState::Shutdown;
    }
    return RenderTargetState::Failed;
}

Base::Ref<Aero::RenderDevice> RenderTarget::GetDevice() const noexcept {
    return device_;
}

Base::Result<void> RenderTarget::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr || !device_) {
        return NotInitialized("Render target is not initialized");
    }
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target dimensions must be nonzero");
    }
    if (device_->State() != Aero::RenderDeviceState::Ready ||
        impl_->GetSurfaceHealth() !=
            ::Aero::Render::SurfaceHealth::Ready) {
        return InvalidState("Render target cannot resize in its current state");
    }
    Base::Result<void> idle = device_->WaitIdle();
    if (!idle) return idle.GetStatus();
    return impl_->Resize(width, height);
}

void RenderTarget::NotifyLost() noexcept {
    if (impl_ == nullptr || !device_ ||
        device_->State() != Aero::RenderDeviceState::Ready ||
        impl_->GetSurfaceHealth() !=
            ::Aero::Render::SurfaceHealth::Ready) {
        return;
    }
    impl_->NotifySurfaceLost();
    Aero::RenderDevice::Access::RefreshHealth(*device_);
}

Base::Result<void> RenderTarget::Restore() noexcept {
    if (impl_ == nullptr || !device_) {
        return NotInitialized("Render target is not initialized");
    }
    if (device_->State() != Aero::RenderDeviceState::Ready ||
        impl_->GetSurfaceHealth() !=
            ::Aero::Render::SurfaceHealth::Lost) {
        return InvalidState("Only a lost render target can be restored");
    }
    Base::Result<void> restored = impl_->RestoreSurface();
    Aero::RenderDevice::Access::RefreshHealth(*device_);
    return restored;
}

Base::Result<Base::Ref<RenderTarget>> RenderTarget::Access::Create(
    Base::Ref<Aero::RenderDevice> device,
    Access* implementation,
    Base::IAllocator* allocator) noexcept {
    if (!device || implementation == nullptr) {
        delete implementation;
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target requires a device and implementation");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<RenderTarget>> made =
        Base::MakeRefWithAllocator<RenderTarget>(
            selected,
            RenderTarget::ConstructionToken{},
            std::move(device),
            implementation);
    if (!made) delete implementation;
    return made;
}

Base::Result<void> RenderTarget::Access::Render(
    RenderTarget& target,
    ::Aero::ViewRenderer& renderer,
    const ::Aero::Render::RenderFrame& frame) noexcept {
    if (target.impl_ == nullptr || !target.device_) {
        return NotInitialized("Render target is not initialized");
    }
    Base::Status deviceReady =
        Aero::RenderDevice::Access::FrameStatus(*target.device_);
    if (!deviceReady.IsOk()) return deviceReady;
    if (target.impl_->GetSurfaceHealth() !=
            ::Aero::Render::SurfaceHealth::Ready) {
        return InvalidState("Render target is not ready");
    }

    Base::Result<Aero::RenderFrameStatistics> statistics =
        Aero::RenderDevice::Access::BeginSurfaceFrame(*target.device_, frame);
    if (!statistics) return statistics.GetStatus();

    Base::Result<void> rendered =
        target.impl_->Render(renderer, frame);
    if (!rendered) {
        Aero::RenderDevice::Access::RecordSurfaceFailure(*target.device_);
        return rendered.GetStatus();
    }

    const ::Aero::Render::FrameEncoderStatistics source =
        renderer.LastStatistics();
    statistics.Value().sourceCommandCount = source.sourceCommandCount;
    statistics.Value().drawPacketCount = source.drawPacketCount;
    statistics.Value().batchCount = source.batchCount;
    statistics.Value().mergedPacketCount = source.mergedPacketCount;
    statistics.Value().barrierCount = source.barrierCount;
    statistics.Value().batchingEnabled = source.batchingEnabled;
    statistics.Value().drawCallCount = source.drawCallCount;
    statistics.Value().instanceCount = source.rectangleInstanceCount +
        source.imageInstanceCount + source.meshInstanceCount +
        source.glyphInstanceCount;
    statistics.Value().stateBindingCount = source.pipelineBindingCount +
        source.vertexBufferBindingCount + source.indexBufferBindingCount +
        source.uniformBufferBindingCount +
        source.textureSamplerBindingCount;

    Aero::RenderDevice::Access::CompleteSurfaceFrame(
        *target.device_, frame, statistics.Value());
    return {};
}

} // namespace Aero

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTarget::Access* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    if (target != nullptr) target->kind = kind;
    return Aero::RenderTarget::Access::Create(
        std::move(device), target, allocator);
}

} // namespace Aero::Render
