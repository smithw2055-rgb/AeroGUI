#pragma once

#include <Aero/Base/Allocator.hpp>
#include "../Window.hpp"

#include <cstddef>

namespace Aero::Platform {

struct X11WindowState;

class X11Window  : public IWindow {
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
    NativeWindowHandle NativeHandle() const noexcept override;

private:
    Base::IAllocator* allocator_ = nullptr;
    alignas(std::max_align_t) std::uint8_t stateStorage_[8192]{};
    X11WindowState* state_ = nullptr;
};

} // namespace Aero::Platform
