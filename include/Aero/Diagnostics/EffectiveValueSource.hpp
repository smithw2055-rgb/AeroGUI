#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero { class DependencyObject; }

namespace Aero::Meta {

struct DependencyPropertyHandle;

// Installed DP headers need this enum and PropertyValueSourceInfo without
// pulling PropertyProviderSet / HashMap-backed provider storage.
enum class EffectiveValueSource : std::uint8_t {
    Default = 0U,
    Local,
    Current
};

enum class PropertyValueRank : std::uint8_t {
    Default = 0U,
    Inherited = 10U,
    ThemeStyleSetter = 20U,
    ThemeStyle = ThemeStyleSetter,
    ThemeStyleTrigger = 30U,
    StyleSetter = 40U,
    Style = StyleSetter,
    TemplateTrigger = 50U,
    StyleTrigger = 60U,
    Trigger = StyleTrigger,
    ImplicitStyle = 70U,
    TemplatedParentSetter = 80U,
    Template = TemplatedParentSetter,
    TemplatedParentTrigger = 90U,
    Local = 100U,
    LocalExpression = Local,
    VisualState = 105U,
    Animation = 110U,
    Coercion = 120U
};

using EffectiveValueProvider = PropertyValueRank;

enum class PropertyExpressionKind : std::uint8_t {
    Custom = 0U,
    Binding,
    DynamicResource
};

using PropertyExpressionEvaluateCallback = Result<Value> (*)(
    void* context,
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept;
using PropertyExpressionCleanupCallback = void (*)(void* context) noexcept;

struct PropertyExpression {
    void* context = nullptr;
    PropertyExpressionEvaluateCallback evaluate = nullptr;
    PropertyExpressionCleanupCallback cleanup = nullptr;
    PropertyExpressionKind kind = PropertyExpressionKind::Custom;

    bool IsValid() const noexcept {
        return evaluate != nullptr;
    }
};

struct PropertyProviderToken {
    PropertyValueRank rank = PropertyValueRank::Default;
    std::uint32_t origin = 0U;
    std::uint32_t ordinal = 0U;

    constexpr bool IsValid() const noexcept {
        return rank != PropertyValueRank::Default && origin != 0U;
    }
};

constexpr bool operator==(
    PropertyProviderToken left,
    PropertyProviderToken right) noexcept {
    return left.rank == right.rank &&
        left.origin == right.origin &&
        left.ordinal == right.ordinal;
}

constexpr bool operator!=(
    PropertyProviderToken left,
    PropertyProviderToken right) noexcept {
    return !(left == right);
}

struct PropertyValueSourceInfo {
    PropertyValueRank rank = PropertyValueRank::Default;
    PropertyProviderToken token;
    PropertyExpressionKind expressionKind =
        PropertyExpressionKind::Custom;
    bool hasExpression = false;
    bool isInherited = false;
    bool isAnimated = false;
    bool isCoerced = false;
    bool isCurrentValue = false;
    std::uint64_t revision = 0U;
};

} // namespace Aero::Meta
