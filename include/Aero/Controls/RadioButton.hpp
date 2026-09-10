#pragma once

#include <Aero/Controls/Primitives/ToggleButton.hpp>

namespace Aero::Controls {

class AERO_GUI_API RadioButton : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(RadioButton, Primitives::ToggleButton)
public:
    RadioButton() noexcept : RadioButton(StaticTypeId()) {}
    ~RadioButton() override;

    StringView GetGroupName() const noexcept;
    void SetGroupName(StringView value) noexcept;

    AERO_DEPENDENCY_PROPERTY(String, GroupName);

protected:
    explicit RadioButton(TypeId runtimeType) noexcept;

    void OnClick() override;
    void OnToggle() noexcept override;

private:
    void UncheckRadioPeers() noexcept;
    struct RadioState;
    RadioState* radioState_ = nullptr;
};

} // namespace Aero::Controls
