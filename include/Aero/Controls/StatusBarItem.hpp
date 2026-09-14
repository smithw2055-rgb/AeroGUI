#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API StatusBarItem
    : public ContentControl {
    AERO_DECLARE_TYPE(StatusBarItem, ContentControl)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    StatusBarItem() noexcept
        : ContentControl(StaticTypeId()) {}
    ~StatusBarItem() override = default;
};

} // namespace Aero::Controls
