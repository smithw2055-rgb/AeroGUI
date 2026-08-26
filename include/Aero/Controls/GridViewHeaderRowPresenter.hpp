#pragma once

#include <Aero/FrameworkElement.hpp>

namespace Aero::Controls {

class AERO_GUI_API GridViewHeaderRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewHeaderRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewHeaderRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    bool GetAllowsColumnReorder() const noexcept {
        return GetValueOr(AllowsColumnReorderProperty, false);
    }
    void SetAllowsColumnReorder(bool value) noexcept {
        SetValue(AllowsColumnReorderProperty, value);
    }

    inline static constexpr DependencyProperty<bool> AllowsColumnReorderProperty{"AllowsColumnReorder"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderContainerStyleProperty{"ColumnHeaderContainerStyle"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderContextMenuProperty{"ColumnHeaderContextMenu"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderTemplateProperty{"ColumnHeaderTemplate"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderTemplateSelectorProperty{"ColumnHeaderTemplateSelector"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderToolTipProperty{"ColumnHeaderToolTip"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnsProperty{"Columns"};
};
} // namespace Aero::Controls
