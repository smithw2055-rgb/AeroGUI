#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Freezable.hpp>

namespace Aero::Media {

class AERO_GUI_API Effect : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Effect, ::Aero::Freezable)
public:

protected:
    explicit Effect(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
};
} // namespace Aero::Media
