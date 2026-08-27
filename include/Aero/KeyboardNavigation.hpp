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

    inline static constexpr AttachedProperty<KeyboardNavigationMode> DirectionalNavigationProperty{"DirectionalNavigation"};

    inline static constexpr AttachedProperty<KeyboardNavigationMode> TabNavigationProperty{"TabNavigation"};
    inline static constexpr AttachedProperty<KeyboardNavigationMode> ControlTabNavigationProperty{"ControlTabNavigation"};
    inline static constexpr AttachedProperty<std::uint32_t> TabIndexProperty{"TabIndex"};
    inline static constexpr AttachedProperty<bool> AcceptsReturnProperty{"AcceptsReturn"};
    inline static constexpr AttachedProperty<bool> IsTabStopProperty{"IsTabStop"};
};
} // namespace Aero::Input

AERO_DECLARE_TYPE_ENUM(Aero::Input::KeyboardNavigationMode)
