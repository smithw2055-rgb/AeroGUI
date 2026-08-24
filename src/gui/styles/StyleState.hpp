#pragma once
#include "gui/core/Facet.hpp"

// Style authoring bridge, compiled state and theme-style lookup.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <Aero/Layout.hpp>
#include <Aero/DependencyProperty.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingState.hpp"
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
    inline static constexpr Meta::AttachedPropertyRef<
        Element, Base::Ref<Media::CompositeTransform3D>>
        Transform3DProperty{"Transform3D"};
};

class TextProperties : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TextProperties, Base::Object, "urn:aero", "Text")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, std::uint32_t>
        PasswordLengthProperty{"PasswordLength"};
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, Base::String>
        PlaceholderProperty{"Placeholder"};
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, Value>
        StrokeProperty{"Stroke"};
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, double>
        StrokeThicknessProperty{"StrokeThickness"};

    static void OnCompatibilityPropertyChanged(
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



namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;

class TriggerEngine;

// Per-object style-application record. Lifted to namespace scope so the
// dedicated TriggerEngine can reference the same vector as StyleEngine.
struct StyleApplication {
    DependencyObject* object = nullptr;
    const Style* style = nullptr;
    Base::Vector<std::uint8_t> triggerStates;
    Base::Vector<std::uint8_t> bindingTriggerStates;
    Base::Vector<std::uint8_t> bindingTriggerKnown;
};

class StyleEngine : public Core::Facet {
public:
    using TriggerActionHandler = Base::Result<void>(*)(
        DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    explicit StyleEngine(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept;
    ~StyleEngine() noexcept;

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SetBindingTriggerState(
        DependencyObject& object,
        const Style& style,
        std::uint32_t triggerIndex,
        bool active) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;
    const Style* AppliedStyle(
        const DependencyObject& object)
        const noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept;
    const Base::Status& LastActionStatus() const noexcept;

private:
    ::Aero::StyleProviderSession providerSession_;
    ::Aero::StyleProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    Base::Vector<StyleApplication> applications_;
    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;

    // Owned trigger-evaluation engine. StyleEngine delegates all trigger
    // state, subscription and deferred-evaluation behavior to it.
    TriggerEngine* triggerEngine_ = nullptr;
};


} // namespace Aero

// Resource-assignment helpers used by style and markup application.

#include <Aero/Resources.hpp>

#include <utility>

namespace Aero {

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
