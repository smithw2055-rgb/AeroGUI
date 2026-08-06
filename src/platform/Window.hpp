#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Platform/NativeWindow.hpp>

#include <cstdint>

namespace Aero::Platform {

enum class WindowEventType : std::uint8_t {
    Invalid = 0U,
    CloseRequested,
    Closed,
    Resized,
    ScaleChanged,
    Exposed,
    PointerMove,
    PointerDown,
    PointerUp,
    PointerWheel,
    KeyDown,
    KeyUp,
    TextInput
};

enum class WindowPointerButton : std::uint8_t {
    Unknown = 0U,
    Left,
    Right,
    Middle,
    XButton1,
    XButton2
};

enum WindowModifier : std::uint32_t {
    WindowModifierNone = 0U,
    WindowModifierShift = 1U << 0U,
    WindowModifierControl = 1U << 1U,
    WindowModifierAlt = 1U << 2U
};

inline constexpr std::uint32_t WindowKeyBackspace = 8U;
inline constexpr std::uint32_t WindowKeyTab = 9U;
inline constexpr std::uint32_t WindowKeyEnter = 13U;
inline constexpr std::uint32_t WindowKeySpace = 32U;
inline constexpr std::uint32_t WindowKeyHome = 0x24U;
inline constexpr std::uint32_t WindowKeyEnd = 0x23U;
inline constexpr std::uint32_t WindowKeyLeft = 0x25U;
inline constexpr std::uint32_t WindowKeyUp = 0x26U;
inline constexpr std::uint32_t WindowKeyRight = 0x27U;
inline constexpr std::uint32_t WindowKeyDown = 0x28U;
inline constexpr std::uint32_t WindowKeyDelete = 0x2EU;

struct WindowDescriptor  {
    Base::StringView title = "AeroGUI";
    // A zero width/height pair requests the platform's preferred initial
    // size. Backends that have no native preference use a portable fallback.
    std::uint32_t width = 900U;
    std::uint32_t height = 640U;
    bool visible = true;
    bool resizable = true;
};

struct WindowEvent  {
    WindowEventType type = WindowEventType::Invalid;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    double dpiScale = 1.0;
    double x = 0.0;
    double y = 0.0;
    double wheelDeltaX = 0.0;
    double wheelDeltaY = 0.0;
    std::uint32_t key = 0U;
    std::uint32_t modifiers = WindowModifierNone;
    WindowPointerButton button = WindowPointerButton::Unknown;
    bool repeat = false;
    char text[8]{};
    std::uint8_t textSize = 0U;

    Base::StringView Text() const noexcept {
        return {text, textSize};
    }
};

class AERO_API IWindow {
public:
    virtual ~IWindow() = default;

    virtual Base::Result<void> Create(
        const WindowDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> Show() noexcept = 0;
    virtual Base::Result<bool> PollEvent(
        WindowEvent& event) noexcept = 0;
    virtual Base::Result<bool> WaitEvent(
        WindowEvent& event) noexcept = 0;
    virtual void Close() noexcept = 0;

    virtual bool IsOpen() const noexcept = 0;
    virtual std::uint32_t ClientWidth() const noexcept = 0;
    virtual std::uint32_t ClientHeight() const noexcept = 0;
    virtual double DpiScale() const noexcept = 0;
    virtual NativeWindowHandle NativeHandle() const noexcept = 0;
};

} // namespace Aero::Platform
