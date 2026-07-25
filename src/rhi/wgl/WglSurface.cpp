#include <Aero/Rhi/WglSurface.hpp>

#if !defined(_WIN32)
#error "WglSurface.cpp is only supported on Windows"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <GL/gl.h>

#include <limits>
#include <new>

namespace Aero::Rhi {
namespace {

constexpr int WglContextMajorVersionArb = 0x2091;
constexpr int WglContextMinorVersionArb = 0x2092;
constexpr int WglContextProfileMaskArb = 0x9126;
constexpr int WglContextCoreProfileBitArb = 0x00000001;

using WglCreateContextAttribsArbProc =
    HGLRC (WINAPI*)(HDC, HGLRC, const int*);
using WglSwapIntervalExtProc = BOOL (WINAPI*)(int);

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory, message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfRange, message);
}

Base::Status InternalError(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InternalError, message);
}

bool IsInvalidWglAddress(const PROC address) noexcept {
    const std::uintptr_t value =
        reinterpret_cast<std::uintptr_t>(address);
    return address == nullptr ||
        value == 1U || value == 2U ||
        value == 3U || value == std::numeric_limits<std::uintptr_t>::max();
}

} // namespace

struct WglSurfaceBackend::Impl final {
    HWND window = nullptr;
    HDC deviceContext = nullptr;
    HGLRC renderContext = nullptr;
    WglSwapIntervalExtProc swapInterval = nullptr;
    NativeSurfaceDescriptor descriptor;
    std::uint64_t activeFrameSerial = 0U;
    GlContextGeneration generation = 0U;
    DWORD owningThread = 0U;
    bool ownsContext = false;
    bool ownsDeviceContext = false;
    bool initialized = false;
    bool lost = false;

    static GlProcAddress Resolve(
        void*, const char* name) noexcept {
        if (name == nullptr || name[0] == '\0') {
            return nullptr;
        }
        PROC address = wglGetProcAddress(name);
        if (IsInvalidWglAddress(address)) {
            const HMODULE module =
                GetModuleHandleW(L"opengl32.dll");
            address = module != nullptr
                ? GetProcAddress(module, name)
                : nullptr;
        }
        return reinterpret_cast<GlProcAddress>(address);
    }

    static bool IsCurrent(
        void* userData,
        const void* contextHandle) noexcept {
        auto* self = static_cast<Impl*>(userData);
        return self != nullptr &&
            GetCurrentThreadId() == self->owningThread &&
            wglGetCurrentContext() ==
                reinterpret_cast<HGLRC>(
                    const_cast<void*>(contextHandle)) &&
            wglGetCurrentDC() == self->deviceContext;
    }

    static GlThreadToken CurrentThread(void*) noexcept {
        return static_cast<GlThreadToken>(
            GetCurrentThreadId());
    }

    Base::Result<void> VerifyReady() const noexcept {
        if (!initialized ||
            deviceContext == nullptr ||
            renderContext == nullptr) {
            return NotInitialized(
                "WGL surface is not initialized");
        }
        if (lost) {
            return InvalidState("WGL surface is lost");
        }
        if (GetCurrentThreadId() != owningThread) {
            return Base::Status::Failure(
                Base::ErrorCode::WrongThread,
                "WGL surface must be used on its owning thread");
        }
        return {};
    }

    Base::Result<void> MakeContextCurrent() noexcept {
        Base::Result<void> ready = VerifyReady();
        if (!ready) {
            return ready;
        }
        if (wglGetCurrentContext() == renderContext &&
            wglGetCurrentDC() == deviceContext) {
            return {};
        }
        if (!wglMakeCurrent(deviceContext, renderContext)) {
            lost = true;
            return InternalError(
                "wglMakeCurrent failed for the WGL surface");
        }
        return {};
    }

    void ReleaseActiveFrame() noexcept {
        activeFrameSerial = 0U;
    }

    void ResetNative() noexcept {
        ReleaseActiveFrame();
        if (ownsContext && renderContext != nullptr) {
            if (wglGetCurrentContext() == renderContext) {
                static_cast<void>(wglMakeCurrent(nullptr, nullptr));
            }
            static_cast<void>(wglDeleteContext(renderContext));
        }
        if (ownsDeviceContext &&
            window != nullptr &&
            deviceContext != nullptr) {
            static_cast<void>(ReleaseDC(window, deviceContext));
        }
        window = nullptr;
        deviceContext = nullptr;
        renderContext = nullptr;
        swapInterval = nullptr;
        descriptor = {};
        owningThread = 0U;
        ownsContext = false;
        ownsDeviceContext = false;
        initialized = false;
    }

    void Reset() noexcept {
        ResetNative();
        generation = 0U;
        lost = false;
    }

    Base::Result<void> ConfigurePixelFormat(HDC dc) noexcept {
        PIXELFORMATDESCRIPTOR requested{};
        requested.nSize = sizeof(requested);
        requested.nVersion = 1U;
        requested.dwFlags =
            PFD_DRAW_TO_WINDOW |
            PFD_SUPPORT_OPENGL |
            PFD_DOUBLEBUFFER;
        requested.iPixelType = PFD_TYPE_RGBA;
        requested.cColorBits = 32U;
        requested.cAlphaBits = 8U;
        requested.cDepthBits = 24U;
        requested.cStencilBits = 8U;
        requested.iLayerType = PFD_MAIN_PLANE;

        int format = GetPixelFormat(dc);
        if (format == 0) {
            format = ChoosePixelFormat(dc, &requested);
            if (format == 0 ||
                !SetPixelFormat(dc, format, &requested)) {
                return Unsupported(
                    "Failed to select a WGL window pixel format");
            }
        }

        PIXELFORMATDESCRIPTOR actual{};
        if (DescribePixelFormat(
                dc,
                format,
                sizeof(actual),
                &actual) == 0 ||
            (actual.dwFlags & PFD_SUPPORT_OPENGL) == 0U ||
            (actual.dwFlags & PFD_DRAW_TO_WINDOW) == 0U ||
            (actual.dwFlags & PFD_DOUBLEBUFFER) == 0U ||
            actual.iPixelType != PFD_TYPE_RGBA) {
            return Unsupported(
                "Window pixel format is incompatible with WGL");
        }
        return {};
    }

    Base::Result<void> CreateOwned(
        const NativeSurfaceDescriptor& candidate) noexcept {
        window = reinterpret_cast<HWND>(candidate.wgl.window);
        if (window == nullptr || !IsWindow(window)) {
            return InvalidArgument(
                "Owned WGL context creation requires a valid window");
        }
        deviceContext = GetDC(window);
        if (deviceContext == nullptr) {
            ResetNative();
            return InternalError(
                "GetDC failed for the WGL window");
        }
        ownsDeviceContext = true;

        Base::Result<void> pixelFormat =
            ConfigurePixelFormat(deviceContext);
        if (!pixelFormat) {
            ResetNative();
            return pixelFormat;
        }

        HGLRC bootstrap = wglCreateContext(deviceContext);
        if (bootstrap == nullptr ||
            !wglMakeCurrent(deviceContext, bootstrap)) {
            if (bootstrap != nullptr) {
                static_cast<void>(wglDeleteContext(bootstrap));
            }
            ResetNative();
            return Unsupported(
                "Failed to create the bootstrap WGL context");
        }

        const auto createContext =
            reinterpret_cast<WglCreateContextAttribsArbProc>(
                wglGetProcAddress("wglCreateContextAttribsARB"));
        if (createContext == nullptr) {
            static_cast<void>(wglMakeCurrent(nullptr, nullptr));
            static_cast<void>(wglDeleteContext(bootstrap));
            ResetNative();
            return Unsupported(
                "WGL_ARB_create_context is required for OpenGL 3.3 Core");
        }

        const int attributes[] = {
            WglContextMajorVersionArb, 3,
            WglContextMinorVersionArb, 3,
            WglContextProfileMaskArb,
            WglContextCoreProfileBitArb,
            0};
        HGLRC core = createContext(
            deviceContext, nullptr, attributes);
        static_cast<void>(wglMakeCurrent(nullptr, nullptr));
        static_cast<void>(wglDeleteContext(bootstrap));
        if (core == nullptr ||
            !wglMakeCurrent(deviceContext, core)) {
            if (core != nullptr) {
                static_cast<void>(wglDeleteContext(core));
            }
            ResetNative();
            return Unsupported(
                "Failed to create an OpenGL 3.3 Core WGL context");
        }

        renderContext = core;
        ownsContext = true;
        return {};
    }

    Base::Result<void> CreateBorrowed(
        const NativeSurfaceDescriptor& candidate) noexcept {
        window = reinterpret_cast<HWND>(candidate.wgl.window);
        deviceContext =
            reinterpret_cast<HDC>(candidate.wgl.deviceContext);
        renderContext =
            reinterpret_cast<HGLRC>(candidate.wgl.renderContext);
        if (deviceContext == nullptr || renderContext == nullptr) {
            ResetNative();
            return InvalidArgument(
                "Borrowed WGL surface requires a DC and rendering context");
        }
        if (wglGetCurrentDC() != deviceContext ||
            wglGetCurrentContext() != renderContext) {
            ResetNative();
            return Base::Status::Failure(
                Base::ErrorCode::WrongThread,
                "Borrowed WGL context must be current on the creating thread");
        }
        return {};
    }

    Base::Result<void> ApplyPresentMode(
        PresentMode mode) noexcept {
        swapInterval = reinterpret_cast<WglSwapIntervalExtProc>(
            wglGetProcAddress("wglSwapIntervalEXT"));
        if (mode == PresentMode::Mailbox) {
            return Unsupported(
                "WGL does not provide mailbox presentation");
        }
        if (swapInterval == nullptr) {
            return mode == PresentMode::Immediate
                ? Base::Result<void>(Unsupported(
                    "Immediate WGL presentation requires WGL_EXT_swap_control"))
                : Base::Result<void>{};
        }
        const int interval =
            mode == PresentMode::Fifo ? 1 : 0;
        return swapInterval(interval)
            ? Base::Result<void>{}
            : Base::Result<void>(InternalError(
                "wglSwapIntervalEXT failed"));
    }

    Base::Result<void> Create(
        const NativeSurfaceDescriptor& candidate) noexcept {
        if (initialized || deviceContext != nullptr ||
            renderContext != nullptr) {
            return InvalidState(
                "WGL surface is already initialized");
        }
        if (candidate.kind != SurfaceKind::WglWindow ||
            candidate.sampleCount != 1U ||
            (candidate.colorFormat !=
                 GraphicsTextureFormat::Rgba8Unorm &&
             candidate.colorFormat !=
                 GraphicsTextureFormat::Bgra8Unorm)) {
            return Unsupported(
                "Initial WGL surfaces require a single-sample RGBA8/BGRA8 window");
        }
        if (generation == std::numeric_limits<
                GlContextGeneration>::max()) {
            return OutOfRange(
                "WGL context generation space is exhausted");
        }

        Base::Result<void> created =
            candidate.ownership == SurfaceOwnership::Owned
            ? CreateOwned(candidate)
            : CreateBorrowed(candidate);
        if (!created) {
            return created;
        }

        owningThread = GetCurrentThreadId();
        Base::Result<void> present =
            ApplyPresentMode(candidate.presentMode);
        if (!present) {
            ResetNative();
            return present;
        }

        descriptor = candidate;
        descriptor.wgl.window =
            reinterpret_cast<std::uintptr_t>(window);
        descriptor.wgl.deviceContext =
            reinterpret_cast<std::uintptr_t>(deviceContext);
        descriptor.wgl.renderContext =
            reinterpret_cast<std::uintptr_t>(renderContext);
        ++generation;
        initialized = true;
        lost = false;
        return {};
    }
};

WglSurfaceBackend::WglSurfaceBackend(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

WglSurfaceBackend::~WglSurfaceBackend() noexcept {
    DestroySurface();
}

std::uintptr_t
WglSurfaceBackend::NativeDeviceContext() const noexcept {
    return impl_ != nullptr
        ? reinterpret_cast<std::uintptr_t>(impl_->deviceContext)
        : 0U;
}

std::uintptr_t
WglSurfaceBackend::NativeRenderContext() const noexcept {
    return impl_ != nullptr
        ? reinterpret_cast<std::uintptr_t>(impl_->renderContext)
        : 0U;
}

bool WglSurfaceBackend::OwnsContext() const noexcept {
    return impl_ != nullptr && impl_->ownsContext;
}

GlContextGeneration
WglSurfaceBackend::ContextGeneration() const noexcept {
    return impl_ != nullptr ? impl_->generation : 0U;
}

Base::Result<GlFunctionTable>
WglSurfaceBackend::LoadFunctions() noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "WGL surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current.GetStatus();
    }
    return LoadGlFunctionTable(&Impl::Resolve, impl_);
}

Base::Result<GlContextContract>
WglSurfaceBackend::ContextContract() noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "WGL surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current.GetStatus();
    }
    GlContextContract contract;
    contract.userData = impl_;
    contract.contextHandle = impl_->renderContext;
    contract.resolve = &Impl::Resolve;
    contract.isCurrent = &Impl::IsCurrent;
    contract.currentThreadToken = &Impl::CurrentThread;
    contract.owningThreadToken =
        static_cast<GlThreadToken>(impl_->owningThread);
    contract.generation = impl_->generation;
    contract.embeddingMode =
        impl_->descriptor.ownership == SurfaceOwnership::Borrowed
        ? GlEmbeddingMode::PreserveAndRestore
        : GlEmbeddingMode::HostReset;
    return contract;
}

Base::Result<void> WglSurfaceBackend::MakeCurrent() noexcept {
    return impl_ != nullptr
        ? impl_->MakeContextCurrent()
        : Base::Result<void>(NotInitialized(
            "WGL surface is not initialized"));
}

SurfaceCapabilities
WglSurfaceBackend::QuerySurfaceCapabilities() const noexcept {
    SurfaceCapabilities capabilities;
    capabilities.supportedKinds =
        SurfaceKindBit(SurfaceKind::WglWindow);
    capabilities.maxWidth = 16384U;
    capabilities.maxHeight = 16384U;
    capabilities.supportsResize = true;
    capabilities.supportsPresent = true;
    capabilities.supportsContextLossRecovery = true;
    capabilities.supportsExternalRenderTargets = true;
    return capabilities;
}

Base::Result<void> WglSurfaceBackend::CreateSurface(
    const NativeSurfaceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateNativeSurfaceDescriptor(
        descriptor, QuerySurfaceCapabilities());
    if (!valid) {
        return valid;
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory(
                "Failed to allocate WGL surface state");
        }
        impl_ = new (memory) Impl();
    }
    return impl_->Create(descriptor);
}

void WglSurfaceBackend::DestroySurface() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->Reset();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;
}

Base::Result<void> WglSurfaceBackend::ResizeSurface(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "WGL surface is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (impl_->activeFrameSerial != 0U) {
        return InvalidState(
            "Cannot resize a WGL surface with an active frame");
    }
    const SurfaceCapabilities capabilities =
        QuerySurfaceCapabilities();
    if (width == 0U || height == 0U ||
        width > capabilities.maxWidth ||
        height > capabilities.maxHeight) {
        return InvalidArgument(
            "WGL surface resize dimensions are invalid");
    }
    impl_->descriptor.width = width;
    impl_->descriptor.height = height;
    return {};
}

Base::Result<ExternalRenderTargetDescriptor>
WglSurfaceBackend::AcquireSurfaceTarget(
    std::uint64_t frameSerial) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "WGL surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current.GetStatus();
    }
    if (frameSerial == 0U ||
        impl_->activeFrameSerial != 0U) {
        return InvalidState(
            "WGL surface frame acquisition is invalid");
    }
    impl_->activeFrameSerial = frameSerial;

    ExternalRenderTargetDescriptor target;
    target.width = impl_->descriptor.width;
    target.height = impl_->descriptor.height;
    target.colorFormat = impl_->descriptor.colorFormat;
    target.depthStencilFormat =
        impl_->descriptor.depthStencilFormat;
    target.sampleCount = impl_->descriptor.sampleCount;
    target.defaultFramebuffer = true;
    target.stableId = impl_->descriptor.stableId;
    return target;
}

Base::Result<void> WglSurfaceBackend::PresentSurface(
    std::uint64_t frameSerial,
    FenceValue signalFence) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "WGL surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current;
    }
    if (frameSerial == 0U ||
        frameSerial != impl_->activeFrameSerial ||
        signalFence == 0U) {
        return InvalidState(
            "WGL surface present frame is invalid");
    }
    const BOOL swapped = SwapBuffers(impl_->deviceContext);
    impl_->ReleaseActiveFrame();
    if (!swapped) {
        impl_->lost = true;
        return InternalError(
            "SwapBuffers failed for the WGL surface");
    }
    return {};
}

void WglSurfaceBackend::DiscardSurfaceFrame(
    std::uint64_t frameSerial) noexcept {
    if (impl_ != nullptr && frameSerial != 0U &&
        frameSerial == impl_->activeFrameSerial) {
        impl_->ReleaseActiveFrame();
    }
}

void WglSurfaceBackend::NotifySurfaceLost() noexcept {
    if (impl_ != nullptr) {
        impl_->ReleaseActiveFrame();
        impl_->lost = true;
    }
}

Base::Result<void> WglSurfaceBackend::RestoreSurface(
    const NativeSurfaceDescriptor& descriptor) noexcept {
    if (impl_ == nullptr || !impl_->lost) {
        return InvalidState(
            "WGL surface is not in the lost state");
    }
    Base::Result<void> valid = ValidateNativeSurfaceDescriptor(
        descriptor, QuerySurfaceCapabilities());
    if (!valid) {
        return valid;
    }
    const GlContextGeneration generation = impl_->generation;
    impl_->ResetNative();
    impl_->generation = generation;
    impl_->lost = true;
    Base::Result<void> restored = impl_->Create(descriptor);
    if (!restored) {
        impl_->lost = true;
        return restored;
    }
    return {};
}

bool WglSurfaceBackend::IsSurfaceLost() const noexcept {
    return impl_ != nullptr && impl_->lost;
}

} // namespace Aero::Rhi
