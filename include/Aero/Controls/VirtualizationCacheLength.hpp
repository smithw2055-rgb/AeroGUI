#pragma once

#include <Aero/Value.hpp>
#include <cstdint>

namespace Aero::Controls {

enum class VirtualizationCacheLengthUnit : std::uint8_t {
    Pixel = 0U,
    Item,
    Page
};

struct VirtualizationCacheLength {
    double cacheBeforeViewport = 0.0;
    double cacheAfterViewport = 0.0;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::VirtualizationCacheLengthUnit)

AERO_DECLARE_TYPE_VALUE(::Aero::Controls::VirtualizationCacheLength, "VirtualizationCacheLength")
