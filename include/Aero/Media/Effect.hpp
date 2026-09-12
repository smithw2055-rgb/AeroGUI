#pragma once

#include <Aero/Base/Geometry.hpp>
#include <cstdint>
#include <Aero/Base/Result.hpp>
#include <Aero/Freezable.hpp>

namespace Aero::Media {

class AERO_GUI_API Effect : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Effect, ::Aero::Freezable)
public:

    // Freezable content revision (render cache invalidation).
    std::uint64_t GetRevision() const noexcept;

protected:
    explicit Effect(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
};
} // namespace Aero::Media
