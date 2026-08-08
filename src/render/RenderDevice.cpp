#include "render/private/RenderDevice.hpp"
#include "render/private/RenderTarget.hpp"

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

    RenderFrameStatistics result;
    result.sourceCommandCount = frame.Commands().Size();
    result.batchingEnabled = true;
    return result;
}

void RenderDevice::MergeBackendStatistics(
    RenderFrameStatistics& result) const noexcept {
    const RenderFrameStatistics native = impl_ != nullptr
        ? impl_->LastFrameStatistics()
        : RenderFrameStatistics{};
    result.sourceCommandCount = native.sourceCommandCount != 0U
        ? native.sourceCommandCount
        : result.sourceCommandCount;
    result.drawPacketCount = native.drawPacketCount;
    result.batchCount = native.batchCount;
    result.mergedPacketCount = native.mergedPacketCount;
    result.barrierCount = native.barrierCount;
    result.batchingEnabled = native.batchingEnabled;
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
    Base::Result<::Aero::Graphics::FenceValue> DrawBatch(
        ::Aero::Render::Detail::RenderBatch&&) noexcept override {
        return ::Aero::Graphics::FenceValue{0U};
    }
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
        const ::Aero::Graphics::PipelineDescriptor&) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device has no pipelines");
    }
    Base::Result<void> SubmitNativeCommands(
        const ::Aero::Graphics::CommandList&,
        ::Aero::Graphics::FenceValue) noexcept override {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Headless render device cannot submit native commands");
    }
    ::Aero::Graphics::FenceValue
    NativeLastSubmittedFence() const noexcept override { return 0U; }
    ::Aero::Graphics::FenceValue
    NativeCompletedFence() const noexcept override { return 0U; }
    bool NativeDeviceLost() const noexcept override { return false; }
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
