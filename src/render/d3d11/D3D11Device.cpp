#include "render/RenderDeviceState.hpp"
#include "render/RenderTargetState.hpp"
#include "render/d3d11/D3D11RenderDevice.hpp"
#include "render/d3d11/D3D11Shaders.hpp"

#include <new>
#include <utility>

namespace Aero::Render {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, message);
}

class D3D11RenderTargetState final : public RenderTargetBase {
public:
    D3D11RenderTargetState(
        Base::Ref<Aero::RenderDevice> owner,
        Graphics::D3D11RenderDevice& device,
        const D3D11EmbeddedTargetOptions& options) noexcept
        : RenderTargetBase(std::move(owner), RenderTargetKind::Embedded),
          device_(&device),
          options_(options) {}

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady() ||
            options_.acquireTarget == nullptr) {
            health_ = RenderTargetState::Lost;
            return NotInitialized("D3D11 target requires a ready render device and acquisition callback");
        }
        deviceGeneration_ = RenderDeviceBase::BackendGeneration(*device_);
        health_ = RenderTargetState::Ready;
        return {};
    }

    Base::Result<FrameTarget> AcquireFrameTarget() noexcept override {
        if (!IsReady()) return InvalidState("D3D11 target is not ready");

        ::Aero::Render::D3D11::EmbeddedTarget target;
        Base::Status acquired = options_.acquireTarget(
            options_.callbackContext, &target);
        if (!acquired.IsOk()) return acquired;
        if (target.texture2D == 0U || target.width == 0U ||
            target.height == 0U) {
            return InvalidArgument("D3D11 acquired target is invalid");
        }

        Graphics::D3D11RenderTargetBinding external;
        external.texture2D = target.texture2D;
        external.renderTargetView = target.renderTargetView;
        external.depthStencilView = target.depthStencilView;
        external.texture.width = target.width;
        external.texture.height = target.height;
        external.texture.sampleCount = 1U;
        external.texture.format = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        external.texture.usage =
            Graphics::TextureUsageBit(Graphics::TextureUsage::RenderTarget) |
            Graphics::TextureUsageBit(Graphics::TextureUsage::CopySource);
        external.stableId = target.stableId;
        Base::Result<Graphics::ResourceHandle> imported =
            Graphics::ImportD3D11ExternalRenderTarget(
                *device_, *device_, external);
        if (!imported) return imported.GetStatus();

        return FrameTarget{
            imported.Value(), target.width, target.height,
            options_.clearBeforeRender
                ? Graphics::LoadOperation::Clear
                : Graphics::LoadOperation::Load};
    }

    Base::Result<void> RetireFrameTarget(
        const FrameTarget& target) noexcept override {
        const Graphics::FenceValue retireFence =
            device_->LastSubmittedFence();
        return static_cast<RenderDeviceBase&>(*device_).DestroyResource(
            target.color, retireFence);
    }

    Base::Result<void> ResizeBackend(
        std::uint32_t, std::uint32_t) noexcept override {
        return {};
    }

    void NotifyBackendLost() noexcept override {
        health_ = RenderTargetState::Lost;
    }

    Base::Result<void> RestoreBackend() noexcept override {
        if (health_ != RenderTargetState::Lost) {
            return InvalidState("Only a lost D3D11 target can be restored");
        }
        return Initialize();
    }

    RenderTargetState BackendState() const noexcept override {
        if (device_ == nullptr) return RenderTargetState::Shutdown;
        if (!device_->IsReady() ||
            deviceGeneration_ != RenderDeviceBase::BackendGeneration(*device_)) {
            return RenderTargetState::Lost;
        }
        return health_;
    }

private:
    bool IsReady() const noexcept {
        return BackendState() == RenderTargetState::Ready &&
            device_->IsReady();
    }

    Graphics::D3D11RenderDevice* device_ = nullptr;
    D3D11EmbeddedTargetOptions options_;
    std::uint64_t deviceGeneration_ = 0U;
    RenderTargetState health_ = RenderTargetState::Shutdown;
};

Graphics::D3D11RenderDevice* DeviceFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device || RenderDeviceBase::Backend(*device) !=
            Aero::RenderBackendKind::D3D11) {
        return nullptr;
    }
    return static_cast<Graphics::D3D11RenderDevice*>(device.Get());
}

} // namespace

Base::Result<Base::Ref<Aero::RenderDevice>> D3D11::CreateDevice(
    const ::Aero::Render::D3D11::DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if ((options.device == 0U) != (options.immediateContext == 0U)) {
        return InvalidArgument(
            "D3D11 borrowed device and immediate context must be provided together");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Graphics::D3D11RenderDeviceOptions nativeOptions;
    const bool borrowed = options.device != 0U;
    nativeOptions.deviceMode = borrowed
        ? Graphics::D3D11DeviceMode::Borrowed
        : (options.useWarp
            ? Graphics::D3D11DeviceMode::Warp
            : Graphics::D3D11DeviceMode::Hardware);
    nativeOptions.statePolicy =
        options.statePolicy ==
            D3D11::StatePreservationPolicy::PreserveRequiredState
        ? Graphics::D3D11StatePolicy::PreserveRequiredState
        : Graphics::D3D11StatePolicy::HostResetsState;
    nativeOptions.enableDebugLayer = options.enableDebugLayer;
    nativeOptions.allowWarpFallback = options.allowWarpFallback;
    nativeOptions.borrowedDevice = options.device;
    nativeOptions.borrowedImmediateContext = options.immediateContext;
    Base::Result<Base::Ref<Graphics::D3D11RenderDevice>> made =
        Base::MakeRefWithAllocator<Graphics::D3D11RenderDevice>(
            selected, nativeOptions, &selected);
    if (!made) return made.GetStatus();
    Base::Ref<Graphics::D3D11RenderDevice> state =
        std::move(made).Value();
    Base::Result<void> initialized = state->Initialize();
    if (!initialized) {
        return initialized.GetStatus();
    }
    return Base::Ref<Aero::RenderDevice>(std::move(state));
}

Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    Graphics::D3D11RenderDevice* backend = DeviceFrom(device);
    if (backend == nullptr || options.acquireTarget == nullptr) {
        return InvalidArgument(
            "D3D11 target requires a matching device and acquisition callback");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<D3D11RenderTargetState>> made =
        Base::MakeRefWithAllocator<D3D11RenderTargetState>(
            selected, device, *backend, options);
    if (!made) return made.GetStatus();
    Base::Ref<D3D11RenderTargetState> target =
        std::move(made).Value();
    Base::Result<void> initialized = target->Initialize();
    if (!initialized) {
        return initialized.GetStatus();
    }
    return Base::Ref<Aero::RenderTarget>(std::move(target));
}

} // namespace Aero::Render
