#include "integration/IntegrationPrivate.hpp"
#include "render/BatchPlanner.hpp"

#include <new>
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

bool ApplyBackendHealth(
    RenderDevice::Impl& device,
    Integration::Detail::BackendHealth health) noexcept {
    const RenderDeviceState previous = device.state;
    switch (health) {
    case Integration::Detail::BackendHealth::Ready:
        device.state = RenderDeviceState::Ready;
        break;
    case Integration::Detail::BackendHealth::DeviceLost:
        device.state = RenderDeviceState::DeviceLost;
        break;
    case Integration::Detail::BackendHealth::Failed:
        device.state = RenderDeviceState::Failed;
        break;
    }
    return previous != device.state;
}


} // namespace

RenderDevice::RenderDevice(
    ConstructionToken,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    void* memory = selected.Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    }
    impl_ = new (memory) Impl(selected);
}

RenderDevice::~RenderDevice() noexcept {
    if (impl_ == nullptr) return;
    const auto* functions = Impl::Functions(*this);
    if (impl_->stateData != nullptr && functions != nullptr) {
        static_cast<void>(functions->waitIdle(impl_->stateData, 5000U));
        functions->destroy(impl_->stateData);
        impl_->stateData = nullptr;
        impl_->functions = nullptr;
        impl_->surfaceFunctions = nullptr;
    }
    impl_->state = RenderDeviceState::Shutdown;
    Base::IAllocator* allocator = impl_->allocator;
    impl_->~Impl();
    allocator->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

RenderDeviceState RenderDevice::State() const noexcept {
    return impl_ != nullptr ? impl_->state : RenderDeviceState::Shutdown;
}

std::uint64_t RenderDevice::Generation() const noexcept {
    return impl_ != nullptr ? impl_->statistics.generation : 0U;
}

RenderDeviceStatistics RenderDevice::Statistics() const noexcept {
    return impl_ != nullptr ? impl_->statistics : RenderDeviceStatistics{};
}

RenderFrameStatistics RenderDevice::LastFrameStatistics() const noexcept {
    return impl_ != nullptr
        ? impl_->lastFrameStatistics
        : RenderFrameStatistics{};
}

Base::Result<RenderFrameStatistics> RenderDevice::Analyze(
    const Integration::RenderFrame& frame) noexcept {
    Base::Result<void> valid = Integration::ValidateRenderFrame(frame);
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
    const RenderFrameStatistics native =
        impl_->stateData != nullptr && Impl::Functions(*this) != nullptr
        ? Impl::Functions(*this)->statistics(impl_->stateData)
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
    const auto* functions = Impl::Functions(*this);
    if (impl_ == nullptr || impl_->stateData == nullptr || functions == nullptr ||
        impl_->state != RenderDeviceState::Ready) {
        return;
    }
    impl_->state = RenderDeviceState::DeviceLost;
    ++impl_->statistics.generation;
    functions->deviceLost(impl_->stateData);
}

Base::Result<void> RenderDevice::Restore() noexcept {
    const auto* functions = Impl::Functions(*this);
    if (impl_ == nullptr || impl_->stateData == nullptr || functions == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (impl_->state != RenderDeviceState::DeviceLost) {
        return InvalidState("Only a lost render device can be restored");
    }

    Base::Result<void> restored = functions->restoreDevice(impl_->stateData);
    if (!restored) {
        ++impl_->statistics.failedFrameCount;
        const auto health = functions->health != nullptr
            ? functions->health(impl_->stateData)
            : Integration::Detail::BackendHealth::Failed;
        if (ApplyBackendHealth(*impl_, health)) {
            ++impl_->statistics.generation;
        }
        return restored.GetStatus();
    }
    ApplyBackendHealth(*impl_, Integration::Detail::BackendHealth::Ready);
    return {};
}

Base::Result<void> RenderDevice::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    const auto* functions = Impl::Functions(*this);
    return impl_ != nullptr && impl_->stateData != nullptr && functions != nullptr
        ? functions->waitIdle(impl_->stateData, timeoutMilliseconds)
        : Base::Result<void>(
              NotInitialized("Render device is not initialized"));
}

} // namespace Aero

namespace Aero::Integration::Detail {

class HeadlessDeviceState {
public:
    Base::Result<void> RenderOffscreen(
        const void*,
        const ::Aero::Integration::RenderFrame&) noexcept { return {}; }
    Base::Result<void> Render(
        const void*,
        const ::Aero::Integration::RenderFrame&) noexcept { return {}; }
    void ReleaseRenderer(const void*) noexcept {}
    Base::Result<void> Resize(
        std::uint32_t,
        std::uint32_t) noexcept { return {}; }
    void NotifySurfaceLost() noexcept {}
    void NotifyDeviceLost() noexcept {}
    Base::Result<void> RestoreDevice() noexcept { return {}; }
    Base::Result<void> RestoreSurface() noexcept { return {}; }
    Base::Result<void> WaitIdle(std::uint32_t) noexcept { return {}; }
    BackendHealth GetDeviceHealth() const noexcept { return BackendHealth::Ready; }
    SurfaceHealth GetSurfaceHealth() const noexcept {
        return SurfaceHealth::Shutdown;
    }
    ::Aero::RenderFrameStatistics
    LastFrameStatistics() const noexcept { return {}; }
    ::Aero::Render::Detail::RenderResources Resources() noexcept { return {}; }
};

Base::Result<Base::Ref<::Aero::RenderDevice>>
RenderDeviceFactory::Adopt(
    RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    const RenderSurfaceFunctions* surfaceFunctions,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::RenderDevice::Impl::Create(
        mode, state, functions, surfaceFunctions, allocator);
}

Base::Result<Base::Ref<::Aero::RenderDevice>>
AdoptRenderDevice(
    RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    const RenderSurfaceFunctions* surfaceFunctions,
    Base::IAllocator* allocator) noexcept {
    return RenderDeviceFactory::Adopt(
        mode, state, functions, surfaceFunctions, allocator);
}

Base::Result<Base::Ref<::Aero::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator) noexcept {
    auto* backend = new (std::nothrow) HeadlessDeviceState();
    if (backend == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the headless render device");
    }
    return AdoptRenderDevice(
        RenderDeviceMode::Headless,
        backend,
        allocator);
}

} // namespace Aero::Integration::Detail

namespace Aero {

Base::Result<void> RenderDevice::RenderOffscreen(
    const void* rendererToken,
    const Integration::RenderFrame& frame) noexcept {
    const auto* functions = Impl::Functions(*this);
    if (impl_ == nullptr || impl_->stateData == nullptr || functions == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (impl_->state != RenderDeviceState::Ready) {
        return InvalidState("Render device is not ready");
    }
    Base::Result<void> rendered = functions->renderOffscreen(
        impl_->stateData, rendererToken, frame);
    if (!rendered) {
        ++impl_->statistics.failedFrameCount;
        const auto health = functions->health != nullptr
            ? functions->health(impl_->stateData)
            : Integration::Detail::BackendHealth::Failed;
        if (ApplyBackendHealth(*impl_, health)) {
            ++impl_->statistics.generation;
        }
        return rendered.GetStatus();
    }
    return {};
}

Base::Result<RenderFrameStatistics>
RenderDevice::Impl::BeginSurfaceFrame(
    RenderDevice& device,
    const Integration::RenderFrame& frame) noexcept {
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
    const Integration::RenderFrame& frame,
    RenderFrameStatistics& statistics) noexcept {
    if (device.impl_ == nullptr) return;
    device.MergeBackendStatistics(statistics);
    device.impl_->lastFrameStatistics = statistics;
    ++device.impl_->statistics.acceptedFrameCount;
    ++device.impl_->statistics.completedFrameCount;
    device.impl_->statistics.lastAcceptedVersion = frame.Version();
    device.impl_->statistics.lastCompletedVersion = frame.Version();
}

void RenderDevice::Impl::RefreshHealth(
    RenderDevice& device) noexcept {
    if (device.impl_ == nullptr) return;
    const auto* functions = Functions(device);
    const auto health = device.impl_->stateData != nullptr &&
            functions != nullptr && functions->health != nullptr
        ? functions->health(device.impl_->stateData)
        : Integration::Detail::BackendHealth::Failed;
    if (ApplyBackendHealth(*device.impl_, health)) {
        ++device.impl_->statistics.generation;
    }
}

void RenderDevice::Impl::RecordSurfaceFailure(
    RenderDevice& device) noexcept {
    if (device.impl_ == nullptr) return;
    ++device.impl_->statistics.failedFrameCount;
    RefreshHealth(device);
}

void RenderDevice::ReleaseRenderer(
    const void* rendererToken) noexcept {
    const auto* functions = Impl::Functions(*this);
    if (impl_ != nullptr && impl_->stateData != nullptr && functions != nullptr &&
        functions->releaseRenderer != nullptr) {
        functions->releaseRenderer(impl_->stateData, rendererToken);
    }
}

Base::Status RenderDevice::GetFrameStatus() noexcept {
    if (impl_ == nullptr || impl_->stateData == nullptr ||
        Impl::Functions(*this) == nullptr) {
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
