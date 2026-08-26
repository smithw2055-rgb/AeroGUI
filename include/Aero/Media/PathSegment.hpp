#pragma once

#include <Aero/Freezable.hpp>

namespace Aero::Media {

class AERO_GUI_API PathSegment : public Freezable {
    AERO_DECLARE_TYPE(PathSegment, Freezable)
protected:
    explicit PathSegment(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
    ~PathSegment() override = default;
};
} // namespace Aero::Media
