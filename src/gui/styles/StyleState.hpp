#pragma once

// Style authoring bridge, compiled state and theme-style lookup.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/TextProperties.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/triggers/TriggerPlan.hpp"

#include <cstdint>

namespace Aero {

// Private compatibility owners used by the built-in theme schema. They are
// registered for XAML compatibility but are not C++ authoring APIs.
class Element : public Base::Object {
    AERO_DECLARE_TYPE(Element, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Meta::AttachedPropertyRef<Element, double>
        PPAAInProperty{"PPAAIn"};
    inline static constexpr Meta::AttachedPropertyRef<Element, double>
        PPAAOutProperty{"PPAAOut"};
    // Compatibility setting retained for authored AeroGUI XAML. SDF text is
    // the renderer default, so this marker never switches back to grayscale.
    inline static constexpr Meta::AttachedPropertyRef<Element, Base::String>
        PPAAModeProperty{"PPAAMode"};
    inline static constexpr Meta::AttachedPropertyRef<Element, bool>
        IsFocusEngagedProperty{"IsFocusEngaged"};
    inline static constexpr Meta::AttachedPropertyRef<Element, BlendMode>
        BlendingModeProperty{"BlendingMode"};
    static void OnBlendingModeChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    // XAML compatibility forwarder: aero:Element.Transform3D writes
    // UIElement::Transform3DProperty. Render/hit read the UIElement DP.
    inline static constexpr Meta::AttachedPropertyRef<
        Element, Base::Ref<Media::Transform3D>>
        Transform3DProperty{"Transform3D"};
    static void OnTransform3DChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

class RichText : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        RichText, Base::Object, "urn:aero", "RichText")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Meta::AttachedPropertyRef<
        RichText, Base::String>
        TextProperty{"Text"};
    static void OnTextChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

} // namespace Aero

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
        const void* properties) noexcept;
    static Base::Span<const StyleSetter> RuntimeSetters(
        const Style& style) noexcept;
    static Base::Span<const TriggerPlan> RuntimeTriggers(
        const Style& style) noexcept;

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

namespace Aero {
using StylePrivate = ::Aero::StyleState;
}

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
