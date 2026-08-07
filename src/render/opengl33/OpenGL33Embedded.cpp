#include "render/private/BackendApi.hpp"
#include "render/private/RenderSurface.hpp"
#include "render/DeviceRenderer.hpp"
#include "render/opengl33/OpenGL33Backend.hpp"
#include "render/opengl33/OpenGL33Shaders.hpp"

#include <functional>
#include <new>
#include <thread>

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

Graphics::GlThreadToken CurrentThreadToken(void*) noexcept {
    Graphics::GlThreadToken value = static_cast<Graphics::GlThreadToken>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return value != 0U ? value : 1U;
}

class OpenGL33EmbeddedSurfaceState;

class OpenGL33DeviceState final
    : public NativeRenderDevice {
public:
    OpenGL33DeviceState(
        const OpenGL33DeviceOptions& options,
        Base::IAllocator& allocator) noexcept
        : options_(options), allocator_(&allocator) {}

    ~OpenGL33DeviceState() noexcept;

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
        return RenderBackendKind::OpenGL33;
    }
    Base::Result<void> MakeCurrent() noexcept;
    Graphics::OpenGL33GraphicsBackend* GraphicsBackend() noexcept {
        return graphics_;
    }
    Graphics::GraphicsDevice* GraphicsDevice() noexcept { return device_; }
    ::Aero::Render::DeviceRenderer* Renderer() noexcept { return renderer_; }
    Graphics::GlContextGeneration ContextGeneration() const noexcept {
        return contextGeneration_;
    }
    bool IsReady() const noexcept {
        return initialized_ && !deviceLost_ && graphics_ != nullptr &&
            device_ != nullptr && renderer_ != nullptr;
    }

    const OpenGL33DeviceOptions& Options() const noexcept { return options_; }
    void Attach(OpenGL33EmbeddedSurfaceState& surface) noexcept;
    void Detach(OpenGL33EmbeddedSurfaceState& surface) noexcept;

private:
    static Graphics::GlProcAddress Resolve(void* context, const char* name) noexcept;
    static bool IsCurrent(void* context, const void*) noexcept;
    Base::Result<Graphics::GlContextBinding> ContextBinding() noexcept;
    void ShutdownDevice(bool notifySurfaces) noexcept;
    void NotifySurfacesDeviceLost() noexcept;
    void RestoreSurfaces() noexcept;

    OpenGL33DeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Graphics::OpenGL33GraphicsBackend* graphics_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
    ::Aero::Render::DeviceRenderer* renderer_ = nullptr;
    OpenGL33EmbeddedSurfaceState* surfaces_ = nullptr;
    Graphics::GlContextGeneration contextGeneration_ = 0U;
    bool initialized_ = false;
    bool deviceLost_ = false;
};

class OpenGL33ExternalSurface final : public Graphics::ISurfaceBackend {
public:
    OpenGL33ExternalSurface(
        OpenGL33DeviceState& device,
        const OpenGL33EmbeddedSurfaceOptions& options) noexcept
        : device_(&device), options_(options) {}

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
        if (lost_ || device_ == nullptr || options_.acquireTarget == nullptr) {
            return InvalidState("OpenGL embedded surface is unavailable");
        }
        Base::Result<void> current = device_->MakeCurrent();
        if (!current) return current.GetStatus();
        OpenGL33EmbeddedTarget target;
        void* targetContext = options_.targetContext != nullptr
            ? options_.targetContext
            : options_.callbackContext;
        Base::Status acquired = options_.acquireTarget(targetContext, &target);
        if (!acquired.IsOk()) return acquired;
        if (target.width == 0U || target.height == 0U ||
            (!target.defaultFramebuffer && target.framebuffer == 0U)) {
            return InvalidArgument("OpenGL embedded target is invalid");
        }
        Graphics::ExternalRenderTargetDescriptor result;
        result.colorTarget = target.framebuffer;
        result.depthStencilTarget = target.depthStencilTexture;
        result.width = target.width;
        result.height = target.height;
        result.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        result.depthStencilFormat = Graphics::GraphicsTextureFormat::Depth24Stencil8;
        result.sampleCount = 1U;
        result.defaultFramebuffer = target.defaultFramebuffer;
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
    OpenGL33DeviceState* device_ = nullptr;
    OpenGL33EmbeddedSurfaceOptions options_;
    bool lost_ = false;
};

class OpenGL33EmbeddedSurfaceState final
    : public NativeRenderTarget {
public:
    OpenGL33EmbeddedSurfaceState(
        OpenGL33DeviceState& device,
        const OpenGL33EmbeddedSurfaceOptions& options,
        Base::IAllocator& allocator) noexcept
        : device_(&device), options_(options), allocator_(&allocator) {
        device_->Attach(*this);
    }

    ~OpenGL33EmbeddedSurfaceState() noexcept {
        ShutdownSurface();
        if (device_ != nullptr) device_->Detach(*this);
    }

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady()) {
            health_ = SurfaceHealth::Lost;
            return NotInitialized("OpenGL surface requires a ready render device");
        }
        ShutdownSurface();
        backend_ = new (std::nothrow) OpenGL33ExternalSurface(*device_, options_);
        if (backend_ == nullptr) {
            health_ = SurfaceHealth::Failed;
            return OutOfMemory("Unable to allocate OpenGL surface backend");
        }
        surface_ = new (std::nothrow) Graphics::SurfaceSession(*backend_);
        if (surface_ == nullptr) {
            ShutdownSurface();
            health_ = SurfaceHealth::Failed;
            return OutOfMemory("Unable to allocate OpenGL surface session");
        }
        descriptor_ = {};
        descriptor_.kind = Graphics::SurfaceKind::ExternalRenderTarget;
        descriptor_.ownership = Graphics::SurfaceOwnership::Borrowed;
        descriptor_.width = 1U;
        descriptor_.height = 1U;
        descriptor_.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        descriptor_.depthStencilFormat = Graphics::GraphicsTextureFormat::Depth24Stencil8;
        descriptor_.sampleCount = 1U;
        descriptor_.stableId = reinterpret_cast<std::uintptr_t>(this);
        descriptor_.external.colorTarget = 1U;
        Base::Result<void> initialized = surface_->Initialize(descriptor_);
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
        if (!IsReady()) return InvalidState("OpenGL embedded surface is not ready");
        Base::Result<void> current = device_->MakeCurrent();
        if (!current) return current.GetStatus();
        Base::Result<Graphics::SurfaceFrame> acquired = surface_->AcquireFrame();
        if (!acquired) {
            RefreshHealth();
            return acquired.GetStatus();
        }
        Graphics::SurfaceFrame nativeFrame = acquired.Value();
        Graphics::OpenGL33ExternalRenderTargetDescriptor external;
        external.framebuffer = static_cast<Graphics::GlUInt>(
            nativeFrame.target.colorTarget);
        external.depthStencilTexture = static_cast<Graphics::GlUInt>(
            nativeFrame.target.depthStencilTarget);
        external.texture.width = nativeFrame.target.width;
        external.texture.height = nativeFrame.target.height;
        external.texture.format = nativeFrame.target.colorFormat;
        external.texture.sampleCount = nativeFrame.target.sampleCount;
        external.texture.usage = Graphics::TextureUsageBit(
            Graphics::TextureUsage::RenderTarget);
        external.contextGeneration = device_->ContextGeneration();
        external.stableId = nativeFrame.target.stableId;
        external.defaultFramebuffer = nativeFrame.target.defaultFramebuffer;
        Base::Result<Graphics::ResourceHandle> imported =
            Graphics::ImportOpenGL33ExternalRenderTarget(
                *device_->GraphicsDevice(), *device_->GraphicsBackend(), external);
        if (!imported) {
            static_cast<void>(surface_->DiscardFrame(nativeFrame));
            return imported.GetStatus();
        }
        Base::Result<Graphics::FenceValue> completed =
            device_->Renderer()->RenderOnscreen(
                rendererToken,
                frame,
                {imported.Value(), nativeFrame.target.width,
                 nativeFrame.target.height, Graphics::LoadOperation::Load},
                *surface_,
                nativeFrame);
        const Graphics::FenceValue retireFence =
            device_->GraphicsDevice()->LastSubmittedFence();
        Base::Result<void> destroyed =
            device_->GraphicsDevice()->DestroyResource(
                imported.Value(), retireFence);
        if (!completed) return completed.GetStatus();
        return destroyed;
    }

    Base::Result<void> Resize(std::uint32_t, std::uint32_t) noexcept { return {}; }

    void NotifySurfaceLost() noexcept {
        if (surface_ != nullptr) {
            static_cast<void>(surface_->NotifyContextLost());
        }
        health_ = SurfaceHealth::Lost;
    }

    Base::Result<void> RestoreSurface() noexcept {
        if (health_ != SurfaceHealth::Lost) {
            return InvalidState("Only a lost OpenGL surface can be restored");
        }
        return Initialize();
    }

    SurfaceHealth GetSurfaceHealth() const noexcept {
        if (device_ == nullptr) return SurfaceHealth::Shutdown;
        if (!device_->IsReady() || deviceGeneration_ != device_->Generation()) {
            return SurfaceHealth::Lost;
        }
        if (health_ != SurfaceHealth::Ready) return health_;
        return surface_ != nullptr && surface_->State() == Graphics::SurfaceState::Ready
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
    friend class OpenGL33DeviceState;

    bool IsReady() const noexcept {
        return GetSurfaceHealth() == SurfaceHealth::Ready &&
            device_->Renderer() != nullptr;
    }

    void RefreshHealth() noexcept {
        if (surface_ != nullptr && surface_->State() != Graphics::SurfaceState::Ready) {
            health_ = SurfaceHealth::Lost;
        }
    }

    void ShutdownSurface() noexcept {
        if (surface_ != nullptr) {
            surface_->Shutdown();
            delete surface_;
            surface_ = nullptr;
        }
        delete backend_;
        backend_ = nullptr;
        deviceGeneration_ = 0U;
    }

    OpenGL33DeviceState* device_ = nullptr;
    OpenGL33EmbeddedSurfaceOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    OpenGL33ExternalSurface* backend_ = nullptr;
    Graphics::SurfaceSession* surface_ = nullptr;
    Graphics::NativeSurfaceDescriptor descriptor_;
    OpenGL33EmbeddedSurfaceState* previous_ = nullptr;
    OpenGL33EmbeddedSurfaceState* next_ = nullptr;
    std::uint64_t deviceGeneration_ = 0U;
    SurfaceHealth health_ = SurfaceHealth::Shutdown;
};

Graphics::GlProcAddress OpenGL33DeviceState::Resolve(
    void* context,
    const char* name) noexcept {
    auto* state = static_cast<OpenGL33DeviceState*>(context);
    return reinterpret_cast<Graphics::GlProcAddress>(
        state->options_.resolve(state->options_.callbackContext, name));
}

bool OpenGL33DeviceState::IsCurrent(void* context, const void*) noexcept {
    auto* state = static_cast<OpenGL33DeviceState*>(context);
    return state->options_.isCurrent(state->options_.callbackContext);
}

Base::Result<void> OpenGL33DeviceState::MakeCurrent() noexcept {
    if (options_.makeCurrent == nullptr) return {};
    Base::Status status = options_.makeCurrent(options_.callbackContext);
    return status.IsOk() ? Base::Result<void>() : Base::Result<void>(status);
}

Base::Result<Graphics::GlContextBinding>
OpenGL33DeviceState::ContextBinding() noexcept {
    Graphics::GlContextBinding binding;
    binding.userData = this;
    binding.contextHandle = this;
    binding.resolve = &Resolve;
    binding.isCurrent = &IsCurrent;
    binding.currentThreadToken = &CurrentThreadToken;
    binding.owningThreadToken = CurrentThreadToken(nullptr);
    binding.generation = options_.contextGeneration(options_.callbackContext);
    binding.embeddingMode = options_.statePolicy ==
            OpenGL33StatePreservationPolicy::PreserveRequiredState
        ? Graphics::GlEmbeddingMode::PreserveAndRestore
        : Graphics::GlEmbeddingMode::HostReset;
    return binding;
}

OpenGL33DeviceState::~OpenGL33DeviceState() noexcept {
    for (OpenGL33EmbeddedSurfaceState* surface = surfaces_;
         surface != nullptr;) {
        OpenGL33EmbeddedSurfaceState* next = surface->next_;
        surface->OnDeviceDestroyed();
        surface = next;
    }
    surfaces_ = nullptr;
    ShutdownDevice(false);
}

Base::Result<void> OpenGL33DeviceState::Initialize() noexcept {
    if (initialized_) return {};
    Base::Result<void> current = MakeCurrent();
    if (!current) return current.GetStatus();
    Base::Result<Graphics::GlFunctionTable> functions =
        Graphics::LoadGlFunctionTable(&Resolve, this);
    if (!functions) return functions.GetStatus();
    Base::Result<Graphics::GlContextBinding> binding = ContextBinding();
    if (!binding) return binding.GetStatus();
    if (binding.Value().generation == 0U) {
        return InvalidArgument("OpenGL context generation is zero");
    }
    contextGeneration_ = binding.Value().generation;
    Graphics::OpenGL33BackendOptions backendOptions;
    backendOptions.embeddingMode = binding.Value().embeddingMode;
    backendOptions.checkErrors = options_.checkErrors;
    graphics_ = new (std::nothrow) Graphics::OpenGL33GraphicsBackend(
        functions.Value(), binding.Value(), backendOptions, allocator_);
    if (graphics_ == nullptr) return OutOfMemory("Unable to allocate OpenGL backend");
    Base::Result<void> initialized = graphics_->Initialize();
    if (!initialized) {
        ShutdownDevice(false);
        return initialized.GetStatus();
    }
    device_ = new (std::nothrow) Graphics::GraphicsDevice(*graphics_, allocator_);
    if (device_ == nullptr) {
        ShutdownDevice(false);
        return OutOfMemory("Unable to allocate OpenGL graphics device");
    }
    initialized = device_->Initialize();
    if (!initialized) {
        ShutdownDevice(false);
        return initialized.GetStatus();
    }
    Base::Result<std::uint64_t> generation = AdvanceGeneration();
    if (!generation) {
        ShutdownDevice(false);
        return generation.GetStatus();
    }
    renderer_ = new (std::nothrow) ::Aero::Render::DeviceRenderer(
        *device_, ::Aero::Render::MakeOpenGL33FrameShaderSet(),
        generation.Value(), allocator_);
    if (renderer_ == nullptr) {
        ShutdownDevice(false);
        return OutOfMemory("Unable to allocate OpenGL device renderer");
    }
    initialized = renderer_->Initialize();
    if (!initialized) {
        ShutdownDevice(false);
        return initialized.GetStatus();
    }
    initialized_ = true;
    deviceLost_ = false;
    RestoreSurfaces();
    return {};
}

Base::Result<void> OpenGL33DeviceState::RenderOffscreen(
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    if (!IsReady()) return NotInitialized("OpenGL device is not initialized");
    Base::Result<void> current = MakeCurrent();
    if (!current) return current.GetStatus();
    Base::Result<Graphics::FenceValue> submitted =
        renderer_->RenderOffscreen(rendererToken, frame);
    if (!submitted) return submitted.GetStatus();
    return {};
}

void OpenGL33DeviceState::ReleaseRenderer(const void* rendererToken) noexcept {
    if (renderer_ == nullptr) return;
    Base::Result<void> current = MakeCurrent();
    if (current) renderer_->ReleaseRenderer(rendererToken);
}

void OpenGL33DeviceState::NotifyDeviceLost() noexcept {
    if (deviceLost_) return;
    NotifySurfacesDeviceLost();
    ShutdownDevice(false);
    deviceLost_ = true;
}

Base::Result<void> OpenGL33DeviceState::RestoreDevice() noexcept {
    if (!deviceLost_) return InvalidState("OpenGL device is not lost");
    deviceLost_ = false;
    Base::Result<void> restored = Initialize();
    if (!restored) deviceLost_ = true;
    return restored;
}

Base::Result<void> OpenGL33DeviceState::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    if (graphics_ == nullptr || device_ == nullptr) return {};
    const Graphics::FenceValue fence = device_->LastSubmittedFence();
    return fence != 0U
        ? graphics_->WaitForFence(
              fence,
              static_cast<std::uint64_t>(timeoutMilliseconds) * UINT64_C(1000000))
        : Base::Result<void>();
}

BackendHealth OpenGL33DeviceState::GetDeviceHealth() const noexcept {
    if (deviceLost_ ||
        (device_ != nullptr && device_->Backend().IsDeviceLost())) {
        return BackendHealth::DeviceLost;
    }
    return IsReady() ? BackendHealth::Ready : BackendHealth::Failed;
}

::Aero::RenderFrameStatistics
OpenGL33DeviceState::LastFrameStatistics() const noexcept {
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

Aero::Render::Detail::RenderResources OpenGL33DeviceState::Resources() noexcept {
    return renderer_ != nullptr
        ? renderer_->Resources()
        : Aero::Render::Detail::RenderResources{};
}

void OpenGL33DeviceState::Attach(OpenGL33EmbeddedSurfaceState& surface) noexcept {
    surface.previous_ = nullptr;
    surface.next_ = surfaces_;
    if (surfaces_ != nullptr) surfaces_->previous_ = &surface;
    surfaces_ = &surface;
}

void OpenGL33DeviceState::Detach(OpenGL33EmbeddedSurfaceState& surface) noexcept {
    if (surface.previous_ != nullptr) surface.previous_->next_ = surface.next_;
    if (surface.next_ != nullptr) surface.next_->previous_ = surface.previous_;
    if (surfaces_ == &surface) surfaces_ = surface.next_;
    surface.previous_ = nullptr;
    surface.next_ = nullptr;
}

void OpenGL33DeviceState::ShutdownDevice(bool notifySurfaces) noexcept {
    if (notifySurfaces) NotifySurfacesDeviceLost();
    initialized_ = false;
    if (renderer_ != nullptr) {
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
    contextGeneration_ = 0U;
}

void OpenGL33DeviceState::NotifySurfacesDeviceLost() noexcept {
    for (OpenGL33EmbeddedSurfaceState* surface = surfaces_;
         surface != nullptr; surface = surface->next_) {
        surface->OnDeviceLost();
    }
}

void OpenGL33DeviceState::RestoreSurfaces() noexcept {
    for (OpenGL33EmbeddedSurfaceState* surface = surfaces_;
         surface != nullptr; surface = surface->next_) {
        surface->OnDeviceRestored();
    }
}

OpenGL33DeviceState* DeviceStateFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device || Aero::RenderDevice::Impl::Backend(*device) !=
            RenderBackendKind::OpenGL33) {
        return nullptr;
    }
    return static_cast<OpenGL33DeviceState*>(
        Aero::RenderDevice::Impl::NativeBackend(*device));
}

OpenGL33DeviceOptions DeviceOptionsFrom(
    const OpenGL33EmbeddedSurfaceOptions& options) noexcept {
    OpenGL33DeviceOptions result;
    result.resolve = options.resolve;
    result.makeCurrent = options.makeCurrent;
    result.isCurrent = options.isCurrent;
    result.contextGeneration = options.contextGeneration;
    result.callbackContext = options.callbackContext;
    result.statePolicy = options.statePolicy;
    return result;
}

bool MatchesDevice(
    const OpenGL33DeviceState& device,
    const OpenGL33EmbeddedSurfaceOptions& options) noexcept {
    const OpenGL33DeviceOptions& selected = device.Options();
    return (options.resolve == nullptr || options.resolve == selected.resolve) &&
        (options.makeCurrent == nullptr || options.makeCurrent == selected.makeCurrent) &&
        (options.isCurrent == nullptr || options.isCurrent == selected.isCurrent) &&
        (options.contextGeneration == nullptr ||
         options.contextGeneration == selected.contextGeneration) &&
        (options.callbackContext == nullptr ||
         options.callbackContext == selected.callbackContext);
}

} // namespace

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (options.resolve == nullptr || options.makeCurrent == nullptr ||
        options.isCurrent == nullptr || options.contextGeneration == nullptr ||
        options.contextGeneration(options.callbackContext) == 0U) {
        return InvalidArgument("OpenGL device options are incomplete");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* state = new (std::nothrow) OpenGL33DeviceState(options, selected);
    if (state == nullptr) return OutOfMemory("Unable to allocate OpenGL device state");
    Base::Result<void> initialized = state->Initialize();
    if (!initialized) {
        delete state;
        return initialized.GetStatus();
    }
    return AdoptRenderDevice(state, &selected);
}

Base::Result<Base::Ref<RenderSurface>> CreateOpenGL33EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    OpenGL33DeviceState* state = DeviceStateFrom(device);
    if (state == nullptr || options.acquireTarget == nullptr ||
        !MatchesDevice(*state, options)) {
        return InvalidArgument(
            "OpenGL embedded surface requires matching device and target callback");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* surface = new (std::nothrow)
        OpenGL33EmbeddedSurfaceState(*state, options, selected);
    if (surface == nullptr) return OutOfMemory("Unable to allocate OpenGL surface state");
    Base::Result<void> initialized = surface->Initialize();
    if (!initialized) {
        delete surface;
        return initialized.GetStatus();
    }
    return AdoptOwnedRenderSurface(
        std::move(device), surface, RenderSurfaceKind::Embedded, &selected);
}

Base::Result<Base::Ref<RenderSurface>> CreateOpenGL33EmbeddedSurface(
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateOpenGL33Device(DeviceOptionsFrom(options), allocator);
    if (!device) return device.GetStatus();
    return CreateOpenGL33EmbeddedSurface(
        std::move(device).Value(), options, allocator);
}

} // namespace Aero::Render::Detail
