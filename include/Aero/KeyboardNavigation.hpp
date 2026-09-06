#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Input {

enum class FocusNavigationDirection : std::uint8_t {
    Next,
    Previous,
};

enum class KeyboardNavigationMode : std::uint8_t {
    Continue = 0U,
    Once,
    Cycle,
    None,
    Contained,
    Local
};

class AERO_GUI_API KeyboardNavigation
    : public Base::Object {
    AERO_DECLARE_TYPE(
        KeyboardNavigation,
        Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    AERO_ATTACHED_PROPERTY(KeyboardNavigationMode, DirectionalNavigation);

    AERO_ATTACHED_PROPERTY(KeyboardNavigationMode, TabNavigation);
    AERO_ATTACHED_PROPERTY(KeyboardNavigationMode, ControlTabNavigation);
    AERO_ATTACHED_PROPERTY(std::uint32_t, TabIndex);
    AERO_ATTACHED_PROPERTY(bool, AcceptsReturn);
    AERO_ATTACHED_PROPERTY(bool, IsTabStop);
};
} // namespace Aero::Input

AERO_DECLARE_TYPE_ENUM(Aero::Input::KeyboardNavigationMode)
