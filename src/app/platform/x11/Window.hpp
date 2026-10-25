#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>
#include "../Window.hpp"

#include <cstddef>
#include <cstdint>

#if AERO_PLATFORM_HAS_X11_WINDOW
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef Status
#undef Status
#endif
#endif

namespace Aero::Platform {

class X11Window : public IWindow {
public:
    explicit X11Window(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~X11Window() override;

    X11Window(const X11Window&) = delete;
    X11Window& operator=(const X11Window&) = delete;

    Base::Result<void> Create(
        const WindowDescriptor& descriptor) noexcept override;
    Base::Result<void> Attach(
        std::uintptr_t display,
        std::uintptr_t window,
        std::uint32_t width,
        std::uint32_t height,
        Base::StringView title = "AeroGUI",
        bool visible = true) noexcept;
    void Show() noexcept override;
    bool PollEvent(
        WindowEvent& event) noexcept override;
    bool WaitEvent(
        WindowEvent& event) noexcept override;
    void Close() noexcept override;

    bool IsOpen() const noexcept override;
    std::uint32_t ClientWidth() const noexcept override;
    std::uint32_t ClientHeight() const noexcept override;
    double DpiScale() const noexcept override;
    NativeWindowHandle NativeHandle() const noexcept override;

private:
#if AERO_PLATFORM_HAS_X11_WINDOW
    void Push(const WindowEvent& event) noexcept;
    bool Dequeue(WindowEvent& event) noexcept;
    double QueryDpiScale() const noexcept;
    Base::Result<void> SetTitle(Base::StringView title) noexcept;
    Base::Result<void> Configure(
        Base::StringView title,
        std::uint32_t requestedWidth,
        std::uint32_t requestedHeight,
        bool visible) noexcept;
    void QueuePointer(
        WindowEventType type,
        double x,
        double y,
        unsigned int state,
        WindowPointerButton button = WindowPointerButton::Unknown) noexcept;
    void QueueText(XKeyEvent& keyEvent) noexcept;
    void HandleEvent(XEvent& native) noexcept;
#endif

    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<WindowEvent> events_;
    std::uint32_t eventHead_ = 0U;
    std::uint32_t width_ = 0U;
    std::uint32_t height_ = 0U;
    double dpiScale_ = 1.0;
    bool open_ = false;

#if AERO_PLATFORM_HAS_X11_WINDOW
    Display* display_ = nullptr;
    ::Window window_ = 0U;
    Atom closeAtom_ = 0U;
    int screen_ = 0;
    bool ownsDisplay_ = false;
    bool ownsWindow_ = false;
#endif
};

} // namespace Aero::Platform
