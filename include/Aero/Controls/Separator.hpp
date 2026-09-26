#pragma once

#include <Aero/Controls/Control.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Separator
    : public Control {
    AERO_DECLARE_TYPE(Separator, Control)
public:
    Separator() noexcept
        : Control(StaticTypeId()) {}
    ~Separator() override = default;
};

} // namespace Aero::Controls
