#include "RenderDeviceInternal.hpp"
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
    if (stateData_ != nullptr && functions_ != nullptr) {
        static_cast<void>(functions_->waitIdle(stateData_, 5000U));
        functions_->destroy(stateData_);
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
        stateData_ != nullptr && functions_ != nullptr
        ? functions_->statistics(stateData_)
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
    if (stateData_ == nullptr || functions_ == nullptr) {
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
    return idle ? functions_->resize(stateData_, width, height) : idle;
}

void RenderDevice::NotifySurfaceLost() noexcept {
    if (stateData_ == nullptr || functions_ == nullptr) {
        return;
    }
    if (state_ != RenderDeviceState::Ready) {
        return;
    }
    state_ = RenderDeviceState::SurfaceLost;
    ++statistics_.generation;
    functions_->surfaceLost(stateData_);
}

void RenderDevice::NotifyDeviceLost() noexcept {
    if (stateData_ == nullptr || functions_ == nullptr) {
        return;
    }
    if (state_ != RenderDeviceState::Ready) {
        return;
    }
    state_ = RenderDeviceState::DeviceLost;
    ++statistics_.generation;
    functions_->deviceLost(stateData_);
}

Base::Result<void> RenderDevice::Restore() noexcept {
    if (stateData_ == nullptr || functions_ == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (state_ != RenderDeviceState::SurfaceLost &&
        state_ != RenderDeviceState::DeviceLost) {
        return InvalidState("Only a lost render device can be restored");
    }

    Base::Result<void> restored = functions_->restore(stateData_);
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
    return stateData_ != nullptr && functions_ != nullptr
        ? functions_->waitIdle(stateData_, timeoutMilliseconds)
        : Base::Result<void>(
              NotInitialized("Render device is not initialized"));
}

} // namespace Aero::Integration

namespace Aero::Internal {

class HeadlessDeviceState final {
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
    RenderResources Resources() noexcept { return {}; }
};

Base::Result<Base::Ref<::Aero::Integration::RenderDevice>>
RenderDeviceFactory::Adopt(
    ::Aero::Integration::RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    Base::IAllocator* allocator) noexcept {
    if (state == nullptr || functions == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render device implementation is required");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<::Aero::Integration::RenderDevice>> made =
        Base::MakeRefWithAllocator<::Aero::Integration::RenderDevice>(
            selected,
            ::Aero::Integration::RenderDevice::ConstructionToken{},
            mode,
            &selected);
    if (!made) {
        functions->destroy(state);
        return made.GetStatus();
    }
    made.Value()->stateData_ = state;
    made.Value()->functions_ = functions;
    return std::move(made).Value();
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

} // namespace Aero::Internal

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
    if (stateData_ == nullptr || functions_ == nullptr) {
        return NotInitialized("Render device is not initialized");
    }
    if (state_ != RenderDeviceState::Ready) {
        return InvalidState("Render device is not ready");
    }

    Base::Result<RenderFrameStatistics> frameStatistics = Analyze(frame);
    if (!frameStatistics) return frameStatistics.GetStatus();

    Base::Result<void> submitted = functions_->submit(stateData_, frame);
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
    if (stateData_ == nullptr || functions_ == nullptr) {
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

Aero::Internal::RenderResources RenderDevice::Resources() noexcept {
    return stateData_ != nullptr && functions_ != nullptr
        ? functions_->resources(stateData_)
        : Aero::Internal::RenderResources{};
}

} // namespace Aero::Integration
