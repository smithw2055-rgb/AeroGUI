#pragma once

#include <Aero/Controls/ButtonBase.hpp>

namespace Aero::Controls {

class AERO_API Button : public Primitives::ButtonBase {
    AERO_DECLARE_TYPE(Button, Primitives::ButtonBase)
public:
    Button() noexcept : Button(StaticTypeId()) {}
    ~Button() override = default;

protected:
    explicit Button(TypeId runtimeType) noexcept
        : Primitives::ButtonBase(runtimeType) {}
};

} // namespace Aero::Controls
