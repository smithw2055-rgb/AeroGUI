#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics/EffectiveValueSource.hpp>
#include <Aero/Value.hpp>

#include <cstdint>
#include <utility>

namespace Aero { class DependencyObject; }

namespace Aero::Meta {

using DependencyObject = ::Aero::DependencyObject;
struct DependencyPropertyHandle;

using PropertyValueKind = ValueKind;
using PropertyValue = Value;

// Local and animation use fixed engine-owned identities. Manager-owned
// Style, ThemeStyle and Template providers allocate from the canonical range.
inline constexpr std::uint32_t LocalValueProviderOrigin = 1U;
inline constexpr std::uint32_t AnimationValueProviderOrigin = 2U;
inline constexpr std::uint32_t VisualStateProviderOrigin = 3U;
inline constexpr std::uint32_t FirstCanonicalProviderOrigin = 16U;

class PropertyProviderOriginAllocator {
public:
    explicit constexpr PropertyProviderOriginAllocator(
        std::uint32_t first = FirstCanonicalProviderOrigin) noexcept
        : next_(first) {}

    Result<std::uint32_t> Allocate() noexcept {
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
        Result<void> added = contributions_.PushBack({token, value});
        if (!added) {
            Base::ReportOutOfMemory(
                sizeof(PropertyProviderContribution),
                alignof(PropertyProviderContribution),
                Base::MemoryTag::Container);
            return true;
        }
        NoteInserted(contributions_.Size() - 1U);
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
        Result<void> added =
            contributions_.PushBack(std::move(contribution));
        if (!added) {
            Base::ReportOutOfMemory(
                sizeof(PropertyProviderContribution),
                alignof(PropertyProviderContribution),
                Base::MemoryTag::Container);
            return true;
        }
        NoteInserted(contributions_.Size() - 1U);
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
                EraseAtUnchecked(index);
                ++removed;
            } else {
                ++index;
            }
        }
        if (removed != 0U) RefreshWinner();
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
                EraseAtUnchecked(index);
                ++removed;
            } else {
                ++index;
            }
        }
        if (removed != 0U) RefreshWinner();
        return removed;
    }

    std::uint32_t RemoveRank(PropertyValueRank rank) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < contributions_.Size()) {
            if (contributions_[index].token.rank == rank) {
                EraseAtUnchecked(index);
                ++removed;
            } else {
                ++index;
            }
        }
        if (removed != 0U) RefreshWinner();
        return removed;
    }

    void Clear() noexcept {
        contributions_.Clear();
        winner_ = UINT32_MAX;
    }

    const PropertyProviderContribution* Winner() const noexcept {
        // C1: cached winner index. Set() maintains it incrementally (one
        // comparison); removals fix it up or rescan once. Hot recompute path
        // drops from O(k) to O(1).
        return winner_ != UINT32_MAX && winner_ < contributions_.Size()
            ? &contributions_[winner_]
            : nullptr;
    }

    const PropertyProviderContribution* FindContribution(
        PropertyProviderToken token) const noexcept {
        const std::uint32_t index = Find(token);
        return index != UINT32_MAX
            ? &contributions_[index]
            : nullptr;
    }

    Span<const PropertyProviderContribution>
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
    // Cached index of the IsStronger-maximum element, or UINT32_MAX when
    // empty. Order-independent because Winner is defined by comparison.
    std::uint32_t winner_ = UINT32_MAX;

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
        // C1: order-independent swap-remove. Winner() selects by
        // IsStronger(rank/origin/ordinal), so element order is irrelevant.
        const std::uint32_t last = contributions_.Size() - 1U;
        if (index != last) {
            contributions_[index] = std::move(contributions_[last]);
        }
        contributions_.PopBack();
        if (contributions_.Empty()) {
            winner_ = UINT32_MAX;
        } else if (winner_ == last) {
            // Removed the tail: either the winner itself (rescan) or the
            // element swapped into the removed slot (winner moved).
            winner_ = (index != last) ? index : UINT32_MAX;
            if (winner_ == UINT32_MAX) RefreshWinner();
        } else if (winner_ == index) {
            RefreshWinner();
        }
    }

    void EraseAtUnchecked(std::uint32_t index) noexcept {
        const std::uint32_t last = contributions_.Size() - 1U;
        if (index != last) {
            contributions_[index] = std::move(contributions_[last]);
        }
        contributions_.PopBack();
    }

    void NoteInserted(std::uint32_t index) noexcept {
        if (winner_ == UINT32_MAX ||
            IsStronger(
                contributions_[index].token,
                contributions_[winner_].token)) {
            winner_ = index;
        }
    }

    void RefreshWinner() noexcept {
        winner_ = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < contributions_.Size();
             ++index) {
            if (winner_ == UINT32_MAX ||
                IsStronger(
                    contributions_[index].token,
                    contributions_[winner_].token)) {
                winner_ = index;
            }
        }
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
