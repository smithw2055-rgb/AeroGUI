#include "render/RenderDeviceInternal.hpp"
#include "render/RenderTargetInternal.hpp"
#include "render/Renderer.hpp"
#include "render/opengl33/OpenGL33Backend.hpp"
#include "render/opengl33/OpenGL33Shaders.hpp"

#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
#include "render/platform/win32/OpenGLRenderContext.hpp"
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
#include "render/platform/x11/OpenGLRenderContext.hpp"
#endif

#include <new>
#include <utility>

namespace Aero::Render::Detail {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
using PlatformWindowRenderContext = Graphics::WglRenderContext;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
using PlatformWindowRenderContext = Graphics::GlxRenderContext;
#else
// Keeps the embeddable, no-window build well-formed without recreating a
// polymorphic surface backend. The factory fails before this local stub is
// allocated.
class PlatformWindowRenderContext final {
public:
    explicit PlatformWindowRenderContext(Base::IAllocator*) noexcept {}
    Graphics::WindowRenderContextCaps Caps() const noexcept { return {}; }
    Base::Result<void> Create(
        const Graphics::WindowRenderContextDescriptor&) noexcept {
        return Unsupported("No OpenGL window context is available");
    }
    void Shutdown() noexcept {}
    Base::Result<void> Resize(std::uint32_t, std::uint32_t) noexcept {
        return Unsupported("No OpenGL window context is available");
    }
    Base::Result<Graphics::RenderTargetBinding> AcquireTarget(
        std::uint64_t) noexcept {
        return Unsupported("No OpenGL window context is available");
    }
    Base::Result<void> Present(
        std::uint64_t, Graphics::FenceValue) noexcept {
        return Unsupported("No OpenGL window context is available");
    }
    void DiscardFrame(std::uint64_t) noexcept {}
    void NotifyLost() noexcept {}
    Base::Result<void> Restore(
        const Graphics::WindowRenderContextDescriptor&) noexcept {
        return Unsupported("No OpenGL window context is available");
    }
    bool IsLost() const noexcept { return true; }
    Base::Result<void> MakeCurrent() noexcept {
        return Unsupported("No OpenGL window context is available");
    }
    Base::Result<Graphics::GlFunctionTable> LoadFunctions() noexcept {
        return Unsupported("No OpenGL window context is available");
    }
    Base::Result<Graphics::GlContextBinding> ContextBinding() noexcept {
        return Unsupported("No OpenGL window context is available");
    }
};
#endif

Graphics::PresentMode ToRhiPresentMode(Graphics::PresentMode value) noexcept {
    switch (value) {
    case Graphics::PresentMode::Immediate: return Graphics::PresentMode::Immediate;
    case Graphics::PresentMode::Mailbox: return Graphics::PresentMode::Mailbox;
    case Graphics::PresentMode::Fifo: return Graphics::PresentMode::Fifo;
    }
    return Graphics::PresentMode::Fifo;
}

class OpenGL33WindowDeviceState final : public Aero::RenderDevice::Impl {
public:
    OpenGL33WindowDeviceState(
        const OpenGL33WindowTargetOptions& options,
        Base::IAllocator& allocator) noexcept
        : Aero::RenderDevice::Impl(allocator),
          allocator_(&allocator),
          options_(options) {}

    ~OpenGL33WindowDeviceState() noexcept override { Shutdown(); }

    RenderBackendKind Backend() const noexcept override {
        return RenderBackendKind::OpenGL33;
    }

    Graphics::DeviceCapabilities QueryNativeDeviceCapabilities() const noexcept override {
        return graphics_ != nullptr ? graphics_->Capabilities() : Graphics::DeviceCapabilities{};
    }
    Graphics::NativeRenderBackendKind NativeBackendKind() const noexcept override {
        return graphics_ != nullptr ? graphics_->Kind() : Graphics::NativeRenderBackendKind::Invalid;
    }
    Graphics::GraphicsCapabilities QueryNativeGraphicsCapabilities() const noexcept override {
        return graphics_ != nullptr ? graphics_->QueryGraphicsCapabilities() : Graphics::GraphicsCapabilities{};
    }
    Base::Result<void> CreateNativeResource(
        Graphics::ResourceHandle handle,
        const Graphics::ResourceDescriptor& descriptor) noexcept override {
        return graphics_->CreateResource(handle, descriptor);
    }
    void DestroyNativeResource(Graphics::ResourceHandle handle) noexcept override {
        if (graphics_ != nullptr) graphics_->DestroyResource(handle);
    }
    Base::Result<void> ConfigureNativeTexture(
        Graphics::ResourceHandle handle,
        const Graphics::TextureResourceDescriptor& descriptor) noexcept override {
        return graphics_->ConfigureTexture(handle, descriptor);
    }
    Base::Result<void> ConfigureNativeSampler(
        Graphics::ResourceHandle handle,
        const Graphics::SamplerDescriptor& descriptor) noexcept override {
        return graphics_->ConfigureSampler(handle, descriptor);
    }
    Base::Result<void> ConfigureNativePipeline(
        Graphics::ResourceHandle handle,
        ::Aero::Render::Detail::UiPipelineKey key) noexcept override {
        return graphics_->ConfigurePipeline(
            handle, ::Aero::Render::MakeOpenGL33UiPipeline(key));
    }
    Base::Result<void> SubmitNativeBatch(
        const ::Aero::Render::Detail::RenderBatch& batch,
        Graphics::FenceValue signalFence) noexcept override {
        return graphics_->Submit(batch, signalFence);
    }
    Graphics::FenceValue NativeLastSubmittedFence() const noexcept override {
        return graphics_ != nullptr ? graphics_->LastSubmittedFence() : 0U;
    }
    Graphics::FenceValue NativeCompletedFence() const noexcept override {
        return graphics_ != nullptr ? graphics_->CompletedFence() : 0U;
    }
    bool NativeDeviceLost() const noexcept override {
        return graphics_ == nullptr || graphics_->IsDeviceLost();
    }

    Base::Result<void> Initialize() noexcept {
        if (initialized_) return {};
        Base::Result<void> surface = CreateWindowContext();
        if (!surface) return surface.GetStatus();
        descriptor_ = MakeDescriptor();
        PlatformWindowRenderContext* backend = WindowContext();
        if (backend == nullptr) {
            Shutdown();
            return NotInitialized("OpenGL window render context is unavailable");
        }
        Base::Result<void> valid = Graphics::ValidateWindowRenderContextDescriptor(
            descriptor_, backend->Caps());
        if (!valid) {
            Shutdown();
            return valid.GetStatus();
        }
        Base::Result<void> created = backend->Create(descriptor_);
        if (!created) {
            Shutdown();
            return created.GetStatus();
        }
        Base::Result<void> current = MakeCurrent();
        if (!current) {
            Shutdown();
            return current.GetStatus();
        }
        Base::Result<Graphics::GlFunctionTable> functions = LoadFunctions();
        if (!functions) {
            Shutdown();
            return functions.GetStatus();
        }
        Base::Result<Graphics::GlContextBinding> binding = ContextBinding();
        if (!binding || binding.Value().generation == 0U) {
            Shutdown();
            return binding
                ? Base::Result<void>(InvalidState("OpenGL context generation is zero"))
                : Base::Result<void>(binding.GetStatus());
        }
        contextGeneration_ = binding.Value().generation;

        Graphics::OpenGL33CommandQueueOptions backendOptions;
        backendOptions.embeddingMode = binding.Value().embeddingMode;
        graphics_ = new (std::nothrow) Graphics::OpenGL33CommandQueue(
            functions.Value(), binding.Value(), backendOptions, allocator_);
        if (graphics_ == nullptr) {
            Shutdown();
            return OutOfMemory("Unable to allocate OpenGL backend");
        }
        Base::Result<void> status = graphics_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }
        status = InitializeResources();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }
        Base::Result<std::uint64_t> generation = AdvanceGeneration();
        if (!generation) {
            Shutdown();
            return generation.GetStatus();
        }
        renderer_ = new (std::nothrow) ::Aero::Render::Renderer(
            *this, generation.Value(), allocator_);
        if (renderer_ == nullptr) {
            Shutdown();
            return OutOfMemory("Unable to allocate OpenGL renderer");
        }
        status = renderer_->Initialize();
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }
        initialized_ = true;
        deviceLost_ = false;
        return {};
    }

    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept override {
        if (!IsReady()) return NotInitialized("OpenGL window device is not ready");
        Base::Result<void> current = MakeCurrent();
        if (!current) return current.GetStatus();
        Base::Result<::Aero::Render::Detail::RenderBatch> batch =
            renderer_->BuildOffscreenBatch(rendererToken, frame);
        if (!batch) return batch.GetStatus();
        Base::Result<Graphics::FenceValue> submitted =
            DrawBatch(std::move(batch).Value());
        return submitted ? Base::Result<void>() : Base::Result<void>(submitted.GetStatus());
    }

    Base::Result<Graphics::FenceValue> DrawBatch(
        ::Aero::Render::Detail::RenderBatch&& batch) noexcept override {
        if (!IsReady()) return NotInitialized("OpenGL window device is not ready");
        if (batch.Empty()) return Graphics::FenceValue{0U};
        return SubmitBatch(batch);
    }

    Base::Result<Graphics::FenceValue> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame,
        std::uint64_t frameSerial) noexcept {
        if (!IsReady() || GetSurfaceHealth() != SurfaceHealth::Ready) {
            return InvalidState("OpenGL window target is not ready");
        }
        if (frameSerial == 0U) {
            return InvalidState("OpenGL window frame serial must be nonzero");
        }
        Base::Result<void> current = MakeCurrent();
        if (!current) return current.GetStatus();
        PlatformWindowRenderContext* surface = WindowContext();
        Base::Result<Graphics::RenderTargetBinding> acquired =
            surface->AcquireTarget(frameSerial);
        if (!acquired) return acquired.GetStatus();
        const auto& native = acquired.Value();
        Graphics::OpenGL33RenderTargetBinding external;
        external.framebuffer = static_cast<Graphics::GlUInt>(native.colorTarget);
        external.depthStencilTexture =
            static_cast<Graphics::GlUInt>(native.depthStencilTarget);
        external.texture.width = native.width;
        external.texture.height = native.height;
        external.texture.format = native.colorFormat;
        external.texture.sampleCount = native.sampleCount;
        external.texture.usage =
            Graphics::TextureUsageBit(Graphics::TextureUsage::RenderTarget);
        external.contextGeneration = contextGeneration_;
        external.stableId = native.stableId;
        external.defaultFramebuffer = native.defaultFramebuffer;
        Base::Result<Graphics::ResourceHandle> imported =
            Graphics::ImportOpenGL33ExternalRenderTarget(
                *this, *graphics_, external);
        if (!imported) {
            surface->DiscardFrame(frameSerial);
            return imported.GetStatus();
        }

        Base::Result<::Aero::Render::Detail::RenderBatch> batch =
            renderer_->BuildOnscreenBatch(
                rendererToken,
                frame,
                {imported.Value(), native.width, native.height,
                 Graphics::LoadOperation::Clear});
        Base::Result<Graphics::FenceValue> submitted = batch
            ? DrawBatch(std::move(batch).Value())
            : Base::Result<Graphics::FenceValue>(batch.GetStatus());
        if (!submitted) surface->DiscardFrame(frameSerial);
        const Graphics::FenceValue retireFence = LastSubmittedFence();
        Base::Result<void> retired =
            DestroyResource(imported.Value(), retireFence);
        if (!submitted) return submitted.GetStatus();
        if (!retired) {
            surface->DiscardFrame(frameSerial);
            return retired.GetStatus();
        }
        return submitted.Value();
    }

    Base::Result<void> Present(
        std::uint64_t frameSerial,
        Graphics::FenceValue fence) noexcept {
        PlatformWindowRenderContext* surface = WindowContext();
        return surface != nullptr
            ? surface->Present(frameSerial, fence)
            : Base::Result<void>(NotInitialized(
                  "OpenGL window render context is unavailable"));
    }

    void Discard(std::uint64_t frameSerial) noexcept {
        PlatformWindowRenderContext* surface = WindowContext();
        if (surface != nullptr && frameSerial != 0U) {
            surface->DiscardFrame(frameSerial);
        }
    }

    void ReleaseRenderer(const void* rendererToken) noexcept override {
        if (renderer_ == nullptr) return;
        Base::Result<void> current = MakeCurrent();
        if (current) renderer_->ReleaseRenderer(rendererToken);
    }

    void NotifyDeviceLost() noexcept override {
        if (deviceLost_) return;
        Shutdown();
        deviceLost_ = true;
    }

    Base::Result<void> RestoreDevice() noexcept override {
        if (!deviceLost_) return InvalidState("OpenGL device is not lost");
        deviceLost_ = false;
        Base::Result<void> restored = Initialize();
        if (!restored) deviceLost_ = true;
        return restored;
    }

    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept override {
        if (graphics_ == nullptr) return {};
        const Graphics::FenceValue fence = LastSubmittedFence();
        return fence != 0U
            ? graphics_->WaitForFence(
                  fence,
                  static_cast<std::uint64_t>(timeoutMilliseconds) * UINT64_C(1000000))
            : Base::Result<void>();
    }

    BackendHealth GetDeviceHealth() const noexcept override {
        if (deviceLost_ || (graphics_ != nullptr && IsNativeDeviceLost())) {
            return BackendHealth::DeviceLost;
        }
        return IsReady() ? BackendHealth::Ready : BackendHealth::Failed;
    }

    ::Aero::RenderFrameStatistics
    LastFrameStatistics() const noexcept override {
        ::Aero::RenderFrameStatistics result;
        if (renderer_ == nullptr) return result;
        const ::Aero::Render::FrameEncoderStatistics source =
            renderer_->LastStatistics();
        result.sourceCommandCount = source.sourceCommandCount;
        result.drawPacketCount = source.drawPacketCount;
        result.batchCount = source.batchCount;
        result.mergedPacketCount = source.mergedPacketCount;
        result.barrierCount = source.barrierCount;
        result.batchingEnabled = source.batchingEnabled;
        result.drawCallCount = source.drawCallCount;
        result.instanceCount = source.rectangleInstanceCount +
            source.imageInstanceCount + source.meshInstanceCount +
            source.glyphInstanceCount;
        result.stateBindingCount = source.pipelineBindingCount +
            source.vertexBufferBindingCount + source.indexBufferBindingCount +
            source.uniformBufferBindingCount + source.textureSamplerBindingCount;
        return result;
    }

    Aero::Render::Detail::RenderResources Resources() noexcept override {
        return renderer_ != nullptr
            ? renderer_->Resources()
            : Aero::Render::Detail::RenderResources{};
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        PlatformWindowRenderContext* surface = WindowContext();
        if (!IsReady() || surface == nullptr) {
            return InvalidState("OpenGL window target is not ready");
        }
        options_.width = width;
        options_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        Base::Result<std::uint32_t> collected = CollectGarbage();
        if (!collected) return collected.GetStatus();
        return surface->Resize(width, height);
    }

    void NotifyLost() noexcept {
        PlatformWindowRenderContext* surface = WindowContext();
        if (surface != nullptr) surface->NotifyLost();
    }

    Base::Result<void> Restore() noexcept {
        PlatformWindowRenderContext* surface = WindowContext();
        if (surface == nullptr) {
            return NotInitialized("OpenGL window render context is unavailable");
        }
        Base::Result<void> restored = surface->Restore(descriptor_);
        if (!restored) return restored.GetStatus();
        return MakeCurrent();
    }

    SurfaceHealth GetSurfaceHealth() const noexcept {
        PlatformWindowRenderContext* surface =
            const_cast<OpenGL33WindowDeviceState*>(this)->WindowContext();
        if (surface == nullptr || surface->IsLost()) {
            return SurfaceHealth::Lost;
        }
        return initialized_ ? SurfaceHealth::Ready : SurfaceHealth::Failed;
    }

private:
    bool IsReady() const noexcept {
        return initialized_ && !deviceLost_ && graphics_ != nullptr &&
            AreResourcesReady() && renderer_ != nullptr;
    }

    Base::Result<void> CreateWindowContext() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        if (options_.window.system != Platform::WindowSystem::Win32) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "WGL requires a Win32 native window");
        }
        wglContext_ = new (std::nothrow) Graphics::WglRenderContext(allocator_);
        return wglContext_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory("Unable to allocate WGL context"));
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        if (options_.window.system != Platform::WindowSystem::X11) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "GLX requires an X11 native window");
        }
        glxContext_ = new (std::nothrow) Graphics::GlxRenderContext(allocator_);
        return glxContext_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory("Unable to allocate GLX context"));
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "OpenGL window targets are unsupported on this platform");
#endif
    }

    PlatformWindowRenderContext* WindowContext() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglContext_;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxContext_;
#else
        return nullptr;
#endif
    }

    Base::Result<void> MakeCurrent() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglContext_ != nullptr
            ? wglContext_->MakeCurrent()
            : Base::Result<void>(NotInitialized("WGL context is unavailable"));
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxContext_ != nullptr
            ? glxContext_->MakeCurrent()
            : Base::Result<void>(NotInitialized("GLX context is unavailable"));
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "No OpenGL window context is available");
#endif
    }

    Base::Result<Graphics::GlFunctionTable> LoadFunctions() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglContext_->LoadFunctions();
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxContext_->LoadFunctions();
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "No OpenGL window function loader is available");
#endif
    }

    Base::Result<Graphics::GlContextBinding> ContextBinding() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglContext_->ContextBinding();
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxContext_->ContextBinding();
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "No OpenGL window context is available");
#endif
    }

    Graphics::WindowRenderContextDescriptor MakeDescriptor() const noexcept {
        Graphics::WindowRenderContextDescriptor descriptor;
        descriptor.width = options_.width;
        descriptor.height = options_.height;
        descriptor.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        descriptor.depthStencilFormat = Graphics::GraphicsTextureFormat::Depth24Stencil8;
        descriptor.sampleCount = 1U;
        descriptor.stableId = UINT64_C(0x4145524F474C3333);
        descriptor.ownership = Graphics::WindowRenderContextOwnership::Owned;
        descriptor.presentMode = ToRhiPresentMode(options_.presentMode);
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        descriptor.kind = Graphics::WindowRenderContextKind::Wgl;
        descriptor.wgl.window = options_.window.window;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        descriptor.kind = Graphics::WindowRenderContextKind::Glx;
        descriptor.glx.display = options_.window.display;
        descriptor.glx.drawable = options_.window.window;
#endif
        return descriptor;
    }



    void Shutdown() noexcept {
        initialized_ = false;
        delete renderer_;
        renderer_ = nullptr;
        ShutdownResources();
        if (graphics_ != nullptr) {
            graphics_->Shutdown();
            delete graphics_;
            graphics_ = nullptr;
        }
        PlatformWindowRenderContext* surface = WindowContext();
        if (surface != nullptr) surface->Shutdown();
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        delete wglContext_;
        wglContext_ = nullptr;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        delete glxContext_;
        glxContext_ = nullptr;
#endif
        contextGeneration_ = 0U;
    }

    Base::IAllocator* allocator_ = nullptr;
    OpenGL33WindowTargetOptions options_;
    bool initialized_ = false;
    bool deviceLost_ = false;
    std::uint64_t contextGeneration_ = 0U;
    Graphics::WindowRenderContextDescriptor descriptor_;
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
    Graphics::WglRenderContext* wglContext_ = nullptr;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
    Graphics::GlxRenderContext* glxContext_ = nullptr;
#endif
    Graphics::OpenGL33CommandQueue* graphics_ = nullptr;
    ::Aero::Render::Renderer* renderer_ = nullptr;
};

class OpenGL33WindowTargetState final : public Aero::RenderTarget::Impl {
public:
    explicit OpenGL33WindowTargetState(
        OpenGL33WindowDeviceState& device) noexcept
        : Aero::RenderTarget::Impl(RenderTargetKind::Window),
          device_(&device) {}

    Base::Result<void> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept override {
        if (device_ == nullptr) {
            return NotInitialized("OpenGL window target has no render device");
        }
        if (nextFrameSerial_ == UINT64_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "OpenGL window frame serial space is exhausted");
        }
        const std::uint64_t frameSerial = nextFrameSerial_++;
        Base::Result<Graphics::FenceValue> submitted =
            device_->Render(rendererToken, frame, frameSerial);
        if (!submitted) return submitted.GetStatus();
        if (frameOpen) {
            pendingFrameSerial_ = frameSerial;
            pendingFence_ = submitted.Value();
            return {};
        }
        return device_->Present(frameSerial, submitted.Value());
    }

    Base::Result<void> PresentFrame() noexcept override {
        if (device_ == nullptr || pendingFrameSerial_ == 0U) {
            return InvalidState("OpenGL window frame is not ready to present");
        }
        const std::uint64_t serial = pendingFrameSerial_;
        const Graphics::FenceValue fence = pendingFence_;
        pendingFrameSerial_ = 0U;
        pendingFence_ = 0U;
        return device_->Present(serial, fence);
    }

    void DiscardFrame() noexcept override {
        if (device_ != nullptr && pendingFrameSerial_ != 0U) {
            device_->Discard(pendingFrameSerial_);
        }
        pendingFrameSerial_ = 0U;
        pendingFence_ = 0U;
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept override {
        return device_ != nullptr
            ? device_->Resize(width, height)
            : Base::Result<void>(NotInitialized(
                  "OpenGL window target has no render device"));
    }

    void NotifySurfaceLost() noexcept override {
        surfaceLost_ = true;
        if (device_ != nullptr) device_->NotifyLost();
    }

    Base::Result<void> RestoreSurface() noexcept override {
        if (device_ == nullptr) {
            return NotInitialized("OpenGL window target has no render device");
        }
        if (!surfaceLost_ &&
            device_->GetSurfaceHealth() != SurfaceHealth::Lost) {
            return InvalidState("OpenGL window target is not lost");
        }
        Base::Result<void> restored = device_->Restore();
        if (restored) surfaceLost_ = false;
        return restored;
    }

    SurfaceHealth GetSurfaceHealth() const noexcept override {
        if (surfaceLost_) return SurfaceHealth::Lost;
        return device_ != nullptr
            ? device_->GetSurfaceHealth()
            : SurfaceHealth::Shutdown;
    }

private:
    OpenGL33WindowDeviceState* device_ = nullptr;
    bool surfaceLost_ = false;
    std::uint64_t nextFrameSerial_ = 1U;
    std::uint64_t pendingFrameSerial_ = 0U;
    Graphics::FenceValue pendingFence_ = 0U;
};

} // namespace

Base::Result<WindowRenderPair> CreateOpenGL33WindowRenderPair(
    const OpenGL33WindowTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!options.window.IsValid() || options.width == 0U || options.height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "OpenGL window target options are invalid");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* state = new (std::nothrow)
        OpenGL33WindowDeviceState(options, selected);
    if (state == nullptr) {
        return OutOfMemory("Unable to allocate OpenGL window device state");
    }
    Base::Result<void> initialized = state->Initialize();
    if (!initialized) {
        delete state;
        return initialized.GetStatus();
    }
    Base::Result<Base::Ref<Aero::RenderDevice>> adoptedDevice =
        AdoptRenderDevice(state, &selected);
    if (!adoptedDevice) return adoptedDevice.GetStatus();

    Base::Ref<Aero::RenderDevice> device =
        std::move(adoptedDevice).Value();
    auto* targetState = new (std::nothrow)
        OpenGL33WindowTargetState(*state);
    if (targetState == nullptr) {
        return OutOfMemory("Unable to allocate OpenGL window target state");
    }
    Base::Result<Base::Ref<Aero::RenderTarget>> adoptedTarget =
        AdoptRenderTarget(
            device, targetState, RenderTargetKind::Window, &selected);
    if (!adoptedTarget) return adoptedTarget.GetStatus();

    WindowRenderPair result;
    result.device = std::move(device);
    result.target = std::move(adoptedTarget).Value();
    return result;
}

} // namespace Aero::Render::Detail
