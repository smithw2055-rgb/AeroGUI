#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/FrameworkElement.hpp>
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"

namespace Aero::Interactivity {

Behavior::~Behavior() {
    Detach();
}

Base::Result<void> Behavior::Attach(FrameworkElement& object) noexcept {
    if (associatedObject_ == &object) return {};
    if (associatedObject_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Behavior is already attached to another object");
    }
    associatedObject_ = &object;
    Base::Result<void> attached = OnAttached();
    if (!attached) associatedObject_ = nullptr;
    return attached;
}

void Behavior::Detach() noexcept {
    if (associatedObject_ == nullptr) return;
    OnDetaching();
    associatedObject_ = nullptr;
}

Base::Result<void> Behavior::AddAuthoredBinding(
    Meta::DependencyPropertyHandle property,
    Base::Ref<Aero::Data::Binding> binding) noexcept {
    if (!property.IsValid() || !binding) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Behavior authored Binding is invalid");
    }
    for (AuthoredBinding& existing : authoredBindings_) {
        if (existing.property == property) {
            existing.binding = std::move(binding);
            return {};
        }
    }
    return authoredBindings_.PushBack(
        {property, std::move(binding)});
}

Base::Result<void> Behavior::CopyAuthoredBindingsTo(
    Behavior& destination) const noexcept {
    for (const AuthoredBinding& binding : authoredBindings_) {
        Base::Result<void> copied = destination.AddAuthoredBinding(
            binding.property, binding.binding);
        if (!copied) return copied.GetStatus();
    }
    return {};
}

Base::Result<void> StyleBehaviorCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    return value ? items_.PushBack(std::move(value))
                 : Base::Result<void>(Base::Status::Failure(
                       Base::ErrorCode::InvalidArgument,
                       "Style behavior cannot be null"));
}

Base::Result<void> StyleTriggerCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    return value ? items_.PushBack(std::move(value))
                 : Base::Result<void>(Base::Status::Failure(
                       Base::ErrorCode::InvalidArgument,
                       "Style trigger cannot be null"));
}

void StyleInteraction::OnBehaviorsChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!object.PropertyRegistry().Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        Aero::Media::Visual::Access::ClearStyleBehaviorPrototypes(element));
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleBehaviorCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& behavior :
         static_cast<StyleBehaviorCollection&>(*value.AsObject()).GetItems()) {
        static_cast<void>(
            Aero::Media::Visual::Access::AddStyleBehaviorPrototype(
                element, behavior));
    }
}

void StyleInteraction::OnTriggersChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!object.PropertyRegistry().Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        Aero::Media::Visual::Access::ClearStyleTriggerPrototypes(element));
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleTriggerCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& trigger :
         static_cast<StyleTriggerCollection&>(*value.AsObject()).GetItems()) {
        static_cast<void>(
            Aero::Media::Visual::Access::AddStyleTriggerPrototype(
                element, trigger));
    }
}

} // namespace Aero::Interactivity
