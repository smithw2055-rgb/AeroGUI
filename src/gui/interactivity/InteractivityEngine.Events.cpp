#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

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

Base::Result<bool> InteractivityEngine::AnimationEventState::EvaluateComparison(
            const Aero::Interactivity::ComparisonCondition& condition) noexcept {
            const Base::Ref<Data::Binding> binding =
                condition.GetLeftOperand();
            if (!binding || runtime == nullptr ||
                runtime->metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior requires a bound left operand");
            }
            if (binding->GetElementName().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "ConditionBehavior currently requires Binding ElementName");
            }
            Base::Object* source = names != nullptr
                ? names->Find(binding->GetElementName())
                : runtime->view->loadedDocument.names.Find(
                      binding->GetElementName());
            if (source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "ConditionBehavior Binding ElementName was not found");
            }
            Base::Result<Meta::BindingPathPlan> plan =
                Meta::BindingPathPlan::Compile(
                    *runtime->metadata,
                    source->RuntimeType(), binding->GetPath().GetPath());
            if (!plan) return plan.GetStatus();
            Base::Result<Meta::PropertyValue> current =
                plan.Value().Get(*runtime->metadata, *source);
            if (!current) return current.GetStatus();
            Meta::PropertyValue expected = condition.GetRightOperand();
            if (expected.IsNullObject()) {
                return current.Value().IsNullObject();
            }
            if (expected.Kind() == Meta::ValueKind::String &&
                expected.Type() != current.Value().Type()) {
                Base::Result<Meta::PropertyValue> converted =
                    Meta::PropertyValue::TryFromString(
                        current.Value().Type(), expected.AsString());
                // WPF-style conditions simply do not match when their two
                // operands cannot be converted to a comparable type.
                if (!converted) return false;
                expected = std::move(converted).Value();
            }
            const auto comparison = condition.GetComparisonOperator();
            if (comparison ==
                Aero::Interactivity::ComparisonCondition::Operator::Equal) {
                return current.Value().Equals(expected);
            }
            if (comparison ==
                Aero::Interactivity::ComparisonCondition::Operator::NotEqual) {
                return !current.Value().Equals(expected);
            }

            const auto isNumeric = [](Meta::ValueKind kind) noexcept {
                return kind == Meta::ValueKind::SignedInteger ||
                    kind == Meta::ValueKind::UnsignedInteger ||
                    kind == Meta::ValueKind::Double;
            };
            const auto numericValue = [](const Meta::PropertyValue& value) noexcept {
                switch (value.Kind()) {
                case Meta::ValueKind::SignedInteger:
                    return static_cast<long double>(value.AsSignedInteger());
                case Meta::ValueKind::UnsignedInteger:
                    return static_cast<long double>(value.AsUnsignedInteger());
                case Meta::ValueKind::Double:
                    return static_cast<long double>(value.AsDouble());
                default:
                    return 0.0L;
                }
            };
            if (isNumeric(current.Value().Kind()) && isNumeric(expected.Kind())) {
                const long double left = numericValue(current.Value());
                const long double right = numericValue(expected);
                switch (comparison) {
                case Aero::Interactivity::ComparisonCondition::Operator::LessThan:
                    return left < right;
                case Aero::Interactivity::ComparisonCondition::Operator::LessThanOrEqual:
                    return left <= right;
                case Aero::Interactivity::ComparisonCondition::Operator::GreaterThan:
                    return left > right;
                case Aero::Interactivity::ComparisonCondition::Operator::GreaterThanOrEqual:
                    return left >= right;
                default:
                    break;
                }
            }
            if (current.Value().Kind() == Meta::ValueKind::String &&
                expected.Kind() == Meta::ValueKind::String) {
                const int result = current.Value().AsString().Compare(
                    expected.AsString());
                switch (comparison) {
                case Aero::Interactivity::ComparisonCondition::Operator::LessThan:
                    return result < 0;
                case Aero::Interactivity::ComparisonCondition::Operator::LessThanOrEqual:
                    return result <= 0;
                case Aero::Interactivity::ComparisonCondition::Operator::GreaterThan:
                    return result > 0;
                case Aero::Interactivity::ComparisonCondition::Operator::GreaterThanOrEqual:
                    return result >= 0;
                default:
                    break;
                }
            }
            return false;
        }

Base::Result<bool> InteractivityEngine::AnimationEventState::BehaviorsAllowExecution() noexcept {
            for (const Base::Ref<Base::Object>& behavior :
                 trigger->GetBehaviors()) {
                if (!behavior) continue;
                if (behavior->RuntimeType() !=
                    Aero::Interactivity::ConditionBehavior::StaticTypeId()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "EventTrigger contains an unsupported behavior");
                }
                const Base::Ref<Aero::Interactivity::ConditionalExpression> expression =
                    static_cast<Aero::Interactivity::ConditionBehavior&>(*behavior).GetExpression();
                if (!expression) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "ConditionBehavior has no expression");
                }
                bool expressionResult = false;
                for (const Base::Ref<Aero::Interactivity::ComparisonCondition>& condition :
                     expression->GetConditions()) {
                    if (!condition) continue;
                    Base::Result<bool> matches = EvaluateComparison(*condition);
                    if (!matches) return matches.GetStatus();
                    expressionResult = matches.Value();
                    if (!expressionResult && expression->GetChaining() ==
                        Aero::Interactivity::ConditionalExpression::ForwardChaining::And) {
                        return false;
                    }
                    if (expressionResult && expression->GetChaining() ==
                        Aero::Interactivity::ConditionalExpression::ForwardChaining::Or) {
                        break;
                    }
                }
                if (!expressionResult) return false;
            }
            return true;
        }

void InteractivityEngine::AnimationEventState::Invoke(
            Base::Object*,
            Aero::RoutedEventArgs&) noexcept {
            if (runtime == nullptr || trigger == nullptr ||
                owner == nullptr ||
                !runtime->animationEventStatus.IsOk()) {
                return;
            }
            Base::Result<bool> allowed = BehaviorsAllowExecution();
            if (!allowed) {
                runtime->animationEventStatus = allowed.GetStatus();
                return;
            }
            if (!allowed.Value()) return;
            for (const Base::Ref<Aero::Interactivity::TriggerAction>& action :
                 trigger->GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    runtime->storyboards->ExecuteAnimationAction(
                        *action, *owner, nullptr, names);
                if (!executed) {
                    runtime->animationEventStatus =
                        executed.GetStatus();
                    return;
                }
            }
        }

Base::Result<bool> InteractivityEngine::StartEventTrigger(
        MediaAnimation::EventTrigger& trigger,
        Base::Object& defaultSource,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept {
        const Base::StringView routedEvent =
            trigger.GetRoutedEvent();
        Base::Object* eventSource =
            trigger.GetSourceName().Empty()
            ? &defaultSource
            : names != nullptr
                ? names->Find(trigger.GetSourceName())
                : view->loadedDocument.names.Find(
                      trigger.GetSourceName());
        if (eventSource == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger SourceName was not found");
        }
        // Microsoft.Xaml.Behaviors EventTrigger defaults EventName to
        // Loaded. Several reference samples intentionally omit EventName
        // to request that startup behavior.
        Base::StringView eventName = routedEvent.Empty()
            ? Base::StringView("Loaded")
            : routedEvent;
        std::uint32_t dot = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < eventName.SizeBytes(); ++index) {
            if (eventName[index] == '.') dot = index;
        }
        Base::StringView eventOwnerName;
        if (dot != UINT32_MAX) {
            eventOwnerName = eventName.Substr(0U, dot);
            eventName = eventName.Substr(
                dot + 1U,
                eventName.SizeBytes() - dot - 1U);
        }
        if (eventName == Base::StringView("Loaded")) {
            for (const Base::Ref<Aero::Interactivity::TriggerAction>& action :
                 trigger.GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    storyboards->ExecuteAnimationAction(
                        *action, actionOwner, nullptr, names);
                if (!executed) return executed.GetStatus();
            }
            return true;
        }
        // WPF's GotFocus is the logical-focus counterpart of Aero's
        // keyboard-focus event. Preserve the authored behavior trigger while
        // routing it through the focus event exposed by the runtime.
        if (eventName == Base::StringView("GotFocus")) {
            eventName = Base::StringView("GotKeyboardFocus");
        }

        const bool uiSource = metadata->Types().IsDerivedFrom(
            eventSource->RuntimeType(), Aero::UIElement::StaticTypeId());
        const bool contentSource = metadata->Types().IsDerivedFrom(
            eventSource->RuntimeType(), Aero::ContentElement::StaticTypeId());
        if (!uiSource && !contentSource) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger source does not support routed events");
        }
        const Meta::EventInfo* event = nullptr;
        if (!eventOwnerName.Empty()) {
            Base::StringView ownerName = eventOwnerName;
            for (std::uint32_t index = 0U;
                 index < ownerName.SizeBytes(); ++index) {
                if (ownerName[index] == ':') {
                    ownerName = ownerName.Substr(
                        index + 1U,
                        ownerName.SizeBytes() - index - 1U);
                }
            }
            for (const Meta::TypeInfo& type :
                 metadata->Types().Types()) {
                if (type.Name() != ownerName) continue;
                event = metadata->Types().FindEvent(
                    type.Id(), eventName, true);
                if (event != nullptr) break;
            }
        } else {
            event = metadata->Types().FindEvent(
                eventSource->RuntimeType(), eventName, true);
        }
        if (event == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger RoutedEvent was not found on its source");
        }
        const Aero::RoutedEventHandle eventHandle{event->Id()};
        AnimationEventState* eventContext = nullptr;
        Base::Result<void> created = AllocateObject(
            *allocator, Base::MemoryTag::Ui, eventContext);
        if (!created) return created.GetStatus();
        eventContext->runtime = this;
        eventContext->trigger = &trigger;
        eventContext->owner = &actionOwner;
        eventContext->names = names;
        auto callback = [eventContext](
            Base::Object* sender,
            Aero::RoutedEventArgs& args) noexcept {
            eventContext->Invoke(sender, args);
        };
        Aero::RoutedEventHandler handler(callback);
        Base::Result<void> subscribed = uiSource
            ? static_cast<Aero::UIElement*>(eventSource)->AddHandlerChecked(
                  eventHandle, handler)
            : static_cast<Aero::ContentElement*>(eventSource)->AddHandlerChecked(
                  eventHandle, handler);
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, eventContext);
            return subscribed.GetStatus();
        }
        AnimationEventSubscription subscription;
        subscription.source = eventSource;
        subscription.visualOwner = &actionOwner;
        subscription.event = eventHandle;
        subscription.handler = handler;
        subscription.context = eventContext;
        subscription.contentSource = contentSource;
        Base::Result<void> retained =
            animationEventSubscriptions.PushBack(
                std::move(subscription));
        if (!retained) {
            if (contentSource) {
                static_cast<void>(
                    static_cast<Aero::ContentElement*>(eventSource)
                        ->RemoveHandler(eventHandle, handler));
            } else {
                static_cast<void>(
                    static_cast<Aero::UIElement*>(eventSource)
                        ->RemoveHandler(eventHandle, handler));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui, eventContext);
            return retained.GetStatus();
        }
        return true;
    }

} // namespace Aero
