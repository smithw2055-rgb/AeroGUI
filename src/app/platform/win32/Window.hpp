#pragma once

#include <Aero/Base/Allocator.hpp>
#include "../Window.hpp"

#include <cstddef>

namespace Aero::Platform {

struct Win32WindowState;

class Win32Window  : public IWindow {
public:
    explicit Win32Window(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Win32Window() override;

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    Base::Result<void> Create(
        const WindowDescriptor& descriptor) noexcept override;
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
    Win32WindowState* state_ = nullptr;
};

} // namespace Aero::Platform
