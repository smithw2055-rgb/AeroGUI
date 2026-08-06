#include <Aero/Integration/D3D11.hpp>

#include "integration/IntegrationPrivate.hpp"

#include "render/DeviceRenderer.hpp"
#include "render/d3d11/D3D11Shaders.hpp"
#include "render/d3d11/D3D11Backend.hpp"

#include <new>

namespace Aero::Integration {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Graphics::PresentMode ToRhiPresentMode(
    PresentMode value) noexcept {
    switch (value) {
    case PresentMode::Immediate:
        return Graphics::PresentMode::Immediate;
    case PresentMode::Mailbox:
        return Graphics::PresentMode::Mailbox;
    case PresentMode::Fifo:
        return Graphics::PresentMode::Fifo;
    }
    return Graphics::PresentMode::Fifo;
}

class D3D11EmbeddedSurface
    : public Graphics::ISurfaceBackend {
public:
    explicit D3D11EmbeddedSurface(
        const D3D11EmbeddedSurfaceOptions& options) noexcept
        : options_(options) {}

    Graphics::SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept {
        Graphics::SurfaceCapabilities result;
        result.supportedKinds =
            Graphics::SurfaceKindBit(
                Graphics::SurfaceKind::ExternalRenderTarget);
        result.supportsResize = false;
        result.supportsPresent = false;
        result.supportsContextLossRecovery = true;
        result.supportsExternalRenderTargets = true;
        return result;
    }

    Base::Result<void> CreateSurface(
        const Graphics::NativeSurfaceDescriptor&) noexcept {
        lost_ = false;
        return {};
    }

    void DestroySurface() noexcept {
        lost_ = true;
    }

    Base::Result<void> ResizeSurface(
        std::uint32_t,
        std::uint32_t) noexcept {
        return {};
    }

    Base::Result<Graphics::ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(
        std::uint64_t) noexcept {
        if (lost_ || options_.acquireTarget == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "D3D11 embedded surface is unavailable");
        }
        D3D11EmbeddedTarget native;
        Base::Status acquired =
            options_.acquireTarget(
                options_.callbackContext, &native);
        if (!acquired.IsOk()) return acquired;
        if (native.texture2D == 0U ||
            native.width == 0U ||
            native.height == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "D3D11 embedded target is invalid");
        }
        Graphics::ExternalRenderTargetDescriptor result;
        result.colorTarget = native.texture2D;
        result.depthStencilTarget =
            native.depthStencilView;
        result.width = native.width;
        result.height = native.height;
        result.colorFormat =
            Graphics::GraphicsTextureFormat::Bgra8Unorm;
        result.depthStencilFormat =
            Graphics::GraphicsTextureFormat::Depth24Stencil8;
        result.sampleCount = 1U;
        result.stableId = native.stableId;
        return result;
    }

    Base::Result<void> PresentSurface(
        std::uint64_t,
        Graphics::FenceValue) noexcept {
        // Embedded devices never own presentation.
        return {};
    }

    void DiscardSurfaceFrame(
        std::uint64_t) noexcept {}

    void NotifySurfaceLost() noexcept {
        lost_ = true;
    }

    Base::Result<void> RestoreSurface(
        const Graphics::NativeSurfaceDescriptor&) noexcept {
        lost_ = false;
        return {};
    }

    bool IsSurfaceLost() const noexcept {
        return lost_;
    }

private:
    D3D11EmbeddedSurfaceOptions options_;
    bool lost_ = false;
};

class D3D11DeviceState {
public:
    D3D11DeviceState(
        const D3D11WindowSurfaceOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          windowOptions_(options),
          embedded_(false) {}

    D3D11DeviceState(
        const D3D11EmbeddedSurfaceOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          embeddedOptions_(options),
          embedded_(true) {}

    ~D3D11DeviceState() {
        Shutdown();
    }

    Base::Result<void> Initialize() noexcept {
        Graphics::D3D11BackendOptions graphicsOptions;
        if (embedded_) {
            graphicsOptions.deviceMode =
                Graphics::D3D11DeviceMode::Borrowed;
            graphicsOptions.borrowedDevice =
                embeddedOptions_.device;
            graphicsOptions.borrowedImmediateContext =
                embeddedOptions_.immediateContext;
            graphicsOptions.statePolicy =
                embeddedOptions_.statePolicy ==
                    D3D11StatePreservationPolicy::
                        PreserveRequiredState
                ? Graphics::D3D11StatePolicy::
                      PreserveRequiredState
                : Graphics::D3D11StatePolicy::HostResetsState;
        } else {
            graphicsOptions.deviceMode =
                windowOptions_.useWarp
                ? Graphics::D3D11DeviceMode::Warp
                : Graphics::D3D11DeviceMode::Hardware;
            graphicsOptions.allowWarpFallback =
                windowOptions_.allowWarpFallback;
            graphicsOptions.enableDebugLayer =
                windowOptions_.enableDebugLayer;
        }

        graphics_ = new (std::nothrow)
            Graphics::D3D11GraphicsBackend(
                graphicsOptions, allocator_);
        if (graphics_ == nullptr) return OutOfMemory();
        Base::Result<void> status = graphics_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }

        device_ = new (std::nothrow)
            Graphics::GraphicsDevice(*graphics_, allocator_);
        if (device_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }
        status = device_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }

        if (embedded_) {
            embeddedSurface_ = new (std::nothrow)
                D3D11EmbeddedSurface(embeddedOptions_);
            surfaceBackend_ = embeddedSurface_;
        } else {
            swapChainSurface_ = new (std::nothrow)
                Graphics::D3D11SwapChainSurface(
                    *graphics_, allocator_);
            surfaceBackend_ = swapChainSurface_;
        }
        if (surfaceBackend_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }

        surface_ = new (std::nothrow)
            Graphics::SurfaceSession(*surfaceBackend_);
        if (surface_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }
        descriptor_ = MakeDescriptor();
        status = surface_->Initialize(descriptor_);
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }

        presenter_ = new (std::nothrow)
            Graphics::D3D11SurfacePresenter(
                *device_, *graphics_, *surface_);
        if (presenter_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }
        status = presenter_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }

        renderer_ = new (std::nothrow)
            Render::DeviceRenderer(
                *device_,
                Render::MakeD3D11FrameShaderSet(),
                allocator_);
        if (renderer_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }
        status = renderer_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }
        initialized_ = true;
        surfaceLost_ = false;
        deviceLost_ = false;
        return {};
    }

    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const Integration::RenderFrame& plan) noexcept {
        if (renderer_ == nullptr || device_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "D3D11 device renderer is not initialized");
        }
        Base::Result<Graphics::CommandList> recorded =
            renderer_->RecordOffscreen(rendererToken, plan);
        if (!recorded) return recorded.GetStatus();
        if (recorded.Value().CommandCount() == 0U) return {};
        Base::Result<Graphics::FenceValue> submitted =
            device_->Submit(recorded.Value());
        if (!submitted) return submitted.GetStatus();
        lastSubmittedFence_ = submitted.Value();
        return {};
    }

    Base::Result<void> Render(
        const void* rendererToken,
        const Integration::RenderFrame& plan) noexcept {
        if (renderer_ == nullptr || presenter_ == nullptr ||
            device_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "D3D11 surface renderer is not initialized");
        }
        if (device_->Backend().IsDeviceLost()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Cannot render to a lost D3D11 device");
        }

        Base::Result<Graphics::D3D11SurfaceFrame> acquired =
            presenter_->AcquireFrame();
        if (!acquired) return acquired.GetStatus();
        Graphics::D3D11SurfaceFrame frame = acquired.Value();
        const std::uint32_t width = frame.surface.target.width;
        const std::uint32_t height = frame.surface.target.height;
        if (width == 0U || height == 0U) {
            static_cast<void>(presenter_->DiscardFrame(frame));
            return InvalidArgument(
                "D3D11 surface frame has an empty render target");
        }

        Base::Result<Graphics::CommandList> recorded =
            renderer_->RecordOnscreen(
                rendererToken,
                plan,
                {frame.renderTarget,
                 width,
                 height,
                 embedded_
                     ? Graphics::LoadOperation::Load
                     : Graphics::LoadOperation::Clear});
        if (!recorded) {
            static_cast<void>(presenter_->DiscardFrame(frame));
            return recorded.GetStatus();
        }
        Base::Result<Graphics::FenceValue> submitted =
            presenter_->SubmitAndPresent(frame, recorded.Value());
        if (!submitted) return submitted.GetStatus();
        lastSubmittedFence_ = submitted.Value();
        return {};
    }

    void ReleaseRenderer(const void* rendererToken) noexcept {
        if (renderer_ != nullptr) {
            renderer_->ReleaseRenderer(rendererToken);
        }
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        if (embedded_) return {};
        if (presenter_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "D3D11 device has no presenter");
        }
        windowOptions_.width = width;
        windowOptions_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        return presenter_->Resize(width, height);
    }

    void NotifySurfaceLost() noexcept {
        if (presenter_ != nullptr) presenter_->Shutdown();
        if (surface_ != nullptr) {
            static_cast<void>(surface_->NotifyContextLost());
        }
        surfaceLost_ = true;
    }

    void NotifyDeviceLost() noexcept {
        Shutdown();
        deviceLost_ = true;
    }

    Base::Result<void> Restore() noexcept {
        if (deviceLost_) {
            deviceLost_ = false;
            return Initialize();
        }
        if (!surfaceLost_ || surface_ == nullptr ||
            presenter_ == nullptr || renderer_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "D3D11 device has no lost surface");
        }
        Base::Result<void> status =
            surface_->Restore(descriptor_);
        if (status) status = presenter_->Initialize();
        if (status) surfaceLost_ = false;
        return status;
    }

    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept {
        if (graphics_ == nullptr || lastSubmittedFence_ == 0U) {
            return {};
        }
        return graphics_->WaitForFence(
            lastSubmittedFence_,
            timeoutMilliseconds);
    }

    Detail::BackendHealth Health() const noexcept {
        if (deviceLost_ ||
            (device_ != nullptr &&
             device_->Backend().IsDeviceLost())) {
            return Detail::BackendHealth::DeviceLost;
        }
        if (surfaceLost_ ||
            (surface_ != nullptr &&
             surface_->State() != Graphics::SurfaceState::Ready)) {
            return Detail::BackendHealth::SurfaceLost;
        }
        return initialized_ && renderer_ != nullptr
            ? Detail::BackendHealth::Ready
            : Detail::BackendHealth::Failed;
    }

    ::Aero::RenderFrameStatistics
    LastFrameStatistics() const noexcept {
        ::Aero::RenderFrameStatistics result;
        if (renderer_ == nullptr) return result;
        const Render::FrameEncoderStatistics source =
            renderer_->LastStatistics();
        result.drawCallCount =
            source.drawCallCount;
        result.instanceCount =
            source.rectangleInstanceCount +
            source.imageInstanceCount +
            source.meshInstanceCount +
            source.glyphInstanceCount;
        result.stateBindingCount =
            source.pipelineBindingCount +
            source.vertexBufferBindingCount +
            source.indexBufferBindingCount +
            source.uniformBufferBindingCount +
            source.textureSamplerBindingCount;
        return result;
    }

    Aero::Render::Detail::RenderResources Resources() noexcept {
        return renderer_ != nullptr
            ? Aero::Render::Detail::RenderResources{
                  renderer_->GetTextResources(),
                  renderer_->GetMeshResources(),
                  renderer_->GetImageResources()}
            : Aero::Render::Detail::RenderResources{};
    }

private:
    Base::Status OutOfMemory() const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate D3D11 device state");
    }

    Graphics::NativeSurfaceDescriptor
    MakeDescriptor() const noexcept {
        Graphics::NativeSurfaceDescriptor descriptor;
        descriptor.width = embedded_
            ? 1U : windowOptions_.width;
        descriptor.height = embedded_
            ? 1U : windowOptions_.height;
        descriptor.colorFormat =
            Graphics::GraphicsTextureFormat::Bgra8Unorm;
        descriptor.depthStencilFormat =
            Graphics::GraphicsTextureFormat::Depth24Stencil8;
        descriptor.sampleCount = 1U;
        descriptor.stableId =
            UINT64_C(0x4145524F44334431);
        if (embedded_) {
            descriptor.kind =
                Graphics::SurfaceKind::ExternalRenderTarget;
            descriptor.ownership =
                Graphics::SurfaceOwnership::Borrowed;
            descriptor.external.colorTarget = 1U;
        } else {
            descriptor.kind =
                Graphics::SurfaceKind::D3D11Window;
            descriptor.ownership =
                Graphics::SurfaceOwnership::Owned;
            descriptor.presentMode =
                ToRhiPresentMode(
                    windowOptions_.presentMode);
            descriptor.d3d11.window =
                windowOptions_.window.window;
            descriptor.d3d11.device =
                graphics_->NativeDevice();
            descriptor.d3d11.immediateContext =
                graphics_->NativeImmediateContext();
        }
        return descriptor;
    }

    void Shutdown() noexcept {
        initialized_ = false;
        lastSubmittedFence_ = 0U;
        if (renderer_ != nullptr) {
            renderer_->Shutdown();
            delete renderer_;
            renderer_ = nullptr;
        }
        if (presenter_ != nullptr) {
            presenter_->Shutdown();
            delete presenter_;
            presenter_ = nullptr;
        }
        if (surface_ != nullptr) {
            surface_->Shutdown();
            delete surface_;
            surface_ = nullptr;
        }
        delete swapChainSurface_;
        swapChainSurface_ = nullptr;
        delete embeddedSurface_;
        embeddedSurface_ = nullptr;
        surfaceBackend_ = nullptr;
        delete device_;
        device_ = nullptr;
        if (graphics_ != nullptr) {
            graphics_->Shutdown();
            delete graphics_;
            graphics_ = nullptr;
        }
    }

    Base::IAllocator* allocator_ = nullptr;
    D3D11WindowSurfaceOptions windowOptions_;
    D3D11EmbeddedSurfaceOptions embeddedOptions_;
    bool embedded_ = false;
    bool initialized_ = false;
    bool surfaceLost_ = false;
    bool deviceLost_ = false;
    Graphics::NativeSurfaceDescriptor descriptor_;
    Graphics::D3D11GraphicsBackend* graphics_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
    D3D11EmbeddedSurface* embeddedSurface_ = nullptr;
    Graphics::D3D11SwapChainSurface* swapChainSurface_ = nullptr;
    Graphics::ISurfaceBackend* surfaceBackend_ = nullptr;
    Graphics::SurfaceSession* surface_ = nullptr;
    Graphics::D3D11SurfacePresenter* presenter_ = nullptr;
    Render::DeviceRenderer* renderer_ = nullptr;
    Graphics::FenceValue lastSubmittedFence_ = 0U;
};

template<class TOptions>
Base::Result<Base::Ref<::Aero::RenderDevice>>
CreateD3D11Device(
    const TOptions& options,
    Detail::RenderDeviceMode mode,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    auto* driver = new (std::nothrow)
        D3D11DeviceState(options, selected);
    if (driver == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the D3D11 device");
    }
    Base::Result<void> initialized = driver->Initialize();
    if (!initialized) {
        delete driver;
        return initialized.GetStatus();
    }
    return ::Aero::Integration::Detail::AdoptRenderDevice(
        mode, driver, &selected);
}

} // namespace

Base::Result<Base::Ref<::Aero::RenderDevice>>
CreateD3D11EmbeddedDevice(
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (options.device == 0U ||
        options.immediateContext == 0U ||
        options.acquireTarget == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "D3D11 embedded device options are incomplete");
    }
    return CreateD3D11Device(
        options,
        Detail::RenderDeviceMode::Embedded,
        allocator);
}

Base::Result<Base::Ref<::Aero::RenderDevice>>
CreateD3D11WindowDevice(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!options.window.IsValid() ||
        options.window.system !=
            Integration::WindowSystem::Win32 ||
        options.width == 0U ||
        options.height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "D3D11 window device options are invalid");
    }
    return CreateD3D11Device(
        options,
        Detail::RenderDeviceMode::Window,
        allocator);
}

} // namespace Aero::Integration
