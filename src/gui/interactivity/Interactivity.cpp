#include <Aero/Interactivity/Behavior.hpp>
#include "gui/meta/MetadataState.hpp"
#include <Aero/FrameworkElement.hpp>
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/PropertyEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
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
    OnAttached();
    return {};
}

void Behavior::Detach() noexcept {
    if (associatedObject_ == nullptr) return;
    OnDetaching();
    associatedObject_ = nullptr;
}

void Behavior::AddAuthoredBinding(
    Meta::DependencyPropertyHandle property,
    Base::Ref<Aero::Data::Binding> binding) noexcept {
    if (!property.IsValid() || !binding) { AERO_ASSERT(false); return; }
    for (AuthoredBinding& existing : authoredBindings_) {
        if (existing.property == property) {
            existing.binding = std::move(binding);
            return;
        }
    }
    Base::Result<void> pushed = authoredBindings_.PushBack(
        {property, std::move(binding)});
    if (!pushed) { AERO_ASSERT(false); return; }
}

void Behavior::CopyAuthoredBindingsTo(
    Behavior& destination) const noexcept {
    for (const AuthoredBinding& binding : authoredBindings_) {
        destination.AddAuthoredBinding(
            binding.property, binding.binding);
    }
}

Base::Result<Base::Ref<Behavior>> Behavior::ClonePrototype(
    const Behavior& prototype,
    Meta::Registry& metadata) noexcept {
    Base::Result<Base::Ref<Base::Object>> created =
        metadata.CreateObject(prototype.RuntimeType());
    if (!created) return created.GetStatus();
    if (!created.Value() ||
        !metadata.Types().IsDerivedFrom(
            created.Value()->RuntimeType(),
            Behavior::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Behavior factory returned an incompatible object");
    }
    Base::Ref<Behavior> clone =
        Base::Ref<Behavior>::FromBorrowed(
            *static_cast<Behavior*>(created.Value().Get()));
    for (const Meta::DependencyProperty& property :
         AeroGuiInternal::PropertyRegistry(prototype).Properties()) {
        if (property.MetadataFor(prototype.RuntimeType()) == nullptr ||
            property.MetadataFor(clone->RuntimeType()) == nullptr) {
            continue;
        }
        Meta::PropertyValue local =
            prototype.ReadLocalValue(property.Handle());
        if (local.IsUnset()) continue;
        clone->SetValue(property.Handle(), local);
    }
    prototype.CopyAuthoredBindingsTo(*clone);
    return clone;
}

void StyleBehaviorCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    Base::Result<void> pushed = items_.PushBack(std::move(value));
    if (!pushed) { AERO_ASSERT(false); return; }
}

void StyleTriggerCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    Base::Result<void> pushed = items_.PushBack(std::move(value));
    if (!pushed) { AERO_ASSERT(false); return; }
}

void StyleInteraction::OnBehaviorsChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!AeroGuiInternal::PropertyRegistry(object).Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        AeroGuiInternal::ClearStyleBehaviorPrototypes(element));
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleBehaviorCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& behavior :
         static_cast<StyleBehaviorCollection&>(*value.AsObject()).GetItems()) {
        AeroGuiInternal::AddStyleBehaviorPrototype(
            element, behavior);
    }
}

void StyleInteraction::OnTriggersChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!AeroGuiInternal::PropertyRegistry(object).Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        AeroGuiInternal::ClearStyleTriggerPrototypes(element));
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleTriggerCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& trigger :
         static_cast<StyleTriggerCollection&>(*value.AsObject()).GetItems()) {
        AeroGuiInternal::AddStyleTriggerPrototype(
            element, trigger);
    }
}

} // namespace Aero::Interactivity
