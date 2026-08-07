#include "render/private/RenderDevice.hpp"
#include "render/private/RenderTarget.hpp"
#include "render/BatchPlanner.hpp"

#include <new>

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

bool ApplyBackendHealth(
    RenderDevice::Impl& device,
    ::Aero::Render::Detail::BackendHealth health) noexcept {
    const RenderDeviceState previous = device.state;
    switch (health) {
    case ::Aero::Render::Detail::BackendHealth::Ready:
        device.state = RenderDeviceState::Ready;
        break;
    case ::Aero::Render::Detail::BackendHealth::DeviceLost:
        device.state = RenderDeviceState::DeviceLost;
        break;
    case ::Aero::Render::Detail::BackendHealth::Failed:
        device.state = RenderDeviceState::Failed;
        break;
    }
    return previous != device.state;
}

} // namespace

RenderDevice::RenderDevice(
    ConstructionToken,
    Impl* implementation) noexcept
    : impl_(implementation) {}

RenderDevice::~RenderDevice() noexcept {
    if (impl_ == nullptr) return;
    static_cast<void>(impl_->WaitIdle(5000U));
    impl_->state = RenderDeviceState::Shutdown;
    delete impl_;
    impl_ = nullptr;
}

RenderDeviceState RenderDevice::State() const noexcept {
    return impl_ != nullptr ? impl_->state : RenderDeviceState::Shutdown;
}

std::uint64_t RenderDevice::Generation() const noexcept {
    return impl_ != nullptr ? impl_->statistics.generation : 0U;
}

Base::Result<RenderFrameStatistics> RenderDevice::Analyze(
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    Base::Result<void> valid = ::Aero::Render::Detail::ValidateRenderFrame(frame);
    if (!valid) return valid.GetStatus();

    Render::Detail::BatchPlanner planner(impl_->allocator);
    Base::Result<Render::Detail::BatchPlan> planned =
        planner.Build(frame, true);
    if (!planned) return planned.GetStatus();

    const auto& source = planned.Value().Statistics();
    RenderFrameStatistics result;
    result.sourceCommandCount = source.sourceCommandCount;
    result.drawPacketCount = source.drawPacketCount;
    result.batchCount = source.batchCount;
    result.mergedPacketCount = source.mergedPacketCount;
    result.barrierCount = source.barrierCount;
    result.batchingEnabled = true;
    return result;
}

void RenderDevice::MergeBackendStatistics(
    RenderFrameStatistics& result) const noexcept {
    const RenderFrameStatistics native = impl_ != nullptr
        ? impl_->LastFrameStatistics()
        : RenderFrameStatistics{};
    result.drawCallCount = native.drawCallCount != 0U
        ? native.drawCallCount
        : result.batchCount;
    result.instanceCount = native.instanceCount != 0U
        ? native.instanceCount
        : result.drawPacketCount;
    result.stateBindingCount = native.stateBindingCount;
}

void RenderDevice::NotifyDeviceLost() noexcept {
    if (impl_ == nullptr || impl_->state != RenderDeviceState::Ready) return;
    impl_->state = RenderDeviceState::DeviceLost;
    ++impl_->statistics.generation;
    impl_->NotifyDeviceLost();
}

Base::Result<void> RenderDevice::Restore() noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (impl_->state != RenderDeviceState::DeviceLost) {
        return InvalidState("Only a lost render device can be restored");
    }

    Base::Result<void> restored = impl_->RestoreDevice();
    if (!restored) {
        ++impl_->statistics.failedFrameCount;
        if (ApplyBackendHealth(*impl_, impl_->GetDeviceHealth())) {
            ++impl_->statistics.generation;
        }
        return restored.GetStatus();
    }
    ApplyBackendHealth(*impl_, ::Aero::Render::Detail::BackendHealth::Ready);
    return {};
}

Base::Result<void> RenderDevice::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    return impl_ != nullptr
        ? impl_->WaitIdle(timeoutMilliseconds)
        : Base::Result<void>(NotInitialized("Render device is not initialized"));
}

} // namespace Aero

namespace Aero::Render::Detail {

class HeadlessDeviceState final : public Aero::RenderDevice::Impl {
public:
    explicit HeadlessDeviceState(Base::IAllocator& allocator) noexcept
        : Aero::RenderDevice::Impl(allocator) {}

    RenderBackendKind Backend() const noexcept override {
        return RenderBackendKind::Headless;
    }
    Base::Result<void> RenderOffscreen(
        const void*,
        const ::Aero::Render::Detail::RenderFrame&) noexcept override { return {}; }
    void ReleaseRenderer(const void*) noexcept override {}
    void NotifyDeviceLost() noexcept override {}
    Base::Result<void> RestoreDevice() noexcept override { return {}; }
    Base::Result<void> WaitIdle(std::uint32_t) noexcept override { return {}; }
    BackendHealth GetDeviceHealth() const noexcept override {
        return BackendHealth::Ready;
    }
    ::Aero::RenderFrameStatistics
    LastFrameStatistics() const noexcept override { return {}; }
    ::Aero::Render::Detail::RenderResources Resources() noexcept override {
        return {};
    }
};

Base::Result<Base::Ref<Aero::RenderDevice>> AdoptRenderDevice(
    Aero::RenderDevice::Impl* backend,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::RenderDevice::Impl::Create(backend, allocator);
}

Base::Result<Base::Ref<Aero::RenderDevice>> CreateHeadlessRenderDevice(
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* backend = new (std::nothrow) HeadlessDeviceState(selected);
    if (backend == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the headless render device");
    }
    return AdoptRenderDevice(backend, &selected);
}

} // namespace Aero::Render::Detail

namespace Aero {

Base::Result<void> RenderDevice::RenderOffscreen(
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (impl_->state != RenderDeviceState::Ready) {
        return InvalidState("Render device is not ready");
    }
    Base::Result<void> rendered = impl_->RenderOffscreen(rendererToken, frame);
    if (!rendered) {
        ++impl_->statistics.failedFrameCount;
        if (ApplyBackendHealth(*impl_, impl_->GetDeviceHealth())) {
            ++impl_->statistics.generation;
        }
        return rendered.GetStatus();
    }
    return {};
}

Base::Result<RenderFrameStatistics> RenderDevice::Impl::BeginSurfaceFrame(
    RenderDevice& device,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    Base::Status ready = device.GetFrameStatus();
    if (!ready.IsOk()) return ready;
    Base::Result<RenderFrameStatistics> statistics = device.Analyze(frame);
    if (!statistics && device.impl_ != nullptr) {
        ++device.impl_->statistics.failedFrameCount;
    }
    return statistics;
}

void RenderDevice::Impl::CompleteSurfaceFrame(
    RenderDevice& device,
    const ::Aero::Render::Detail::RenderFrame& frame,
    RenderFrameStatistics& statistics) noexcept {
    if (device.impl_ == nullptr) return;
    device.MergeBackendStatistics(statistics);
    device.impl_->lastFrameStatistics = statistics;
    ++device.impl_->statistics.acceptedFrameCount;
    ++device.impl_->statistics.completedFrameCount;
    device.impl_->statistics.lastAcceptedVersion = frame.Version();
    device.impl_->statistics.lastCompletedVersion = frame.Version();
}

void RenderDevice::Impl::RefreshHealth(RenderDevice& device) noexcept {
    if (device.impl_ == nullptr) return;
    if (ApplyBackendHealth(*device.impl_, device.impl_->GetDeviceHealth())) {
        ++device.impl_->statistics.generation;
    }
}

void RenderDevice::Impl::RecordSurfaceFailure(RenderDevice& device) noexcept {
    if (device.impl_ == nullptr) return;
    ++device.impl_->statistics.failedFrameCount;
    RefreshHealth(device);
}

void RenderDevice::ReleaseRenderer(const void* rendererToken) noexcept {
    if (impl_ != nullptr) impl_->ReleaseRenderer(rendererToken);
}

Base::Status RenderDevice::GetFrameStatus() noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    switch (impl_->state) {
    case RenderDeviceState::Ready:
        return {};
    case RenderDeviceState::DeviceLost:
        return InvalidState("Render device is lost");
    case RenderDeviceState::Failed:
        return InvalidState("Render device has failed");
    case RenderDeviceState::Shutdown:
        return InvalidState("Render device is shut down");
    }
    return InvalidState("Render device state is invalid");
}

} // namespace Aero

namespace Aero::Diagnostics {

RenderDeviceStatistics GetRenderDeviceStatistics(
    const Aero::RenderDevice& device) noexcept {
    return device.impl_ != nullptr
        ? device.impl_->statistics
        : RenderDeviceStatistics{};
}

RenderFrameStatistics GetLastRenderFrameStatistics(
    const Aero::RenderDevice& device) noexcept {
    return device.impl_ != nullptr
        ? device.impl_->lastFrameStatistics
        : RenderFrameStatistics{};
}

} // namespace Aero::Diagnostics
