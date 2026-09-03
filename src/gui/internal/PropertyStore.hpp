#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstdint>
#include <new>
#include <utility>

namespace Aero {

struct StoredValueRarePool {
    static constexpr std::size_t kMaxPooled = 256U;
    struct Node { Node* next; };
    Node* head = nullptr;
    std::size_t count = 0U;

    ~StoredValueRarePool() noexcept {
        while (head != nullptr) {
            Node* next = head->next;
            ::operator delete(head);
            head = next;
        }
        count = 0U;
    }

    void* Allocate(std::size_t size) noexcept {
        if (head != nullptr) {
            Node* node = head;
            head = node->next;
            --count;
            return node;
        }
        return ::operator new(size, std::nothrow);
    }

    void Deallocate(void* ptr) noexcept {
        if (ptr == nullptr) return;
        if (count < kMaxPooled) {
            auto* node = static_cast<Node*>(ptr);
            node->next = head;
            head = node;
            ++count;
        } else {
            ::operator delete(ptr);
        }
    }
};

inline thread_local StoredValueRarePool g_storedValueRarePool;

// Uncommon DP data. Allocated when a local/inherited/provider/expression/
// animation/current/queue payload exists. GetValue of a simple stored
// local/style entry reads only StoredValueEntry::effectiveValue.
struct StoredValueRare {
    PropertyExpression localExpression{};
    PropertyValue localValue{};
    PropertyValue inheritedValue{};
    PropertyValue currentValue{};
    PropertyValue animationValue{};
    PropertyProviderSet baseProviders;
    PropertyValueSourceInfo sourceInfo{};
    std::uint64_t queueSequence = 0U;

    static void* operator new(std::size_t size) noexcept {
        return g_storedValueRarePool.Allocate(size);
    }
    static void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
        return g_storedValueRarePool.Allocate(size);
    }
    static void* operator new(std::size_t size, std::align_val_t, const std::nothrow_t&) noexcept {
        return g_storedValueRarePool.Allocate(size);
    }

    static void operator delete(void* ptr) noexcept {
        g_storedValueRarePool.Deallocate(ptr);
    }
    static void operator delete(void* ptr, const std::nothrow_t&) noexcept {
        g_storedValueRarePool.Deallocate(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t) noexcept {
        g_storedValueRarePool.Deallocate(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
        g_storedValueRarePool.Deallocate(ptr);
    }
};

// Wave 4 hot entry (HashMap value; MemberId is the map key):
//   PropertyValue effectiveValue
//   StoredValueRare* rare
//   std::uint32_t packedFlags   (origin/rank/has* bits)
// Local / inherited / animation / expression / providers live in rare.
// packedFlags:
//   [0] HasLocal  [1] HasCurrent  [2] HasExpression
//   [3] HasInherited  [4] HasAnimation  [5] Queued
//   [6] IsCoerced  [7] IsCurrentValue
//   [8..15] PropertyValueRank  [16..31] origin (truncated to 16 bits)
// Approximate sizeof: one Value + pointer + flags vs three Values plus
// provider set plus sourceInfo before this wave.
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
    StoredValueRare* rare = nullptr;
    std::uint32_t packedFlags = 0U;

    StoredValueEntry() = default;

    StoredValueEntry(const StoredValueEntry& other)
        : effectiveValue(other.effectiveValue),
          rare(nullptr),
          packedFlags(other.packedFlags) {
        CopyRareFrom(other.rare);
    }

    StoredValueEntry(StoredValueEntry&& other) noexcept
        : effectiveValue(std::move(other.effectiveValue)),
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

    Base::Result<StoredValueRare*> EnsureRare() noexcept {
        if (rare != nullptr) {
            return rare;
        }
        rare = new (std::nothrow) StoredValueRare();
        if (rare == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Dependency property uncommon-value block allocation failed");
        }
        return rare;
    }

    void DropRareIfUnused() noexcept {
        if (rare == nullptr) {
            return;
        }
        const PropertyValueSourceInfo& source = rare->sourceInfo;
        if (HasLocal() || HasCurrent() || HasExpression() || HasInherited() ||
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
        return (rare != nullptr && HasLocal())
            ? rare->localValue
            : PropertyValue::Unset();
    }

    Base::Result<void> SetLocalValue(const PropertyValue& value) noexcept {
        Base::Result<StoredValueRare*> block = EnsureRare();
        if (!block) {
            return block.GetStatus();
        }
        block.Value()->localValue = value;
        SetHasLocal(true);
        return {};
    }

    void ClearLocal() noexcept {
        SetHasLocal(false);
        if (rare != nullptr) {
            rare->localValue = PropertyValue::Unset();
        }
        DropRareIfUnused();
    }

    PropertyValue InheritedValueOrUnset() const noexcept {
        return (rare != nullptr && HasInherited())
            ? rare->inheritedValue
            : PropertyValue::Unset();
    }

    Base::Result<void> SetInheritedValue(const PropertyValue& value) noexcept {
        Base::Result<StoredValueRare*> block = EnsureRare();
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
        const PropertyValueSourceInfo& info) noexcept {
        PackOriginFlags(info);
        Base::Result<StoredValueRare*> block = EnsureRare();
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

    Base::Result<PropertyProviderSet*> EnsureProviders() noexcept {
        Base::Result<StoredValueRare*> block = EnsureRare();
        if (!block) {
            return block.GetStatus();
        }
        return &block.Value()->baseProviders;
    }

    std::uint64_t QueueSequence() const noexcept {
        return (rare != nullptr) ? rare->queueSequence : 0U;
    }

    Base::Result<void> SetQueueSequence(std::uint64_t sequence) noexcept {
        Base::Result<StoredValueRare*> block = EnsureRare();
        if (!block) {
            return block.GetStatus();
        }
        block.Value()->queueSequence = sequence;
        return {};
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

struct PropertyStorePool {
    static constexpr std::size_t kMaxPooled = 256U;
    struct Node { Node* next; };
    Node* head = nullptr;
    std::size_t count = 0U;

    ~PropertyStorePool() noexcept {
        while (head != nullptr) {
            Node* next = head->next;
            ::operator delete(head);
            head = next;
        }
        count = 0U;
    }

    void* Allocate(std::size_t size) noexcept {
        if (head != nullptr) {
            Node* node = head;
            head = node->next;
            --count;
            return node;
        }
        return ::operator new(size, std::nothrow);
    }

    void Deallocate(void* ptr) noexcept {
        if (ptr == nullptr) return;
        if (count < kMaxPooled) {
            auto* node = static_cast<Node*>(ptr);
            node->next = head;
            head = node;
            ++count;
        } else {
            ::operator delete(ptr);
        }
    }
};

inline thread_local PropertyStorePool g_propertyStorePool;

struct PropertyStore {
    Base::HashMap<MemberId, StoredValueEntry> entries;

    static void* operator new(std::size_t size) noexcept {
        return g_propertyStorePool.Allocate(size);
    }
    static void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
        return g_propertyStorePool.Allocate(size);
    }
    static void* operator new(std::size_t size, std::align_val_t, const std::nothrow_t&) noexcept {
        return g_propertyStorePool.Allocate(size);
    }

    static void operator delete(void* ptr) noexcept {
        g_propertyStorePool.Deallocate(ptr);
    }
    static void operator delete(void* ptr, const std::nothrow_t&) noexcept {
        g_propertyStorePool.Deallocate(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t) noexcept {
        g_propertyStorePool.Deallocate(ptr);
    }
    static void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
        g_propertyStorePool.Deallocate(ptr);
    }
};

static_assert(
    sizeof(StoredValueEntry) <= 128U,
    "Hot DP entry must stay one Value plus packed flags and a rare pointer");

} // namespace Aero
