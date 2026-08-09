#include "render/RenderTargetState.hpp"
#include "gui/ViewRenderer.hpp"

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

RenderTarget::~RenderTarget() noexcept = default;

RenderTargetKind RenderTarget::Kind() const noexcept {
    return BackendKind();
}

RenderTargetState RenderTarget::State() const noexcept {
    const Render::RenderTargetBase& implementation =
        *Render::RenderTargetBase::From(*this);
    if (!implementation.device) return RenderTargetState::Shutdown;
    switch (implementation.device->State()) {
    case Aero::RenderDeviceState::Ready:
        break;
    case Aero::RenderDeviceState::DeviceLost:
        return RenderTargetState::DeviceLost;
    case Aero::RenderDeviceState::Failed:
        return RenderTargetState::Failed;
    case Aero::RenderDeviceState::Shutdown:
        return RenderTargetState::Shutdown;
    }
    return BackendState();
}

Base::Ref<Aero::RenderDevice> RenderTarget::GetDevice() const noexcept {
    return Render::RenderTargetBase::From(*this)->device;
}

Base::Result<void> RenderTarget::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    Render::RenderTargetBase& implementation =
        *Render::RenderTargetBase::From(*this);
    if (!implementation.device) return NotInitialized("Render target is not initialized");
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target dimensions must be nonzero");
    }
    if (implementation.device->State() != Aero::RenderDeviceState::Ready ||
        BackendState() != RenderTargetState::Ready) {
        return InvalidState("Render target cannot resize in its current state");
    }
    Base::Result<void> idle = implementation.device->WaitIdle();
    if (!idle) return idle.GetStatus();
    return ResizeBackend(width, height);
}

void RenderTarget::NotifyLost() noexcept {
    Render::RenderTargetBase& implementation =
        *Render::RenderTargetBase::From(*this);
    if (!implementation.device ||
        implementation.device->State() != Aero::RenderDeviceState::Ready ||
        BackendState() != RenderTargetState::Ready) {
        return;
    }
    NotifyBackendLost();
    Render::RenderDeviceBase::RefreshHealth(*implementation.device);
}

Base::Result<void> RenderTarget::Restore() noexcept {
    Render::RenderTargetBase& implementation =
        *Render::RenderTargetBase::From(*this);
    if (!implementation.device) return NotInitialized("Render target is not initialized");
    if (implementation.device->State() != Aero::RenderDeviceState::Ready ||
        BackendState() != RenderTargetState::Lost) {
        return InvalidState("Only a lost render target can be restored");
    }
    Base::Result<void> restored = RestoreBackend();
    Render::RenderDeviceBase::RefreshHealth(*implementation.device);
    return restored;
}

} // namespace Aero

namespace Aero::Render {

Base::Result<void> RenderTargetServices::Render(
    Aero::RenderTarget& target,
    ::Aero::ViewRenderer& renderer,
    const ::Aero::Render::RenderFrame& frame) noexcept {
    RenderTargetBase& implementation = *RenderTargetBase::From(target);
    if (!implementation.device) {
        return NotInitialized("Render target is not initialized");
    }
    Base::Status deviceReady =
        RenderDeviceBase::FrameStatus(*implementation.device);
    if (!deviceReady.IsOk()) return deviceReady;
    if (target.BackendState() != Aero::RenderTargetState::Ready) {
        return InvalidState("Render target is not ready");
    }

    Base::Result<Aero::RenderFrameStatistics> statistics =
        RenderDeviceBase::BeginSurfaceFrame(*implementation.device, frame);
    if (!statistics) return statistics.GetStatus();

    Base::Result<FrameTarget> acquired =
        implementation.AcquireFrameTarget();
    if (!acquired) {
        RenderDeviceBase::RecordSurfaceFailure(*implementation.device);
        return acquired.GetStatus();
    }

    Base::Result<::Aero::Graphics::FenceValue> rendered =
        renderer.RenderOnscreenFrame(
        frame, acquired.Value());
    Base::Result<void> retired =
        implementation.RetireFrameTarget(acquired.Value());
    if (!rendered) {
        RenderDeviceBase::RecordSurfaceFailure(*implementation.device);
        return rendered.GetStatus();
    }
    if (!retired) {
        RenderDeviceBase::RecordSurfaceFailure(*implementation.device);
        return retired.GetStatus();
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

    RenderDeviceBase::CompleteSurfaceFrame(
        *implementation.device, frame, statistics.Value());
    return {};
}

} // namespace Aero::Render
