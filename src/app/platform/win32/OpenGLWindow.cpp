#include "OpenGLWindow.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>

namespace Aero::App::Win32 {
namespace {

constexpr int WglContextMajorVersion = 0x2091;
constexpr int WglContextMinorVersion = 0x2092;
constexpr int WglContextProfileMask = 0x9126;
constexpr int WglContextCoreProfileBit = 0x00000001;

using WglCreateContextAttribs = HGLRC (WINAPI *)(HDC, HGLRC, const int*);
using WglSwapInterval = BOOL (WINAPI *)(int);

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
    if (!window.IsValid() ||
        window.system != Platform::WindowSystem::Win32 ||
        !validSize || renderContext_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "WGL initialization requires an unused Win32 window and valid dimensions");
    }

    auto nativeWindow = reinterpret_cast<HWND>(window.window);
    HDC deviceContext = GetDC(nativeWindow);
    if (deviceContext == nullptr) {
        return NativeFailure("Unable to acquire the Win32 device context for OpenGL");
    }
    window_ = nativeWindow;
    deviceContext_ = deviceContext;

    if (GetPixelFormat(deviceContext) == 0) {
        PIXELFORMATDESCRIPTOR descriptor{};
        descriptor.nSize = sizeof(descriptor);
        descriptor.nVersion = 1U;
        descriptor.dwFlags =
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        descriptor.iPixelType = PFD_TYPE_RGBA;
        descriptor.cColorBits = 32U;
        descriptor.cAlphaBits = 8U;
        descriptor.cDepthBits = 24U;
        descriptor.cStencilBits = 8U;
        descriptor.iLayerType = PFD_MAIN_PLANE;
        const int pixelFormat = ChoosePixelFormat(deviceContext, &descriptor);
        if (pixelFormat == 0 ||
            !SetPixelFormat(deviceContext, pixelFormat, &descriptor)) {
            Shutdown();
            return NativeFailure("Unable to configure the WGL pixel format");
        }
    }

    HGLRC bootstrap = wglCreateContext(deviceContext);
    if (bootstrap == nullptr || !wglMakeCurrent(deviceContext, bootstrap)) {
        if (bootstrap != nullptr) wglDeleteContext(bootstrap);
        Shutdown();
        return NativeFailure("Unable to create the bootstrap WGL context");
    }
    const auto createContext = reinterpret_cast<WglCreateContextAttribs>(
        wglGetProcAddress("wglCreateContextAttribsARB"));
    if (createContext == nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(bootstrap);
        Shutdown();
        return Unsupported("WGL_ARB_create_context is required for OpenGL 3.3 Core");
    }
    const int attributes[] = {
        WglContextMajorVersion, 3,
        WglContextMinorVersion, 3,
        WglContextProfileMask, WglContextCoreProfileBit,
        0};
    HGLRC core = createContext(deviceContext, nullptr, attributes);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(bootstrap);
    if (core == nullptr || !wglMakeCurrent(deviceContext, core)) {
        if (core != nullptr) wglDeleteContext(core);
        Shutdown();
        return Unsupported("Unable to create an OpenGL 3.3 Core WGL context");
    }

    renderContext_ = core;
    size_ = size;
    ++generation_;
    const auto swapInterval = reinterpret_cast<WglSwapInterval>(
        wglGetProcAddress("wglSwapIntervalEXT"));
    if (swapInterval != nullptr) static_cast<void>(swapInterval(1));
    return {};
}

Base::Result<void> OpenGLWindow::MakeCurrent() noexcept {
    auto deviceContext = static_cast<HDC>(deviceContext_);
    auto renderContext = static_cast<HGLRC>(renderContext_);
    if (deviceContext == nullptr || renderContext == nullptr) {
        return InvalidState("WGL context is not initialized");
    }
    if (wglGetCurrentContext() == renderContext &&
        wglGetCurrentDC() == deviceContext) {
        return {};
    }
    return wglMakeCurrent(deviceContext, renderContext)
        ? Base::Result<void>()
        : Base::Result<void>(NativeFailure(
              "Unable to make the WGL context current"));
}

Base::Result<void> OpenGLWindow::Present() noexcept {
    Base::Result<void> current = MakeCurrent();
    if (!current) return current.GetStatus();
    return SwapBuffers(static_cast<HDC>(deviceContext_))
        ? Base::Result<void>()
        : Base::Result<void>(NativeFailure(
              "Unable to swap the WGL window buffers"));
}

Base::Result<void> OpenGLWindow::Resize(PresentationSize size) noexcept {
    Base::Result<void> valid = ValidatePresentationSize(size);
    if (!valid) return valid.GetStatus();
    if (renderContext_ == nullptr) {
        return InvalidState("WGL context is not initialized");
    }
    size_ = size;
    return {};
}

void OpenGLWindow::Shutdown() noexcept {
    auto renderContext = static_cast<HGLRC>(renderContext_);
    if (renderContext != nullptr) {
        if (wglGetCurrentContext() == renderContext) {
            wglMakeCurrent(nullptr, nullptr);
        }
        wglDeleteContext(renderContext);
    }
    if (window_ != nullptr && deviceContext_ != nullptr) {
        ReleaseDC(
            static_cast<HWND>(window_),
            static_cast<HDC>(deviceContext_));
    }
    window_ = nullptr;
    deviceContext_ = nullptr;
    renderContext_ = nullptr;
    size_ = {};
    generation_ = 0U;
}

bool OpenGLWindow::IsCurrent() const noexcept {
    return renderContext_ != nullptr &&
        wglGetCurrentContext() == static_cast<HGLRC>(renderContext_) &&
        wglGetCurrentDC() == static_cast<HDC>(deviceContext_);
}

std::uint64_t OpenGLWindow::StableId() const noexcept {
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(renderContext_));
}

Render::OpenGL33::ProcAddress OpenGLWindow::ResolveCallback(
    void*,
    const char* name) noexcept {
    if (name == nullptr) return nullptr;
    PROC address = wglGetProcAddress(name);
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    if (address == nullptr || value == 1U || value == 2U ||
        value == 3U || value == static_cast<std::uintptr_t>(-1)) {
        HMODULE module = GetModuleHandleW(L"opengl32.dll");
        address = module != nullptr ? GetProcAddress(module, name) : nullptr;
    }
    return reinterpret_cast<Render::OpenGL33::ProcAddress>(address);
}

Base::Status OpenGLWindow::MakeCurrentCallback(void* context) noexcept {
    auto* window = static_cast<OpenGLWindow*>(context);
    if (window == nullptr) return InvalidState("WGL callback has no window");
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

} // namespace Aero::App::Win32
