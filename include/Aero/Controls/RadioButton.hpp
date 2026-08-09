#pragma once

#include <Aero/Controls/ToggleButton.hpp>

namespace Aero::Controls {

class AERO_GUI_API RadioButton : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(RadioButton, Primitives::ToggleButton)
public:
    RadioButton() noexcept : RadioButton(StaticTypeId()) {}
    ~RadioButton() override = default;

    StringView GetGroupName() const noexcept;
    void SetGroupName(
        StringView value) noexcept;

    inline static constexpr DependencyProperty<String> GroupNameProperty{"GroupName"};

protected:
    explicit RadioButton(TypeId runtimeType) noexcept
        : Primitives::ToggleButton(runtimeType) {}
};

} // namespace Aero::Controls
