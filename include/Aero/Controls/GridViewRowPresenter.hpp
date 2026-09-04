#pragma once

#include <Aero/FrameworkElement.hpp>

namespace Aero::Controls {

class AERO_GUI_API GridViewRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnsProperty{"Columns"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ContentProperty{"Content"};
};
} // namespace Aero::Controls
