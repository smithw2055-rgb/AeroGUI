#pragma once

#include <Aero/FrameworkElement.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {

class AERO_GUI_API GridViewRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewRowPresenter,
        Aero::FrameworkElement)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    GridViewRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Columns);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Content);
};
} // namespace Aero::Controls
