#pragma once

#include <Aero/FrameworkElement.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {

class AERO_GUI_API GridViewHeaderRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewHeaderRowPresenter,
        Aero::FrameworkElement)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    GridViewHeaderRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    bool GetAllowsColumnReorder() const noexcept {
        return GetValue(AllowsColumnReorderProperty);
    }
    void SetAllowsColumnReorder(bool value) noexcept {
        SetValue(AllowsColumnReorderProperty, value);
    }

    AERO_DEPENDENCY_PROPERTY(bool, AllowsColumnReorder);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderContainerStyle);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderContextMenu);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplateSelector);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderToolTip);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Columns);
};
} // namespace Aero::Controls
