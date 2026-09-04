#pragma once

#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero {

enum class GridUnitType : std::uint8_t {
    Auto = 0U,
    Pixel,
    Star
};

struct GridLength {
    double value = 1.0;
    GridUnitType unit = GridUnitType::Star;

    constexpr double GetValue() const noexcept { return value; }
    constexpr GridUnitType GetGridUnitType() const noexcept { return unit; }
    constexpr bool GetIsAbsolute() const noexcept {
        return unit == GridUnitType::Pixel;
    }
    constexpr bool GetIsAuto() const noexcept {
        return unit == GridUnitType::Auto;
    }
    constexpr bool GetIsStar() const noexcept {
        return unit == GridUnitType::Star;
    }

    static constexpr GridLength Auto() noexcept {
        return {0.0, GridUnitType::Auto};
    }
    static constexpr GridLength Pixel(double value) noexcept {
        return {value, GridUnitType::Pixel};
    }
    static constexpr GridLength Star(double weight = 1.0) noexcept {
        return {weight, GridUnitType::Star};
    }
};

} // namespace Aero

namespace Aero::Meta {

template<>
struct TypeTraits<::Aero::GridLength> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("GridLength");
    }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept {
        return "GridLength";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Meta
