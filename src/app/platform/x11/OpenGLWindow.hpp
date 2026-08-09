#pragma once

#include "app/Presentation.hpp"

#include <AeroApp/WindowInterop.hpp>
#include <AeroRender/OpenGL33.hpp>

#include <cstdint>

namespace Aero::App::X11 {

// Direct owner of the GLX context attached to the App-owned X11 window.
// Display and drawable remain borrowed from the platform window object.
class OpenGLWindow final {
public:
    OpenGLWindow() noexcept = default;
    ~OpenGLWindow() noexcept { Shutdown(); }

    OpenGLWindow(const OpenGLWindow&) = delete;
    OpenGLWindow& operator=(const OpenGLWindow&) = delete;

    Base::Result<void> Initialize(
        Platform::NativeWindowHandle window,
        PresentationSize size) noexcept;
    Base::Result<void> MakeCurrent() noexcept;
    Base::Result<void> Present() noexcept;
    Base::Result<void> Resize(PresentationSize size) noexcept;
    void Shutdown() noexcept;

    bool IsCurrent() const noexcept;
    std::uint64_t Generation() const noexcept { return generation_; }
    std::uint32_t Width() const noexcept { return size_.width; }
    std::uint32_t Height() const noexcept { return size_.height; }
    std::uint64_t StableId() const noexcept;

    static Render::OpenGL33::ProcAddress ResolveCallback(
        void* context,
        const char* name) noexcept;
    static Base::Status MakeCurrentCallback(void* context) noexcept;
    static bool IsCurrentCallback(void* context) noexcept;
    static std::uint64_t GenerationCallback(void* context) noexcept;

private:
    void* display_ = nullptr;
    std::uintptr_t drawable_ = 0U;
    void* renderContext_ = nullptr;
    PresentationSize size_;
    std::uint64_t generation_ = 0U;
};

} // namespace Aero::App::X11
