#include "render/private/BackendApi.hpp"
#include "render/private/RenderTarget.hpp"
#include "render/Renderer.hpp"
#include "render/d3d11/D3D11Backend.hpp"
#include "render/d3d11/D3D11Shaders.hpp"

#include <new>
#include <utility>

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

class D3D11TargetState;

class D3D11DeviceState final : public Aero::RenderDevice::Impl {
public:
    D3D11DeviceState(
        const D3D11DeviceOptions& options,
        Base::IAllocator& allocator) noexcept
        : Aero::RenderDevice::Impl(allocator),
          options_(options),
          allocator_(&allocator) {}

    ~D3D11DeviceState() noexcept override;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept override;
    void ReleaseRenderer(const void* rendererToken) noexcept override;
    void NotifyDeviceLost() noexcept override;
    Base::Result<void> RestoreDevice() noexcept override;
    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept override;
    BackendHealth GetDeviceHealth() const noexcept override;
    ::Aero::RenderFrameStatistics
        LastFrameStatistics() const noexcept override;
    Aero::Render::Detail::RenderResources Resources() noexcept override;

    RenderBackendKind Backend() const noexcept override {
        return RenderBackendKind::D3D11;
    }
    Graphics::D3D11GraphicsBackend* GraphicsBackend() noexcept {
        return graphics_;
    }
    Graphics::GraphicsDevice* GraphicsDevice() noexcept { return device_; }
    ::Aero::Render::Renderer* Renderer() noexcept { return renderer_; }
    bool IsReady() const noexcept {
        return initialized_ && !deviceLost_ && graphics_ != nullptr &&
            device_ != nullptr && renderer_ != nullptr;
    }

    void Attach(D3D11TargetState& target) noexcept;
    void Detach(D3D11TargetState& target) noexcept;

private:
    void ShutdownDevice(bool notifyTargets) noexcept;
    void NotifyTargetsDeviceLost() noexcept;
    void RestoreTargets() noexcept;

    D3D11DeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Graphics::D3D11GraphicsBackend* graphics_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
    ::Aero::Render::Renderer* renderer_ = nullptr;
    D3D11TargetState* targets_ = nullptr;
    bool initialized_ = false;
    bool deviceLost_ = false;
};

class D3D11TargetState final : public Aero::RenderTarget::Impl {
public:
    D3D11TargetState(
        D3D11DeviceState& device,
        const D3D11EmbeddedTargetOptions& options,
        Base::IAllocator& allocator) noexcept
        : Aero::RenderTarget::Impl(RenderTargetKind::Embedded),
          device_(&device),
          allocator_(&allocator),
          embeddedOptions_(options),
          embedded_(true) {
        device_->Attach(*this);
    }

    D3D11TargetState(
        D3D11DeviceState& device,
        const D3D11WindowTargetOptions& options,
        Base::IAllocator& allocator) noexcept
        : Aero::RenderTarget::Impl(RenderTargetKind::Window),
          device_(&device),
          allocator_(&allocator),
          windowOptions_(options),
          embedded_(false) {
        device_->Attach(*this);
    }

    ~D3D11TargetState() noexcept override {
        ShutdownTarget();
        if (device_ != nullptr) device_->Detach(*this);
    }

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady()) {
            health_ = SurfaceHealth::Lost;
            return NotInitialized("D3D11 target requires a ready render device");
        }
        ShutdownTarget();
        descriptor_ = MakeDescriptor();
        if (!embedded_) {
            swapChain_ = new (std::nothrow)
                Graphics::D3D11SwapChainSurface(
                    *device_->GraphicsBackend(), allocator_);
            if (swapChain_ == nullptr) {
                health_ = SurfaceHealth::Failed;
                return OutOfMemory("Unable to allocate D3D11 window target");
            }
            const Graphics::SurfaceCapabilities capabilities =
                swapChain_->QuerySurfaceCapabilities();
            Base::Result<void> valid =
                Graphics::ValidateNativeSurfaceDescriptor(
                    descriptor_, capabilities);
            if (!valid) {
                ShutdownTarget();
                health_ = SurfaceHealth::Failed;
                return valid.GetStatus();
            }
            Base::Result<void> created = swapChain_->CreateSurface(descriptor_);
            if (!created) {
                ShutdownTarget();
                health_ = SurfaceHealth::Failed;
                return created.GetStatus();
            }
        }
        deviceGeneration_ = device_->BackendGeneration();
        nextFrameSerial_ = 1U;
        health_ = SurfaceHealth::Ready;
        return {};
    }

    Base::Result<Graphics::ExternalRenderTargetDescriptor> Acquire(
        std::uint64_t frameSerial) noexcept {
        if (!embedded_) {
            if (swapChain_ == nullptr) {
                return NotInitialized("D3D11 window target is unavailable");
            }
            return swapChain_->AcquireSurfaceTarget(frameSerial);
        }
        if (embeddedOptions_.acquireTarget == nullptr) {
            return InvalidState("D3D11 embedded target callback is unavailable");
        }
        D3D11EmbeddedTarget target;
        Base::Status acquired = embeddedOptions_.acquireTarget(
            embeddedOptions_.callbackContext, &target);
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
        result.depthStencilFormat = Graphics::GraphicsTextureFormat::Depth24Stencil8;
        result.sampleCount = 1U;
        result.stableId = target.stableId;
        Base::Result<void> valid =
            Graphics::ValidateExternalRenderTargetDescriptor(result);
        return valid
            ? Base::Result<Graphics::ExternalRenderTargetDescriptor>(result)
            : Base::Result<Graphics::ExternalRenderTargetDescriptor>(valid.GetStatus());
    }

    Base::Result<void> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept override {
        if (!IsReady()) return InvalidState("D3D11 target is not ready");
        if (nextFrameSerial_ == UINT64_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "D3D11 target frame serial space is exhausted");
        }
        const std::uint64_t frameSerial = nextFrameSerial_++;
        Base::Result<Graphics::ExternalRenderTargetDescriptor> acquired =
            Acquire(frameSerial);
        if (!acquired) {
            RefreshHealth();
            return acquired.GetStatus();
        }
        const auto& native = acquired.Value();

        Graphics::D3D11ExternalRenderTargetDescriptor external;
        external.texture2D = native.colorTarget;
        external.depthStencilView = native.depthStencilTarget;
        external.texture.width = native.width;
        external.texture.height = native.height;
        external.texture.sampleCount = native.sampleCount;
        external.texture.format = native.colorFormat;
        external.texture.usage =
            Graphics::TextureUsageBit(Graphics::TextureUsage::RenderTarget) |
            Graphics::TextureUsageBit(Graphics::TextureUsage::CopySource);
        external.stableId = native.stableId;
        Base::Result<Graphics::ResourceHandle> imported =
            Graphics::ImportD3D11ExternalRenderTarget(
                *device_->GraphicsDevice(),
                *device_->GraphicsBackend(),
                external);
        if (!imported) {
            if (!embedded_ && swapChain_ != nullptr) {
                swapChain_->DiscardSurfaceFrame(frameSerial);
            }
            return imported.GetStatus();
        }

        Base::Result<Graphics::FenceValue> submitted =
            device_->Renderer()->RenderOnscreen(
                rendererToken,
                frame,
                {imported.Value(), native.width, native.height,
                 embedded_
                     ? Graphics::LoadOperation::Load
                     : Graphics::LoadOperation::Clear});
        if (!submitted && !embedded_ && swapChain_ != nullptr) {
            swapChain_->DiscardSurfaceFrame(frameSerial);
        }

        Base::Result<void> presented;
        if (submitted && !embedded_ && swapChain_ != nullptr) {
            presented = swapChain_->PresentSurface(
                frameSerial, submitted.Value());
        }

        const Graphics::FenceValue retireFence =
            device_->GraphicsDevice()->LastSubmittedFence();
        Base::Result<void> retired =
            device_->GraphicsDevice()->DestroyResource(
                imported.Value(), retireFence);
        if (!submitted) {
            RefreshHealth();
            return submitted.GetStatus();
        }
        if (!presented) {
            RefreshHealth();
            return presented.GetStatus();
        }
        return retired;
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept override {
        if (embedded_) return {};
        if (!IsReady() || swapChain_ == nullptr) {
            return InvalidState("D3D11 window target is not ready");
        }
        windowOptions_.width = width;
        windowOptions_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        Base::Result<std::uint32_t> collected =
            device_->GraphicsDevice()->CollectGarbage();
        if (!collected) return collected.GetStatus();
        Base::Result<void> resized = swapChain_->ResizeSurface(width, height);
        if (!resized) RefreshHealth();
        return resized;
    }

    void NotifySurfaceLost() noexcept override {
        if (swapChain_ != nullptr) swapChain_->NotifySurfaceLost();
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
        if (health_ != SurfaceHealth::Ready) return health_;
        return !embedded_ &&
                (swapChain_ == nullptr || swapChain_->IsSurfaceLost())
            ? SurfaceHealth::Lost
            : SurfaceHealth::Ready;
    }

    void OnDeviceLost() noexcept {
        ShutdownTarget();
        health_ = SurfaceHealth::Lost;
    }

    void OnDeviceRestored() noexcept {
        if (device_ == nullptr || !device_->IsReady()) return;
        Base::Result<void> restored = Initialize();
        if (!restored) health_ = SurfaceHealth::Lost;
    }

    void OnDeviceDestroyed() noexcept {
        ShutdownTarget();
        device_ = nullptr;
        previous_ = nullptr;
        next_ = nullptr;
        health_ = SurfaceHealth::Shutdown;
    }

private:
    friend class D3D11DeviceState;

    bool IsReady() const noexcept {
        return GetSurfaceHealth() == SurfaceHealth::Ready &&
            device_->Renderer() != nullptr;
    }

    void RefreshHealth() noexcept {
        if (!embedded_ && swapChain_ != nullptr && swapChain_->IsSurfaceLost()) {
            health_ = SurfaceHealth::Lost;
        }
    }

    Graphics::NativeSurfaceDescriptor MakeDescriptor() const noexcept {
        Graphics::NativeSurfaceDescriptor descriptor;
        descriptor.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        descriptor.depthStencilFormat = Graphics::GraphicsTextureFormat::Depth24Stencil8;
        descriptor.sampleCount = 1U;
        descriptor.stableId = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(this));
        if (!embedded_) {
            descriptor.width = windowOptions_.width;
            descriptor.height = windowOptions_.height;
            descriptor.kind = Graphics::SurfaceKind::D3D11Window;
            descriptor.ownership = Graphics::SurfaceOwnership::Owned;
            descriptor.presentMode = ToRhiPresentMode(windowOptions_.presentMode);
            descriptor.d3d11.window = windowOptions_.window.window;
            descriptor.d3d11.device = device_->GraphicsBackend()->NativeDevice();
            descriptor.d3d11.immediateContext =
                device_->GraphicsBackend()->NativeImmediateContext();
        }
        return descriptor;
    }

    void ShutdownTarget() noexcept {
        if (swapChain_ != nullptr) {
            swapChain_->DestroySurface();
            delete swapChain_;
            swapChain_ = nullptr;
        }
        deviceGeneration_ = 0U;
    }

    D3D11DeviceState* device_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    D3D11EmbeddedTargetOptions embeddedOptions_;
    D3D11WindowTargetOptions windowOptions_;
    bool embedded_ = false;
    Graphics::NativeSurfaceDescriptor descriptor_;
    Graphics::D3D11SwapChainSurface* swapChain_ = nullptr;
    std::uint64_t deviceGeneration_ = 0U;
    std::uint64_t nextFrameSerial_ = 1U;
    SurfaceHealth health_ = SurfaceHealth::Lost;
    D3D11TargetState* previous_ = nullptr;
    D3D11TargetState* next_ = nullptr;
};

D3D11DeviceState::~D3D11DeviceState() noexcept {
    while (targets_ != nullptr) {
        D3D11TargetState* target = targets_;
        targets_ = target->next_;
        target->OnDeviceDestroyed();
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
            options_.statePolicy == D3D11StatePreservationPolicy::PreserveRequiredState
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
    if (graphics_ == nullptr) {
        return OutOfMemory("Unable to allocate D3D11 backend");
    }
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
    Base::Result<std::uint64_t> generation = AdvanceGeneration();
    if (!generation) {
        ShutdownDevice(false);
        return generation.GetStatus();
    }
    renderer_ = new (std::nothrow) ::Aero::Render::Renderer(
        *device_, ::Aero::Render::MakeD3D11FrameShaderSet(),
        generation.Value(), allocator_);
    if (renderer_ == nullptr) {
        ShutdownDevice(false);
        return OutOfMemory("Unable to allocate D3D11 renderer");
    }
    status = renderer_->Initialize();
    if (!status) {
        ShutdownDevice(false);
        return status.GetStatus();
    }
    initialized_ = true;
    deviceLost_ = false;
    RestoreTargets();
    return {};
}

Base::Result<void> D3D11DeviceState::RenderOffscreen(
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    if (!IsReady()) return NotInitialized("D3D11 device is not initialized");
    Base::Result<Graphics::FenceValue> submitted =
        renderer_->RenderOffscreen(rendererToken, frame);
    return submitted ? Base::Result<void>() : Base::Result<void>(submitted.GetStatus());
}

void D3D11DeviceState::ReleaseRenderer(const void* rendererToken) noexcept {
    if (renderer_ != nullptr) renderer_->ReleaseRenderer(rendererToken);
}

void D3D11DeviceState::NotifyDeviceLost() noexcept {
    if (deviceLost_) return;
    NotifyTargetsDeviceLost();
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
    if (graphics_ == nullptr || device_ == nullptr) return {};
    const Graphics::FenceValue fence = device_->LastSubmittedFence();
    return fence != 0U
        ? graphics_->WaitForFence(fence, timeoutMilliseconds)
        : Base::Result<void>();
}

BackendHealth D3D11DeviceState::GetDeviceHealth() const noexcept {
    if (deviceLost_ ||
        (device_ != nullptr && device_->Backend().IsDeviceLost())) {
        return BackendHealth::DeviceLost;
    }
    return IsReady() ? BackendHealth::Ready : BackendHealth::Failed;
}

::Aero::RenderFrameStatistics
D3D11DeviceState::LastFrameStatistics() const noexcept {
    ::Aero::RenderFrameStatistics result;
    if (renderer_ == nullptr) return result;
    const ::Aero::Render::FrameEncoderStatistics source =
        renderer_->LastStatistics();
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
        ? renderer_->Resources()
        : Aero::Render::Detail::RenderResources{};
}

void D3D11DeviceState::Attach(D3D11TargetState& target) noexcept {
    target.previous_ = nullptr;
    target.next_ = targets_;
    if (targets_ != nullptr) targets_->previous_ = &target;
    targets_ = &target;
}

void D3D11DeviceState::Detach(D3D11TargetState& target) noexcept {
    if (target.previous_ != nullptr) target.previous_->next_ = target.next_;
    if (target.next_ != nullptr) target.next_->previous_ = target.previous_;
    if (targets_ == &target) targets_ = target.next_;
    target.previous_ = nullptr;
    target.next_ = nullptr;
}

void D3D11DeviceState::ShutdownDevice(bool notifyTargets) noexcept {
    if (notifyTargets) NotifyTargetsDeviceLost();
    initialized_ = false;
    delete renderer_;
    renderer_ = nullptr;
    delete device_;
    device_ = nullptr;
    if (graphics_ != nullptr) {
        graphics_->Shutdown();
        delete graphics_;
        graphics_ = nullptr;
    }
}

void D3D11DeviceState::NotifyTargetsDeviceLost() noexcept {
    for (D3D11TargetState* target = targets_;
         target != nullptr; target = target->next_) {
        target->OnDeviceLost();
    }
}

void D3D11DeviceState::RestoreTargets() noexcept {
    for (D3D11TargetState* target = targets_;
         target != nullptr; target = target->next_) {
        target->OnDeviceRestored();
    }
}

D3D11DeviceState* DeviceStateFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device ||
        Aero::RenderDevice::Impl::Backend(*device) != RenderBackendKind::D3D11) {
        return nullptr;
    }
    return static_cast<D3D11DeviceState*>(
        Aero::RenderDevice::Impl::BackendState(*device));
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

Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    D3D11DeviceState* state = DeviceStateFrom(device);
    if (state == nullptr || options.acquireTarget == nullptr) {
        return InvalidArgument(
            "D3D11 embedded target requires a matching device and target callback");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* target = new (std::nothrow)
        D3D11TargetState(*state, options, selected);
    if (target == nullptr) return OutOfMemory("Unable to allocate D3D11 target state");
    Base::Result<void> initialized = target->Initialize();
    if (!initialized) {
        delete target;
        return initialized.GetStatus();
    }
    return AdoptRenderTarget(
        std::move(device), target, RenderTargetKind::Embedded, &selected);
}

Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11WindowTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    D3D11DeviceState* state = DeviceStateFrom(device);
    if (state == nullptr || !options.window.IsValid() ||
        options.window.system != Platform::WindowSystem::Win32 ||
        options.width == 0U || options.height == 0U) {
        return InvalidArgument("D3D11 window target options are invalid");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* target = new (std::nothrow)
        D3D11TargetState(*state, options, selected);
    if (target == nullptr) return OutOfMemory("Unable to allocate D3D11 target state");
    Base::Result<void> initialized = target->Initialize();
    if (!initialized) {
        delete target;
        return initialized.GetStatus();
    }
    return AdoptRenderTarget(
        std::move(device), target, RenderTargetKind::Window, &selected);
}

} // namespace Aero::Render::Detail
