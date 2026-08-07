#include "render/private/BackendApi.hpp"
#include "render/private/RenderTarget.hpp"
#include "render/Renderer.hpp"
#include "render/opengl33/OpenGL33Backend.hpp"
#include "render/opengl33/OpenGL33Shaders.hpp"

#include <functional>
#include <new>
#include <thread>
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

Graphics::GlThreadToken CurrentThreadToken(void*) noexcept {
    Graphics::GlThreadToken value = static_cast<Graphics::GlThreadToken>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return value != 0U ? value : 1U;
}

class OpenGL33TargetState;

class OpenGL33DeviceState final : public Aero::RenderDevice::Impl {
public:
    OpenGL33DeviceState(
        const OpenGL33DeviceOptions& options,
        Base::IAllocator& allocator) noexcept
        : Aero::RenderDevice::Impl(allocator),
          options_(options),
          allocator_(&allocator) {}

    ~OpenGL33DeviceState() noexcept override;

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
        return RenderBackendKind::OpenGL33;
    }
    Base::Result<void> MakeCurrent() noexcept;
    Graphics::OpenGL33GraphicsBackend* GraphicsBackend() noexcept {
        return graphics_;
    }
    Graphics::GraphicsDevice* GraphicsDevice() noexcept { return device_; }
    ::Aero::Render::Renderer* Renderer() noexcept { return renderer_; }
    Graphics::GlContextGeneration ContextGeneration() const noexcept {
        return contextGeneration_;
    }
    bool IsReady() const noexcept {
        return initialized_ && !deviceLost_ && graphics_ != nullptr &&
            device_ != nullptr && renderer_ != nullptr;
    }

    void Attach(OpenGL33TargetState& target) noexcept;
    void Detach(OpenGL33TargetState& target) noexcept;

private:
    static Graphics::GlProcAddress Resolve(void* context, const char* name) noexcept;
    static bool IsCurrent(void* context, const void*) noexcept;
    Base::Result<Graphics::GlContextBinding> ContextBinding() noexcept;
    void ShutdownDevice(bool notifyTargets) noexcept;
    void NotifyTargetsDeviceLost() noexcept;
    void RestoreTargets() noexcept;

    OpenGL33DeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Graphics::OpenGL33GraphicsBackend* graphics_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
    ::Aero::Render::Renderer* renderer_ = nullptr;
    OpenGL33TargetState* targets_ = nullptr;
    Graphics::GlContextGeneration contextGeneration_ = 0U;
    bool initialized_ = false;
    bool deviceLost_ = false;
};

class OpenGL33TargetState final : public Aero::RenderTarget::Impl {
public:
    OpenGL33TargetState(
        OpenGL33DeviceState& device,
        const OpenGL33EmbeddedTargetOptions& options,
        Base::IAllocator& allocator) noexcept
        : Aero::RenderTarget::Impl(RenderTargetKind::Embedded),
          device_(&device),
          options_(options),
          allocator_(&allocator) {
        device_->Attach(*this);
    }

    ~OpenGL33TargetState() noexcept override {
        if (device_ != nullptr) device_->Detach(*this);
    }

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady() ||
            options_.acquireTarget == nullptr) {
            health_ = SurfaceHealth::Lost;
            return NotInitialized("OpenGL target requires a ready render device");
        }
        deviceGeneration_ = device_->BackendGeneration();
        health_ = SurfaceHealth::Ready;
        return {};
    }

    Base::Result<void> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept override {
        if (!IsReady()) return InvalidState("OpenGL target is not ready");
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

        Graphics::OpenGL33ExternalRenderTargetDescriptor external;
        external.framebuffer = static_cast<Graphics::GlUInt>(target.framebuffer);
        external.depthStencilTexture =
            static_cast<Graphics::GlUInt>(target.depthStencilTexture);
        external.texture.width = target.width;
        external.texture.height = target.height;
        external.texture.format = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        external.texture.sampleCount = 1U;
        external.texture.usage =
            Graphics::TextureUsageBit(Graphics::TextureUsage::RenderTarget);
        external.contextGeneration = device_->ContextGeneration();
        external.stableId = target.stableId;
        external.defaultFramebuffer = target.defaultFramebuffer;
        Base::Result<Graphics::ResourceHandle> imported =
            Graphics::ImportOpenGL33ExternalRenderTarget(
                *device_->GraphicsDevice(), *device_->GraphicsBackend(), external);
        if (!imported) return imported.GetStatus();

        Base::Result<Graphics::FenceValue> submitted =
            device_->Renderer()->RenderOnscreen(
                rendererToken,
                frame,
                {imported.Value(), target.width, target.height,
                 Graphics::LoadOperation::Load});
        const Graphics::FenceValue retireFence =
            device_->GraphicsDevice()->LastSubmittedFence();
        Base::Result<void> retired =
            device_->GraphicsDevice()->DestroyResource(
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
            return InvalidState("Only a lost OpenGL target can be restored");
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

    void OnDeviceLost() noexcept {
        health_ = SurfaceHealth::Lost;
        deviceGeneration_ = 0U;
    }

    void OnDeviceRestored() noexcept {
        if (device_ == nullptr || !device_->IsReady()) return;
        Base::Result<void> restored = Initialize();
        if (!restored) health_ = SurfaceHealth::Lost;
    }

    void OnDeviceDestroyed() noexcept {
        device_ = nullptr;
        previous_ = nullptr;
        next_ = nullptr;
        deviceGeneration_ = 0U;
        health_ = SurfaceHealth::Shutdown;
    }

private:
    friend class OpenGL33DeviceState;

    bool IsReady() const noexcept {
        return GetSurfaceHealth() == SurfaceHealth::Ready &&
            device_->Renderer() != nullptr;
    }

    OpenGL33DeviceState* device_ = nullptr;
    OpenGL33EmbeddedTargetOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    OpenGL33TargetState* previous_ = nullptr;
    OpenGL33TargetState* next_ = nullptr;
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
    while (targets_ != nullptr) {
        OpenGL33TargetState* target = targets_;
        targets_ = target->next_;
        target->OnDeviceDestroyed();
    }
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
    Base::Result<void> status = graphics_->Initialize();
    if (!status) {
        ShutdownDevice(false);
        return status.GetStatus();
    }
    device_ = new (std::nothrow) Graphics::GraphicsDevice(*graphics_, allocator_);
    if (device_ == nullptr) {
        ShutdownDevice(false);
        return OutOfMemory("Unable to allocate OpenGL graphics device");
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
        *device_, ::Aero::Render::MakeOpenGL33FrameShaderSet(),
        generation.Value(), allocator_);
    if (renderer_ == nullptr) {
        ShutdownDevice(false);
        return OutOfMemory("Unable to allocate OpenGL renderer");
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

Base::Result<void> OpenGL33DeviceState::RenderOffscreen(
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    if (!IsReady()) return NotInitialized("OpenGL device is not initialized");
    Base::Result<void> current = MakeCurrent();
    if (!current) return current.GetStatus();
    Base::Result<Graphics::FenceValue> submitted =
        renderer_->RenderOffscreen(rendererToken, frame);
    return submitted ? Base::Result<void>() : Base::Result<void>(submitted.GetStatus());
}

void OpenGL33DeviceState::ReleaseRenderer(const void* rendererToken) noexcept {
    if (renderer_ == nullptr) return;
    Base::Result<void> current = MakeCurrent();
    if (current) renderer_->ReleaseRenderer(rendererToken);
}

void OpenGL33DeviceState::NotifyDeviceLost() noexcept {
    if (deviceLost_) return;
    NotifyTargetsDeviceLost();
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

void OpenGL33DeviceState::Attach(OpenGL33TargetState& target) noexcept {
    target.previous_ = nullptr;
    target.next_ = targets_;
    if (targets_ != nullptr) targets_->previous_ = &target;
    targets_ = &target;
}

void OpenGL33DeviceState::Detach(OpenGL33TargetState& target) noexcept {
    if (target.previous_ != nullptr) target.previous_->next_ = target.next_;
    if (target.next_ != nullptr) target.next_->previous_ = target.previous_;
    if (targets_ == &target) targets_ = target.next_;
    target.previous_ = nullptr;
    target.next_ = nullptr;
}

void OpenGL33DeviceState::ShutdownDevice(bool notifyTargets) noexcept {
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
    contextGeneration_ = 0U;
}

void OpenGL33DeviceState::NotifyTargetsDeviceLost() noexcept {
    for (OpenGL33TargetState* target = targets_;
         target != nullptr; target = target->next_) {
        target->OnDeviceLost();
    }
}

void OpenGL33DeviceState::RestoreTargets() noexcept {
    for (OpenGL33TargetState* target = targets_;
         target != nullptr; target = target->next_) {
        target->OnDeviceRestored();
    }
}

OpenGL33DeviceState* DeviceStateFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device || Aero::RenderDevice::Impl::Backend(*device) !=
            RenderBackendKind::OpenGL33) {
        return nullptr;
    }
    return static_cast<OpenGL33DeviceState*>(
        Aero::RenderDevice::Impl::BackendState(*device));
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

Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    OpenGL33DeviceState* state = DeviceStateFrom(device);
    if (state == nullptr || options.acquireTarget == nullptr) {
        return InvalidArgument(
            "OpenGL embedded target requires a matching device and target callback");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* target = new (std::nothrow)
        OpenGL33TargetState(*state, options, selected);
    if (target == nullptr) return OutOfMemory("Unable to allocate OpenGL target state");
    Base::Result<void> initialized = target->Initialize();
    if (!initialized) {
        delete target;
        return initialized.GetStatus();
    }
    return AdoptOwnedRenderTarget(
        std::move(device), target, RenderTargetKind::Embedded, &selected);
}

} // namespace Aero::Render::Detail
