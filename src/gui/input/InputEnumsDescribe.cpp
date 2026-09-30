#include "gui/core/EnumRegistration.hpp"

#include <Aero/InputScope.hpp>
#include <Aero/Input.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/KeyboardNavigation.hpp>

namespace Aero {

Base::Result<void> PopulateInputEnums(
    Meta::Registration& context) noexcept {
    using namespace Input;

    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        InputScope,
        "InputScope",
        description
            .Value("Default", InputScope::Default)
            .Value("Url", InputScope::Url)
            .Value("EmailSmtpAddress", InputScope::EmailSmtpAddress)
            .Value("Digits", InputScope::Digits)
            .Value("Number", InputScope::Number)
            .Value("Password", InputScope::Password)
            .Value("TelephoneNumber", InputScope::TelephoneNumber););
    AERO_REGISTER_ENUM(
        DragDropEffects,
        "DragDropEffects",
        description
            .Value("None", DragDropEffects::None)
            .Value("Copy", DragDropEffects::Copy)
            .Value("Move", DragDropEffects::Move)
            .Value("Link", DragDropEffects::Link)
            .Value("All", DragDropEffects::All););
    AERO_REGISTER_ENUM(
        CursorType,
        "CursorType",
        description
            .Value("None", CursorType::None)
            .Value("No", CursorType::No)
            .Value("Arrow", CursorType::Arrow)
            .Value("AppStarting", CursorType::AppStarting)
            .Value("Cross", CursorType::Cross)
            .Value("Help", CursorType::Help)
            .Value("IBeam", CursorType::IBeam)
            .Value("SizeAll", CursorType::SizeAll)
            .Value("SizeNESW", CursorType::SizeNESW)
            .Value("SizeNS", CursorType::SizeNS)
            .Value("SizeNWSE", CursorType::SizeNWSE)
            .Value("SizeWE", CursorType::SizeWE)
            .Value("UpArrow", CursorType::UpArrow)
            .Value("Wait", CursorType::Wait)
            .Value("Hand", CursorType::Hand)
            .Value("Pen", CursorType::Pen)
            .Value("ScrollNS", CursorType::ScrollNS)
            .Value("ScrollWE", CursorType::ScrollWE)
            .Value("ScrollAll", CursorType::ScrollAll)
            .Value("ScrollN", CursorType::ScrollN)
            .Value("ScrollS", CursorType::ScrollS)
            .Value("ScrollW", CursorType::ScrollW)
            .Value("ScrollE", CursorType::ScrollE)
            .Value("ScrollNW", CursorType::ScrollNW)
            .Value("ScrollNE", CursorType::ScrollNE)
            .Value("ScrollSW", CursorType::ScrollSW)
            .Value("ScrollSE", CursorType::ScrollSE)
            .Value("ArrowCD", CursorType::ArrowCD)
            .Value("Custom", CursorType::Custom););
    AERO_REGISTER_ENUM(
        KeyboardNavigationMode,
        "KeyboardNavigationMode",
        description
            .Value("Continue", KeyboardNavigationMode::Continue)
            .Value("Once", KeyboardNavigationMode::Once)
            .Value("Cycle", KeyboardNavigationMode::Cycle)
            .Value("None", KeyboardNavigationMode::None)
            .Value("Contained", KeyboardNavigationMode::Contained)
            .Value("Local", KeyboardNavigationMode::Local););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
