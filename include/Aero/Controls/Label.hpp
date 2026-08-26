#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Controls {

class AERO_GUI_API Label : public ContentControl {
    AERO_DECLARE_TYPE(Label, ContentControl)
public:
    Label() noexcept : ContentControl(StaticTypeId()) {}
};

} // namespace Aero::Controls
