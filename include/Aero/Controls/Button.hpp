#pragma once

#include <Aero/Controls/Primitives/ButtonBase.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {

class AERO_GUI_API Button : public Primitives::ButtonBase {
    AERO_DECLARE_TYPE(Button, Primitives::ButtonBase)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Button() noexcept : Button(StaticTypeId()) {}
    ~Button() override = default;

protected:
    explicit Button(TypeId runtimeType) noexcept
        : Primitives::ButtonBase(runtimeType) {}
};

} // namespace Aero::Controls
