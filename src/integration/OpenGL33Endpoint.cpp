#include <Aero/Integration/OpenGL33.hpp>

#include "RenderEndpointInternal.hpp"
#include "render/TextBackendAccess.hpp"

#include "render/opengl33/OpenGL33RendererBackend.hpp"
#include "rhi/OpenGL33Backend.hpp"

#if defined(_WIN32)
#include "rhi/WglSurface.hpp"
#elif defined(__linux__)
#include "rhi/GlxSurface.hpp"
#endif

#include <functional>
#include <new>
#include <thread>

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

Rhi::GlThreadToken CurrentThreadToken(
    void*) noexcept {
    Rhi::GlThreadToken value =
        static_cast<Rhi::GlThreadToken>(
            std::hash<std::thread::id>{}(
                std::this_thread::get_id()));
    return value != 0U ? value : 1U;
}

class OpenGL33EmbeddedSurface final
    : public Rhi::ISurfaceBackend {
public:
    explicit OpenGL33EmbeddedSurface(
        const OpenGL33EmbeddedEndpointOptions& options) noexcept
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
        Rhi::ExternalRenderTargetDescriptor result;
        result.colorTarget = native.framebuffer;
        result.depthStencilTarget =
            native.depthStencilTexture;
        result.width = native.width;
        result.height = native.height;
        result.colorFormat =
            Rhi::GraphicsTextureFormat::Bgra8Unorm;
        result.depthStencilFormat =
            Rhi::GraphicsTextureFormat::Depth24Stencil8;
        result.sampleCount = 1U;
        result.defaultFramebuffer =
            native.defaultFramebuffer;
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
    OpenGL33EmbeddedEndpointOptions options_;
    bool lost_ = false;
};

class OpenGL33EndpointDriver final
    : public Detail::EndpointDriver {
public:
    OpenGL33EndpointDriver(
        const OpenGL33WindowEndpointOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          windowOptions_(options),
          embedded_(false) {}

    OpenGL33EndpointDriver(
        const OpenGL33EmbeddedEndpointOptions& options,
        Base::IAllocator& allocator) noexcept
        : allocator_(&allocator),
          embeddedOptions_(options),
          embedded_(true) {}

    ~OpenGL33EndpointDriver() override {
        Shutdown();
    }

    bool SupportsDedicatedThread() const noexcept override {
        // WGL/GLX and embedded context callbacks remain owner-thread affine.
        return false;
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
            Rhi::SurfaceSession(*surfaceBackend_);
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

        Base::Result<Rhi::GlFunctionTable> functions =
            LoadFunctions();
        if (!functions) {
            Shutdown();
            return functions.GetStatus();
        }
        Base::Result<Rhi::GlContextContract> contract =
            ContextContract();
        if (!contract) {
            Shutdown();
            return contract.GetStatus();
        }
        contextGeneration_ = contract.Value().generation;
        Rhi::OpenGL33BackendOptions backendOptions;
        backendOptions.embeddingMode = embedded_ &&
            embeddedOptions_.statePolicy ==
                OpenGL33StatePreservationPolicy::
                    PreserveRequiredState
            ? Rhi::GlEmbeddingMode::PreserveAndRestore
            : Rhi::GlEmbeddingMode::HostReset;
        backendOptions.checkErrors =
            !embedded_ && windowOptions_.enableDebugContext;
        graphics_ = new (std::nothrow)
            Rhi::OpenGL33GraphicsBackend(
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

        renderer_ = new (std::nothrow)
            Render::OpenGL33RenderPlanBackend(
                *device_,
                *graphics_,
                *surface_,
                contextGeneration_,
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
        renderer_->SetBatchingEnabled(
            batchingEnabled_);
        return {};
    }

    Base::Result<void> Submit(
        const Presentation::RenderPlan& plan) noexcept override {
        Base::Result<void> current = MakeContextCurrent();
        if (!current) return current.GetStatus();
        return renderer_ != nullptr
            ? renderer_->Submit(plan)
            : Base::Result<void>(
                  Base::Status::Failure(
                      Base::ErrorCode::NotInitialized,
                      "OpenGL endpoint is not initialized"));
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept override {
        if (embedded_) return {};
        if (surface_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "OpenGL endpoint has no surface");
        }
        windowOptions_.width = width;
        windowOptions_.height = height;
        descriptor_.width = width;
        descriptor_.height = height;
        return surface_->Resize(width, height);
    }

    void NotifySurfaceLost() noexcept override {
        Shutdown();
        lost_ = true;
    }

    void NotifyDeviceLost() noexcept override {
        Shutdown();
        lost_ = true;
    }

    Base::Result<void> Restore() noexcept override {
        if (!lost_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "OpenGL endpoint is not lost");
        }
        lost_ = false;
        return Initialize();
    }

    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept override {
        if (graphics_ == nullptr || renderer_ == nullptr ||
            renderer_->LastSubmittedFence() == 0U) {
            return {};
        }
        return graphics_->WaitForFence(
            renderer_->LastSubmittedFence(),
            static_cast<std::uint64_t>(
                timeoutMilliseconds) * UINT64_C(1000000));
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
    static Rhi::GlProcAddress ResolveEmbedded(
        void* context,
        const char* name) noexcept {
        auto* driver =
            static_cast<OpenGL33EndpointDriver*>(context);
        return reinterpret_cast<Rhi::GlProcAddress>(
            driver->embeddedOptions_.resolve(
                driver->embeddedOptions_.callbackContext,
                name));
    }

    static bool IsEmbeddedCurrent(
        void* context,
        const void*) noexcept {
        auto* driver =
            static_cast<OpenGL33EndpointDriver*>(context);
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
#if defined(_WIN32)
        if (windowOptions_.window.system !=
            Platform::WindowSystem::Win32) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "WGL requires a Win32 native window");
        }
        wglSurface_ = new (std::nothrow)
            Rhi::WglSurfaceBackend(allocator_);
        surfaceBackend_ = wglSurface_;
        return surfaceBackend_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory());
#elif defined(__linux__)
        if (windowOptions_.window.system !=
            Platform::WindowSystem::X11) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "GLX requires an X11 native window");
        }
        glxSurface_ = new (std::nothrow)
            Rhi::GlxSurfaceBackend(allocator_);
        surfaceBackend_ = glxSurface_;
        return surfaceBackend_ != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(OutOfMemory());
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "OpenGL window endpoints are unsupported on this platform");
#endif
    }

    Base::Result<Rhi::GlFunctionTable>
    LoadFunctions() noexcept {
        if (embedded_) {
            return Rhi::LoadGlFunctionTable(
                &ResolveEmbedded, this);
        }
#if defined(_WIN32)
        return wglSurface_->LoadFunctions();
#elif defined(__linux__)
        return glxSurface_->LoadFunctions();
#else
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "No OpenGL window function loader is available");
#endif
    }

    Base::Result<Rhi::GlContextContract>
    ContextContract() noexcept {
        if (!embedded_) {
#if defined(_WIN32)
            return wglSurface_->ContextContract();
#elif defined(__linux__)
            return glxSurface_->ContextContract();
#else
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "No OpenGL window context is available");
#endif
        }
        Rhi::GlContextContract result;
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
            ? Rhi::GlEmbeddingMode::PreserveAndRestore
            : Rhi::GlEmbeddingMode::HostReset;
        return result;
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
            UINT64_C(0x4145524F474C3333);
        if (embedded_) {
            descriptor.kind =
                Rhi::SurfaceKind::ExternalRenderTarget;
            descriptor.ownership =
                Rhi::SurfaceOwnership::Borrowed;
            descriptor.external.colorTarget = 1U;
        } else {
            descriptor.ownership =
                Rhi::SurfaceOwnership::Owned;
            descriptor.presentMode =
                ToRhiPresentMode(
                    windowOptions_.presentMode);
#if defined(_WIN32)
            descriptor.kind =
                Rhi::SurfaceKind::WglWindow;
            descriptor.wgl.window =
                windowOptions_.window.window;
#elif defined(__linux__)
            descriptor.kind =
                Rhi::SurfaceKind::GlxWindow;
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
            "Unable to allocate OpenGL endpoint state");
    }

    void Shutdown() noexcept {
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
#if defined(_WIN32)
        delete wglSurface_;
        wglSurface_ = nullptr;
#elif defined(__linux__)
        delete glxSurface_;
        glxSurface_ = nullptr;
#endif
        surfaceBackend_ = nullptr;
    }

    Base::IAllocator* allocator_ = nullptr;
    OpenGL33WindowEndpointOptions windowOptions_;
    OpenGL33EmbeddedEndpointOptions embeddedOptions_;
    bool embedded_ = false;
    bool lost_ = false;
    bool batchingEnabled_ = true;
    std::uint64_t contextGeneration_ = 0U;
    Rhi::NativeSurfaceDescriptor descriptor_;
    OpenGL33EmbeddedSurface* embeddedSurface_ = nullptr;
#if defined(_WIN32)
    Rhi::WglSurfaceBackend* wglSurface_ = nullptr;
#elif defined(__linux__)
    Rhi::GlxSurfaceBackend* glxSurface_ = nullptr;
#endif
    Rhi::ISurfaceBackend* surfaceBackend_ = nullptr;
    Rhi::SurfaceSession* surface_ = nullptr;
    Rhi::OpenGL33GraphicsBackend* graphics_ = nullptr;
    Rhi::RhiDevice* device_ = nullptr;
    Render::OpenGL33RenderPlanBackend* renderer_ = nullptr;
};

template<class TOptions>
Base::Result<Base::Ref<RenderEndpoint>>
CreateOpenGL33Endpoint(
    const TOptions& options,
    RenderEndpointMode mode,
    RenderSubmissionMode submissionMode,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    auto* driver = new (std::nothrow)
        OpenGL33EndpointDriver(options, selected);
    if (driver == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the OpenGL endpoint");
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
CreateOpenGL33EmbeddedEndpoint(
    const OpenGL33EmbeddedEndpointOptions& options,
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
            "OpenGL embedded endpoint options are incomplete");
    }
    return CreateOpenGL33Endpoint(
        options,
        RenderEndpointMode::Embedded,
        options.submissionMode,
        allocator);
}

Base::Result<Base::Ref<RenderEndpoint>>
CreateOpenGL33WindowEndpoint(
    const OpenGL33WindowEndpointOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!options.window.IsValid() ||
        options.width == 0U ||
        options.height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "OpenGL window endpoint options are invalid");
    }
    return CreateOpenGL33Endpoint(
        options,
        RenderEndpointMode::Window,
        options.submissionMode,
        allocator);
}

} // namespace Aero::Integration
