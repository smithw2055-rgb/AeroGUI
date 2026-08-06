#pragma once

#include <cstdint>

namespace Aero::Platform {

enum class WindowSystem : std::uint8_t {
    Unknown = 0U,
    Win32,
    X11,
    Cocoa,
    Android,
    Web
};

struct NativeWindowHandle {
    WindowSystem system = WindowSystem::Unknown;
    std::uintptr_t display = 0U;
    std::uintptr_t window = 0U;
    std::uintptr_t instance = 0U;

    bool IsValid() const noexcept {
        return system != WindowSystem::Unknown && window != 0U;
    }
};

} // namespace Aero::Platform
