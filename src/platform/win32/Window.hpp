#pragma once

#include <Aero/Base/Allocator.hpp>
#include "../Window.hpp"

namespace Aero::Platform {

class AERO_API Win32Window final : public IWindow {
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
    Integration::NativeWindowHandle NativeHandle() const noexcept override;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Platform
