#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include "gui/core/DispatcherReentrancyGuard.hpp"
#include <Aero/Diagnostics.hpp>
#include "gui/core/PropertyProviderSet.hpp"
#include <Aero/DependencyProperty.hpp>
#include "gui/core/PropertySlab.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace Aero {

class DependencyObject;
using ::Aero::Threading::DispatcherReentrancyGuard;

using Meta::PropertyProviderSet;

// Uncommon DP data. Allocated when an inherited/provider/expression/
// animation/current/queue payload exists. Simple local values live inline in
// StoredValueEntry::inlineLocal (P2.1), so GetValue/SetValue of an ordinary
// local property never touches the heap.
//
// P2.4: blocks are owned by the Dispatcher PropertySlab. Hot paths allocate
// from the slab explicitly (EnsureRare(slab)); the class operator new stays
// as the plain-heap fallback for context-free copies (CopyRareFrom). Every
// block carries a slab header, so operator delete routes back to the owning
// slab (or the heap) with no calling context, safe from any thread.
struct StoredValueRare {
    PropertyExpression localExpression{};
    PropertyValue inheritedValue{};
    PropertyValue currentValue{};
    PropertyValue animationValue{};
    PropertyValue localBackup{};
    PropertyProviderSet baseProviders;
    PropertyValueSourceInfo sourceInfo{};

    static void* operator new(std::size_t size) noexcept {
        return PropertySlab::AllocateHeap(size);
    }
    static void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
        return PropertySlab::AllocateHeap(size);
    }
    static void* operator new(std::size_t size, std::align_val_t, const std::nothrow_t&) noexcept {
        return PropertySlab::AllocateHeap(size);
    }

    static void operator delete(void* ptr) noexcept {
        PropertySlab::ReleaseRare(ptr);
    }
    static void operator delete(void* ptr, const std::nothrow_t&) noexcept {
        PropertySlab::ReleaseRare(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t) noexcept {
        PropertySlab::ReleaseRare(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
        PropertySlab::ReleaseRare(ptr);
    }

    // P2.4: slab allocation for hot paths. Callers pass their Dispatcher's
    // slab (via DependencyObject::GetDispatcher().GetPropertySlab()).
    static void* AllocateFromSlab(PropertySlab& slab) noexcept {
        return slab.AllocateRare(sizeof(StoredValueRare));
    }

    // Placement new for slab-provided memory (class operator new overloads
    // hide the global placement new; the memory already carries its header).
    static void* operator new(std::size_t, void* memory) noexcept {
        return memory;
    }
    static void operator delete(void*, void*) noexcept {
    }
};

// Single-slot hot entry (HashMap value; MemberId is the map key):
//   PropertyValue effectiveValue
//   StoredValueRare* rare
//   std::uint32_t packedFlags   (origin/rank/has* bits)
// Inherited / animation / expression / providers / local-backup live in rare.
// When no override is active, effectiveValue directly holds the Local value.
struct StoredValueEntry {
    enum : std::uint32_t {
        HasLocalBit = 1U << 0U,
        HasCurrentBit = 1U << 1U,
        HasExpressionBit = 1U << 2U,
        HasInheritedBit = 1U << 3U,
        HasAnimationBit = 1U << 4U,
        QueuedBit = 1U << 5U,
        IsCoercedBit = 1U << 6U,
        IsCurrentValueBit = 1U << 7U,
        DescendantsQueuedBit = 1U << 8U,
        HasInlineProviderBit = 1U << 9U,
        RankShift = 10U,
        RankMask = 0x7FU,
        OriginShift = 17U,
        OriginMask = 0x7FFFU,

        EngineStateMask = QueuedBit | HasInlineProviderBit | HasExpressionBit |
            HasInheritedBit | HasAnimationBit | HasCurrentBit,
        ShieldingStateMask = HasLocalBit | HasExpressionBit | HasInlineProviderBit |
            HasCurrentBit | HasAnimationBit,
        ActiveValueMask = HasLocalBit | HasCurrentBit | HasExpressionBit |
            HasInheritedBit | HasAnimationBit | HasInlineProviderBit
    };

    PropertyValue effectiveValue;
    StoredValueRare* rare = nullptr;
    std::uint32_t packedFlags = 0U;
    std::uint32_t revision = 0U;

    StoredValueEntry() = default;

    StoredValueEntry(const StoredValueEntry& other)
        : effectiveValue(other.effectiveValue),
          rare(nullptr),
          packedFlags(other.packedFlags),
          revision(other.revision) {
        CopyRareFrom(other.rare);
    }

    StoredValueEntry(StoredValueEntry&& other) noexcept
        : effectiveValue(std::move(other.effectiveValue)),
          rare(other.rare),
          packedFlags(other.packedFlags),
          revision(other.revision) {
        other.rare = nullptr;
        other.packedFlags = 0U;
        other.revision = 0U;
    }

    ~StoredValueEntry() noexcept {
        delete rare;
        rare = nullptr;
    }

    StoredValueEntry& operator=(const StoredValueEntry& other) {
        if (this == &other) {
            return *this;
        }
        effectiveValue = other.effectiveValue;
        packedFlags = other.packedFlags;
        revision = other.revision;
        delete rare;
        rare = nullptr;
        CopyRareFrom(other.rare);
        return *this;
    }

    StoredValueEntry& operator=(StoredValueEntry&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        delete rare;
        effectiveValue = std::move(other.effectiveValue);
        packedFlags = other.packedFlags;
        revision = other.revision;
        rare = other.rare;
        other.rare = nullptr;
        other.packedFlags = 0U;
        other.revision = 0U;
        return *this;
    }

    bool HasLocal() const noexcept { return Flag(HasLocalBit); }
    bool HasCurrent() const noexcept { return Flag(HasCurrentBit); }
    bool HasExpression() const noexcept { return Flag(HasExpressionBit); }
    bool HasInherited() const noexcept { return Flag(HasInheritedBit); }
    bool HasAnimation() const noexcept { return Flag(HasAnimationBit); }
    bool Queued() const noexcept { return Flag(QueuedBit); }
    bool IsCoerced() const noexcept { return Flag(IsCoercedBit); }
    bool DescendantsQueued() const noexcept { return Flag(DescendantsQueuedBit); }
    bool HasInlineProvider() const noexcept { return Flag(HasInlineProviderBit); }

    // True when the entry holds an un-overridden, un-coerced inline local value
    // with no active expressions, animations, inheritance, or provider state.
    bool IsSimpleLocal() const noexcept {
        constexpr std::uint32_t kNonSimpleLocalMask =
            EngineStateMask | IsCoercedBit | IsCurrentValueBit | DescendantsQueuedBit;
        return rare == nullptr &&
            HasLocal() &&
            (packedFlags & kNonSimpleLocalMask) == 0U &&
            PackedRank() == Meta::PropertyValueRank::Local;
    }

    bool HasProviders() const noexcept {
        return HasInlineProvider() ||
            (rare != nullptr && !rare->baseProviders.GetIsEmpty());
    }

    bool HasEngineState() const noexcept {
        return (packedFlags & EngineStateMask) != 0U ||
            (rare != nullptr && !rare->baseProviders.GetIsEmpty());
    }

    bool HasInheritanceShieldingValue() const noexcept {
        return (packedFlags & ShieldingStateMask) != 0U ||
            (rare != nullptr && !rare->baseProviders.GetIsEmpty());
    }

    bool HasNonDefaultState(const PropertyValue& defaultValue) const noexcept {
        return (packedFlags & ActiveValueMask) != 0U ||
            (rare != nullptr && !rare->baseProviders.GetIsEmpty()) ||
            effectiveValue != defaultValue;
    }

    void SetHasLocal(bool value) noexcept { SetFlag(HasLocalBit, value); }
    void SetHasCurrent(bool value) noexcept { SetFlag(HasCurrentBit, value); }
    void SetHasExpression(bool value) noexcept {
        SetFlag(HasExpressionBit, value);
    }
    void SetHasInherited(bool value) noexcept {
        SetFlag(HasInheritedBit, value);
    }
    void SetHasAnimation(bool value) noexcept {
        SetFlag(HasAnimationBit, value);
    }
    void SetQueued(bool value) noexcept { SetFlag(QueuedBit, value); }
    void SetDescendantsQueued(bool value) noexcept {
        SetFlag(DescendantsQueuedBit, value);
    }
    void SetHasInlineProvider(bool value) noexcept {
        SetFlag(HasInlineProviderBit, value);
    }

    // P2.4: rare blocks come from the owning Dispatcher's slab. The
    // parameterless overload is intentionally absent so new call sites
    // cannot silently fall back to the heap.
    Base::Result<StoredValueRare*> EnsureRare(PropertySlab& slab) noexcept {
        if (rare != nullptr) {
            return rare;
        }
        void* memory = StoredValueRare::AllocateFromSlab(slab);
        if (memory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Dependency property uncommon-value block allocation failed");
        }
        const PropertyValueSourceInfo existingInfo = SourceInfo();
        rare = new (memory) StoredValueRare();
        rare->sourceInfo = existingInfo;
        if (HasInlineProvider()) {
            const Meta::PropertyProviderToken inlineToken{
                PackedRank(), PackedOrigin(), 0U};
            SetHasInlineProvider(false);
            static_cast<void>(rare->baseProviders.Set(inlineToken, effectiveValue));
        } else if (HasInherited() && !HasLocal()) {
            rare->inheritedValue = effectiveValue;
        }
        return rare;
    }

    void DropRareIfUnused() noexcept {
        if (rare == nullptr) {
            return;
        }
        const PropertyValueSourceInfo& source = rare->sourceInfo;
        if (HasCurrent() || HasExpression() || HasAnimation() || Queued() ||
            !rare->localBackup.IsUnset() ||
            source.isCoerced || source.isCurrentValue ||
            source.isAnimated || source.hasExpression ||
            source.token.ordinal != 0U ||
            source.token.origin > OriginMask ||
            source.expressionKind != Meta::PropertyExpressionKind::Custom ||
            source.revision > UINT32_MAX) {
            return;
        }
        if (!HasInherited() && rare->baseProviders.GetIsEmpty() &&
            (source.rank == Meta::PropertyValueRank::Local ||
             source.rank == Meta::PropertyValueRank::Default)) {
            PackOriginFlags(source);
            revision = static_cast<std::uint32_t>(source.revision);
            delete rare;
            rare = nullptr;
            return;
        }
        if (HasInherited() && !HasLocal() && rare->baseProviders.GetIsEmpty() &&
            source.rank == Meta::PropertyValueRank::Inherited &&
            effectiveValue == rare->inheritedValue) {
            PackOriginFlags(source);
            revision = static_cast<std::uint32_t>(source.revision);
            delete rare;
            rare = nullptr;
            return;
        }
        if (!HasLocal() && !HasInherited() &&
            rare->baseProviders.GetCount() == 1U) {
            const Meta::PropertyProviderContribution* win =
                rare->baseProviders.Winner();
            if (win != nullptr &&
                win->token.ordinal == 0U &&
                win->token.origin <= OriginMask &&
                source.rank == win->token.rank &&
                source.token == win->token &&
                effectiveValue == win->value) {
                PackOriginFlags(source);
                SetHasInlineProvider(true);
                revision = static_cast<std::uint32_t>(source.revision);
                delete rare;
                rare = nullptr;
            }
        }
    }

    PropertyValue LocalValueOrUnset() const noexcept {
        if (!HasLocal()) {
            return PropertyValue::Unset();
        }
        return (rare != nullptr && !rare->localBackup.IsUnset())
            ? rare->localBackup
            : effectiveValue;
    }

    // Single-slot local value store: when no animation/expression/coercion/provider override is active,
    // effectiveValue directly holds the local value.
    void SetLocalValue(const PropertyValue& value) noexcept {
        SetHasLocal(true);
        if (rare != nullptr &&
            (HasCurrent() || HasAnimation() || HasExpression() || Flag(IsCoercedBit) ||
             PackedRank() != Meta::PropertyValueRank::Local ||
             !rare->baseProviders.GetIsEmpty() || HasInherited())) {
            rare->localBackup = value;
        } else {
            effectiveValue = value;
            if (rare != nullptr) {
                rare->localBackup = PropertyValue::Unset();
            }
        }
    }

    void ClearLocal() noexcept {
        SetHasLocal(false);
        if (rare != nullptr) {
            rare->localBackup = PropertyValue::Unset();
        }
        DropRareIfUnused();
    }

    PropertyValue InheritedValueOrUnset() const noexcept {
        if (!HasInherited()) {
            return PropertyValue::Unset();
        }
        return (rare != nullptr) ? rare->inheritedValue : effectiveValue;
    }

    Base::Result<void> SetInheritedValue(
        const PropertyValue& value,
        PropertySlab& slab) noexcept {
        if (rare == nullptr &&
            !HasLocal() && !HasInlineProvider() &&
            !HasCurrent() && !HasExpression() && !HasAnimation() &&
            !Flag(IsCoercedBit)) {
            effectiveValue = value;
            SetHasInherited(true);
            return {};
        }
        Base::Result<StoredValueRare*> block = EnsureRare(slab);
        if (!block) {
            return block.GetStatus();
        }
        block->inheritedValue = value;
        SetHasInherited(true);
        return {};
    }

    void ClearInherited() noexcept {
        SetHasInherited(false);
        if (rare != nullptr) {
            rare->inheritedValue = PropertyValue::Unset();
        }
        DropRareIfUnused();
    }

    void ClearCurrent() noexcept {
        SetHasCurrent(false);
        if (rare != nullptr) {
            rare->currentValue = PropertyValue::Unset();
        }
        DropRareIfUnused();
    }

    void ClearAnimation() noexcept {
        SetHasAnimation(false);
        if (rare != nullptr) {
            rare->animationValue = PropertyValue::Unset();
        }
        DropRareIfUnused();
    }

    PropertyValue CurrentValueOrUnset() const noexcept {
        return (rare != nullptr) ? rare->currentValue : PropertyValue::Unset();
    }

    PropertyValue AnimationValueOrUnset() const noexcept {
        return (rare != nullptr) ? rare->animationValue : PropertyValue::Unset();
    }

    PropertyExpression ExpressionOrEmpty() const noexcept {
        return (rare != nullptr) ? rare->localExpression : PropertyExpression{};
    }

    PropertyValueSourceInfo SourceInfo() const noexcept {
        if (rare != nullptr) {
            return rare->sourceInfo;
        }
        PropertyValueSourceInfo info;
        info.rank = PackedRank();
        info.token.rank = info.rank;
        info.token.origin = PackedOrigin();
        info.hasExpression = HasExpression();
        info.isInherited = HasInherited();
        info.isAnimated = HasAnimation();
        info.isCoerced = Flag(IsCoercedBit);
        info.isCurrentValue = Flag(IsCurrentValueBit);
        info.revision = revision;
        return info;
    }

    Base::Result<void> SetSourceInfo(
        const PropertyValueSourceInfo& info,
        PropertySlab& slab) noexcept {
        PackOriginFlags(info);
        revision = static_cast<std::uint32_t>(info.revision);
        if (rare == nullptr &&
            info.token.ordinal == 0U &&
            info.token.origin <= OriginMask &&
            info.expressionKind == Meta::PropertyExpressionKind::Custom &&
            info.revision <= UINT32_MAX) {
            return {};
        }
        Base::Result<StoredValueRare*> block = EnsureRare(slab);
        if (!block) {
            return block.GetStatus();
        }
        block->sourceInfo = info;
        return {};
    }

    bool TryGetWinnerProvider(
        Meta::PropertyProviderToken& outToken,
        PropertyValue& outValue) const noexcept {
        if (HasInlineProvider()) {
            outToken = {PackedRank(), PackedOrigin(), 0U};
            outValue = effectiveValue;
            return true;
        }
        if (rare != nullptr) {
            if (const Meta::PropertyProviderContribution* win =
                    rare->baseProviders.Winner()) {
                outToken = win->token;
                outValue = win->value;
                return true;
            }
        }
        return false;
    }

    void SetInlineProviderContribution(
        Meta::PropertyProviderToken token,
        const PropertyValue& value) noexcept {
        packedFlags &= ~((RankMask << RankShift) | (OriginMask << OriginShift));
        packedFlags |= (static_cast<std::uint32_t>(token.rank) & RankMask)
            << RankShift;
        packedFlags |= (token.origin & OriginMask) << OriginShift;
        SetHasInlineProvider(true);
        effectiveValue = value;
    }

    bool RemoveProviderToken(Meta::PropertyProviderToken token) noexcept {
        if (HasInlineProvider()) {
            const Meta::PropertyProviderToken inlineToken{
                PackedRank(), PackedOrigin(), 0U};
            if (inlineToken == token) {
                SetHasInlineProvider(false);
                return true;
            }
            return false;
        }
        if (rare != nullptr && rare->baseProviders.Remove(token)) {
            DropRareIfUnused();
            return true;
        }
        return false;
    }

    std::uint32_t RemoveProviderOrigin(std::uint32_t origin) noexcept {
        if (HasInlineProvider()) {
            if (PackedOrigin() == origin) {
                SetHasInlineProvider(false);
                return 1U;
            }
            return 0U;
        }
        if (rare != nullptr) {
            const std::uint32_t removed = rare->baseProviders.RemoveOrigin(origin);
            if (removed != 0U) {
                DropRareIfUnused();
            }
            return removed;
        }
        return 0U;
    }

    void ClearAllProviders() noexcept {
        SetHasInlineProvider(false);
        if (rare != nullptr) {
            rare->baseProviders.Clear();
        }
    }

    const PropertyProviderSet& Providers() const noexcept {
        static const PropertyProviderSet empty;
        return (rare != nullptr) ? rare->baseProviders : empty;
    }

    PropertyProviderSet* ProvidersMutable() noexcept {
        return (rare != nullptr) ? &rare->baseProviders : nullptr;
    }

    Base::Result<PropertyProviderSet*> EnsureProviders(
        PropertySlab& slab) noexcept {
        Base::Result<StoredValueRare*> block = EnsureRare(slab);
        if (!block) {
            return block.GetStatus();
        }
        return &block->baseProviders;
    }

private:
    bool Flag(std::uint32_t bit) const noexcept {
        return (packedFlags & bit) != 0U;
    }

    void SetFlag(std::uint32_t bit, bool value) noexcept {
        if (value) {
            packedFlags |= bit;
        } else {
            packedFlags &= ~bit;
        }
    }

    Meta::PropertyValueRank PackedRank() const noexcept {
        return static_cast<Meta::PropertyValueRank>(
            (packedFlags >> RankShift) & RankMask);
    }

    std::uint32_t PackedOrigin() const noexcept {
        return (packedFlags >> OriginShift) & OriginMask;
    }

    void PackOriginFlags(const PropertyValueSourceInfo& info) noexcept {
        packedFlags &= ~((RankMask << RankShift) | (OriginMask << OriginShift));
        packedFlags |= (static_cast<std::uint32_t>(info.rank) & RankMask)
            << RankShift;
        packedFlags |= (info.token.origin & OriginMask) << OriginShift;
        SetFlag(IsCoercedBit, info.isCoerced);
        SetFlag(IsCurrentValueBit, info.isCurrentValue);
    }

    void CopyRareFrom(const StoredValueRare* source) {
        if (source == nullptr) {
            return;
        }
        rare = new (std::nothrow) StoredValueRare(*source);
        if (rare == nullptr) {
            Base::ReportOutOfMemory(
                sizeof(StoredValueRare),
                alignof(StoredValueRare),
                Base::MemoryTag::Object);
        }
    }
};

struct PropertyStoreEntry {
    MemberId key = 0U;
    StoredValueEntry value;

    PropertyStoreEntry() = default;
    PropertyStoreEntry(MemberId k, StoredValueEntry v)
        : key(k), value(std::move(v)) {}

    MemberId Key() const noexcept { return key; }
    StoredValueEntry& Value() noexcept { return value; }
    const StoredValueEntry& Value() const noexcept { return value; }
};

struct PropertyStore {
    struct Entries {
        static constexpr std::uint32_t InlineCapacity = 2U;

        std::uint64_t bloomMask_ = 0ULL;
        std::uint32_t size_ = 0U;
        std::uint32_t capacity_ = InlineCapacity;
        PropertyStoreEntry* data_ = inlineEntries_;
        PropertyStoreEntry inlineEntries_[InlineCapacity];

        static constexpr std::uint64_t KeyBloomBits(MemberId key) noexcept {
            return (1ULL << (key & 63ULL)) | (1ULL << ((key >> 6U) & 63ULL));
        }

        Entries() noexcept = default;
        ~Entries() noexcept {
            if (data_ != inlineEntries_) {
                delete[] data_;
                data_ = inlineEntries_;
            }
        }

        Entries(const Entries&) = delete;
        Entries& operator=(const Entries&) = delete;

        Entries(Entries&& other) noexcept {
            MoveFrom(std::move(other));
        }

        Entries& operator=(Entries&& other) noexcept {
            if (this == &other) return *this;
            Clear();
            MoveFrom(std::move(other));
            return *this;
        }

        std::uint32_t Size() const noexcept { return size_; }
        bool Empty() const noexcept { return size_ == 0U; }

        PropertyStoreEntry* begin() noexcept { return data_; }
        const PropertyStoreEntry* begin() const noexcept { return data_; }
        PropertyStoreEntry* end() noexcept { return data_ + size_; }
        const PropertyStoreEntry* end() const noexcept { return data_ + size_; }

        StoredValueEntry* Find(MemberId key) noexcept {
            const std::uint32_t idx = FindIndex(key);
            return idx != UINT32_MAX ? &data_[idx].value : nullptr;
        }

        const StoredValueEntry* Find(MemberId key) const noexcept {
            const std::uint32_t idx = FindIndex(key);
            return idx != UINT32_MAX ? &data_[idx].value : nullptr;
        }

        struct InsertResult {
            PropertyStoreEntry* entry = nullptr;
            bool inserted = false;
        };

        InsertResult Insert(MemberId key, StoredValueEntry entry) noexcept {
            const std::uint32_t idx = LowerBound(key);
            if (idx < size_ && data_[idx].key == key) {
                data_[idx].value = std::move(entry);
                return { &data_[idx], false };
            }
            if (size_ == capacity_) {
                if (!Grow()) {
                    return { nullptr, false };
                }
            }
            for (std::uint32_t j = size_; j > idx; --j) {
                data_[j] = std::move(data_[j - 1U]);
            }
            ++size_;
            bloomMask_ |= KeyBloomBits(key);
            PropertyStoreEntry* target = &data_[idx];
            target->key = key;
            target->value = std::move(entry);
            return { target, true };
        }

        void Erase(MemberId key) noexcept {
            const std::uint32_t idx = LowerBound(key);
            if (idx < size_ && data_[idx].key == key) {
                for (std::uint32_t j = idx + 1U; j < size_; ++j) {
                    data_[j - 1U] = std::move(data_[j]);
                }
                --size_;
                data_[size_] = PropertyStoreEntry{};
                std::uint64_t rebuilt = 0ULL;
                for (std::uint32_t i = 0U; i < size_; ++i) {
                    rebuilt |= KeyBloomBits(data_[i].key);
                }
                bloomMask_ = rebuilt;
            }
        }

        void Clear() noexcept {
            if (data_ != inlineEntries_) {
                delete[] data_;
                data_ = inlineEntries_;
                capacity_ = InlineCapacity;
            } else {
                for (std::uint32_t i = 0; i < size_; ++i) {
                    data_[i] = PropertyStoreEntry{};
                }
            }
            bloomMask_ = 0ULL;
            size_ = 0U;
        }

    private:
        void MoveFrom(Entries&& other) noexcept {
            if (other.data_ == other.inlineEntries_) {
                for (std::uint32_t i = 0U; i < other.size_; ++i) {
                    inlineEntries_[i] = std::move(other.inlineEntries_[i]);
                }
                data_ = inlineEntries_;
            } else {
                data_ = other.data_;
                other.data_ = other.inlineEntries_;
            }
            bloomMask_ = other.bloomMask_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.bloomMask_ = 0ULL;
            other.size_ = 0U;
            other.capacity_ = InlineCapacity;
        }

        std::uint32_t FindIndex(MemberId key) const noexcept {
            const std::uint64_t bits = KeyBloomBits(key);
            if ((bloomMask_ & bits) != bits) {
                return UINT32_MAX;
            }
            if (size_ <= 8U) {
                for (std::uint32_t i = 0U; i < size_; ++i) {
                    if (data_[i].key == key) {
                        return i;
                    }
                }
                return UINT32_MAX;
            }
            const std::uint32_t idx = LowerBound(key);
            return (idx < size_ && data_[idx].key == key) ? idx : UINT32_MAX;
        }

        std::uint32_t LowerBound(MemberId key) const noexcept {
            std::uint32_t lo = 0U;
            std::uint32_t hi = size_;
            while (lo < hi) {
                const std::uint32_t mid = lo + ((hi - lo) >> 1U);
                if (data_[mid].key < key) {
                    lo = mid + 1U;
                } else {
                    hi = mid;
                }
            }
            return lo;
        }

        bool Grow() noexcept {
            std::uint32_t newCap = capacity_ * 2U;
            auto* newData = new (std::nothrow) PropertyStoreEntry[newCap];
            if (newData == nullptr) return false;
            for (std::uint32_t i = 0; i < size_; ++i) {
                newData[i] = std::move(data_[i]);
            }
            if (data_ != inlineEntries_) {
                delete[] data_;
            }
            data_ = newData;
            capacity_ = newCap;
            return true;
        }
    } entries;

    // P2.4: same slab discipline as StoredValueRare. Allocated explicitly
    // from the owning Dispatcher's slab; operator delete routes heap and
    // slab blocks alike via the block header.
    static void* operator new(std::size_t size) noexcept {
        return PropertySlab::AllocateHeap(size);
    }
    static void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
        return PropertySlab::AllocateHeap(size);
    }
    static void* operator new(std::size_t size, std::align_val_t, const std::nothrow_t&) noexcept {
        return PropertySlab::AllocateHeap(size);
    }

    static void operator delete(void* ptr) noexcept {
        PropertySlab::ReleaseStore(ptr);
    }
    static void operator delete(void* ptr, const std::nothrow_t&) noexcept {
        PropertySlab::ReleaseStore(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t) noexcept {
        PropertySlab::ReleaseStore(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
        PropertySlab::ReleaseStore(ptr);
    }

    // Placement new for slab-provided memory. Required because the
    // class-specific operator new overloads above hide the global placement
    // new; the memory already carries its slab header.
    static void* operator new(std::size_t, void* memory) noexcept {
        return memory;
    }
    static void operator delete(void*, void*) noexcept {
    }
};


// ---- Moved from include/Aero/DependencyObject.hpp (Noesis-parity slimming) ----
// Change notification records and the mutation guard are engine details;
// the public header keeps only forward declarations. ChangeKind stays public
// (referenced by include/Aero/Resources.hpp).
struct ChangeHandlerRecord {
    DependencyPropertyHandle property;
    DependencyPropertyChangedEventHandler handler;
    bool active = false;
};

struct DependencyObjectRare {
    Base::Vector<ChangeHandlerRecord> changeHandlers;
    DependencyObject* inheritanceParent = nullptr;
    DependencyObject* firstInheritanceChild = nullptr;
    DependencyObject* nextInheritanceSibling = nullptr;
    DependencyObject* nextExplicitParent = nullptr;
    DependencyObject* distinctLogicalParent = nullptr;
    std::uint32_t changeHandlerNotificationDepth = 0U;

    bool IsEmpty() const noexcept {
        return changeHandlers.Empty() &&
            changeHandlerNotificationDepth == 0U &&
            inheritanceParent == nullptr &&
            firstInheritanceChild == nullptr &&
            nextInheritanceSibling == nullptr &&
            nextExplicitParent == nullptr &&
            distinctLogicalParent == nullptr;
    }
};

class DependencyMutationScope {
public:
    DependencyMutationScope() noexcept = default;
    DependencyMutationScope(DependencyMutationScope&& other) noexcept;
    DependencyMutationScope& operator=(DependencyMutationScope&& other) noexcept;
    ~DependencyMutationScope();

    DependencyMutationScope(const DependencyMutationScope&) = delete;
    DependencyMutationScope& operator=(const DependencyMutationScope&) = delete;

    void Release() noexcept;

private:
    friend class DependencyObject;

    DependencyMutationScope(
        DependencyObject* owner,
        DispatcherReentrancyGuard&& guard) noexcept;

    DependencyObject* owner_ = nullptr;
    DispatcherReentrancyGuard dispatcherGuard_;
};

static_assert(
    sizeof(StoredValueEntry) <= 96U,
    "Hot DP entry must stay single Value plus packed flags and a rare pointer");

} // namespace Aero
