#pragma once

// Style authoring bridge, compiled state and theme-style lookup.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>

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
        PPAAOutProperty{"PPAAOut"};
    // Compatibility setting retained for authored AeroGUI XAML. SDF text is
    // the renderer default, so this marker never switches back to grayscale.
    inline static constexpr Meta::AttachedPropertyRef<Element, Base::String>
        PPAAModeProperty{"PPAAMode"};
    inline static constexpr Meta::AttachedPropertyRef<Element, bool>
        IsFocusEngagedProperty{"IsFocusEngaged"};
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

#include <Aero/Styling.hpp>
#include <Aero/Style.hpp>

namespace Aero {

struct StyleSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StyleTriggerSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TriggerPlan {
    DependencyPropertyHandle property;
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
    bool IsBindingTrigger() const noexcept { return static_cast<bool>(binding); }
    Base::Vector<StyleTriggerSetter> setters;
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
};

struct Style::Impl {
    static Base::Result<void> Seal(
        Style& style,
        const void* properties) noexcept;
    static Base::Span<const StyleSetter> RuntimeSetters(
        const Style& style) noexcept;
    static Base::Span<const TriggerPlan> RuntimeTriggers(
        const Style& style) noexcept;

    Impl() noexcept
        : authoredSetters(&Base::GetDefaultAllocator()),
          authoredTriggers(&Base::GetDefaultAllocator()),
          setters(&Base::GetDefaultAllocator()),
          triggers(&Base::GetDefaultAllocator()) {}
    Impl(Impl&&) noexcept = default;
    Impl& operator=(Impl&&) noexcept = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

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

namespace Aero::GuiPrivate::Detail {
using StylePrivate = ::Aero::Style::Impl;
}

#include "gui/GuiPrivate.hpp"


namespace Aero::GuiPrivate::Detail {

using namespace Aero::Meta;
using namespace Aero::Threading;

class AERO_API StyleEngine {
public:
    using TriggerActionHandler = Base::Result<void>(*)(
        DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    explicit StyleEngine(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept
        : providerSession_(values),
          values_(&providerSession_),
          properties_(&properties),
          applications_(),
          propertyChangedHandler_(
              this, &StyleEngine::OnPropertyChanged) {}
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
        void* context) noexcept {
        triggerActionHandler_ = handler;
        triggerActionContext_ = context;
    }
    const Base::Status& LastActionStatus() const noexcept {
        return lastActionStatus_;
    }

private:
    ::Aero::GuiPrivate::Detail::StyleProviderSession providerSession_;
    ::Aero::GuiPrivate::Detail::StyleProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    struct Application {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
        Base::Vector<std::uint8_t> triggerStates;
        Base::Vector<std::uint8_t> bindingTriggerStates;
        Base::Vector<std::uint8_t> bindingTriggerKnown;
    };
    Base::Vector<Application> applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Dispatcher* dispatcher_ = nullptr;
    DispatcherFrameHookHandle triggerPhaseHook_;
    Base::Vector<DependencyObject*>
        pendingTriggerEvaluations_;
    TriggerActionHandler triggerActionHandler_ = nullptr;
    void* triggerActionContext_ = nullptr;
    Base::Status lastActionStatus_;

    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> EvaluateTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> ExecuteTriggerActions(
        DependencyObject& object,
        Base::Span<const Base::Ref<Base::Object>>
            actions) noexcept;
    Base::Result<void> EnsureTriggerPhaseHook(
        DependencyObject& object) noexcept;
    Base::Result<void> QueueTriggerEvaluation(
        DependencyObject& object) noexcept;
    void RemovePendingTriggerEvaluation(
        DependencyObject& object) noexcept;
    Base::Result<std::uint32_t>
        FlushPendingTriggerEvaluations() noexcept;
    static void TriggerPhaseHook(void* context) noexcept;
    Base::Result<void> ClearTriggerSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};


} // namespace Aero::GuiPrivate::Detail

// Resource-assignment helpers used by style and markup application.

#include <Aero/Resources.hpp>

#include <utility>

namespace Aero::GuiPrivate::Detail {

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

} // namespace Aero::GuiPrivate::Detail
