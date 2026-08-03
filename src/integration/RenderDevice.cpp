#include "integration/IntegrationPrivate.hpp"
#include "render/BatchPlanner.hpp"

#include <new>
#include <utility>

namespace Aero::Integration {
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

RenderDevice::RenderDevice(
    ConstructionToken,
    RenderDeviceMode mode,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      mode_(mode) {}

RenderDevice::~RenderDevice() noexcept {
    const auto* functions = Impl::Functions(*this);
    if (stateData_ != nullptr && functions != nullptr) {
        static_cast<void>(functions->waitIdle(stateData_, 5000U));
        functions->destroy(stateData_);
        stateData_ = nullptr;
        functions_ = nullptr;
    }
    state_ = RenderDeviceState::Shutdown;
}

Base::Result<RenderFrameStatistics> RenderDevice::Analyze(
    const Integration::RenderFrame& frame) noexcept {
    Base::Result<void> valid = Integration::ValidateRenderFrame(frame);
    if (!valid) return valid.GetStatus();

    Render::Detail::BatchPlanner planner(allocator_);
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
        stateData_ != nullptr && Impl::Functions(*this) != nullptr
        ? Impl::Functions(*this)->statistics(stateData_)
        : RenderFrameStatistics{};
    result.drawCallCount = native.drawCallCount != 0U
        ? native.drawCallCount
        : result.batchCount;
    result.instanceCount = native.instanceCount != 0U
        ? native.instanceCount
        : result.drawPacketCount;
    result.stateBindingCount = native.stateBindingCount;
}

Base::Result<void> RenderDevice::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    const auto* functions = Impl::Functions(*this);
    if (stateData_ == nullptr || functions == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render device dimensions must be nonzero");
    }
    if (state_ != RenderDeviceState::Ready) {
        return InvalidState(
            "Render device cannot resize in its current state");
    }
    Base::Result<void> idle = WaitIdle();
    return idle ? functions->resize(stateData_, width, height) : idle;
}

void RenderDevice::NotifySurfaceLost() noexcept {
    const auto* functions = Impl::Functions(*this);
    if (stateData_ == nullptr || functions == nullptr) {
        return;
    }
    if (state_ != RenderDeviceState::Ready) {
        return;
    }
    state_ = RenderDeviceState::SurfaceLost;
    ++statistics_.generation;
    functions->surfaceLost(stateData_);
}

void RenderDevice::NotifyDeviceLost() noexcept {
    const auto* functions = Impl::Functions(*this);
    if (stateData_ == nullptr || functions == nullptr) {
        return;
    }
    if (state_ != RenderDeviceState::Ready) {
        return;
    }
    state_ = RenderDeviceState::DeviceLost;
    ++statistics_.generation;
    functions->deviceLost(stateData_);
}

Base::Result<void> RenderDevice::Restore() noexcept {
    const auto* functions = Impl::Functions(*this);
    if (stateData_ == nullptr || functions == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (state_ != RenderDeviceState::SurfaceLost &&
        state_ != RenderDeviceState::DeviceLost) {
        return InvalidState("Only a lost render device can be restored");
    }

    Base::Result<void> restored = functions->restore(stateData_);
    if (!restored) {
        state_ = RenderDeviceState::Failed;
        ++statistics_.failedFrameCount;
        return restored.GetStatus();
    }
    state_ = RenderDeviceState::Ready;
    return {};
}

Base::Result<void> RenderDevice::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    const auto* functions = Impl::Functions(*this);
    return stateData_ != nullptr && functions != nullptr
        ? functions->waitIdle(stateData_, timeoutMilliseconds)
        : Base::Result<void>(
              NotInitialized("Render device is not initialized"));
}

} // namespace Aero::Integration

namespace Aero::Integration::Detail {

class HeadlessDeviceState {
public:
    Base::Result<void> Submit(
        const ::Aero::Integration::RenderFrame&) noexcept { return {}; }
    Base::Result<void> Resize(
        std::uint32_t,
        std::uint32_t) noexcept { return {}; }
    void NotifySurfaceLost() noexcept {}
    void NotifyDeviceLost() noexcept {}
    Base::Result<void> Restore() noexcept { return {}; }
    Base::Result<void> WaitIdle(std::uint32_t) noexcept { return {}; }
    ::Aero::Integration::RenderFrameStatistics
    LastFrameStatistics() const noexcept { return {}; }
    ::Aero::Render::Detail::RenderResources Resources() noexcept { return {}; }
};

Base::Result<Base::Ref<::Aero::Integration::RenderDevice>>
RenderDeviceFactory::Adopt(
    ::Aero::Integration::RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::Integration::RenderDevice::Impl::Create(
        mode, state, functions, allocator);
}

Base::Result<Base::Ref<::Aero::Integration::RenderDevice>>
AdoptRenderDevice(
    ::Aero::Integration::RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    Base::IAllocator* allocator) noexcept {
    return RenderDeviceFactory::Adopt(
        mode, state, functions, allocator);
}

Base::Result<Base::Ref<::Aero::Integration::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator) noexcept {
    auto* backend = new (std::nothrow) HeadlessDeviceState();
    if (backend == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the headless render device");
    }
    return AdoptRenderDevice(
        ::Aero::Integration::RenderDeviceMode::Headless,
        backend,
        allocator);
}

} // namespace Aero::Integration::Detail

namespace Aero::Integration {

Base::Result<void> RenderDevice::Bind(const void* owner) noexcept {
    if (owner == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render device binding requires an owner");
    }
    if (state_ != RenderDeviceState::Ready) {
        return InvalidState("Render device is not ready for binding");
    }
    if (owner_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Render device is already bound to a View");
    }
    owner_ = owner;
    return {};
}

void RenderDevice::Unbind(const void* owner) noexcept {
    if (owner == nullptr || owner_ != owner) return;
    static_cast<void>(WaitIdle());
    owner_ = nullptr;
}

Base::Result<void> RenderDevice::Submit(
    const Integration::RenderFrame& frame) noexcept {
    const auto* functions = Impl::Functions(*this);
    if (stateData_ == nullptr || functions == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (state_ != RenderDeviceState::Ready) {
        return InvalidState("Render device is not ready");
    }

    Base::Result<RenderFrameStatistics> frameStatistics = Analyze(frame);
    if (!frameStatistics) return frameStatistics.GetStatus();

    Base::Result<void> submitted = functions->submit(stateData_, frame);
    if (!submitted) {
        ++statistics_.failedFrameCount;
        state_ = RenderDeviceState::Failed;
        return submitted.GetStatus();
    }

    MergeBackendStatistics(frameStatistics.Value());
    lastFrameStatistics_ = frameStatistics.Value();
    ++statistics_.acceptedFrameCount;
    ++statistics_.completedFrameCount;
    statistics_.lastAcceptedVersion = frame.Version();
    statistics_.lastCompletedVersion = frame.Version();
    return {};
}

Base::Status RenderDevice::GetFrameStatus() noexcept {
    if (stateData_ == nullptr || Impl::Functions(*this) == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    switch (state_) {
    case RenderDeviceState::Ready:
        return {};
    case RenderDeviceState::SurfaceLost:
        return InvalidState("Render device surface is lost");
    case RenderDeviceState::DeviceLost:
        return InvalidState("Render device device is lost");
    case RenderDeviceState::Failed:
        return InvalidState("Render device has failed");
    case RenderDeviceState::Shutdown:
        return InvalidState("Render device is shut down");
    }
    return InvalidState("Render device state is invalid");
}

} // namespace Aero::Integration
