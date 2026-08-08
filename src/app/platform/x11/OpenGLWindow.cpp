#include "OpenGLWindow.hpp"

#if !defined(__linux__) && !defined(__unix__)
#error "OpenGLWindow.cpp is only supported on Unix/X11"
#endif

#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>

#ifdef Status
#undef Status
#endif

namespace Aero::App::X11 {
namespace {

using SwapInterval = void (*)(Display*, GLXDrawable, int);

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

Base::Status NativeFailure(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InternalError, message);
}

} // namespace

Base::Result<void> OpenGLWindow::Initialize(
    Platform::NativeWindowHandle window,
    PresentationSize size) noexcept {
    Base::Result<void> validSize = ValidatePresentationSize(size);
    if (!window.IsValid() || window.system != Platform::WindowSystem::X11 ||
        window.display == 0U || !validSize || renderContext_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GLX initialization requires an unused X11 window and valid dimensions");
    }
    auto* display = reinterpret_cast<Display*>(window.display);
    const GLXDrawable drawable = static_cast<GLXDrawable>(window.window);
    const int screen = DefaultScreen(display);
    const int configAttributes[] = {
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
    int configCount = 0;
    GLXFBConfig* configs =
        glXChooseFBConfig(display, screen, configAttributes, &configCount);
    if (configs == nullptr || configCount <= 0) {
        if (configs != nullptr) XFree(configs);
        return Unsupported("No compatible double-buffered GLX configuration was found");
    }
    const GLXFBConfig config = configs[0];
    XFree(configs);

    const auto createContext = reinterpret_cast<
        PFNGLXCREATECONTEXTATTRIBSARBPROC>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(
                "glXCreateContextAttribsARB")));
    if (createContext == nullptr) {
        return Unsupported("GLX_ARB_create_context is required for OpenGL 3.3 Core");
    }
    const int contextAttributes[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB,
        GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None};
    GLXContext context = createContext(
        display, config, nullptr, True, contextAttributes);
    if (context == nullptr ||
        glXMakeCurrent(display, drawable, context) == False) {
        if (context != nullptr) glXDestroyContext(display, context);
        return Unsupported("Unable to create an OpenGL 3.3 Core GLX context");
    }

    display_ = display;
    drawable_ = static_cast<std::uintptr_t>(drawable);
    renderContext_ = context;
    size_ = size;
    ++generation_;
    const auto swapInterval = reinterpret_cast<SwapInterval>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(
            "glXSwapIntervalEXT")));
    if (swapInterval != nullptr) swapInterval(display, drawable, 1);
    return {};
}

Base::Result<void> OpenGLWindow::MakeCurrent() noexcept {
    auto* display = static_cast<Display*>(display_);
    const auto drawable = static_cast<GLXDrawable>(drawable_);
    auto context = reinterpret_cast<GLXContext>(renderContext_);
    if (display == nullptr || drawable == 0U || context == nullptr) {
        return InvalidState("GLX context is not initialized");
    }
    if (glXGetCurrentDisplay() == display &&
        glXGetCurrentDrawable() == drawable &&
        glXGetCurrentContext() == context) {
        return {};
    }
    return glXMakeCurrent(display, drawable, context) != False
        ? Base::Result<void>()
        : Base::Result<void>(NativeFailure(
              "Unable to make the GLX context current"));
}

Base::Result<void> OpenGLWindow::Present() noexcept {
    Base::Result<void> current = MakeCurrent();
    if (!current) return current.GetStatus();
    glXSwapBuffers(
        static_cast<Display*>(display_),
        static_cast<GLXDrawable>(drawable_));
    return {};
}

Base::Result<void> OpenGLWindow::Resize(PresentationSize size) noexcept {
    Base::Result<void> valid = ValidatePresentationSize(size);
    if (!valid) return valid.GetStatus();
    if (renderContext_ == nullptr) {
        return InvalidState("GLX context is not initialized");
    }
    size_ = size;
    return {};
}

void OpenGLWindow::Shutdown() noexcept {
    auto* display = static_cast<Display*>(display_);
    auto context = reinterpret_cast<GLXContext>(renderContext_);
    if (display != nullptr && context != nullptr) {
        if (glXGetCurrentContext() == context) {
            static_cast<void>(glXMakeCurrent(display, None, nullptr));
        }
        glXDestroyContext(display, context);
    }
    display_ = nullptr;
    drawable_ = 0U;
    renderContext_ = nullptr;
    size_ = {};
    generation_ = 0U;
}

bool OpenGLWindow::IsCurrent() const noexcept {
    return display_ != nullptr && renderContext_ != nullptr &&
        glXGetCurrentDisplay() == static_cast<Display*>(display_) &&
        glXGetCurrentDrawable() == static_cast<GLXDrawable>(drawable_) &&
        glXGetCurrentContext() ==
            reinterpret_cast<GLXContext>(renderContext_);
}

std::uint64_t OpenGLWindow::StableId() const noexcept {
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(renderContext_));
}

Render::OpenGL33ProcAddress OpenGLWindow::ResolveCallback(
    void*,
    const char* name) noexcept {
    if (name == nullptr) return nullptr;
    const __GLXextFuncPtr address = glXGetProcAddressARB(
        reinterpret_cast<const GLubyte*>(name));
    return reinterpret_cast<Render::OpenGL33ProcAddress>(address);
}

Base::Status OpenGLWindow::MakeCurrentCallback(void* context) noexcept {
    auto* window = static_cast<OpenGLWindow*>(context);
    if (window == nullptr) return InvalidState("GLX callback has no window");
    Base::Result<void> current = window->MakeCurrent();
    return current ? Base::Status::Ok() : current.GetStatus();
}

bool OpenGLWindow::IsCurrentCallback(void* context) noexcept {
    const auto* window = static_cast<const OpenGLWindow*>(context);
    return window != nullptr && window->IsCurrent();
}

std::uint64_t OpenGLWindow::GenerationCallback(void* context) noexcept {
    const auto* window = static_cast<const OpenGLWindow*>(context);
    return window != nullptr ? window->Generation() : 0U;
}

} // namespace Aero::App::X11
