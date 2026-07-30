#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Core {

// Base-value precedence is ordered from weakest to strongest. Animation and
// coercion are represented here for diagnostics, but are applied after the
// winning base provider has been selected.
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
    Animation = 110U,
    Coercion = 120U
};

using EffectiveValueProvider = PropertyValueRank;

// Origin 1 is reserved by the compatibility EffectiveValueEngine APIs. The
// legacy StyleManager writes every active Trigger setter through one fixed
// token. PropertyProviderSet expands repeated writes of that token into an
// ordered contribution stack and removes that stack as one compatibility
// origin. Canonical token-aware callers allocate origins at or above
// FirstCanonicalProviderOrigin.
inline constexpr std::uint32_t LegacyStyleTriggerOrigin = 1U;
inline constexpr std::uint32_t FirstCanonicalProviderOrigin = 16U;

enum class PropertyExpressionKind : std::uint8_t {
    Custom = 0U,
    Binding,
    DynamicResource
};

struct PropertyProviderToken final {
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

struct PropertyValueSourceInfo final {
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

struct PropertyProviderContribution final {
    PropertyProviderToken token;
    PropertyValue value;
};

// Token-scoped provider storage used while Style, Template and Trigger are
// migrated away from one mutable slot per precedence layer. Later declarations
// win within one origin; later allocated origins win between active providers
// at the same rank. Origin allocation is owned by the compiling/runtime service
// and must therefore be stable and deterministic.
class PropertyProviderSet final {
public:
    Base::Result<void> Set(
        PropertyProviderToken token,
        const PropertyValue& value) noexcept {
        if (!token.IsValid() || value.IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "A property contribution requires a valid token and value");
        }
        const std::uint32_t existing = Find(token);
        if (existing != UINT32_MAX && !IsLegacyStyleTriggerToken(token)) {
            contributions_[existing].value = value;
            return {};
        }
        if (existing != UINT32_MAX) {
            Base::Result<PropertyProviderToken> expanded =
                NextLegacyStyleTriggerToken(token);
            if (!expanded) return expanded.GetStatus();
            token = expanded.Value();
        }
        return contributions_.TryPushBack({token, value});
    }

    Base::Result<void> Set(
        PropertyProviderToken token,
        PropertyValue&& value) noexcept {
        if (!token.IsValid() || value.IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "A property contribution requires a valid token and value");
        }
        const std::uint32_t existing = Find(token);
        if (existing != UINT32_MAX && !IsLegacyStyleTriggerToken(token)) {
            contributions_[existing].value = std::move(value);
            return {};
        }
        if (existing != UINT32_MAX) {
            Base::Result<PropertyProviderToken> expanded =
                NextLegacyStyleTriggerToken(token);
            if (!expanded) return expanded.GetStatus();
            token = expanded.Value();
        }
        PropertyProviderContribution contribution;
        contribution.token = token;
        contribution.value = std::move(value);
        return contributions_.TryPushBack(std::move(contribution));
    }

    bool Remove(PropertyProviderToken token) noexcept {
        if (IsLegacyStyleTriggerToken(token)) {
            return Remove(
                token.rank,
                token.origin) != 0U;
        }
        const std::uint32_t index = Find(token);
        if (index == UINT32_MAX) return false;
        RemoveAt(index);
        return true;
    }

    std::uint32_t RemoveOrigin(std::uint32_t origin) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < contributions_.Size()) {
            if (contributions_[index].token.origin == origin) {
                RemoveAt(index);
                ++removed;
            } else {
                ++index;
            }
        }
        return removed;
    }

    std::uint32_t Remove(
        PropertyValueRank rank,
        std::uint32_t origin) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < contributions_.Size()) {
            const PropertyProviderToken token =
                contributions_[index].token;
            if (token.rank == rank && token.origin == origin) {
                RemoveAt(index);
                ++removed;
            } else {
                ++index;
            }
        }
        return removed;
    }

    std::uint32_t RemoveRank(PropertyValueRank rank) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < contributions_.Size()) {
            if (contributions_[index].token.rank == rank) {
                RemoveAt(index);
                ++removed;
            } else {
                ++index;
            }
        }
        return removed;
    }

    void Clear() noexcept {
        contributions_.Clear();
    }

    const PropertyProviderContribution* Winner() const noexcept {
        if (contributions_.Empty()) return nullptr;
        std::uint32_t winner = 0U;
        for (std::uint32_t index = 1U;
             index < contributions_.Size();
             ++index) {
            if (IsStronger(
                    contributions_[index].token,
                    contributions_[winner].token)) {
                winner = index;
            }
        }
        return &contributions_[winner];
    }

    const PropertyProviderContribution* FindContribution(
        PropertyProviderToken token) const noexcept {
        const std::uint32_t index = Find(token);
        return index != UINT32_MAX
            ? &contributions_[index]
            : nullptr;
    }

    Base::Span<const PropertyProviderContribution>
    Contributions() const noexcept {
        return contributions_.AsSpan();
    }

    std::uint32_t Count() const noexcept {
        return contributions_.Size();
    }

    bool Empty() const noexcept {
        return contributions_.Empty();
    }

private:
    Base::Vector<PropertyProviderContribution> contributions_;

    static constexpr bool IsLegacyStyleTriggerToken(
        PropertyProviderToken token) noexcept {
        return token.rank == PropertyValueRank::StyleTrigger &&
            token.origin == LegacyStyleTriggerOrigin &&
            token.ordinal == 0U;
    }

    Base::Result<PropertyProviderToken>
    NextLegacyStyleTriggerToken(
        PropertyProviderToken token) const noexcept {
        std::uint32_t maximumOrdinal = 0U;
        for (const PropertyProviderContribution& contribution :
             contributions_) {
            if (contribution.token.rank == token.rank &&
                contribution.token.origin == token.origin &&
                contribution.token.ordinal > maximumOrdinal) {
                maximumOrdinal = contribution.token.ordinal;
            }
        }
        if (maximumOrdinal == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Legacy Style trigger contribution ordinal limit reached");
        }
        token.ordinal = maximumOrdinal + 1U;
        return token;
    }

    std::uint32_t Find(PropertyProviderToken token) const noexcept {
        for (std::uint32_t index = 0U;
             index < contributions_.Size();
             ++index) {
            if (contributions_[index].token == token) {
                return index;
            }
        }
        return UINT32_MAX;
    }

    void RemoveAt(std::uint32_t index) noexcept {
        for (std::uint32_t next = index + 1U;
             next < contributions_.Size();
             ++next) {
            contributions_[next - 1U] =
                std::move(contributions_[next]);
        }
        contributions_.PopBack();
    }

    static constexpr bool IsStronger(
        PropertyProviderToken left,
        PropertyProviderToken right) noexcept {
        if (left.rank != right.rank) {
            return static_cast<std::uint8_t>(left.rank) >
                static_cast<std::uint8_t>(right.rank);
        }
        if (left.origin != right.origin) {
            return left.origin > right.origin;
        }
        return left.ordinal > right.ordinal;
    }
};

} // namespace Aero::Core
