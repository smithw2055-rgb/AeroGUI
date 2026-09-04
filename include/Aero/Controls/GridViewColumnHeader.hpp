#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Controls {
enum class GridViewColumnHeaderRole : std::uint8_t {
    Normal = 0U,
    Floating,
    Padding
};

class AERO_GUI_API GridViewColumnHeader
    : public ContentControl {
    AERO_DECLARE_TYPE(GridViewColumnHeader, ContentControl)
public:
    GridViewColumnHeader() noexcept
        : ContentControl(StaticTypeId()) {}

    GridViewColumnHeaderRole GetRole() const noexcept {
        return GetValue(RoleProperty);
    }
    void SetRole(
        GridViewColumnHeaderRole value) noexcept {
        SetValue(RoleProperty, value);
    }

    inline static constexpr DependencyProperty<GridViewColumnHeaderRole> RoleProperty{"Role"};
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridViewColumnHeaderRole)
