#pragma once

#include <Aero/Base/Allocator.hpp>
#include "../Window.hpp"

namespace Aero::Platform {

class AERO_API X11Window  : public IWindow {
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
    Base::Result<void> Show() noexcept override;
    Base::Result<bool> PollEvent(
        WindowEvent& event) noexcept override;
    Base::Result<bool> WaitEvent(
        WindowEvent& event) noexcept override;
    void Close() noexcept override;

    bool IsOpen() const noexcept override;
    std::uint32_t ClientWidth() const noexcept override;
    std::uint32_t ClientHeight() const noexcept override;
    double DpiScale() const noexcept override;
    Integration::NativeWindowHandle NativeHandle() const noexcept override;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Platform
