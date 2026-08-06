#include "integration/IntegrationPrivate.hpp"

#include "render/DeviceRenderer.hpp"
#include "render/opengl33/OpenGL33Shaders.hpp"
#include "render/opengl33/OpenGL33Backend.hpp"

#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
#include "platform/win32/OpenGLSurface.hpp"
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
#include "platform/x11/OpenGLSurface.hpp"
#endif

#include <functional>
#include <new>
#include <thread>

namespace Aero::Render::Detail {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
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

Graphics::GlThreadToken CurrentThreadToken(
    void*) noexcept {
    Graphics::GlThreadToken value =
        static_cast<Graphics::GlThreadToken>(
            std::hash<std::thread::id>{}(
                std::this_thread::get_id()));
    return value != 0U ? value : 1U;
}

class OpenGL33EmbeddedSurface
    : public Graphics::ISurfaceBackend {
public:
    explicit OpenGL33EmbeddedSurface(
        const OpenGL33EmbeddedSurfaceOptions& options) noexcept
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
                "OpenGL embedded surface is unavailable");
        }
        if (options_.makeCurrent != nullptr) {
            Base::Status current =
                options_.makeCurrent(
                    options_.callbackContext);
            if (!current.IsOk()) return current;
        }
        OpenGL33EmbeddedTarget native;
        Base::Status acquired =
            options_.acquireTarget(
                options_.callbackContext, &native);
        if (!acquired.IsOk()) return acquired;
        if (native.width == 0U || native.height == 0U ||
            (!native.defaultFramebuffer &&
             native.framebuffer == 0U)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "OpenGL embedded target is invalid");
        }
        Graphics::ExternalRenderTargetDescriptor result;
        result.colorTarget = native.framebuffer;
        result.depthStencilTarget =
            native.depthStencilTexture;
        result.width = native.width;
        result.height = native.height;
        result.colorFormat =
            Graphics::GraphicsTextureFormat::Bgra8Unorm;
        result.depthStencilFormat =
            Graphics::GraphicsTextureFormat::Depth24Stencil8;
        result.sampleCount = 1U;
        result.defaultFramebuffer =
            native.defaultFramebuffer;
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
    OpenGL33EmbeddedSurfaceOptions options_;
    bool lost_ = false;
};

class OpenGL33DeviceState final
    : public NativeRenderDevice,
      public NativeRenderTarget {
public:
    OpenGL33DeviceState(
        const OpenGL33WindowSurfaceOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          windowOptions_(options),
          embedded_(false) {}

    OpenGL33DeviceState(
        const OpenGL33EmbeddedSurfaceOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          embeddedOptions_(options),
          embedded_(true) {}

    ~OpenGL33DeviceState() {
        Shutdown();
    }

    RenderBackendKind Backend() const noexcept override {
        return RenderBackendKind::OpenGL33;
    }
    NativeRenderTarget* DefaultTarget() noexcept override {
        return this;
    }

    Base::Result<void> Initialize() noexcept {
        Base::Result<void> current = MakeContextCurrent();
        if (!current) return current.GetStatus();

        if (embedded_) {
            embeddedSurface_ = new (std::nothrow)
                OpenGL33EmbeddedSurface(
                    embeddedOptions_);
            surfaceBackend_ = embeddedSurface_;
        } else {
            Base::Result<void> created =
                CreateWindowSurface();
            if (!created) {
                Shutdown();
                return created.GetStatus();
            }
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
        Base::Result<void> status =
            surface_->Initialize(descriptor_);
        if (!status) {
            Shutdown();
            return status.GetStatus();
        }

        Base::Result<Graphics::GlFunctionTable> functions =
            LoadFunctions();
        if (!functions) {
            Shutdown();
            return functions.GetStatus();
        }
        Base::Result<Graphics::GlContextBinding> contract =
            ContextBinding();
        if (!contract) {
            Shutdown();
            return contract.GetStatus();
        }
        contextGeneration_ = contract.Value().generation;
        Graphics::OpenGL33BackendOptions backendOptions;
        backendOptions.embeddingMode = embedded_ &&
            embeddedOptions_.statePolicy ==
                OpenGL33StatePreservationPolicy::
                    PreserveRequiredState
            ? Graphics::GlEmbeddingMode::PreserveAndRestore
            : Graphics::GlEmbeddingMode::HostReset;
        backendOptions.checkErrors =
            !embedded_ && windowOptions_.enableDebugContext;
        graphics_ = new (std::nothrow)
            Graphics::OpenGL33GraphicsBackend(
                functions.Value(),
                contract.Value(),
                backendOptions,
                allocator_);
        if (graphics_ == nullptr) {
            Shutdown();
            return OutOfMemory();
        }
        status = graphics_->Initialize();
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

        renderer_ = new (std::nothrow)
            ::Aero::Render::DeviceRenderer(
                *device_,
                ::Aero::Render::MakeOpenGL33FrameShaderSet(),
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
        surfaceLost_ = false;
        deviceLost_ = false;
        return {};
    }

    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& plan) noexcept {
        Base::Result<void> current = MakeContextCurrent();
        if (!current) return current.GetStatus();
        if (renderer_ == nullptr || device_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "OpenGL device renderer is not initialized");
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
        const ::Aero::Render::Detail::RenderFrame& plan) noexcept {
        Base::Result<void> current = MakeContextCurrent();
        if (!current) return current.GetStatus();
        if (renderer_ == nullptr || device_ == nullptr ||
            graphics_ == nullptr || surface_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "OpenGL surface renderer is not initialized");
        }
        if (device_->Backend().IsDeviceLost() ||
            surface_->State() != Graphics::SurfaceState::Ready) {
            return InvalidState(
                "Cannot render to a lost OpenGL surface");
        }

        Base::Result<Graphics::SurfaceFrame> acquired =
            surface_->AcquireFrame();
        if (!acquired) return acquired.GetStatus();
        Graphics::SurfaceFrame frame = acquired.Value();
        if (frame.target.width == 0U ||
            frame.target.height == 0U ||
            (!frame.target.defaultFramebuffer &&
             frame.target.colorTarget == 0U)) {
            static_cast<void>(surface_->DiscardFrame(frame));
            return InvalidArgument(
                "OpenGL surface frame has no render target");
        }

        Graphics::OpenGL33ExternalRenderTargetDescriptor external;
        external.framebuffer = static_cast<Graphics::GlUInt>(
            frame.target.colorTarget);
        external.depthStencilTexture = static_cast<Graphics::GlUInt>(
            frame.target.depthStencilTarget);
        external.texture.width = frame.target.width;
        external.texture.height = frame.target.height;
        external.texture.format = frame.target.colorFormat;
        external.texture.sampleCount = frame.target.sampleCount;
        external.texture.usage =
            Graphics::TextureUsageBit(
                Graphics::TextureUsage::RenderTarget);
        external.contextGeneration = contextGeneration_;
        external.stableId = frame.target.stableId;
        external.defaultFramebuffer =
            frame.target.defaultFramebuffer;
        Base::Result<Graphics::ResourceHandle> imported =
            Graphics::ImportOpenGL33ExternalRenderTarget(
                *device_, *graphics_, external);
        if (!imported) {
            static_cast<void>(surface_->DiscardFrame(frame));
            return imported.GetStatus();
        }

        Base::Result<Graphics::CommandList> recorded =
            renderer_->RecordOnscreen(
                rendererToken,
                plan,
                {imported.Value(),
                 frame.target.width,
                 frame.target.height,
                 embedded_
                     ? Graphics::LoadOperation::Load
                     : Graphics::LoadOperation::Clear});
        if (!recorded) {
            static_cast<void>(surface_->DiscardFrame(frame));
            static_cast<void>(
                device_->DestroyResource(imported.Value(), 0U));
            return recorded.GetStatus();
        }
        Base::Result<Graphics::FenceValue> submitted =
            device_->Submit(recorded.Value());
        if (!submitted) {
            static_cast<void>(surface_->DiscardFrame(frame));
            static_cast<void>(
                device_->DestroyResource(imported.Value(), 0U));
            return submitted.GetStatus();
        }
        lastSubmittedFence_ = submitted.Value();
        Base::Result<void> presented =
            surface_->Present(frame, submitted.Value());
        Base::Result<void> destroyed =
            device_->DestroyResource(
                imported.Value(), submitted.Value());
        if (!presented) return presented;
        return destroyed;
    }

    void ReleaseRenderer(const void* rendererToken) noexcept {
        Base::Result<void> current = MakeContextCurrent();
        if (current && renderer_ != nullptr) {
            renderer_->ReleaseRenderer(rendererToken);
        }
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        if (embedded_) return {};
        if (surface_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "OpenGL device has no surface");
        }
        windowOptions_.width = width;
        windowOptions_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        return surface_->Resize(width, height);
    }

    void NotifySurfaceLost() noexcept {
        if (!embedded_) {
            NotifyDeviceLost();
            return;
        }
        if (surface_ != nullptr) {
            static_cast<void>(surface_->NotifyContextLost());
        }
        surfaceLost_ = true;
    }

    void NotifyDeviceLost() noexcept {
        Shutdown();
        deviceLost_ = true;
    }

    Base::Result<void> RestoreDevice() noexcept {
        if (!deviceLost_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "OpenGL device is not lost");
        }
        deviceLost_ = false;
        return Initialize();
    }

    Base::Result<void> RestoreSurface() noexcept {
        if (!surfaceLost_ || surface_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "OpenGL surface is not lost");
        }
        Base::Result<void> restored =
            surface_->Restore(descriptor_);
        if (restored) surfaceLost_ = false;
        return restored;
    }

    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept {
        if (graphics_ == nullptr || lastSubmittedFence_ == 0U) {
            return {};
        }
        return graphics_->WaitForFence(
            lastSubmittedFence_,
            static_cast<std::uint64_t>(
                timeoutMilliseconds) * UINT64_C(1000000));
    }

    BackendHealth GetDeviceHealth() const noexcept {
        if (deviceLost_ ||
            (device_ != nullptr &&
             device_->Backend().IsDeviceLost())) {
            return BackendHealth::DeviceLost;
        }
        return renderer_ != nullptr && graphics_ != nullptr &&
                device_ != nullptr
            ? BackendHealth::Ready
            : BackendHealth::Failed;
    }

    SurfaceHealth GetSurfaceHealth() const noexcept {
        if (surfaceLost_ ||
            (surface_ != nullptr &&
             surface_->State() != Graphics::SurfaceState::Ready)) {
            return SurfaceHealth::Lost;
        }
        return surface_ != nullptr
            ? SurfaceHealth::Ready
            : SurfaceHealth::Failed;
    }

    ::Aero::RenderFrameStatistics
    LastFrameStatistics() const noexcept {
        ::Aero::RenderFrameStatistics result;
        if (renderer_ == nullptr) return result;
        const ::Aero::Render::FrameEncoderStatistics source =
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
    static Graphics::GlProcAddress ResolveEmbedded(
        void* context,
        const char* name) noexcept {
        auto* driver =
            static_cast<OpenGL33DeviceState*>(context);
        return reinterpret_cast<Graphics::GlProcAddress>(
            driver->embeddedOptions_.resolve(
                driver->embeddedOptions_.callbackContext,
                name));
    }

    static bool IsEmbeddedCurrent(
        void* context,
        const void*) noexcept {
        auto* driver =
            static_cast<OpenGL33DeviceState*>(context);
        return driver->embeddedOptions_.isCurrent(
            driver->embeddedOptions_.callbackContext);
    }

    Base::Result<void> MakeContextCurrent() noexcept {
        if (!embedded_ ||
            embeddedOptions_.makeCurrent == nullptr) {
            return {};
        }
        Base::Status status =
            embeddedOptions_.makeCurrent(
                embeddedOptions_.callbackContext);
        return status.IsOk()
            ? Base::Result<void>()
            : Base::Result<void>(status);
    }

    Base::Result<void> CreateWindowSurface() noexcept {
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        if (windowOptions_.window.system !=
            ::Aero::Platform::WindowSystem::Win32) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "WGL requires a Win32 native window");
        }
        wglSurface_ = new (std::nothrow)
            Graphics::WglSurfaceBackend(allocator_);
        surfaceBackend_ = wglSurface_;
        return surfaceBackend_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory());
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        if (windowOptions_.window.system !=
            ::Aero::Platform::WindowSystem::X11) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "GLX requires an X11 native window");
        }
        glxSurface_ = new (std::nothrow)
            Graphics::GlxSurfaceBackend(allocator_);
        surfaceBackend_ = glxSurface_;
        return surfaceBackend_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory());
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "OpenGL window devices are unsupported on this platform");
#endif
    }

    Base::Result<Graphics::GlFunctionTable>
    LoadFunctions() noexcept {
        if (embedded_) {
            return Graphics::LoadGlFunctionTable(
                &ResolveEmbedded, this);
        }
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

    Base::Result<Graphics::GlContextBinding>
    ContextBinding() noexcept {
        if (!embedded_) {
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
        Graphics::GlContextBinding result;
        result.userData = this;
        result.contextHandle = this;
        result.resolve = &ResolveEmbedded;
        result.isCurrent = &IsEmbeddedCurrent;
        result.currentThreadToken = &CurrentThreadToken;
        result.owningThreadToken =
            CurrentThreadToken(nullptr);
        result.generation =
            embeddedOptions_.contextGeneration(
                embeddedOptions_.callbackContext);
        result.embeddingMode =
            embeddedOptions_.statePolicy ==
                OpenGL33StatePreservationPolicy::
                    PreserveRequiredState
            ? Graphics::GlEmbeddingMode::PreserveAndRestore
            : Graphics::GlEmbeddingMode::HostReset;
        return result;
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
            UINT64_C(0x4145524F474C3333);
        if (embedded_) {
            descriptor.kind =
                Graphics::SurfaceKind::ExternalRenderTarget;
            descriptor.ownership =
                Graphics::SurfaceOwnership::Borrowed;
            descriptor.external.colorTarget = 1U;
        } else {
            descriptor.ownership =
                Graphics::SurfaceOwnership::Owned;
            descriptor.presentMode =
                ToRhiPresentMode(
                    windowOptions_.presentMode);
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
            descriptor.kind =
                Graphics::SurfaceKind::WglWindow;
            descriptor.wgl.window =
                windowOptions_.window.window;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
            descriptor.kind =
                Graphics::SurfaceKind::GlxWindow;
            descriptor.glx.display =
                windowOptions_.window.display;
            descriptor.glx.drawable =
                windowOptions_.window.window;
#endif
        }
        return descriptor;
    }

    Base::Status OutOfMemory() const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate OpenGL device state");
    }

    void Shutdown() noexcept {
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
        if (surface_ != nullptr) {
            surface_->Shutdown();
            delete surface_;
            surface_ = nullptr;
        }
        delete embeddedSurface_;
        embeddedSurface_ = nullptr;
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
        delete wglSurface_;
        wglSurface_ = nullptr;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
        delete glxSurface_;
        glxSurface_ = nullptr;
#endif
        surfaceBackend_ = nullptr;
    }

    Base::IAllocator* allocator_ = nullptr;
    OpenGL33WindowSurfaceOptions windowOptions_;
    OpenGL33EmbeddedSurfaceOptions embeddedOptions_;
    bool embedded_ = false;
    bool surfaceLost_ = false;
    bool deviceLost_ = false;
    std::uint64_t contextGeneration_ = 0U;
    Graphics::NativeSurfaceDescriptor descriptor_;
    OpenGL33EmbeddedSurface* embeddedSurface_ = nullptr;
#if defined(_WIN32) && AERO_HAS_WGL_SURFACE
    Graphics::WglSurfaceBackend* wglSurface_ = nullptr;
#elif defined(__linux__) && AERO_HAS_GLX_SURFACE
    Graphics::GlxSurfaceBackend* glxSurface_ = nullptr;
#endif
    Graphics::ISurfaceBackend* surfaceBackend_ = nullptr;
    Graphics::SurfaceSession* surface_ = nullptr;
    Graphics::OpenGL33GraphicsBackend* graphics_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
    ::Aero::Render::DeviceRenderer* renderer_ = nullptr;
    Graphics::FenceValue lastSubmittedFence_ = 0U;
};

template<class TOptions>
Base::Result<Base::Ref<Aero::RenderDevice>>
CreateOpenGL33Device(
    const TOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    auto* driver = new (std::nothrow)
        OpenGL33DeviceState(options, selected);
    if (driver == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the OpenGL device");
    }
    Base::Result<void> initialized = driver->Initialize();
    if (!initialized) {
        delete driver;
        return initialized.GetStatus();
    }
    return ::Aero::Render::Detail::AdoptRenderDevice(
        driver, &selected);
}

} // namespace

Base::Result<Base::Ref<Aero::RenderDevice>>
CreateOpenGL33EmbeddedDevice(
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (options.resolve == nullptr ||
        options.makeCurrent == nullptr ||
        options.isCurrent == nullptr ||
        options.contextGeneration == nullptr ||
        options.acquireTarget == nullptr ||
        options.contextGeneration(
            options.callbackContext) == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "OpenGL embedded device options are incomplete");
    }
    return CreateOpenGL33Device(
        options,
        allocator);
}

Base::Result<Base::Ref<Aero::RenderDevice>>
CreateOpenGL33WindowDevice(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!options.window.IsValid() ||
        options.width == 0U ||
        options.height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "OpenGL window device options are invalid");
    }
    return CreateOpenGL33Device(
        options,
        allocator);
}

} // namespace Aero::Render::Detail
