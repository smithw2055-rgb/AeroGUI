#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include <Aero/Interactivity/InteractionTriggers.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

Base::Result<InteractivityEngine::InteractionTriggerProperty>
InteractivityEngine::ResolveInteractionTriggerProperty(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        Base::Object* sourceObject = ResolveAuthoredBindingSource(
            binding, owner, nullptr, names, nullptr);
        if (sourceObject == nullptr || metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Interaction Trigger Binding source was not found");
        }
        const Base::StringView path = binding.GetPath().GetPath();
        if (path.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Interaction Trigger Binding requires a property path");
        }

        InteractionTriggerProperty resolved;
        resolved.source = sourceObject;
        if (metadata->Types().IsDerivedFrom(
                sourceObject->RuntimeType(),
                ::Aero::DependencyObject::StaticTypeId())) {
            resolved.dependencySource =
                static_cast<::Aero::DependencyObject*>(sourceObject);
            const Meta::DependencyProperty* property =
                Aero::MetadataPrivate::DependencyProperties(
                    *metadata).Find(sourceObject->RuntimeType(), path);
            if (property != nullptr) {
                resolved.dependencyProperty = property->Handle();
                return resolved;
            }
            // CLR properties on a DependencyObject (or a non-DP path) fall
            // through to metadata lookup below.
            resolved.dependencySource = nullptr;
        }

        Base::StringView rootPath = path;
        for (std::uint32_t index = 0U; index < path.SizeBytes(); ++index) {
            if (path[index] == '.') {
                rootPath = path.Substr(0U, index);
                break;
            }
        }
        const Meta::PropertyInfo* property = metadata->Types().FindProperty(
            sourceObject->RuntimeType(), rootPath, true);
        if (property == nullptr ||
            !metadata->CanReadProperty(property->Id())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Interaction Trigger Binding property was not found");
        }
        resolved.metadataProperty = property->Id();
        return resolved;
    }

Base::Result<bool> InteractivityEngine::EvaluateInteractionDataTrigger(
        InteractionDataTriggerState& state) noexcept {
        if (state.trigger == nullptr || state.owner == nullptr ||
            !state.trigger->GetBinding()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Interaction DataTrigger state is invalid");
        }
        Base::Result<Meta::PropertyValue> actual =
            EvaluateAuthoredBinding(
                *state.trigger->GetBinding(),
                *state.owner,
                nullptr,
                state.names,
                nullptr);
        if (!actual) return actual.GetStatus();
        Base::Result<bool> matches = EvaluateTriggerComparison(
            actual.Value(),
            state.trigger->GetAuthoredValue(),
            state.trigger->GetComparison());
        if (!matches) return matches.GetStatus();
        const bool active = matches.Value();
        if (active == state.active) return false;
        Base::Result<bool> allowed = ConditionBehaviorsAllowExecution(
            state.trigger->GetBehaviors(), *state.owner, state.names);
        if (!allowed) return allowed.GetStatus();
        if (allowed.Value()) {
            Base::Result<void> executed = ExecuteTriggerActions(
                active
                    ? state.trigger->GetEnterActions()
                    : state.trigger->GetExitActions(),
                *state.owner,
                state.names);
            if (!executed) return executed.GetStatus();
        }
        state.active = active;
        return true;
    }

Base::Result<bool> InteractivityEngine::StartPropertyChangedTrigger(
        Aero::Interactivity::PropertyChangedTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        if (!trigger.GetBinding()) return false;
        for (const PropertyChangedTriggerSubscription& existing :
             propertyChangedTriggerSubscriptions) {
            if (existing.owner == &owner && existing.context != nullptr &&
                existing.context->trigger == &trigger) {
                return false;
            }
        }
        Base::Result<InteractionTriggerProperty> property =
            ResolveInteractionTriggerProperty(
                *trigger.GetBinding(), owner, names);
        if (property.GetStatus().code == Base::ErrorCode::NotFound) {
            Base::Result<void> pending = PendUntilDataContext(
                owner, names, nullptr, &trigger);
            if (!pending) return pending.GetStatus();
            return false;
        }
        if (!property) return property.GetStatus();
        PropertyChangedTriggerState* context = nullptr;
        Base::Result<void> allocated = AllocateObject(
            *allocator, Base::MemoryTag::Ui, context);
        if (!allocated) return allocated.GetStatus();
        context->runtime = this;
        context->trigger = &trigger;
        context->owner = &owner;
        context->names = names;
        context->metadataProperty = property.Value().metadataProperty;
        Meta::DependencyPropertyChangedEventHandler handler(
            [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                    context->Invoke(object, args);
                });
        std::uint64_t metadataSubscription = 0U;
        Base::Result<void> subscribed;
        if (property.Value().dependencySource != nullptr) {
            subscribed = property.Value().dependencySource
                ->AddValueChangedHandlerChecked(
                    property.Value().dependencyProperty, handler);
        } else {
            Base::Result<std::uint64_t> notification =
                metadata->SubscribePropertyChanged(
                    *property.Value().source,
                    &PropertyChangedTriggerState::MetadataInvoke,
                    context);
            if (notification) {
                metadataSubscription = notification.Value();
            } else {
                subscribed = notification.GetStatus();
            }
        }
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return subscribed.GetStatus();
        }
        PropertyChangedTriggerSubscription subscription;
        subscription.owner = &owner;
        subscription.source = property.Value().dependencySource;
        subscription.metadataSource = property.Value().dependencySource == nullptr
            ? property.Value().source : nullptr;
        subscription.property = property.Value().dependencyProperty;
        subscription.metadataSubscription = metadataSubscription;
        subscription.handler = handler;
        subscription.context = context;
        Base::Result<void> retained =
            propertyChangedTriggerSubscriptions.PushBack(
                std::move(subscription));
        if (!retained) {
            if (property.Value().dependencySource != nullptr) {
                static_cast<void>(property.Value().dependencySource
                    ->RemoveValueChangedHandler(
                        property.Value().dependencyProperty, handler));
            } else if (metadataSubscription != 0U) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *property.Value().source, metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return retained.GetStatus();
        }
        return true;
    }

Base::Result<bool> InteractivityEngine::StartInteractionDataTrigger(
        Aero::DataTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        if (!trigger.GetBinding()) return false;
        for (const InteractionDataTriggerSubscription& existing :
             interactionDataTriggerSubscriptions) {
            if (existing.owner == &owner && existing.context != nullptr &&
                existing.context->trigger == &trigger) {
                return false;
            }
        }
        Base::Result<InteractionTriggerProperty> property =
            ResolveInteractionTriggerProperty(
                *trigger.GetBinding(), owner, names);
        if (property.GetStatus().code == Base::ErrorCode::NotFound) {
            Base::Result<void> pending = PendUntilDataContext(
                owner, names, &trigger, nullptr);
            if (!pending) return pending.GetStatus();
            return false;
        }
        if (!property) return property.GetStatus();
        InteractionDataTriggerState* context = nullptr;
        Base::Result<void> allocated = AllocateObject(
            *allocator, Base::MemoryTag::Ui, context);
        if (!allocated) return allocated.GetStatus();
        context->runtime = this;
        context->trigger = &trigger;
        context->owner = &owner;
        context->names = names;
        context->metadataProperty = property.Value().metadataProperty;
        Meta::DependencyPropertyChangedEventHandler handler(
            [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                    context->Invoke(object, args);
                });
        std::uint64_t metadataSubscription = 0U;
        Base::Result<void> subscribed;
        if (property.Value().dependencySource != nullptr) {
            subscribed = property.Value().dependencySource
                ->AddValueChangedHandlerChecked(
                    property.Value().dependencyProperty, handler);
        } else {
            Base::Result<std::uint64_t> notification =
                metadata->SubscribePropertyChanged(
                    *property.Value().source,
                    &InteractionDataTriggerState::MetadataInvoke,
                    context);
            if (notification) {
                metadataSubscription = notification.Value();
            } else {
                subscribed = notification.GetStatus();
            }
        }
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return subscribed.GetStatus();
        }
        InteractionDataTriggerSubscription subscription;
        subscription.owner = &owner;
        subscription.source = property.Value().dependencySource;
        subscription.metadataSource = property.Value().dependencySource == nullptr
            ? property.Value().source : nullptr;
        subscription.property = property.Value().dependencyProperty;
        subscription.metadataSubscription = metadataSubscription;
        subscription.handler = handler;
        subscription.context = context;
        Base::Result<void> retained =
            interactionDataTriggerSubscriptions.PushBack(
                std::move(subscription));
        if (!retained) {
            if (property.Value().dependencySource != nullptr) {
                static_cast<void>(property.Value().dependencySource
                    ->RemoveValueChangedHandler(
                        property.Value().dependencyProperty, handler));
            } else if (metadataSubscription != 0U) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *property.Value().source, metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return retained.GetStatus();
        }
        Base::Result<bool> evaluated =
            EvaluateInteractionDataTrigger(*context);
        if (!evaluated) return evaluated.GetStatus();
        return true;
    }

std::uint32_t InteractivityEngine::KeyCodeFromName(
        Base::StringView key) noexcept {
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Enter") ||
            Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Return")) {
            return Input::KeyboardKeyEnter;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Space")) {
            return Input::KeyboardKeySpace;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Escape") ||
            Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Esc")) {
            return Input::KeyboardKeyEscape;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Tab")) {
            return Input::KeyboardKeyTab;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Left")) {
            return Input::KeyboardKeyLeft;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Right")) {
            return Input::KeyboardKeyRight;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Up")) {
            return Input::KeyboardKeyUp;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                key, "Down")) {
            return Input::KeyboardKeyDown;
        }
        return 0U;
    }

Base::Result<bool> InteractivityEngine::StartKeyTrigger(
        Aero::Interactivity::KeyTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        Aero::UIElement* source = ::Aero::TryCast<::Aero::UIElement>(&(owner));
        if (source == nullptr) return false;
        if (KeyCodeFromName(trigger.GetKey()) == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "KeyTrigger Key is not supported");
        }
        for (const KeyTriggerSubscription& existing :
             keyTriggerSubscriptions) {
            if (existing.owner == &owner && existing.context != nullptr &&
                existing.context->trigger == &trigger) {
                return false;
            }
        }
        KeyTriggerState* context = nullptr;
        Base::Result<void> allocated = AllocateObject(
            *allocator, Base::MemoryTag::Ui, context);
        if (!allocated) return allocated.GetStatus();
        context->runtime = this;
        context->trigger = &trigger;
        context->owner = &owner;
        context->names = names;
        Aero::KeyEventHandler handler(
            [context](Base::Object* sender, Aero::KeyEventArgs& args) noexcept {
                context->Invoke(sender, args);
            });
        Base::Result<void> subscribed = source->AddHandlerChecked(
            Aero::UIElement::KeyDownEvent.Handle(), handler);
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return subscribed.GetStatus();
        }
        Base::Result<void> retained = keyTriggerSubscriptions.PushBack({
            &owner, source, handler, context});
        if (!retained) {
            static_cast<void>(source->RemoveHandler(
                Aero::UIElement::KeyDownEvent.Handle(), handler));
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return retained.GetStatus();
        }
        return true;
    }

void InteractivityEngine::PropertyChangedTriggerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept
{
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> executed = runtime->ExecuteTriggerActions(
        trigger->GetActions(), *owner, names);
    if (!executed) {
        runtime->animationEventStatus = executed.GetStatus();
    }
}

void InteractivityEngine::PropertyChangedTriggerState::MetadataInvoke(
    Base::Object&,
    Meta::MemberId property,
    void* context) noexcept
{
    auto* state = static_cast<PropertyChangedTriggerState*>(context);
    if (state == nullptr || (state->metadataProperty != Meta::InvalidMemberId &&
        property != state->metadataProperty)) {
        return;
    }
    if (state->runtime == nullptr || state->trigger == nullptr ||
        state->owner == nullptr ||
        !state->runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> executed = state->runtime->ExecuteTriggerActions(
        state->trigger->GetActions(), *state->owner, state->names);
    if (!executed) {
        state->runtime->animationEventStatus = executed.GetStatus();
    }
}

void InteractivityEngine::InteractionDataTriggerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept
{
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<bool> evaluated =
        runtime->EvaluateInteractionDataTrigger(*this);
    if (!evaluated) {
        runtime->animationEventStatus = evaluated.GetStatus();
    }
}

void InteractivityEngine::InteractionDataTriggerState::MetadataInvoke(
    Base::Object&,
    Meta::MemberId property,
    void* context) noexcept
{
    auto* state = static_cast<InteractionDataTriggerState*>(context);
    if (state == nullptr || (state->metadataProperty != Meta::InvalidMemberId &&
        property != state->metadataProperty)) {
        return;
    }
    if (state->runtime == nullptr || state->trigger == nullptr ||
        state->owner == nullptr ||
        !state->runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<bool> evaluated =
        state->runtime->EvaluateInteractionDataTrigger(*state);
    if (!evaluated) {
        state->runtime->animationEventStatus = evaluated.GetStatus();
    }
}

void InteractivityEngine::KeyTriggerState::Invoke(
    Base::Object*,
    Aero::KeyEventArgs& args) noexcept
{
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk() ||
        args.GetAction() != Input::KeyboardAction::Down ||
        args.GetKey() != InteractivityEngine::KeyCodeFromName(trigger->GetKey())) {
        return;
    }
    if (trigger->GetActiveOnFocus()) {
        Aero::UIElement* expected = ::Aero::TryCast<::Aero::UIElement>(owner);
        if (expected == nullptr || runtime->input == nullptr ||
            runtime->input->GetFocusedElement() != expected) {
            return;
        }
    }
    Base::Result<void> executed = runtime->ExecuteTriggerActions(
        trigger->GetActions(), *owner, names);
    if (!executed) {
        runtime->animationEventStatus = executed.GetStatus();
    }
}

Base::Result<void> InteractivityEngine::PendUntilDataContext(
    Aero::FrameworkElement& owner,
    const Aero::NameScope* names,
    Aero::DataTrigger* dataTrigger,
    Aero::Interactivity::PropertyChangedTrigger* propertyTrigger) noexcept {
    for (const PendingInteractionTrigger& existing :
         pendingInteractionTriggers) {
        if (existing.owner == &owner &&
            existing.dataTrigger == dataTrigger &&
            existing.propertyTrigger == propertyTrigger) {
            return {};
        }
    }
    // Do not subscribe to DataContext here. Inherited DataContext
    // notifications run inside ApplyChange; starting triggers from that
    // stack evaluates ChangePropertyAction and re-enters the property
    // engine. DataBind retries the pending list after SetDataContext.
    PendingInteractionTrigger pending;
    pending.owner = &owner;
    pending.names = names;
    pending.dataTrigger = dataTrigger;
    pending.propertyTrigger = propertyTrigger;
    return pendingInteractionTriggers.PushBack(std::move(pending));
}

void InteractivityEngine::ClearPendingInteractionTriggers() noexcept {
    pendingInteractionTriggers.Clear();
}

void InteractivityEngine::RetryPendingInteractionTriggers() noexcept {
    if (retryingPendingInteractionTriggers_ ||
        pendingInteractionTriggers.Empty()) {
        return;
    }
    retryingPendingInteractionTriggers_ = true;
    Base::Vector<PendingInteractionTrigger> snapshot(allocator);
    for (PendingInteractionTrigger& pending : pendingInteractionTriggers) {
        Base::Result<void> retained =
            snapshot.PushBack(std::move(pending));
        if (!retained) {
            retryingPendingInteractionTriggers_ = false;
            return;
        }
    }
    pendingInteractionTriggers.Clear();
    for (PendingInteractionTrigger& pending : snapshot) {
        if (pending.owner == nullptr) continue;
        if (pending.dataTrigger != nullptr) {
            Base::Result<bool> started = StartInteractionDataTrigger(
                *pending.dataTrigger, *pending.owner, pending.names);
            if (!started && view != nullptr) {
                view->ReportUpdateFailure(started.GetStatus());
            }
        } else if (pending.propertyTrigger != nullptr) {
            Base::Result<bool> started = StartPropertyChangedTrigger(
                *pending.propertyTrigger, *pending.owner, pending.names);
            if (!started && view != nullptr) {
                view->ReportUpdateFailure(started.GetStatus());
            }
        }
    }
    retryingPendingInteractionTriggers_ = false;
}

} // namespace Aero
