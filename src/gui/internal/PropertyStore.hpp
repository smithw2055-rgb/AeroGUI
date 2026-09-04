#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/PropertySlab.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace Aero {

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

// Wave 4 hot entry (HashMap value; MemberId is the map key):
//   PropertyValue effectiveValue
//   PropertyValue inlineLocal      (P2.1: simple Local values, no heap)
//   StoredValueRare* rare
//   std::uint32_t packedFlags   (origin/rank/has* bits)
// Inherited / animation / expression / providers live in rare. Local values
// live inline; rare is only allocated for the uncommon payloads above.
// packedFlags:
//   [0] HasLocal  [1] HasCurrent  [2] HasExpression
//   [3] HasInherited  [4] HasAnimation  [5] Queued
//   [6] IsCoerced  [7] IsCurrentValue
//   [8..15] PropertyValueRank  [16..31] origin (truncated to 16 bits)
// Approximate sizeof: two Values + pointer + flags vs three Values plus
// provider set plus sourceInfo before this wave. The second Value buys
// allocation-free Local reads/writes for the common case.
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
        RankShift = 8U,
        RankMask = 0xFFU,
        OriginShift = 16U,
        OriginMask = 0xFFFFU
    };

    PropertyValue effectiveValue;
    // P2.1: 90% of properties hold a simple Local value. Kept inline so
    // Set/Get/ClearLocal never allocate. Valid only when HasLocalBit is set;
    // default-constructed Value is Unset.
    PropertyValue inlineLocal;
    StoredValueRare* rare = nullptr;
    std::uint32_t packedFlags = 0U;

    StoredValueEntry() = default;

    StoredValueEntry(const StoredValueEntry& other)
        : effectiveValue(other.effectiveValue),
          inlineLocal(other.inlineLocal),
          rare(nullptr),
          packedFlags(other.packedFlags) {
        CopyRareFrom(other.rare);
    }

    StoredValueEntry(StoredValueEntry&& other) noexcept
        : effectiveValue(std::move(other.effectiveValue)),
          inlineLocal(std::move(other.inlineLocal)),
          rare(other.rare),
          packedFlags(other.packedFlags) {
        other.rare = nullptr;
        other.packedFlags = 0U;
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
        inlineLocal = other.inlineLocal;
        packedFlags = other.packedFlags;
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
        inlineLocal = std::move(other.inlineLocal);
        packedFlags = other.packedFlags;
        rare = other.rare;
        other.rare = nullptr;
        other.packedFlags = 0U;
        return *this;
    }

    bool HasLocal() const noexcept { return Flag(HasLocalBit); }
    bool HasCurrent() const noexcept { return Flag(HasCurrentBit); }
    bool HasExpression() const noexcept { return Flag(HasExpressionBit); }
    bool HasInherited() const noexcept { return Flag(HasInheritedBit); }
    bool HasAnimation() const noexcept { return Flag(HasAnimationBit); }
    bool Queued() const noexcept { return Flag(QueuedBit); }

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
        rare = new (memory) StoredValueRare();
        return rare;
    }

    void DropRareIfUnused() noexcept {
        if (rare == nullptr) {
            return;
        }
        const PropertyValueSourceInfo& source = rare->sourceInfo;
        // NOTE: HasLocal is intentionally absent: Local values live inline
        // (P2.1) and never pin the rare block.
        if (HasCurrent() || HasExpression() || HasInherited() ||
            HasAnimation() || Queued() ||
            !rare->baseProviders.GetIsEmpty() ||
            source.revision != 0U ||
            source.token.ordinal != 0U ||
            source.expressionKind != Meta::PropertyExpressionKind::Custom) {
            return;
        }
        delete rare;
        rare = nullptr;
    }

    PropertyValue LocalValueOrUnset() const noexcept {
        return HasLocal() ? inlineLocal : PropertyValue::Unset();
    }

    // P2.1: infallible. A simple Local set never allocates.
    void SetLocalValue(const PropertyValue& value) noexcept {
        inlineLocal = value;
        SetHasLocal(true);
    }

    void ClearLocal() noexcept {
        SetHasLocal(false);
        inlineLocal = PropertyValue::Unset();
        DropRareIfUnused();
    }

    PropertyValue InheritedValueOrUnset() const noexcept {
        return (rare != nullptr && HasInherited())
            ? rare->inheritedValue
            : PropertyValue::Unset();
    }

    Base::Result<void> SetInheritedValue(
        const PropertyValue& value,
        PropertySlab& slab) noexcept {
        Base::Result<StoredValueRare*> block = EnsureRare(slab);
        if (!block) {
            return block.GetStatus();
        }
        block.Value()->inheritedValue = value;
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
        return info;
    }

    Base::Result<void> SetSourceInfo(
        const PropertyValueSourceInfo& info,
        PropertySlab& slab) noexcept {
        PackOriginFlags(info);
        Base::Result<StoredValueRare*> block = EnsureRare(slab);
        if (!block) {
            return block.GetStatus();
        }
        block.Value()->sourceInfo = info;
        return {};
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
        return &block.Value()->baseProviders;
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

struct PropertyStore {
    Base::HashMap<MemberId, StoredValueEntry> entries;

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

static_assert(
    sizeof(StoredValueEntry) <= 192U,
    "Hot DP entry must stay two Values plus packed flags and a rare pointer");

} // namespace Aero
