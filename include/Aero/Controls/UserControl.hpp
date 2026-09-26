#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_GUI_API UserControl : public ContentControl {
    AERO_DECLARE_TYPE(UserControl, ContentControl)
public:
    UserControl() noexcept : ContentControl(StaticTypeId()) {}
    ~UserControl() override = default;
protected:
    explicit UserControl(TypeId runtimeType) noexcept : ContentControl(runtimeType) {}
};

} // namespace Aero::Controls
