#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {

class AERO_GUI_API Label : public ContentControl {
    AERO_DECLARE_TYPE(Label, ContentControl)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Label() noexcept : ContentControl(StaticTypeId()) {}
};

} // namespace Aero::Controls
