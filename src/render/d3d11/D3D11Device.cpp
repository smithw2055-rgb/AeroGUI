#include "render/RenderDeviceState.hpp"
#include "render/RenderTargetState.hpp"
#include "gui/ViewRenderer.hpp"
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

class D3D11RenderTargetState final : public Aero::RenderTarget::Access {
public:
    D3D11RenderTargetState(
        Graphics::D3D11RenderDevice& device,
        const D3D11EmbeddedTargetOptions& options) noexcept
        : Aero::RenderTarget::Access(RenderTargetKind::Embedded),
          device_(&device),
          options_(options) {}

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady() ||
            options_.acquireTarget == nullptr) {
            health_ = SurfaceHealth::Lost;
            return NotInitialized("D3D11 target requires a ready render device and acquisition callback");
        }
        deviceGeneration_ = device_->BackendGeneration();
        health_ = SurfaceHealth::Ready;
        return {};
    }

    Base::Result<void> Render(
        ::Aero::ViewRenderer& renderer,
        const ::Aero::Render::RenderFrame& frame) noexcept override {
        if (!IsReady()) return InvalidState("D3D11 target is not ready");

        ::Aero::Render::D3D11EmbeddedTarget target;
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

        Base::Result<Graphics::FenceValue> submitted =
            renderer.RenderOnscreenFrame(
                frame,
                {imported.Value(), target.width, target.height,
                 options_.clearBeforeRender
                     ? Graphics::LoadOperation::Clear
                     : Graphics::LoadOperation::Load});
        const Graphics::FenceValue retireFence =
            device_->LastSubmittedFence();
        Base::Result<void> retired =
            static_cast<Aero::RenderDevice::Access&>(*device_).DestroyResource(
                imported.Value(), retireFence);
        if (!submitted) return submitted.GetStatus();
        return retired;
    }

    Base::Result<void> Resize(std::uint32_t, std::uint32_t) noexcept override {
        return {};
    }

    void NotifySurfaceLost() noexcept override {
        health_ = SurfaceHealth::Lost;
    }

    Base::Result<void> RestoreSurface() noexcept override {
        if (health_ != SurfaceHealth::Lost) {
            return InvalidState("Only a lost D3D11 target can be restored");
        }
        return Initialize();
    }

    SurfaceHealth GetSurfaceHealth() const noexcept override {
        if (device_ == nullptr) return SurfaceHealth::Shutdown;
        if (!device_->IsReady() ||
            deviceGeneration_ != device_->BackendGeneration()) {
            return SurfaceHealth::Lost;
        }
        return health_;
    }

private:
    bool IsReady() const noexcept {
        return GetSurfaceHealth() == SurfaceHealth::Ready &&
            device_->IsReady();
    }

    Graphics::D3D11RenderDevice* device_ = nullptr;
    D3D11EmbeddedTargetOptions options_;
    std::uint64_t deviceGeneration_ = 0U;
    SurfaceHealth health_ = SurfaceHealth::Shutdown;
};

Graphics::D3D11RenderDevice* DeviceFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device || Aero::RenderDevice::Access::Backend(*device) !=
            RenderBackendKind::D3D11) {
        return nullptr;
    }
    return static_cast<Graphics::D3D11RenderDevice*>(
        Aero::RenderDevice::Access::BackendState(*device));
}

} // namespace

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const ::Aero::Render::D3D11DeviceOptions& options,
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
            D3D11StatePreservationPolicy::PreserveRequiredState
        ? Graphics::D3D11StatePolicy::PreserveRequiredState
        : Graphics::D3D11StatePolicy::HostResetsState;
    nativeOptions.enableDebugLayer = options.enableDebugLayer;
    nativeOptions.allowWarpFallback = options.allowWarpFallback;
    nativeOptions.borrowedDevice = options.device;
    nativeOptions.borrowedImmediateContext = options.immediateContext;
    auto* state = new (std::nothrow)
        Graphics::D3D11RenderDevice(nativeOptions, &selected);
    if (state == nullptr) {
        return OutOfMemory("Unable to allocate the D3D11 render device");
    }
    Base::Result<void> initialized = state->Initialize();
    if (!initialized) {
        delete state;
        return initialized.GetStatus();
    }
    return AdoptRenderDevice(state, &selected);
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
    auto* target = new (std::nothrow)
        D3D11RenderTargetState(*backend, options);
    if (target == nullptr) {
        return OutOfMemory("Unable to allocate the D3D11 render target");
    }
    Base::Result<void> initialized = target->Initialize();
    if (!initialized) {
        delete target;
        return initialized.GetStatus();
    }
    return AdoptRenderTarget(
        std::move(device), target, RenderTargetKind::Embedded, &selected);
}

} // namespace Aero::Render
