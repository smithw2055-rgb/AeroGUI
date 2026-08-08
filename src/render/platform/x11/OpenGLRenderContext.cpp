#include "render/platform/x11/OpenGLRenderContext.hpp"

#if !defined(__linux__) && !defined(__unix__)
#error "OpenGLRenderContext.cpp is only supported on Unix/X11"
#endif

#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef Status
#undef Status
#endif

#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <thread>
#include <utility>

namespace Aero::Graphics {
namespace {

thread_local int CapturedXError = 0;
std::mutex XErrorMutex;

int CaptureXError(Display*, XErrorEvent* event) {
    CapturedXError = event != nullptr
        ? static_cast<int>(event->error_code)
        : -1;
    return 0;
}

template <typename Operation>
int RunCheckedXOperation(
    Display* display,
    Operation&& operation) noexcept {
    std::lock_guard<std::mutex> lock(XErrorMutex);
    XErrorHandler previous = XSetErrorHandler(&CaptureXError);
    XSync(display, False);
    CapturedXError = 0;
    std::forward<Operation>(operation)();
    XSync(display, False);
    const int error = CapturedXError;
    static_cast<void>(XSetErrorHandler(previous));
    return error;
}

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

GlThreadToken ThreadToken() noexcept {
    return static_cast<GlThreadToken>(
        std::hash<std::thread::id>{}(
            std::this_thread::get_id()));
}

} // namespace

struct GlxRenderContext::Impl  {
    using SwapIntervalExtProc =
        void (*)(Display*, GLXDrawable, int);

    Display* display = nullptr;
    GLXDrawable drawable = 0U;
    GLXContext context = nullptr;
    GLXFBConfig fbConfig = nullptr;
    Window ownedWindow = 0U;
    Colormap ownedColormap = 0U;
    SwapIntervalExtProc swapInterval = nullptr;
    WindowRenderContextDescriptor descriptor;
    std::uint64_t activeFrameSerial = 0U;
    GlContextGeneration generation = 0U;
    GlThreadToken owningThread = 0U;
    bool ownsDisplay = false;
    bool ownsDrawable = false;
    bool ownsContext = false;
    bool initialized = false;
    bool lost = false;

    static GlProcAddress Resolve(
        void*, const char* name) noexcept {
        if (name == nullptr || name[0] == '\0') {
            return nullptr;
        }
        const __GLXextFuncPtr address =
            glXGetProcAddressARB(
                reinterpret_cast<const GLubyte*>(name));
        return reinterpret_cast<GlProcAddress>(address);
    }

    static bool IsCurrent(
        void* userData,
        const void* contextHandle) noexcept {
        auto* self = static_cast<Impl*>(userData);
        return self != nullptr &&
            ThreadToken() == self->owningThread &&
            glXGetCurrentContext() ==
                reinterpret_cast<GLXContext>(
                    const_cast<void*>(contextHandle)) &&
            glXGetCurrentDrawable() == self->drawable &&
            glXGetCurrentDisplay() == self->display;
    }

    static GlThreadToken CurrentThread(void*) noexcept {
        return ThreadToken();
    }

    Base::Result<void> VerifyReady() const noexcept {
        if (!initialized || display == nullptr ||
            drawable == 0U || context == nullptr) {
            return NotInitialized(
                "GLX surface is not initialized");
        }
        if (lost) {
            return InvalidState("GLX surface is lost");
        }
        if (ThreadToken() != owningThread) {
            return Base::Status::Failure(
                Base::ErrorCode::WrongThread,
                "GLX surface must be used on its owning thread");
        }
        return {};
    }

    Base::Result<void> MakeContextCurrent() noexcept {
        Base::Result<void> ready = VerifyReady();
        if (!ready) {
            return ready;
        }
        if (glXGetCurrentContext() == context &&
            glXGetCurrentDrawable() == drawable) {
            return {};
        }
        Bool madeCurrent = False;
        const int error = RunCheckedXOperation(
            display,
            [&]() noexcept {
                madeCurrent =
                    glXMakeCurrent(display, drawable, context);
            });
        if (error != 0 || madeCurrent == False) {
            lost = true;
            return InternalError(
                "glXMakeCurrent failed for the GLX surface");
        }
        return {};
    }

    void ReleaseActiveFrame() noexcept {
        activeFrameSerial = 0U;
    }

    void ResetNative() noexcept {
        ReleaseActiveFrame();
        if (display != nullptr &&
            ownsContext && context != nullptr) {
            if (glXGetCurrentContext() == context) {
                static_cast<void>(
                    glXMakeCurrent(display, None, nullptr));
            }
            glXDestroyContext(display, context);
        }
        if (display != nullptr &&
            ownsDrawable && ownedWindow != 0U) {
            XDestroyWindow(display, ownedWindow);
        }
        if (display != nullptr &&
            ownedColormap != 0U) {
            XFreeColormap(display, ownedColormap);
        }
        if (ownsDisplay && display != nullptr) {
            XCloseDisplay(display);
        }
        display = nullptr;
        drawable = 0U;
        context = nullptr;
        fbConfig = nullptr;
        ownedWindow = 0U;
        ownedColormap = 0U;
        swapInterval = nullptr;
        descriptor = {};
        owningThread = 0U;
        ownsDisplay = false;
        ownsDrawable = false;
        ownsContext = false;
        initialized = false;
    }

    void Reset() noexcept {
        ResetNative();
        generation = 0U;
        lost = false;
    }

    Base::Result<void> ChooseConfig(int screen) noexcept {
        const int attributes[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            GLX_STENCIL_SIZE, 8,
            GLX_DOUBLEBUFFER, True,
            None};
        int count = 0;
        GLXFBConfig* configs =
            glXChooseFBConfig(
                display, screen, attributes, &count);
        if (configs == nullptr || count <= 0) {
            if (configs != nullptr) {
                XFree(configs);
            }
            return Unsupported(
                "No compatible double-buffered GLX FBConfig was found");
        }
        fbConfig = configs[0];
        XFree(configs);
        return {};
    }

    Base::Result<void> CreateDrawable(
        int screen,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        XVisualInfo* visual =
            glXGetVisualFromFBConfig(display, fbConfig);
        if (visual == nullptr) {
            return Unsupported(
                "GLX FBConfig has no X11 visual");
        }
        XSetWindowAttributes attributes{};
        attributes.border_pixel = 0U;
        attributes.event_mask =
            StructureNotifyMask | ExposureMask;
        const int error = RunCheckedXOperation(
            display,
            [&]() noexcept {
                ownedColormap = XCreateColormap(
                    display,
                    RootWindow(display, screen),
                    visual->visual,
                    AllocNone);
                attributes.colormap = ownedColormap;
                ownedWindow = XCreateWindow(
                    display,
                    RootWindow(display, screen),
                    0,
                    0,
                    width,
                    height,
                    0,
                    visual->depth,
                    InputOutput,
                    visual->visual,
                    CWBorderPixel | CWColormap | CWEventMask,
                    &attributes);
            });
        XFree(visual);
        if (error != 0 || ownedWindow == 0U) {
            return InternalError(
                "Failed to create the hidden GLX window");
        }
        drawable = static_cast<GLXDrawable>(ownedWindow);
        ownsDrawable = true;
        return {};
    }

    Base::Result<void> CreateOwned(
        const WindowRenderContextDescriptor& candidate) noexcept {
        display = candidate.glx.display != 0U
            ? reinterpret_cast<Display*>(
                candidate.glx.display)
            : XOpenDisplay(nullptr);
        ownsDisplay = candidate.glx.display == 0U;
        if (display == nullptr) {
            ResetNative();
            return Unsupported(
                "Unable to open the X11 display for GLX");
        }
        const int screen = candidate.glx.screen >= 0
            ? candidate.glx.screen
            : DefaultScreen(display);
        if (screen < 0 || screen >= ScreenCount(display)) {
            ResetNative();
            return InvalidArgument(
                "GLX surface screen index is invalid");
        }
        Base::Result<void> config = ChooseConfig(screen);
        if (!config) {
            ResetNative();
            return config;
        }

        if (candidate.glx.drawable != 0U) {
            drawable =
                static_cast<GLXDrawable>(
                    candidate.glx.drawable);
        } else {
            Base::Result<void> createdDrawable =
                CreateDrawable(
                    screen,
                    candidate.width,
                    candidate.height);
            if (!createdDrawable) {
                ResetNative();
                return createdDrawable;
            }
        }

        const auto createContext =
            reinterpret_cast<
                PFNGLXCREATECONTEXTATTRIBSARBPROC>(
                    glXGetProcAddressARB(
                        reinterpret_cast<const GLubyte*>(
                            "glXCreateContextAttribsARB")));
        if (createContext == nullptr) {
            ResetNative();
            return Unsupported(
                "GLX_ARB_create_context is required for OpenGL 3.3 Core");
        }
        const int attributes[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB,
            GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None};
        const int createError = RunCheckedXOperation(
            display,
            [&]() noexcept {
                context = createContext(
                    display,
                    fbConfig,
                    nullptr,
                    True,
                    attributes);
            });
        Bool madeCurrent = False;
        const int currentError =
            context != nullptr && createError == 0
            ? RunCheckedXOperation(
                  display,
                  [&]() noexcept {
                      madeCurrent = glXMakeCurrent(
                          display, drawable, context);
                  })
            : 0;
        if (context == nullptr || createError != 0 ||
            currentError != 0 || madeCurrent == False) {
            if (context != nullptr) {
                glXDestroyContext(display, context);
                context = nullptr;
            }
            ResetNative();
            return Unsupported(
                "Failed to create an OpenGL 3.3 Core GLX context");
        }
        ownsContext = true;
        return {};
    }

    Base::Result<void> CreateBorrowed(
        const WindowRenderContextDescriptor& candidate) noexcept {
        display = reinterpret_cast<Display*>(
            candidate.glx.display);
        drawable = static_cast<GLXDrawable>(
            candidate.glx.drawable);
        context = reinterpret_cast<GLXContext>(
            candidate.glx.context);
        if (display == nullptr || drawable == 0U ||
            context == nullptr) {
            ResetNative();
            return InvalidArgument(
                "Borrowed GLX surface requires display, drawable, and context");
        }
        if (glXGetCurrentDisplay() != display ||
            glXGetCurrentDrawable() != drawable ||
            glXGetCurrentContext() != context) {
            ResetNative();
            return Base::Status::Failure(
                Base::ErrorCode::WrongThread,
                "Borrowed GLX context must be current on the creating thread");
        }
        return {};
    }

    Base::Result<void> ApplyPresentMode(
        PresentMode mode) noexcept {
        if (mode == PresentMode::Mailbox) {
            return Unsupported(
                "GLX does not provide mailbox presentation");
        }
        swapInterval = reinterpret_cast<SwapIntervalExtProc>(
            glXGetProcAddressARB(
                reinterpret_cast<const GLubyte*>(
                    "glXSwapIntervalEXT")));
        if (swapInterval == nullptr) {
            return mode == PresentMode::Immediate
                ? Base::Result<void>(Unsupported(
                    "Immediate GLX presentation requires GLX_EXT_swap_control"))
                : Base::Result<void>{};
        }
        const int error = RunCheckedXOperation(
            display,
            [&]() noexcept {
                swapInterval(
                    display,
                    drawable,
                    mode == PresentMode::Fifo ? 1 : 0);
            });
        if (error != 0) {
            lost = true;
            return InternalError(
                "Failed to set the GLX swap interval");
        }
        return {};
    }

    Base::Result<void> Create(
        const WindowRenderContextDescriptor& candidate) noexcept {
        if (initialized || display != nullptr ||
            drawable != 0U || context != nullptr) {
            return InvalidState(
                "GLX surface is already initialized");
        }
        if (candidate.kind != WindowRenderContextKind::Glx ||
            candidate.sampleCount != 1U ||
            (candidate.colorFormat !=
                 GraphicsTextureFormat::Rgba8Unorm &&
             candidate.colorFormat !=
                 GraphicsTextureFormat::Bgra8Unorm)) {
            return Unsupported(
                "Initial GLX surfaces require a single-sample RGBA8/BGRA8 window");
        }
        if (generation == std::numeric_limits<
                GlContextGeneration>::max()) {
            return OutOfRange(
                "GLX context generation space is exhausted");
        }
        Base::Result<void> created =
            candidate.ownership == WindowRenderContextOwnership::Owned
            ? CreateOwned(candidate)
            : CreateBorrowed(candidate);
        if (!created) {
            return created;
        }
        owningThread = ThreadToken();
        Base::Result<void> present =
            ApplyPresentMode(candidate.presentMode);
        if (!present) {
            ResetNative();
            return present;
        }
        descriptor = candidate;
        descriptor.glx.display =
            reinterpret_cast<std::uintptr_t>(display);
        descriptor.glx.drawable =
            static_cast<std::uintptr_t>(drawable);
        descriptor.glx.context =
            reinterpret_cast<std::uintptr_t>(context);
        ++generation;
        initialized = true;
        lost = false;
        return {};
    }
};

GlxRenderContext::GlxRenderContext(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

GlxRenderContext::~GlxRenderContext() noexcept {
    Shutdown();
}

std::uintptr_t GlxRenderContext::NativeDisplay() const noexcept {
    return impl_ != nullptr
        ? reinterpret_cast<std::uintptr_t>(impl_->display)
        : 0U;
}

std::uintptr_t GlxRenderContext::NativeDrawable() const noexcept {
    return impl_ != nullptr
        ? static_cast<std::uintptr_t>(impl_->drawable)
        : 0U;
}

std::uintptr_t GlxRenderContext::NativeContext() const noexcept {
    return impl_ != nullptr
        ? reinterpret_cast<std::uintptr_t>(impl_->context)
        : 0U;
}

bool GlxRenderContext::OwnsDisplay() const noexcept {
    return impl_ != nullptr && impl_->ownsDisplay;
}

bool GlxRenderContext::OwnsDrawable() const noexcept {
    return impl_ != nullptr && impl_->ownsDrawable;
}

bool GlxRenderContext::OwnsContext() const noexcept {
    return impl_ != nullptr && impl_->ownsContext;
}

GlContextGeneration
GlxRenderContext::ContextGeneration() const noexcept {
    return impl_ != nullptr ? impl_->generation : 0U;
}

Base::Result<GlFunctionTable>
GlxRenderContext::LoadFunctions() noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "GLX surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current.GetStatus();
    }
    return LoadGlFunctionTable(&Impl::Resolve, impl_);
}

Base::Result<GlContextBinding>
GlxRenderContext::ContextBinding() noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "GLX surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current.GetStatus();
    }
    GlContextBinding contract;
    contract.userData = impl_;
    contract.contextHandle = impl_->context;
    contract.resolve = &Impl::Resolve;
    contract.isCurrent = &Impl::IsCurrent;
    contract.currentThreadToken = &Impl::CurrentThread;
    contract.owningThreadToken = impl_->owningThread;
    contract.generation = impl_->generation;
    contract.embeddingMode =
        impl_->descriptor.ownership == WindowRenderContextOwnership::Borrowed
        ? GlEmbeddingMode::PreserveAndRestore
        : GlEmbeddingMode::HostReset;
    return contract;
}

Base::Result<void> GlxRenderContext::MakeCurrent() noexcept {
    return impl_ != nullptr
        ? impl_->MakeContextCurrent()
        : Base::Result<void>(NotInitialized(
            "GLX surface is not initialized"));
}

WindowRenderContextCaps
GlxRenderContext::Caps() const noexcept {
    WindowRenderContextCaps capabilities;
    capabilities.supportedKinds =
        WindowRenderContextKindBit(WindowRenderContextKind::Glx);
    capabilities.maxWidth = 16384U;
    capabilities.maxHeight = 16384U;
    capabilities.supportsResize = true;
    capabilities.supportsPresent = true;
    capabilities.supportsContextLossRecovery = true;
    return capabilities;
}

Base::Result<void> GlxRenderContext::Create(
    const WindowRenderContextDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateWindowRenderContextDescriptor(
        descriptor, Caps());
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
                "Failed to allocate GLX surface state");
        }
        impl_ = new (memory) Impl();
    }
    return impl_->Create(descriptor);
}

void GlxRenderContext::Shutdown() noexcept {
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

Base::Result<void> GlxRenderContext::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "GLX surface is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (impl_->activeFrameSerial != 0U) {
        return InvalidState(
            "Cannot resize a GLX surface with an active frame");
    }
    const WindowRenderContextCaps capabilities =
        Caps();
    if (width == 0U || height == 0U ||
        width > capabilities.maxWidth ||
        height > capabilities.maxHeight) {
        return InvalidArgument(
            "GLX surface resize dimensions are invalid");
    }
    if (impl_->ownsDrawable &&
        impl_->ownedWindow != 0U) {
        const int error = RunCheckedXOperation(
            impl_->display,
            [&]() noexcept {
                XResizeWindow(
                    impl_->display,
                    impl_->ownedWindow,
                    width,
                    height);
            });
        if (error != 0) {
            impl_->lost = true;
            return InternalError(
                "Failed to resize the GLX drawable");
        }
    }
    impl_->descriptor.width = width;
    impl_->descriptor.height = height;
    return {};
}

Base::Result<RenderTargetBinding>
GlxRenderContext::AcquireTarget(
    std::uint64_t frameSerial) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "GLX surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current.GetStatus();
    }
    if (frameSerial == 0U ||
        impl_->activeFrameSerial != 0U) {
        return InvalidState(
            "GLX surface frame acquisition is invalid");
    }
    impl_->activeFrameSerial = frameSerial;
    RenderTargetBinding target;
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

Base::Result<void> GlxRenderContext::Present(
    std::uint64_t frameSerial,
    FenceValue signalFence) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "GLX surface is not initialized");
    }
    Base::Result<void> current = impl_->MakeContextCurrent();
    if (!current) {
        return current;
    }
    if (frameSerial == 0U ||
        frameSerial != impl_->activeFrameSerial ||
        signalFence == 0U) {
        return InvalidState(
            "GLX surface present frame is invalid");
    }
    const int error = RunCheckedXOperation(
        impl_->display,
        [&]() noexcept {
            glXSwapBuffers(
                impl_->display, impl_->drawable);
        });
    impl_->ReleaseActiveFrame();
    if (error != 0) {
        impl_->lost = true;
        return InternalError(
            "Failed to swap the GLX drawable buffers");
    }
    return {};
}

void GlxRenderContext::DiscardFrame(
    std::uint64_t frameSerial) noexcept {
    if (impl_ != nullptr && frameSerial != 0U &&
        frameSerial == impl_->activeFrameSerial) {
        impl_->ReleaseActiveFrame();
    }
}

void GlxRenderContext::NotifyLost() noexcept {
    if (impl_ != nullptr) {
        impl_->ReleaseActiveFrame();
        impl_->lost = true;
    }
}

Base::Result<void> GlxRenderContext::Restore(
    const WindowRenderContextDescriptor& descriptor) noexcept {
    if (impl_ == nullptr || !impl_->lost) {
        return InvalidState(
            "GLX surface is not in the lost state");
    }
    Base::Result<void> valid = ValidateWindowRenderContextDescriptor(
        descriptor, Caps());
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

bool GlxRenderContext::IsLost() const noexcept {
    return impl_ != nullptr && impl_->lost;
}

} // namespace Aero::Graphics
