#include <Aero/Integration/D3D11.hpp>

#include "RenderEndpointInternal.hpp"
#include "render/TextBackendAccess.hpp"

#include "render/d3d11/D3D11RendererBackend.hpp"
#include "rhi/D3D11Backend.hpp"

#include <new>

namespace Aero::Integration {
namespace {

Rhi::PresentMode ToRhiPresentMode(
    RenderPresentMode value) noexcept {
    switch (value) {
    case RenderPresentMode::Immediate:
        return Rhi::PresentMode::Immediate;
    case RenderPresentMode::Mailbox:
        return Rhi::PresentMode::Mailbox;
    case RenderPresentMode::Fifo:
        return Rhi::PresentMode::Fifo;
    }
    return Rhi::PresentMode::Fifo;
}

class D3D11EmbeddedSurface final
    : public Rhi::ISurfaceBackend {
public:
    explicit D3D11EmbeddedSurface(
        const D3D11EmbeddedEndpointOptions& options) noexcept
        : options_(options) {}

    Rhi::SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept override {
        Rhi::SurfaceCapabilities result;
        result.supportedKinds =
            Rhi::SurfaceKindBit(
                Rhi::SurfaceKind::ExternalRenderTarget);
        result.supportsResize = false;
        result.supportsPresent = false;
        result.supportsContextLossRecovery = true;
        result.supportsExternalRenderTargets = true;
        return result;
    }

    Base::Result<void> CreateSurface(
        const Rhi::NativeSurfaceDescriptor&) noexcept override {
        lost_ = false;
        return {};
    }

    void DestroySurface() noexcept override {
        lost_ = true;
    }

    Base::Result<void> ResizeSurface(
        std::uint32_t,
        std::uint32_t) noexcept override {
        return {};
    }

    Base::Result<Rhi::ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(
        std::uint64_t) noexcept override {
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
        Rhi::ExternalRenderTargetDescriptor result;
        result.colorTarget = native.texture2D;
        result.depthStencilTarget =
            native.depthStencilView;
        result.width = native.width;
        result.height = native.height;
        result.colorFormat =
            Rhi::GraphicsTextureFormat::Bgra8Unorm;
        result.depthStencilFormat =
            Rhi::GraphicsTextureFormat::Depth24Stencil8;
        result.sampleCount = 1U;
        result.stableId = native.stableId;
        return result;
    }

    Base::Result<void> PresentSurface(
        std::uint64_t,
        Rhi::FenceValue) noexcept override {
        // Embedded endpoints never own presentation.
        return {};
    }

    void DiscardSurfaceFrame(
        std::uint64_t) noexcept override {}

    void NotifySurfaceLost() noexcept override {
        lost_ = true;
    }

    Base::Result<void> RestoreSurface(
        const Rhi::NativeSurfaceDescriptor&) noexcept override {
        lost_ = false;
        return {};
    }

    bool IsSurfaceLost() const noexcept override {
        return lost_;
    }

private:
    D3D11EmbeddedEndpointOptions options_;
    bool lost_ = false;
};

class D3D11EndpointDriver final
    : public Detail::EndpointDriver {
public:
    D3D11EndpointDriver(
        const D3D11WindowEndpointOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          windowOptions_(options),
          embedded_(false) {}

    D3D11EndpointDriver(
        const D3D11EmbeddedEndpointOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          embeddedOptions_(options),
          embedded_(true) {}

    ~D3D11EndpointDriver() override {
        Shutdown();
    }

    bool SupportsDedicatedThread() const noexcept override {
        // Text/resource preparation currently shares the immediate context
        // with submission. Keep the factory honest until those leases are
        // staged entirely on the endpoint worker.
        return false;
    }

    Base::Result<void> Initialize() noexcept {
        Rhi::D3D11BackendOptions graphicsOptions;
        if (embedded_) {
            graphicsOptions.deviceMode =
                Rhi::D3D11DeviceMode::Borrowed;
            graphicsOptions.borrowedDevice =
                embeddedOptions_.device;
            graphicsOptions.borrowedImmediateContext =
                embeddedOptions_.immediateContext;
            graphicsOptions.statePolicy =
                embeddedOptions_.statePolicy ==
                    D3D11StatePreservationPolicy::
                        PreserveRequiredState
                ? Rhi::D3D11StatePolicy::
                      PreserveRequiredState
                : Rhi::D3D11StatePolicy::HostResetsState;
        } else {
            graphicsOptions.deviceMode =
                windowOptions_.useWarp
                ? Rhi::D3D11DeviceMode::Warp
                : Rhi::D3D11DeviceMode::Hardware;
            graphicsOptions.allowWarpFallback =
                windowOptions_.allowWarpFallback;
            graphicsOptions.enableDebugLayer =
                windowOptions_.enableDebugLayer;
        }

        graphics_ = new (std::nothrow)
            Rhi::D3D11GraphicsBackend(
                graphicsOptions, allocator_);
        if (graphics_ == nullptr) return OutOfMemory();
        Base::Result<void> status = graphics_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }

        device_ = new (std::nothrow)
            Rhi::RhiDevice(*graphics_, allocator_);
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
                Rhi::D3D11SwapChainSurface(
                    *graphics_, allocator_);
            surfaceBackend_ = swapChainSurface_;
        }
        if (surfaceBackend_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }

        surface_ = new (std::nothrow)
            Rhi::SurfaceSession(*surfaceBackend_);
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
            Rhi::D3D11SurfacePresenter(
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
            Render::D3D11RenderPlanBackend(
                *device_, *presenter_, allocator_);
        if (renderer_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }
        status = renderer_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }
        renderer_->SetBatchingEnabled(
            batchingEnabled_);
        initialized_ = true;
        return {};
    }

    Base::Result<void> Submit(
        const Render::RenderPlan& plan) noexcept override {
        return renderer_ != nullptr
            ? renderer_->Submit(plan)
            : Base::Result<void>(
                  Base::Status::Failure(
                      Base::ErrorCode::NotInitialized,
                      "D3D11 endpoint is not initialized"));
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept override {
        if (embedded_) return {};
        if (presenter_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "D3D11 endpoint has no presenter");
        }
        windowOptions_.width = width;
        windowOptions_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        return presenter_->Resize(width, height);
    }

    void NotifySurfaceLost() noexcept override {
        if (renderer_ != nullptr) renderer_->Shutdown();
        if (presenter_ != nullptr) presenter_->Shutdown();
        if (surface_ != nullptr) {
            static_cast<void>(surface_->NotifyContextLost());
        }
        surfaceLost_ = true;
    }

    void NotifyDeviceLost() noexcept override {
        Shutdown();
        deviceLost_ = true;
    }

    Base::Result<void> Restore() noexcept override {
        if (deviceLost_) {
            deviceLost_ = false;
            return Initialize();
        }
        if (!surfaceLost_ || surface_ == nullptr ||
            presenter_ == nullptr || renderer_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "D3D11 endpoint has no lost surface");
        }
        Base::Result<void> status =
            surface_->Restore(descriptor_);
        if (status) status = presenter_->Initialize();
        if (status) status = renderer_->Initialize();
        if (status) surfaceLost_ = false;
        return status;
    }

    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept override {
        if (graphics_ == nullptr || renderer_ == nullptr ||
            renderer_->LastSubmittedFence() == 0U) {
            return {};
        }
        return graphics_->WaitForFence(
            renderer_->LastSubmittedFence(),
            timeoutMilliseconds);
    }

    RenderFrameStatistics
    LastFrameStatistics() const noexcept override {
        RenderFrameStatistics result;
        if (renderer_ == nullptr) return result;
        const Render::RendererStatistics source =
            renderer_->LastSubmitStatistics();
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

    void SetBatchingEnabled(
        bool enabled) noexcept override {
        batchingEnabled_ = enabled;
        if (renderer_ != nullptr) {
            renderer_->SetBatchingEnabled(
                enabled);
        }
    }

    void* QueryInternalService(
        std::uint64_t service) noexcept override {
        return renderer_ != nullptr
            ? Render::Detail::RenderBackendAccess::
                  InternalService(
                      *renderer_, service)
            : nullptr;
    }

private:
    Base::Status OutOfMemory() const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate D3D11 endpoint state");
    }

    Rhi::NativeSurfaceDescriptor
    MakeDescriptor() const noexcept {
        Rhi::NativeSurfaceDescriptor descriptor;
        descriptor.width = embedded_
            ? 1U : windowOptions_.width;
        descriptor.height = embedded_
            ? 1U : windowOptions_.height;
        descriptor.colorFormat =
            Rhi::GraphicsTextureFormat::Bgra8Unorm;
        descriptor.depthStencilFormat =
            Rhi::GraphicsTextureFormat::Depth24Stencil8;
        descriptor.sampleCount = 1U;
        descriptor.stableId =
            UINT64_C(0x4145524F44334431);
        if (embedded_) {
            descriptor.kind =
                Rhi::SurfaceKind::ExternalRenderTarget;
            descriptor.ownership =
                Rhi::SurfaceOwnership::Borrowed;
            descriptor.external.colorTarget = 1U;
        } else {
            descriptor.kind =
                Rhi::SurfaceKind::D3D11Window;
            descriptor.ownership =
                Rhi::SurfaceOwnership::Owned;
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
    D3D11WindowEndpointOptions windowOptions_;
    D3D11EmbeddedEndpointOptions embeddedOptions_;
    bool embedded_ = false;
    bool initialized_ = false;
    bool surfaceLost_ = false;
    bool deviceLost_ = false;
    bool batchingEnabled_ = true;
    Rhi::NativeSurfaceDescriptor descriptor_;
    Rhi::D3D11GraphicsBackend* graphics_ = nullptr;
    Rhi::RhiDevice* device_ = nullptr;
    D3D11EmbeddedSurface* embeddedSurface_ = nullptr;
    Rhi::D3D11SwapChainSurface* swapChainSurface_ = nullptr;
    Rhi::ISurfaceBackend* surfaceBackend_ = nullptr;
    Rhi::SurfaceSession* surface_ = nullptr;
    Rhi::D3D11SurfacePresenter* presenter_ = nullptr;
    Render::D3D11RenderPlanBackend* renderer_ = nullptr;
};

template<class TOptions>
Base::Result<Base::Ref<RenderEndpoint>>
CreateD3D11Endpoint(
    const TOptions& options,
    RenderEndpointMode mode,
    RenderSubmissionMode submissionMode,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    auto* driver = new (std::nothrow)
        D3D11EndpointDriver(options, selected);
    if (driver == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the D3D11 endpoint");
    }
    Base::Result<void> initialized = driver->Initialize();
    if (!initialized) {
        delete driver;
        return initialized.GetStatus();
    }
    return Detail::RenderEndpointAccess::Create(
        mode, submissionMode, driver, &selected);
}

} // namespace

Base::Result<Base::Ref<RenderEndpoint>>
CreateD3D11EmbeddedEndpoint(
    const D3D11EmbeddedEndpointOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (options.device == 0U ||
        options.immediateContext == 0U ||
        options.acquireTarget == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "D3D11 embedded endpoint options are incomplete");
    }
    return CreateD3D11Endpoint(
        options,
        RenderEndpointMode::Embedded,
        options.submissionMode,
        allocator);
}

Base::Result<Base::Ref<RenderEndpoint>>
CreateD3D11WindowEndpoint(
    const D3D11WindowEndpointOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!options.window.IsValid() ||
        options.window.system !=
            Platform::WindowSystem::Win32 ||
        options.width == 0U ||
        options.height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "D3D11 window endpoint options are invalid");
    }
    return CreateD3D11Endpoint(
        options,
        RenderEndpointMode::Window,
        options.submissionMode,
        allocator);
}

} // namespace Aero::Integration
