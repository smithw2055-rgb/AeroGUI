#include "integration/IntegrationPrivate.hpp"
#include "integration/private/RenderSurface.hpp"
#include "render/DeviceRenderer.hpp"
#include "render/d3d11/D3D11Backend.hpp"
#include "render/d3d11/D3D11Shaders.hpp"

#include <new>

namespace Aero::Render::Detail {
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

Graphics::PresentMode ToRhiPresentMode(PresentMode value) noexcept {
    switch (value) {
    case PresentMode::Immediate: return Graphics::PresentMode::Immediate;
    case PresentMode::Mailbox: return Graphics::PresentMode::Mailbox;
    case PresentMode::Fifo: return Graphics::PresentMode::Fifo;
    }
    return Graphics::PresentMode::Fifo;
}

class D3D11SurfaceState;

class D3D11DeviceState final
    : public NativeRenderDevice {
public:
    D3D11DeviceState(
        const D3D11DeviceOptions& options,
        Base::IAllocator& allocator) noexcept
        : options_(options), allocator_(&allocator) {}

    ~D3D11DeviceState() noexcept;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;
    void NotifyDeviceLost() noexcept;
    Base::Result<void> RestoreDevice() noexcept;
    Base::Result<void> WaitIdle(std::uint32_t timeoutMilliseconds) noexcept;
    BackendHealth GetDeviceHealth() const noexcept;
    ::Aero::RenderFrameStatistics LastFrameStatistics() const noexcept;
    Aero::Render::Detail::RenderResources Resources() noexcept;

    RenderBackendKind Backend() const noexcept override {
        return RenderBackendKind::D3D11;
    }
    Graphics::D3D11GraphicsBackend* GraphicsBackend() noexcept {
        return graphics_;
    }
    Graphics::GraphicsDevice* GraphicsDevice() noexcept { return device_; }
    ::Aero::Render::DeviceRenderer* Renderer() noexcept { return renderer_; }
    std::uint64_t Generation() const noexcept { return generation_; }
    bool IsReady() const noexcept {
        return initialized_ && !deviceLost_ && graphics_ != nullptr &&
            device_ != nullptr && renderer_ != nullptr;
    }
    void SetLastSubmittedFence(Graphics::FenceValue fence) noexcept {
        lastSubmittedFence_ = fence;
    }

    void Attach(D3D11SurfaceState& surface) noexcept;
    void Detach(D3D11SurfaceState& surface) noexcept;

private:
    void ShutdownDevice(bool notifySurfaces) noexcept;
    void NotifySurfacesDeviceLost() noexcept;
    void RestoreSurfaces() noexcept;

    D3D11DeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Graphics::D3D11GraphicsBackend* graphics_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
    ::Aero::Render::DeviceRenderer* renderer_ = nullptr;
    D3D11SurfaceState* surfaces_ = nullptr;
    Graphics::FenceValue lastSubmittedFence_ = 0U;
    std::uint64_t generation_ = 0U;
    bool initialized_ = false;
    bool deviceLost_ = false;
};

class D3D11ExternalSurface final : public Graphics::ISurfaceBackend {
public:
    explicit D3D11ExternalSurface(
        const D3D11EmbeddedSurfaceOptions& options) noexcept
        : options_(options) {}

    Graphics::SurfaceCapabilities QuerySurfaceCapabilities() const noexcept override {
        Graphics::SurfaceCapabilities result;
        result.supportedKinds = Graphics::SurfaceKindBit(
            Graphics::SurfaceKind::ExternalRenderTarget);
        result.supportsResize = false;
        result.supportsPresent = false;
        result.supportsContextLossRecovery = true;
        result.supportsExternalRenderTargets = true;
        return result;
    }

    Base::Result<void> CreateSurface(
        const Graphics::NativeSurfaceDescriptor&) noexcept override {
        lost_ = false;
        return {};
    }

    void DestroySurface() noexcept override { lost_ = true; }

    Base::Result<void> ResizeSurface(
        std::uint32_t,
        std::uint32_t) noexcept override { return {}; }

    Base::Result<Graphics::ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(std::uint64_t) noexcept override {
        if (lost_ || options_.acquireTarget == nullptr) {
            return InvalidState("D3D11 embedded surface is unavailable");
        }
        D3D11EmbeddedTarget target;
        Base::Status acquired = options_.acquireTarget(
            options_.callbackContext, &target);
        if (!acquired.IsOk()) return acquired;
        if (target.texture2D == 0U || target.width == 0U || target.height == 0U) {
            return InvalidArgument("D3D11 embedded target is invalid");
        }
        Graphics::ExternalRenderTargetDescriptor result;
        result.colorTarget = target.texture2D;
        result.depthStencilTarget = target.depthStencilView;
        result.width = target.width;
        result.height = target.height;
        result.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        result.depthStencilFormat =
            Graphics::GraphicsTextureFormat::Depth24Stencil8;
        result.sampleCount = 1U;
        result.stableId = target.stableId;
        return result;
    }

    Base::Result<void> PresentSurface(
        std::uint64_t,
        Graphics::FenceValue) noexcept override { return {}; }

    void DiscardSurfaceFrame(std::uint64_t) noexcept override {}
    void NotifySurfaceLost() noexcept override { lost_ = true; }

    Base::Result<void> RestoreSurface(
        const Graphics::NativeSurfaceDescriptor&) noexcept override {
        lost_ = false;
        return {};
    }

    bool IsSurfaceLost() const noexcept override { return lost_; }

private:
    D3D11EmbeddedSurfaceOptions options_;
    bool lost_ = false;
};

class D3D11SurfaceState final
    : public NativeRenderTarget {
public:
    D3D11SurfaceState(
        D3D11DeviceState& device,
        const D3D11EmbeddedSurfaceOptions& options,
        Base::IAllocator& allocator) noexcept
        : device_(&device), allocator_(&allocator),
          embeddedOptions_(options), embedded_(true) {
        device_->Attach(*this);
    }

    D3D11SurfaceState(
        D3D11DeviceState& device,
        const D3D11WindowSurfaceOptions& options,
        Base::IAllocator& allocator) noexcept
        : device_(&device), allocator_(&allocator),
          windowOptions_(options), embedded_(false) {
        device_->Attach(*this);
    }

    ~D3D11SurfaceState() noexcept {
        ShutdownSurface();
        if (device_ != nullptr) device_->Detach(*this);
    }

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady()) {
            health_ = SurfaceHealth::Lost;
            return NotInitialized("D3D11 surface requires a ready render device");
        }
        ShutdownSurface();
        if (embedded_) {
            external_ = new (std::nothrow)
                D3D11ExternalSurface(embeddedOptions_);
            surfaceBackend_ = external_;
        } else {
            swapChain_ = new (std::nothrow)
                Graphics::D3D11SwapChainSurface(
                    *device_->GraphicsBackend(), allocator_);
            surfaceBackend_ = swapChain_;
        }
        if (surfaceBackend_ == nullptr) {
            health_ = SurfaceHealth::Failed;
            return OutOfMemory("Unable to allocate D3D11 surface backend");
        }
        surface_ = new (std::nothrow) Graphics::SurfaceSession(*surfaceBackend_);
        if (surface_ == nullptr) {
            ShutdownSurface();
            health_ = SurfaceHealth::Failed;
            return OutOfMemory("Unable to allocate D3D11 surface session");
        }
        descriptor_ = MakeDescriptor();
        Base::Result<void> initialized = surface_->Initialize(descriptor_);
        if (!initialized) {
            ShutdownSurface();
            health_ = SurfaceHealth::Failed;
            return initialized.GetStatus();
        }
        presenter_ = new (std::nothrow) Graphics::D3D11SurfacePresenter(
            *device_->GraphicsDevice(),
            *device_->GraphicsBackend(),
            *surface_);
        if (presenter_ == nullptr) {
            ShutdownSurface();
            health_ = SurfaceHealth::Failed;
            return OutOfMemory("Unable to allocate D3D11 surface presenter");
        }
        initialized = presenter_->Initialize();
        if (!initialized) {
            ShutdownSurface();
            health_ = SurfaceHealth::Failed;
            return initialized.GetStatus();
        }
        deviceGeneration_ = device_->Generation();
        health_ = SurfaceHealth::Ready;
        return {};
    }

    Base::Result<void> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
        if (!IsReady()) {
            return InvalidState("D3D11 surface is not ready");
        }
        Base::Result<Graphics::D3D11SurfaceFrame> acquired =
            presenter_->AcquireFrame();
        if (!acquired) {
            RefreshHealth();
            return acquired.GetStatus();
        }
        Graphics::D3D11SurfaceFrame nativeFrame = acquired.Value();
        const std::uint32_t width = nativeFrame.surface.target.width;
        const std::uint32_t height = nativeFrame.surface.target.height;
        if (width == 0U || height == 0U) {
            static_cast<void>(presenter_->DiscardFrame(nativeFrame));
            return InvalidArgument("D3D11 surface frame has an empty target");
        }
        Base::Result<Graphics::CommandList> commands =
            device_->Renderer()->RecordOnscreen(
                rendererToken,
                frame,
                {nativeFrame.renderTarget,
                 width,
                 height,
                 embedded_
                     ? Graphics::LoadOperation::Load
                     : Graphics::LoadOperation::Clear});
        if (!commands) {
            static_cast<void>(presenter_->DiscardFrame(nativeFrame));
            return commands.GetStatus();
        }
        Base::Result<Graphics::FenceValue> submitted =
            presenter_->SubmitAndPresent(nativeFrame, commands.Value());
        if (!submitted) {
            RefreshHealth();
            return submitted.GetStatus();
        }
        device_->SetLastSubmittedFence(submitted.Value());
        return {};
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        if (embedded_) return {};
        if (!IsReady()) return InvalidState("D3D11 surface is not ready");
        windowOptions_.width = width;
        windowOptions_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        Base::Result<void> resized = presenter_->Resize(width, height);
        if (!resized) RefreshHealth();
        return resized;
    }

    void NotifySurfaceLost() noexcept {
        ShutdownSurface();
        health_ = SurfaceHealth::Lost;
    }

    Base::Result<void> RestoreSurface() noexcept {
        if (health_ != SurfaceHealth::Lost) {
            return InvalidState("Only a lost D3D11 surface can be restored");
        }
        return Initialize();
    }

    SurfaceHealth GetSurfaceHealth() const noexcept {
        if (device_ == nullptr) return SurfaceHealth::Shutdown;
        if (!device_->IsReady() || deviceGeneration_ != device_->Generation()) {
            return SurfaceHealth::Lost;
        }
        if (health_ != SurfaceHealth::Ready) return health_;
        return surface_ != nullptr &&
                surface_->State() == Graphics::SurfaceState::Ready
            ? SurfaceHealth::Ready
            : SurfaceHealth::Lost;
    }

    void OnDeviceLost() noexcept {
        ShutdownSurface();
        health_ = SurfaceHealth::Lost;
    }

    void OnDeviceRestored() noexcept {
        if (device_ == nullptr || !device_->IsReady()) return;
        Base::Result<void> restored = Initialize();
        if (!restored) health_ = SurfaceHealth::Lost;
    }

    void OnDeviceDestroyed() noexcept {
        ShutdownSurface();
        device_ = nullptr;
        previous_ = nullptr;
        next_ = nullptr;
        health_ = SurfaceHealth::Shutdown;
    }

private:
    friend class D3D11DeviceState;

    bool IsReady() const noexcept {
        return GetSurfaceHealth() == SurfaceHealth::Ready &&
            presenter_ != nullptr && device_->Renderer() != nullptr;
    }

    void RefreshHealth() noexcept {
        if (surface_ != nullptr &&
            surface_->State() != Graphics::SurfaceState::Ready) {
            health_ = SurfaceHealth::Lost;
        }
    }

    Graphics::NativeSurfaceDescriptor MakeDescriptor() const noexcept {
        Graphics::NativeSurfaceDescriptor descriptor;
        descriptor.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        descriptor.depthStencilFormat =
            Graphics::GraphicsTextureFormat::Depth24Stencil8;
        descriptor.sampleCount = 1U;
        descriptor.stableId = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(this));
        if (embedded_) {
            descriptor.width = 1U;
            descriptor.height = 1U;
            descriptor.kind = Graphics::SurfaceKind::ExternalRenderTarget;
            descriptor.ownership = Graphics::SurfaceOwnership::Borrowed;
            descriptor.external.colorTarget = 1U;
        } else {
            descriptor.width = windowOptions_.width;
            descriptor.height = windowOptions_.height;
            descriptor.kind = Graphics::SurfaceKind::D3D11Window;
            descriptor.ownership = Graphics::SurfaceOwnership::Owned;
            descriptor.presentMode = ToRhiPresentMode(windowOptions_.presentMode);
            descriptor.d3d11.window = windowOptions_.window.window;
            descriptor.d3d11.device =
                device_->GraphicsBackend()->NativeDevice();
            descriptor.d3d11.immediateContext =
                device_->GraphicsBackend()->NativeImmediateContext();
        }
        return descriptor;
    }

    void ShutdownSurface() noexcept {
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
        delete swapChain_;
        swapChain_ = nullptr;
        delete external_;
        external_ = nullptr;
        surfaceBackend_ = nullptr;
        deviceGeneration_ = 0U;
    }

    D3D11DeviceState* device_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    D3D11EmbeddedSurfaceOptions embeddedOptions_;
    D3D11WindowSurfaceOptions windowOptions_;
    bool embedded_ = false;
    Graphics::NativeSurfaceDescriptor descriptor_;
    D3D11ExternalSurface* external_ = nullptr;
    Graphics::D3D11SwapChainSurface* swapChain_ = nullptr;
    Graphics::ISurfaceBackend* surfaceBackend_ = nullptr;
    Graphics::SurfaceSession* surface_ = nullptr;
    Graphics::D3D11SurfacePresenter* presenter_ = nullptr;
    std::uint64_t deviceGeneration_ = 0U;
    SurfaceHealth health_ = SurfaceHealth::Lost;
    D3D11SurfaceState* previous_ = nullptr;
    D3D11SurfaceState* next_ = nullptr;
};

D3D11DeviceState::~D3D11DeviceState() noexcept {
    while (surfaces_ != nullptr) {
        D3D11SurfaceState* surface = surfaces_;
        surfaces_ = surface->next_;
        surface->OnDeviceDestroyed();
    }
    ShutdownDevice(false);
}

Base::Result<void> D3D11DeviceState::Initialize() noexcept {
    if (initialized_) return {};
    Graphics::D3D11BackendOptions graphicsOptions;
    const bool borrowed = options_.device != 0U ||
        options_.immediateContext != 0U;
    if (borrowed) {
        if (options_.device == 0U || options_.immediateContext == 0U) {
            return InvalidArgument(
                "Borrowed D3D11 device requires device and immediate context");
        }
        graphicsOptions.deviceMode = Graphics::D3D11DeviceMode::Borrowed;
        graphicsOptions.borrowedDevice = options_.device;
        graphicsOptions.borrowedImmediateContext = options_.immediateContext;
        graphicsOptions.statePolicy =
            options_.statePolicy ==
                D3D11StatePreservationPolicy::PreserveRequiredState
            ? Graphics::D3D11StatePolicy::PreserveRequiredState
            : Graphics::D3D11StatePolicy::HostResetsState;
    } else {
        graphicsOptions.deviceMode = options_.useWarp
            ? Graphics::D3D11DeviceMode::Warp
            : Graphics::D3D11DeviceMode::Hardware;
        graphicsOptions.allowWarpFallback = options_.allowWarpFallback;
        graphicsOptions.enableDebugLayer = options_.enableDebugLayer;
    }
    graphics_ = new (std::nothrow)
        Graphics::D3D11GraphicsBackend(graphicsOptions, allocator_);
    if (graphics_ == nullptr) return OutOfMemory("Unable to allocate D3D11 backend");
    Base::Result<void> status = graphics_->Initialize();
    if (!status) {
        ShutdownDevice(false);
        return status.GetStatus();
    }
    device_ = new (std::nothrow) Graphics::GraphicsDevice(*graphics_, allocator_);
    if (device_ == nullptr) {
        ShutdownDevice(false);
        return OutOfMemory("Unable to allocate D3D11 graphics device");
    }
    status = device_->Initialize();
    if (!status) {
        ShutdownDevice(false);
        return status.GetStatus();
    }
    renderer_ = new (std::nothrow) ::Aero::Render::DeviceRenderer(
        *device_, ::Aero::Render::MakeD3D11FrameShaderSet(), allocator_);
    if (renderer_ == nullptr) {
        ShutdownDevice(false);
        return OutOfMemory("Unable to allocate D3D11 device renderer");
    }
    status = renderer_->Initialize();
    if (!status) {
        ShutdownDevice(false);
        return status.GetStatus();
    }
    initialized_ = true;
    deviceLost_ = false;
    ++generation_;
    if (generation_ == 0U) ++generation_;
    RestoreSurfaces();
    return {};
}

Base::Result<void> D3D11DeviceState::RenderOffscreen(
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    if (!IsReady()) return NotInitialized("D3D11 device is not initialized");
    Base::Result<Graphics::CommandList> commands =
        renderer_->RecordOffscreen(rendererToken, frame);
    if (!commands) return commands.GetStatus();
    if (commands.Value().CommandCount() == 0U) return {};
    Base::Result<Graphics::FenceValue> submitted =
        device_->Submit(commands.Value());
    if (!submitted) return submitted.GetStatus();
    lastSubmittedFence_ = submitted.Value();
    return {};
}

void D3D11DeviceState::ReleaseRenderer(const void* rendererToken) noexcept {
    if (renderer_ != nullptr) renderer_->ReleaseRenderer(rendererToken);
}

void D3D11DeviceState::NotifyDeviceLost() noexcept {
    if (deviceLost_) return;
    NotifySurfacesDeviceLost();
    ShutdownDevice(false);
    deviceLost_ = true;
}

Base::Result<void> D3D11DeviceState::RestoreDevice() noexcept {
    if (!deviceLost_) return InvalidState("D3D11 device is not lost");
    deviceLost_ = false;
    Base::Result<void> restored = Initialize();
    if (!restored) deviceLost_ = true;
    return restored;
}

Base::Result<void> D3D11DeviceState::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    if (graphics_ == nullptr || lastSubmittedFence_ == 0U) return {};
    return graphics_->WaitForFence(lastSubmittedFence_, timeoutMilliseconds);
}

BackendHealth D3D11DeviceState::GetDeviceHealth() const noexcept {
    if (deviceLost_ ||
        (device_ != nullptr && device_->Backend().IsDeviceLost())) {
        return BackendHealth::DeviceLost;
    }
    return IsReady()
        ? BackendHealth::Ready
        : BackendHealth::Failed;
}

::Aero::RenderFrameStatistics
D3D11DeviceState::LastFrameStatistics() const noexcept {
    ::Aero::RenderFrameStatistics result;
    if (renderer_ == nullptr) return result;
    const ::Aero::Render::FrameEncoderStatistics source = renderer_->LastStatistics();
    result.drawCallCount = source.drawCallCount;
    result.instanceCount = source.rectangleInstanceCount +
        source.imageInstanceCount + source.meshInstanceCount +
        source.glyphInstanceCount;
    result.stateBindingCount = source.pipelineBindingCount +
        source.vertexBufferBindingCount + source.indexBufferBindingCount +
        source.uniformBufferBindingCount + source.textureSamplerBindingCount;
    return result;
}

Aero::Render::Detail::RenderResources D3D11DeviceState::Resources() noexcept {
    return renderer_ != nullptr
        ? Aero::Render::Detail::RenderResources{
              renderer_->GetTextResources(),
              renderer_->GetMeshResources(),
              renderer_->GetImageResources()}
        : Aero::Render::Detail::RenderResources{};
}

void D3D11DeviceState::Attach(D3D11SurfaceState& surface) noexcept {
    surface.previous_ = nullptr;
    surface.next_ = surfaces_;
    if (surfaces_ != nullptr) surfaces_->previous_ = &surface;
    surfaces_ = &surface;
}

void D3D11DeviceState::Detach(D3D11SurfaceState& surface) noexcept {
    if (surface.previous_ != nullptr) surface.previous_->next_ = surface.next_;
    if (surface.next_ != nullptr) surface.next_->previous_ = surface.previous_;
    if (surfaces_ == &surface) surfaces_ = surface.next_;
    surface.previous_ = nullptr;
    surface.next_ = nullptr;
}

void D3D11DeviceState::ShutdownDevice(bool notifySurfaces) noexcept {
    if (notifySurfaces) NotifySurfacesDeviceLost();
    initialized_ = false;
    lastSubmittedFence_ = 0U;
    if (renderer_ != nullptr) {
        renderer_->Shutdown();
        delete renderer_;
        renderer_ = nullptr;
    }
    delete device_;
    device_ = nullptr;
    if (graphics_ != nullptr) {
        graphics_->Shutdown();
        delete graphics_;
        graphics_ = nullptr;
    }
}

void D3D11DeviceState::NotifySurfacesDeviceLost() noexcept {
    for (D3D11SurfaceState* surface = surfaces_;
         surface != nullptr; surface = surface->next_) {
        surface->OnDeviceLost();
    }
}

void D3D11DeviceState::RestoreSurfaces() noexcept {
    for (D3D11SurfaceState* surface = surfaces_;
         surface != nullptr; surface = surface->next_) {
        surface->OnDeviceRestored();
    }
}

D3D11DeviceOptions DeviceOptionsFrom(
    const D3D11EmbeddedSurfaceOptions& options) noexcept {
    D3D11DeviceOptions result;
    result.device = options.device;
    result.immediateContext = options.immediateContext;
    result.statePolicy = options.statePolicy;
    return result;
}

D3D11DeviceOptions DeviceOptionsFrom(
    const D3D11WindowSurfaceOptions& options) noexcept {
    D3D11DeviceOptions result;
    result.useWarp = options.useWarp;
    result.allowWarpFallback = options.allowWarpFallback;
    result.enableDebugLayer = options.enableDebugLayer;
    return result;
}

D3D11DeviceState* DeviceStateFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device ||
        Aero::RenderDevice::Impl::Backend(*device) !=
            RenderBackendKind::D3D11) {
        return nullptr;
    }
    return static_cast<D3D11DeviceState*>(
        Aero::RenderDevice::Impl::NativeBackend(*device));
}

} // namespace

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const D3D11DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if ((options.device == 0U) != (options.immediateContext == 0U)) {
        return InvalidArgument(
            "D3D11 borrowed device and immediate context must be provided together");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* state = new (std::nothrow) D3D11DeviceState(options, selected);
    if (state == nullptr) return OutOfMemory("Unable to allocate D3D11 device state");
    Base::Result<void> initialized = state->Initialize();
    if (!initialized) {
        delete state;
        return initialized.GetStatus();
    }
    return AdoptRenderDevice(state, &selected);
}

Base::Result<Base::Ref<RenderSurface>> CreateD3D11EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    D3D11DeviceState* state = DeviceStateFrom(device);
    if (state == nullptr || options.acquireTarget == nullptr) {
        return InvalidArgument(
            "D3D11 embedded surface requires matching device and target callback");
    }
    if ((options.device != 0U &&
         options.device != state->GraphicsBackend()->NativeDevice()) ||
        (options.immediateContext != 0U &&
         options.immediateContext !=
             state->GraphicsBackend()->NativeImmediateContext())) {
        return InvalidArgument(
            "D3D11 embedded surface native handles do not match render device");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* surface = new (std::nothrow)
        D3D11SurfaceState(*state, options, selected);
    if (surface == nullptr) return OutOfMemory("Unable to allocate D3D11 surface state");
    Base::Result<void> initialized = surface->Initialize();
    if (!initialized) {
        delete surface;
        return initialized.GetStatus();
    }
    return AdoptOwnedRenderSurface(
        std::move(device), surface, RenderSurfaceKind::Embedded, &selected);
}

Base::Result<Base::Ref<RenderSurface>> CreateD3D11WindowSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    D3D11DeviceState* state = DeviceStateFrom(device);
    if (state == nullptr || !options.window.IsValid() ||
        options.window.system != WindowSystem::Win32 ||
        options.width == 0U || options.height == 0U) {
        return InvalidArgument("D3D11 window surface options are invalid");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* surface = new (std::nothrow)
        D3D11SurfaceState(*state, options, selected);
    if (surface == nullptr) return OutOfMemory("Unable to allocate D3D11 surface state");
    Base::Result<void> initialized = surface->Initialize();
    if (!initialized) {
        delete surface;
        return initialized.GetStatus();
    }
    return AdoptOwnedRenderSurface(
        std::move(device), surface, RenderSurfaceKind::Window, &selected);
}

Base::Result<Base::Ref<RenderSurface>> CreateD3D11EmbeddedSurface(
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateD3D11Device(DeviceOptionsFrom(options), allocator);
    if (!device) return device.GetStatus();
    return CreateD3D11EmbeddedSurface(
        std::move(device).Value(), options, allocator);
}

Base::Result<Base::Ref<RenderSurface>> CreateD3D11WindowSurface(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateD3D11Device(DeviceOptionsFrom(options), allocator);
    if (!device) return device.GetStatus();
    return CreateD3D11WindowSurface(
        std::move(device).Value(), options, allocator);
}

} // namespace Aero::Render::Detail
