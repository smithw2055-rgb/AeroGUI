#pragma once

#include <Aero/Value.hpp>

#include <cstdint>
#include <type_traits>

namespace Aero {

enum class GridUnitType : std::uint8_t {
    Auto = 0U,
    Pixel,
    Star
};

struct GridLength {
    float value = 1.0f;
    GridUnitType unit = GridUnitType::Star;

    constexpr GridLength() noexcept = default;

    template <typename T = float, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    constexpr GridLength(T val, GridUnitType u = GridUnitType::Pixel) noexcept
        : value(static_cast<float>(val)), unit(u) {}

    constexpr float GetValue() const noexcept { return value; }
    constexpr GridUnitType GetGridUnitType() const noexcept { return unit; }
    constexpr bool GetIsAbsolute() const noexcept { return unit == GridUnitType::Pixel; }
    constexpr bool GetIsAuto() const noexcept { return unit == GridUnitType::Auto; }
    constexpr bool GetIsStar() const noexcept { return unit == GridUnitType::Star; }

    static constexpr GridLength Auto() noexcept { return {0.0f, GridUnitType::Auto}; }
    template <typename T = float, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    static constexpr GridLength Pixel(T value) noexcept { return {static_cast<float>(value), GridUnitType::Pixel}; }
    template <typename T = float, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    static constexpr GridLength Star(T weight = 1.0f) noexcept { return {static_cast<float>(weight), GridUnitType::Star}; }

    constexpr bool operator==(const GridLength& other) const noexcept {
        return value == other.value && unit == other.unit;
    }
    constexpr bool operator!=(const GridLength& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace Aero

AERO_DECLARE_TYPE_VALUE(::Aero::GridLength, "GridLength")
