#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>

#include <cstdint>
#include <utility>

namespace Aero { class DependencyObject; }

namespace Aero::Meta {

using DependencyObject = ::Aero::DependencyObject;
struct DependencyPropertyHandle;

using PropertyValueKind = ValueKind;
using PropertyValue = Value;

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

// Local and animation use fixed engine-owned identities. Manager-owned
// Style, ThemeStyle and Template providers allocate from the canonical range.
inline constexpr std::uint32_t LocalValueProviderOrigin = 1U;
inline constexpr std::uint32_t AnimationValueProviderOrigin = 2U;
inline constexpr std::uint32_t FirstCanonicalProviderOrigin = 16U;

enum class PropertyExpressionKind : std::uint8_t {
    Custom = 0U,
    Binding,
    DynamicResource
};

using PropertyExpressionEvaluateCallback = Base::Result<PropertyValue> (*)(
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

class PropertyProviderOriginAllocator {
public:
    explicit constexpr PropertyProviderOriginAllocator(
        std::uint32_t first = FirstCanonicalProviderOrigin) noexcept
        : next_(first) {}

    Base::Result<std::uint32_t> Allocate() noexcept {
        if (next_ < FirstCanonicalProviderOrigin || next_ == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Property provider origin limit reached");
        }
        return next_++;
    }

    std::uint32_t Next() const noexcept {
        return next_;
    }

private:
    std::uint32_t next_ = FirstCanonicalProviderOrigin;
};

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

struct PropertyProviderContribution {
    PropertyProviderToken token;
    PropertyValue value;
};

// Canonical token-scoped provider storage. Exact-token writes replace one
// contribution; distinct ordinals represent simultaneous declarations. Higher
// ranks win first, followed by later provider origins and declaration ordinals.
// Origins are allocated by EffectiveValueEngine and are unique across all
// provider sessions attached to that engine.
class PropertyProviderSet {
public:
    bool Set(
        PropertyProviderToken token,
        const PropertyValue& value) noexcept {
        if (!token.IsValid() || value.IsUnset()) {
            return false;
        }
        const std::uint32_t existing = Find(token);
        if (existing != UINT32_MAX) {
            contributions_[existing].value = value;
            return true;
        }
        Base::Result<void> added = contributions_.PushBack({token, value});
        if (!added) {
            Base::ReportOutOfMemory(
                sizeof(PropertyProviderContribution),
                alignof(PropertyProviderContribution),
                Base::MemoryTag::Container);
        }
        return true;
    }

    bool Set(
        PropertyProviderToken token,
        PropertyValue&& value) noexcept {
        if (!token.IsValid() || value.IsUnset()) {
            return false;
        }
        const std::uint32_t existing = Find(token);
        if (existing != UINT32_MAX) {
            contributions_[existing].value = std::move(value);
            return true;
        }
        PropertyProviderContribution contribution;
        contribution.token = token;
        contribution.value = std::move(value);
        Base::Result<void> added =
            contributions_.PushBack(std::move(contribution));
        if (!added) {
            Base::ReportOutOfMemory(
                sizeof(PropertyProviderContribution),
                alignof(PropertyProviderContribution),
                Base::MemoryTag::Container);
        }
        return true;
    }

    bool Remove(PropertyProviderToken token) noexcept {
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

    std::uint32_t GetCount() const noexcept {
        return contributions_.Size();
    }

    bool GetIsEmpty() const noexcept {
        return contributions_.Empty();
    }

private:
    Base::Vector<PropertyProviderContribution> contributions_;

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

} // namespace Aero::Meta
