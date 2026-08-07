#pragma once

#include <Aero/Controls/Primitives.hpp>

namespace Aero::Controls {

class AERO_API RadioButton : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(RadioButton, Primitives::ToggleButton)
public:
    RadioButton() noexcept : RadioButton(StaticTypeId()) {}
    ~RadioButton() override = default;

    Base::StringView GetGroupName() const noexcept;
    void SetGroupName(
        Base::StringView value) noexcept;

    inline static constexpr Members::Property<Base::String> GroupNameProperty{"GroupName"};

protected:
    explicit RadioButton(TypeId runtimeType) noexcept
        : Primitives::ToggleButton(runtimeType) {}
};

} // namespace Aero::Controls
