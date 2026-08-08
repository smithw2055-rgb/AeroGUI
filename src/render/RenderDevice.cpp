#include "render/RenderDeviceState.hpp"
#include "render/RenderTargetState.hpp"

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
    RenderDevice::Access& device,
    ::Aero::Render::BackendHealth health) noexcept {
    const RenderDeviceState previous = device.state;
    switch (health) {
    case ::Aero::Render::BackendHealth::Ready:
        device.state = RenderDeviceState::Ready;
        break;
    case ::Aero::Render::BackendHealth::DeviceLost:
        device.state = RenderDeviceState::DeviceLost;
        break;
    case ::Aero::Render::BackendHealth::Failed:
        device.state = RenderDeviceState::Failed;
        break;
    }
    return previous != device.state;
}

} // namespace

RenderDevice::RenderDevice(
    ConstructionToken,
    Access* implementation) noexcept
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
    const ::Aero::Render::RenderFrame& frame) noexcept {
    Base::Result<void> valid = ::Aero::Render::ValidateRenderFrame(frame);
    if (!valid) return valid.GetStatus();

    RenderFrameStatistics result;
    result.sourceCommandCount = frame.Commands().Size();
    result.batchingEnabled = true;
    return result;
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
    ApplyBackendHealth(*impl_, ::Aero::Render::BackendHealth::Ready);
    return {};
}

Base::Result<void> RenderDevice::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    return impl_ != nullptr
        ? impl_->WaitIdle(timeoutMilliseconds)
        : Base::Result<void>(NotInitialized("Render device is not initialized"));
}

} // namespace Aero

namespace Aero::Render {

class HeadlessDeviceState final : public Aero::RenderDevice::Access {
public:
    explicit HeadlessDeviceState(Base::IAllocator& allocator) noexcept
        : Aero::RenderDevice::Access(allocator) {}

    RenderBackendKind Backend() const noexcept override {
        return RenderBackendKind::Headless;
    }
    Base::Result<::Aero::Graphics::FenceValue> DrawBatch(
        ::Aero::Render::RenderBatch&&) noexcept override {
        return ::Aero::Graphics::FenceValue{0U};
    }
    void NotifyDeviceLost() noexcept override {}
    Base::Result<void> RestoreDevice() noexcept override { return {}; }
    Base::Result<void> WaitIdle(std::uint32_t) noexcept override { return {}; }
    BackendHealth GetDeviceHealth() const noexcept override {
        return BackendHealth::Ready;
    }
    ::Aero::Graphics::DeviceCapabilities
    QueryNativeDeviceCapabilities() const noexcept override { return {}; }
    ::Aero::Graphics::NativeRenderBackendKind
    NativeBackendKind() const noexcept override {
        return ::Aero::Graphics::NativeRenderBackendKind::Invalid;
    }
    ::Aero::Graphics::GraphicsCapabilities
    QueryNativeGraphicsCapabilities() const noexcept override { return {}; }
    Base::Result<void> CreateNativeResource(
        ::Aero::Graphics::ResourceHandle,
        const ::Aero::Graphics::ResourceDescriptor&) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device has no native resources");
    }
    void DestroyNativeResource(
        ::Aero::Graphics::ResourceHandle) noexcept override {}
    Base::Result<void> ConfigureNativeTexture(
        ::Aero::Graphics::ResourceHandle,
        const ::Aero::Graphics::TextureResourceDescriptor&) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device has no textures");
    }
    Base::Result<void> ConfigureNativeSampler(
        ::Aero::Graphics::ResourceHandle,
        const ::Aero::Graphics::SamplerDescriptor&) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device has no samplers");
    }
    Base::Result<void> ConfigureNativePipeline(
        ::Aero::Graphics::ResourceHandle,
        ::Aero::Render::UiPipelineKey) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device has no pipelines");
    }
    Base::Result<void> SubmitNativeBatch(
        const ::Aero::Render::RenderBatch&,
        ::Aero::Graphics::ResourceHandle,
        ::Aero::Graphics::FenceValue) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device cannot submit native commands");
    }
    Base::Result<void> UpdateNativeBuffer(
        ::Aero::Graphics::ResourceHandle,
        std::uint64_t,
        Base::Span<const std::uint8_t>) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device has no buffers");
    }
    Base::Result<void> UpdateNativeTexture(
        ::Aero::Graphics::ResourceHandle,
        const ::Aero::Graphics::TextureRegion&,
        Base::Span<const std::uint8_t>) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device has no textures");
    }
    ::Aero::Graphics::FenceValue
    NativeLastSubmittedFence() const noexcept override { return 0U; }
    ::Aero::Graphics::FenceValue
    NativeCompletedFence() const noexcept override { return 0U; }
    bool NativeDeviceLost() const noexcept override { return false; }
};

Base::Result<Base::Ref<Aero::RenderDevice>> AdoptRenderDevice(
    Aero::RenderDevice::Access* backend,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::RenderDevice::Access::Create(backend, allocator);
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

} // namespace Aero::Render

namespace Aero {

Base::Result<RenderFrameStatistics> RenderDevice::Access::BeginSurfaceFrame(
    RenderDevice& device,
    const ::Aero::Render::RenderFrame& frame) noexcept {
    Base::Status ready = device.GetFrameStatus();
    if (!ready.IsOk()) return ready;
    Base::Result<RenderFrameStatistics> statistics = device.Analyze(frame);
    if (!statistics && device.impl_ != nullptr) {
        ++device.impl_->statistics.failedFrameCount;
    }
    return statistics;
}

void RenderDevice::Access::CompleteSurfaceFrame(
    RenderDevice& device,
    const ::Aero::Render::RenderFrame& frame,
    RenderFrameStatistics& statistics) noexcept {
    if (device.impl_ == nullptr) return;
    device.impl_->lastFrameStatistics = statistics;
    ++device.impl_->statistics.acceptedFrameCount;
    ++device.impl_->statistics.completedFrameCount;
    device.impl_->statistics.lastAcceptedVersion = frame.Version();
    device.impl_->statistics.lastCompletedVersion = frame.Version();
}

void RenderDevice::Access::RefreshHealth(RenderDevice& device) noexcept {
    if (device.impl_ == nullptr) return;
    if (ApplyBackendHealth(*device.impl_, device.impl_->GetDeviceHealth())) {
        ++device.impl_->statistics.generation;
    }
}

void RenderDevice::Access::RecordSurfaceFailure(RenderDevice& device) noexcept {
    if (device.impl_ == nullptr) return;
    ++device.impl_->statistics.failedFrameCount;
    RefreshHealth(device);
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
