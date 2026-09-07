#pragma once

// Style program state, Seal/Apply statics, and resource-assignment helpers
// used by style and markup application. Theme-compat Element/RichText live in
// gui/meta/MetadataState.hpp.

#include <Aero/Value.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/DependencyProperty.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"

#include <cstdint>

#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Style.hpp>

namespace Aero {

struct StyleSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StyleState {
    static Base::Result<void> Seal(
        Style& style,
        const Meta::DependencyPropertyRegistry& properties) noexcept;
    static Base::Span<const StyleSetter> RuntimeSetters(
        const Style& style) noexcept;
    static Base::Span<const TriggerPlan> RuntimeTriggers(
        const Style& style) noexcept;
    // Per-instance setter application (StyleEngine tracks applications /
    // triggers; these only push/clear style-source values + EventSetters).
    static Base::Result<void> ApplySetters(
        const Style& style,
        DependencyObject& object,
        StyleProviderSession& values) noexcept;
    static Base::Result<void> ClearSetters(
        const Style& style,
        DependencyObject& object,
        StyleProviderSession& values) noexcept;

    StyleState() noexcept
        : authoredSetters(&Base::GetDefaultAllocator()),
          authoredTriggers(&Base::GetDefaultAllocator()),
          setters(&Base::GetDefaultAllocator()),
          triggers(&Base::GetDefaultAllocator()) {}
    StyleState(StyleState&&) noexcept = default;
    StyleState& operator=(StyleState&&) noexcept = default;
    StyleState(const StyleState&) = delete;
    StyleState& operator=(const StyleState&) = delete;

    TypeId TargetType() const noexcept { return targetType; }
    Base::Span<const StyleSetter> Setters() const noexcept {
        return {setters.Data(), setters.Size()};
    }
    Base::Span<const TriggerPlan> Triggers() const noexcept {
        return {triggers.Data(), triggers.Size()};
    }
    Base::Result<void> Freeze(
        TypeId valueTargetType,
        Base::Vector<StyleSetter>&& valueSetters,
        Base::Vector<TriggerPlan>&& valueTriggers) noexcept;
    Base::Result<void> AddAuthoredSetter(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Base::Result<void> AddAuthoredTrigger(
        TriggerPlan trigger) noexcept;
    void ClearAuthored() noexcept;
    void Reset() noexcept;

    TypeId targetType = InvalidTypeId;
    Base::Vector<StyleSetter> authoredSetters;
    Base::Vector<TriggerPlan> authoredTriggers;
    Base::Vector<StyleSetter> setters;
    Base::Vector<TriggerPlan> triggers;
    bool frozen = false;
};

} // namespace Aero


// Resource-assignment helpers used by style and markup application.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Resources.hpp>

#include <new>
#include <utility>

namespace Aero {

inline ResourceDictionary& EnsureOwnedResources(
    ResourceDictionary*& slot) noexcept {
    if (slot != nullptr) {
        return *slot;
    }
    slot = new (std::nothrow) ResourceDictionary();
    if (slot == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(ResourceDictionary),
            alignof(ResourceDictionary),
            Base::MemoryTag::Object);
        static ResourceDictionary fallback;
        return fallback;
    }
    return *slot;
}

inline Base::Result<void> AssignResourceDictionary(
    ResourceDictionary& target,
    Base::Ref<ResourceDictionary> source,
    const char* alreadyAssignedMessage) noexcept {
    if (!source) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Resources expects a non-null ResourceDictionary");
    }
    if (target.Size() != 0U ||
        target.MergedDictionaryCount() != 0U ||
        !target.GetSource().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            alreadyAssignedMessage);
    }
    target = std::move(*source);
    return {};
}

} // namespace Aero
