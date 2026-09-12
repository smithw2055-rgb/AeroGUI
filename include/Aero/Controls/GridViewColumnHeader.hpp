#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Meta { class Registration; }

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
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    GridViewColumnHeader() noexcept
        : ContentControl(StaticTypeId()) {}

    GridViewColumnHeaderRole GetRole() const noexcept {
        return GetValue(RoleProperty);
    }
    void SetRole(GridViewColumnHeaderRole value) noexcept {
        SetValue(RoleProperty, value);
    }

    AERO_DEPENDENCY_PROPERTY(GridViewColumnHeaderRole, Role);
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridViewColumnHeaderRole)
