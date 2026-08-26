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

// Uncommon DP data: allocated only when an expression, animation, or
// SetCurrentValue is live. Typical local/style entries never pay this.
struct StoredValueRare {
    PropertyExpression localExpression{};
    PropertyValue currentValue{};
    PropertyValue animationValue{};
};

// Opaque per-object DP store. Defined only in src; the installed
// DependencyObject header keeps a void* handle so the packed entry layout
// is not part of the public ABI.
//
// Hot entry keeps local, inherited, and effective PropertyValues plus the
// provider set. The unused base snapshot is not stored; Recompute writes
// effectiveValue directly. Expression / animation / current live in rare.
struct StoredValueEntry {
    PropertyProviderSet baseProviders;
    PropertyValue localValue;
    PropertyValue inheritedValue;
    PropertyValue effectiveValue;
    PropertyValueSourceInfo sourceInfo;
    std::uint64_t queueSequence = 0U;
    StoredValueRare* rare = nullptr;
    bool hasLocal = false;
    bool hasCurrent = false;
    bool hasExpression = false;
    bool hasInherited = false;
    bool hasAnimation = false;
    bool queued = false;

    StoredValueEntry() = default;

    StoredValueEntry(const StoredValueEntry& other)
        : baseProviders(other.baseProviders),
          localValue(other.localValue),
          inheritedValue(other.inheritedValue),
          effectiveValue(other.effectiveValue),
          sourceInfo(other.sourceInfo),
          queueSequence(other.queueSequence),
          rare(nullptr),
          hasLocal(other.hasLocal),
          hasCurrent(other.hasCurrent),
          hasExpression(other.hasExpression),
          hasInherited(other.hasInherited),
          hasAnimation(other.hasAnimation),
          queued(other.queued) {
        CopyRareFrom(other.rare);
    }

    StoredValueEntry(StoredValueEntry&& other) noexcept
        : baseProviders(other.baseProviders),
          localValue(std::move(other.localValue)),
          inheritedValue(std::move(other.inheritedValue)),
          effectiveValue(std::move(other.effectiveValue)),
          sourceInfo(other.sourceInfo),
          queueSequence(other.queueSequence),
          rare(other.rare),
          hasLocal(other.hasLocal),
          hasCurrent(other.hasCurrent),
          hasExpression(other.hasExpression),
          hasInherited(other.hasInherited),
          hasAnimation(other.hasAnimation),
          queued(other.queued) {
        other.rare = nullptr;
        other.hasLocal = false;
        other.hasCurrent = false;
        other.hasExpression = false;
        other.hasInherited = false;
        other.hasAnimation = false;
        other.queued = false;
        other.queueSequence = 0U;
        other.sourceInfo = {};
    }

    ~StoredValueEntry() noexcept {
        delete rare;
        rare = nullptr;
    }

    StoredValueEntry& operator=(const StoredValueEntry& other) {
        if (this == &other) {
            return *this;
        }
        baseProviders = other.baseProviders;
        localValue = other.localValue;
        inheritedValue = other.inheritedValue;
        effectiveValue = other.effectiveValue;
        sourceInfo = other.sourceInfo;
        queueSequence = other.queueSequence;
        hasLocal = other.hasLocal;
        hasCurrent = other.hasCurrent;
        hasExpression = other.hasExpression;
        hasInherited = other.hasInherited;
        hasAnimation = other.hasAnimation;
        queued = other.queued;
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
        baseProviders = other.baseProviders;
        localValue = std::move(other.localValue);
        inheritedValue = std::move(other.inheritedValue);
        effectiveValue = std::move(other.effectiveValue);
        sourceInfo = other.sourceInfo;
        queueSequence = other.queueSequence;
        rare = other.rare;
        hasLocal = other.hasLocal;
        hasCurrent = other.hasCurrent;
        hasExpression = other.hasExpression;
        hasInherited = other.hasInherited;
        hasAnimation = other.hasAnimation;
        queued = other.queued;
        other.rare = nullptr;
        other.hasLocal = false;
        other.hasCurrent = false;
        other.hasExpression = false;
        other.hasInherited = false;
        other.hasAnimation = false;
        other.queued = false;
        other.queueSequence = 0U;
        other.sourceInfo = {};
        return *this;
    }

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
        if (hasExpression || hasAnimation || hasCurrent) {
            return;
        }
        delete rare;
        rare = nullptr;
    }

    void ClearCurrent() noexcept {
        hasCurrent = false;
        if (rare != nullptr) {
            rare->currentValue = PropertyValue::Unset();
        }
        DropRareIfUnused();
    }

    void ClearAnimation() noexcept {
        hasAnimation = false;
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

private:
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
};

} // namespace Aero
