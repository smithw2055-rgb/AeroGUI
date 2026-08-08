#pragma once

#include <Aero/Gui/ContentControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_API UserControl : public ContentControl {
    AERO_DECLARE_TYPE(UserControl, ContentControl)
public:
    UserControl() noexcept : ContentControl(StaticTypeId()) {}
    ~UserControl() override = default;
protected:
    explicit UserControl(TypeId runtimeType) noexcept : ContentControl(runtimeType) {}
};

// Navigable content surface. It shares UserControl's single-child layout but
// remains a distinct XAML/runtime type so Page-targeted WPF styles resolve.
class AERO_API Page : public UserControl {
    AERO_DECLARE_TYPE(Page, UserControl)
public:
    Page() noexcept : UserControl(StaticTypeId()) {}
    ~Page() override = default;
};
} // namespace Aero::Controls
