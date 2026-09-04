#pragma once

#include <Aero/Controls/UserControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

// Navigable content surface. It shares UserControl's single-child layout but
// remains a distinct XAML/runtime type so Page-targeted WPF styles resolve.
class AERO_GUI_API Page : public UserControl {
    AERO_DECLARE_TYPE(Page, UserControl)
public:
    Page() noexcept : UserControl(StaticTypeId()) {}
    ~Page() override = default;
};

} // namespace Aero::Controls
