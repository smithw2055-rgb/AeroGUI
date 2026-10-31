#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>
#include "../Window.hpp"

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef ShowWindow
#undef ShowWindow
#endif
#endif

namespace Aero::Platform {

class Win32Window : public IWindow {
public:
    explicit Win32Window(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Win32Window() override;

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    Base::Result<void> Create(
        const WindowDescriptor& descriptor) noexcept override;
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
#if defined(_WIN32)
    void Push(const WindowEvent& event) noexcept;
    bool Dequeue(WindowEvent& event) noexcept;
    void UpdateClientMetrics() noexcept;
    void QueuePointer(
        WindowEventType type,
        double x,
        double y,
        WindowPointerButton button = WindowPointerButton::Unknown) noexcept;
    void QueueKey(
        WindowEventType type,
        std::uint32_t key,
        bool repeat = false) noexcept;
    void QueueCodePoint(std::uint32_t codePoint) noexcept;
    void QueueUtf16(std::uint16_t value) noexcept;
    LRESULT HandleMessage(
        HWND nativeWindow,
        UINT message,
        WPARAM word,
        LPARAM value) noexcept;
    static LRESULT CALLBACK WindowProcedure(
        HWND nativeWindow,
        UINT message,
        WPARAM word,
        LPARAM value) noexcept;
#endif

    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<WindowEvent> events_;
    std::uint32_t eventHead_ = 0U;
    std::uint32_t width_ = 0U;
    std::uint32_t height_ = 0U;
    double dpiScale_ = 1.0;
    bool open_ = false;

#if defined(_WIN32)
    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    std::uint16_t pendingHighSurrogate_ = 0U;
#endif
};

} // namespace Aero::Platform
