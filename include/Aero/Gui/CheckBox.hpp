#pragma once

#include <Aero/Gui/ToggleButton.hpp>

namespace Aero::Controls {

class AERO_API CheckBox : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(CheckBox, Primitives::ToggleButton)
public:
    CheckBox() noexcept : CheckBox(StaticTypeId()) {}
    ~CheckBox() override = default;

protected:
    explicit CheckBox(TypeId runtimeType) noexcept
        : Primitives::ToggleButton(runtimeType) {}
};

} // namespace Aero::Controls
