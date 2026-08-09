#include "render/RenderDeviceState.hpp"
#include "render/RenderTargetState.hpp"

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
    ::Aero::Render::RenderDeviceBase& device,
    RenderBackendHealth health) noexcept {
    const RenderDeviceState previous = device.state;
    switch (health) {
    case RenderBackendHealth::Ready:
        device.state = RenderDeviceState::Ready;
        break;
    case RenderBackendHealth::DeviceLost:
        device.state = RenderDeviceState::DeviceLost;
        break;
    case RenderBackendHealth::Failed:
        device.state = RenderDeviceState::Failed;
        break;
    }
    return previous != device.state;
}

} // namespace

RenderDevice::~RenderDevice() noexcept = default;

RenderDeviceState RenderDevice::State() const noexcept {
    return Render::RenderDeviceBase::From(*this)->state;
}

RenderBackendKind RenderDevice::Backend() const noexcept {
    return BackendKind();
}

std::uint64_t RenderDevice::Generation() const noexcept {
    return Render::RenderDeviceBase::From(*this)->statistics.generation;
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
    Render::RenderDeviceBase& implementation =
        *Render::RenderDeviceBase::From(*this);
    if (implementation.state != RenderDeviceState::Ready) return;
    implementation.state = RenderDeviceState::DeviceLost;
    ++implementation.statistics.generation;
    NotifyBackendDeviceLost();
}

Base::Result<void> RenderDevice::Restore() noexcept {
    Render::RenderDeviceBase& implementation =
        *Render::RenderDeviceBase::From(*this);
    if (implementation.state != RenderDeviceState::DeviceLost) {
        return InvalidState("Only a lost render device can be restored");
    }

    Base::Result<void> restored = RestoreBackendDevice();
    if (!restored) {
        ++implementation.statistics.failedFrameCount;
        if (ApplyBackendHealth(implementation, BackendHealth())) {
            ++implementation.statistics.generation;
        }
        return restored.GetStatus();
    }
    ApplyBackendHealth(implementation, RenderBackendHealth::Ready);
    return {};
}

Base::Result<void> RenderDevice::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    return WaitBackendIdle(timeoutMilliseconds);
}

} // namespace Aero

namespace Aero::Render {

class HeadlessDeviceState final : public RenderDeviceBase {
public:
    explicit HeadlessDeviceState(Base::IAllocator& allocator) noexcept
        : RenderDeviceBase(allocator) {}

    Aero::RenderBackendKind BackendKind() const noexcept override {
        return Aero::RenderBackendKind::Headless;
    }
    Base::Result<::Aero::Graphics::FenceValue> DrawBatch(
        ::Aero::Render::RenderBatch&&) noexcept override {
        return ::Aero::Graphics::FenceValue{0U};
    }
    void NotifyBackendDeviceLost() noexcept override {}
    Base::Result<void> RestoreBackendDevice() noexcept override { return {}; }
    Base::Result<void> WaitBackendIdle(std::uint32_t) noexcept override {
        return {};
    }
    Aero::RenderBackendHealth BackendHealth() const noexcept override {
        return Aero::RenderBackendHealth::Ready;
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

Base::Result<Base::Ref<Aero::RenderDevice>> CreateHeadlessRenderDevice(
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<HeadlessDeviceState>> made =
        Base::MakeRefWithAllocator<HeadlessDeviceState>(selected, selected);
    if (!made) return made.GetStatus();
    Base::Ref<HeadlessDeviceState> backend = std::move(made).Value();
    return Base::Ref<Aero::RenderDevice>(std::move(backend));
}

} // namespace Aero::Render

namespace Aero::Render {

Base::Result<RenderFrameStatistics> RenderDeviceBase::BeginSurfaceFrame(
    Aero::RenderDevice& device,
    const ::Aero::Render::RenderFrame& frame) noexcept {
    Base::Status ready = device.GetFrameStatus();
    if (!ready.IsOk()) return ready;
    Base::Result<RenderFrameStatistics> statistics = device.Analyze(frame);
    RenderDeviceBase& implementation = *From(device);
    if (!statistics) {
        ++implementation.statistics.failedFrameCount;
    }
    return statistics;
}

void RenderDeviceBase::CompleteSurfaceFrame(
    Aero::RenderDevice& device,
    const ::Aero::Render::RenderFrame& frame,
    RenderFrameStatistics& statistics) noexcept {
    RenderDeviceBase& implementation = *From(device);
    implementation.lastFrameStatistics = statistics;
    ++implementation.statistics.acceptedFrameCount;
    ++implementation.statistics.completedFrameCount;
    implementation.statistics.lastAcceptedVersion = frame.Version();
    implementation.statistics.lastCompletedVersion = frame.Version();
}

void RenderDeviceBase::RefreshHealth(Aero::RenderDevice& device) noexcept {
    RenderDeviceBase& implementation = *From(device);
    if (ApplyBackendHealth(implementation, device.BackendHealth())) {
        ++implementation.statistics.generation;
    }
}

void RenderDeviceBase::RecordSurfaceFailure(Aero::RenderDevice& device) noexcept {
    RenderDeviceBase& implementation = *From(device);
    ++implementation.statistics.failedFrameCount;
    RefreshHealth(device);
}

} // namespace Aero::Render

namespace Aero {

Base::Status RenderDevice::GetFrameStatus() noexcept {
    const Render::RenderDeviceBase& implementation =
        *Render::RenderDeviceBase::From(*this);
    switch (implementation.state) {
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
    return Aero::Render::RenderDeviceBase::From(device)->statistics;
}

RenderFrameStatistics GetLastRenderFrameStatistics(
    const Aero::RenderDevice& device) noexcept {
    return Aero::Render::RenderDeviceBase::From(device)->lastFrameStatistics;
}

} // namespace Aero::Diagnostics
