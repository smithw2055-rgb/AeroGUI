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

namespace Aero::Meta {

template<>
struct TypeTraits<::Aero::Controls::VirtualizationCacheLength> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("VirtualizationCacheLength");
    }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept {
        return "VirtualizationCacheLength";
    }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta
