#include "render/private/BackendApi.hpp"
#include "render/private/RenderTarget.hpp"
#include "render/Renderer.hpp"
#include "render/opengl33/OpenGL33Backend.hpp"
#include "render/opengl33/OpenGL33Shaders.hpp"

#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
#include "render/platform/win32/OpenGLSurface.hpp"
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
#include "render/platform/x11/OpenGLSurface.hpp"
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

Graphics::PresentMode ToRhiPresentMode(PresentMode value) noexcept {
    switch (value) {
    case PresentMode::Immediate: return Graphics::PresentMode::Immediate;
    case PresentMode::Mailbox: return Graphics::PresentMode::Mailbox;
    case PresentMode::Fifo: return Graphics::PresentMode::Fifo;
    }
    return Graphics::PresentMode::Fifo;
}

class OpenGL33WindowDeviceState final
    : public Aero::RenderDevice::Impl {
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

    Base::Result<void> Initialize() noexcept {
        if (initialized_) return {};
        Base::Result<void> surface = CreateWindowSurface();
        if (!surface) return surface.GetStatus();
        descriptor_ = MakeDescriptor();
        Graphics::WindowSurfaceBackend* backend = SurfaceBackend();
        if (backend == nullptr) {
            Shutdown();
            return NotInitialized("OpenGL window surface backend is unavailable");
        }
        Base::Result<void> valid = Graphics::ValidateNativeSurfaceDescriptor(
            descriptor_, backend->QuerySurfaceCapabilities());
        if (!valid) {
            Shutdown();
            return valid.GetStatus();
        }
        Base::Result<void> created = backend->CreateSurface(descriptor_);
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

        Graphics::OpenGL33BackendOptions backendOptions;
        backendOptions.embeddingMode = binding.Value().embeddingMode;
        graphics_ = new (std::nothrow) Graphics::OpenGL33GraphicsBackend(
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
        device_ = new (std::nothrow) Graphics::GraphicsDevice(*graphics_, allocator_);
        if (device_ == nullptr) {
            Shutdown();
            return OutOfMemory("Unable to allocate OpenGL graphics device");
        }
        status = device_->Initialize();
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
            *device_, ::Aero::Render::MakeOpenGL33FrameShaderSet(),
            generation.Value(), allocator_);
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
        surfaceLost_ = false;
        deviceLost_ = false;
        nextFrameSerial_ = 1U;
        return {};
    }

    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept override {
        if (!IsReady()) return NotInitialized("OpenGL window device is not ready");
        Base::Result<void> current = MakeCurrent();
        if (!current) return current.GetStatus();
        Base::Result<Graphics::FenceValue> submitted =
            renderer_->RenderOffscreen(rendererToken, frame);
        return submitted ? Base::Result<void>() : Base::Result<void>(submitted.GetStatus());
    }

    Base::Result<void> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
        if (!IsReady() || GetSurfaceHealth() != SurfaceHealth::Ready) {
            return InvalidState("OpenGL window target is not ready");
        }
        Base::Result<void> current = MakeCurrent();
        if (!current) return current.GetStatus();
        if (nextFrameSerial_ == UINT64_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "OpenGL window frame serial space is exhausted");
        }
        const std::uint64_t frameSerial = nextFrameSerial_++;
        Graphics::ISurfaceBackend* surface = SurfaceBackend();
        Base::Result<Graphics::ExternalRenderTargetDescriptor> acquired =
            surface->AcquireSurfaceTarget(frameSerial);
        if (!acquired) {
            RefreshSurfaceHealth();
            return acquired.GetStatus();
        }
        const auto& native = acquired.Value();
        Graphics::OpenGL33ExternalRenderTargetDescriptor external;
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
                *device_, *graphics_, external);
        if (!imported) {
            surface->DiscardSurfaceFrame(frameSerial);
            return imported.GetStatus();
        }

        Base::Result<Graphics::FenceValue> submitted =
            renderer_->RenderOnscreen(
                rendererToken,
                frame,
                {imported.Value(), native.width, native.height,
                 Graphics::LoadOperation::Clear});
        if (!submitted) surface->DiscardSurfaceFrame(frameSerial);
        Base::Result<void> presented;
        if (submitted) {
            presented = surface->PresentSurface(frameSerial, submitted.Value());
        }
        const Graphics::FenceValue retireFence = device_->LastSubmittedFence();
        Base::Result<void> retired =
            device_->DestroyResource(imported.Value(), retireFence);
        if (!submitted) {
            RefreshSurfaceHealth();
            return submitted.GetStatus();
        }
        if (!presented) {
            RefreshSurfaceHealth();
            return presented.GetStatus();
        }
        return retired;
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
        if (graphics_ == nullptr || device_ == nullptr) return {};
        const Graphics::FenceValue fence = device_->LastSubmittedFence();
        return fence != 0U
            ? graphics_->WaitForFence(
                  fence,
                  static_cast<std::uint64_t>(timeoutMilliseconds) * UINT64_C(1000000))
            : Base::Result<void>();
    }

    BackendHealth GetDeviceHealth() const noexcept override {
        if (deviceLost_ ||
            (device_ != nullptr && device_->Backend().IsDeviceLost())) {
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
        Graphics::WindowSurfaceBackend* surface = SurfaceBackend();
        if (!IsReady() || surface == nullptr) {
            return InvalidState("OpenGL window target is not ready");
        }
        options_.width = width;
        options_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        Base::Result<std::uint32_t> collected = device_->CollectGarbage();
        if (!collected) return collected.GetStatus();
        Base::Result<void> resized = surface->ResizeSurface(width, height);
        if (!resized) RefreshSurfaceHealth();
        return resized;
    }

    void NotifySurfaceLost() noexcept {
        Graphics::WindowSurfaceBackend* surface = SurfaceBackend();
        if (surface != nullptr) surface->NotifySurfaceLost();
        surfaceLost_ = true;
    }

    Base::Result<void> RestoreSurface() noexcept {
        if (!surfaceLost_) {
            return InvalidState("OpenGL window target is not lost");
        }
        Graphics::WindowSurfaceBackend* surface = SurfaceBackend();
        if (surface == nullptr) {
            return NotInitialized("OpenGL window surface backend is unavailable");
        }
        Base::Result<void> restored = surface->RestoreSurface(descriptor_);
        if (restored) {
            Base::Result<void> current = MakeCurrent();
            if (!current) return current.GetStatus();
            surfaceLost_ = false;
        }
        return restored;
    }

    SurfaceHealth GetSurfaceHealth() const noexcept {
        Graphics::WindowSurfaceBackend* surface =
            const_cast<OpenGL33WindowDeviceState*>(this)->SurfaceBackend();
        if (surfaceLost_ || surface == nullptr || surface->IsSurfaceLost()) {
            return SurfaceHealth::Lost;
        }
        return initialized_ ? SurfaceHealth::Ready : SurfaceHealth::Failed;
    }

private:
    bool IsReady() const noexcept {
        return initialized_ && !deviceLost_ && graphics_ != nullptr &&
            device_ != nullptr && renderer_ != nullptr;
    }

    Base::Result<void> CreateWindowSurface() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        if (options_.window.system != Platform::WindowSystem::Win32) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "WGL requires a Win32 native window");
        }
        wglSurface_ = new (std::nothrow) Graphics::WglSurfaceBackend(allocator_);
        return wglSurface_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory("Unable to allocate WGL target"));
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        if (options_.window.system != Platform::WindowSystem::X11) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "GLX requires an X11 native window");
        }
        glxSurface_ = new (std::nothrow) Graphics::GlxSurfaceBackend(allocator_);
        return glxSurface_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory("Unable to allocate GLX target"));
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "OpenGL window targets are unsupported on this platform");
#endif
    }

    Graphics::WindowSurfaceBackend* SurfaceBackend() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglSurface_;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxSurface_;
#else
        return nullptr;
#endif
    }

    Base::Result<void> MakeCurrent() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglSurface_ != nullptr
            ? wglSurface_->MakeCurrent()
            : Base::Result<void>(NotInitialized("WGL target is unavailable"));
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxSurface_ != nullptr
            ? glxSurface_->MakeCurrent()
            : Base::Result<void>(NotInitialized("GLX target is unavailable"));
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "No OpenGL window context is available");
#endif
    }

    Base::Result<Graphics::GlFunctionTable> LoadFunctions() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglSurface_->LoadFunctions();
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxSurface_->LoadFunctions();
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "No OpenGL window function loader is available");
#endif
    }

    Base::Result<Graphics::GlContextBinding> ContextBinding() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        return wglSurface_->ContextBinding();
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        return glxSurface_->ContextBinding();
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "No OpenGL window context is available");
#endif
    }

    Graphics::NativeSurfaceDescriptor MakeDescriptor() const noexcept {
        Graphics::NativeSurfaceDescriptor descriptor;
        descriptor.width = options_.width;
        descriptor.height = options_.height;
        descriptor.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        descriptor.depthStencilFormat = Graphics::GraphicsTextureFormat::Depth24Stencil8;
        descriptor.sampleCount = 1U;
        descriptor.stableId = UINT64_C(0x4145524F474C3333);
        descriptor.ownership = Graphics::SurfaceOwnership::Owned;
        descriptor.presentMode = ToRhiPresentMode(options_.presentMode);
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        descriptor.kind = Graphics::SurfaceKind::WglWindow;
        descriptor.wgl.window = options_.window.window;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        descriptor.kind = Graphics::SurfaceKind::GlxWindow;
        descriptor.glx.display = options_.window.display;
        descriptor.glx.drawable = options_.window.window;
#endif
        return descriptor;
    }

    void RefreshSurfaceHealth() noexcept {
        Graphics::WindowSurfaceBackend* surface = SurfaceBackend();
        if (surface == nullptr || surface->IsSurfaceLost()) surfaceLost_ = true;
    }

    void Shutdown() noexcept {
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
        Graphics::WindowSurfaceBackend* surface = SurfaceBackend();
        if (surface != nullptr) surface->DestroySurface();
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        delete wglSurface_;
        wglSurface_ = nullptr;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        delete glxSurface_;
        glxSurface_ = nullptr;
#endif
        contextGeneration_ = 0U;
    }

    Base::IAllocator* allocator_ = nullptr;
    OpenGL33WindowTargetOptions options_;
    bool initialized_ = false;
    bool surfaceLost_ = false;
    bool deviceLost_ = false;
    std::uint64_t contextGeneration_ = 0U;
    std::uint64_t nextFrameSerial_ = 1U;
    Graphics::NativeSurfaceDescriptor descriptor_;
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
    Graphics::WglSurfaceBackend* wglSurface_ = nullptr;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
    Graphics::GlxSurfaceBackend* glxSurface_ = nullptr;
#endif
    Graphics::OpenGL33GraphicsBackend* graphics_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
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
        return device_ != nullptr
            ? device_->Render(rendererToken, frame)
            : Base::Result<void>(NotInitialized(
                  "OpenGL window target has no render device"));
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
        if (device_ != nullptr) device_->NotifySurfaceLost();
    }

    Base::Result<void> RestoreSurface() noexcept override {
        return device_ != nullptr
            ? device_->RestoreSurface()
            : Base::Result<void>(NotInitialized(
                  "OpenGL window target has no render device"));
    }

    SurfaceHealth GetSurfaceHealth() const noexcept override {
        return device_ != nullptr
            ? device_->GetSurfaceHealth()
            : SurfaceHealth::Shutdown;
    }

private:
    OpenGL33WindowDeviceState* device_ = nullptr;
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
