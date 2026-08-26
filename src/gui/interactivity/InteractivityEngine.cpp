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

InteractivityEngine::InteractivityEngine(ViewState& owner) noexcept
    : view(&owner),
      allocator(owner.allocator),
      animationEventSubscriptions(owner.allocator),
      styleDataTriggerSubscriptions(owner.allocator),
      attachedBehaviorInstances(owner.allocator),
      propertyChangedTriggerSubscriptions(owner.allocator),
      interactionDataTriggerSubscriptions(owner.allocator),
      keyTriggerSubscriptions(owner.allocator),
      dataTemplateTriggerSubscriptions(owner.allocator) {}

void InteractivityEngine::Bind() noexcept {
    allocator = view->allocator;
    metadata = view->metadata;
    animations = view->animations;
    events = view->events;
    input = view->input;
    tree = view->tree;
    styles = view->styles;
    values = view->values;
    dispatcher = view->dispatcher;
    templates = view->templates;
    bindings = view->bindings;
    storyboards = view->storyboards;
}

void InteractivityEngine::NotifyLayoutUpdated() noexcept {
    for (auto& behavior : attachedBehaviorInstances) {
        if (behavior.instance) {
            behavior.instance->NotifyLayoutUpdated();
        }
    }
}

bool InteractivityEngine::IsInVisualSubtree(
        Aero::Media::Visual* node,
        const Aero::Media::Visual& fragmentRoot) const noexcept {
    while (node != nullptr) {
        if (node == &fragmentRoot) return true;
        node = node->GetLogicalParent() != nullptr
            ? node->GetLogicalParent()
            : node->GetVisualParent();
    }
    return false;
}


Base::Result<void> InteractivityEngine::ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>>
            actions,
        void* context) noexcept {
        auto* runtime = static_cast<InteractivityEngine*>(context);
        if (runtime == nullptr ||
            !runtime->metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Aero::FrameworkElement::
                    StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Style Trigger action owner is not a FrameworkElement");
        }
        auto& element =
            static_cast<Aero::FrameworkElement&>(
                owner);
        for (const Base::Ref<Base::Object>& authored :
             actions) {
            if (!authored ||
                !runtime->metadata->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    Aero::Interactivity::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Style Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                runtime->storyboards->ExecuteAnimationAction(
                    static_cast<Aero::Interactivity::TriggerAction&>(
                        *authored),
                    element);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

Base::Result<bool> InteractivityEngine::ConditionBehaviorsAllowExecution(
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        const auto numeric = [](const Meta::PropertyValue& value,
                                long double& output) noexcept {
            switch (value.Kind()) {
            case Meta::ValueKind::SignedInteger:
                output = static_cast<long double>(value.AsSignedInteger());
                return true;
            case Meta::ValueKind::UnsignedInteger:
                output = static_cast<long double>(value.AsUnsignedInteger());
                return true;
            case Meta::ValueKind::Double:
                output = static_cast<long double>(value.AsDouble());
                return true;
            default:
                return false;
            }
        };
        const auto evaluate = [&](const Aero::Interactivity::ComparisonCondition& condition)
            noexcept -> Base::Result<bool> {
            const Base::Ref<Data::Binding> binding = condition.GetLeftOperand();
            if (!binding) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior requires a bound left operand");
            }
            Base::Result<Meta::PropertyValue> current =
                EvaluateAuthoredBinding(
                    *binding, owner, nullptr, names, nullptr);
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
            long double left = 0.0L;
            long double right = 0.0L;
            if (numeric(current.Value(), left) && numeric(expected, right)) {
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
                    return false;
                }
            }
            if (current.Value().Kind() == Meta::ValueKind::String &&
                expected.Kind() == Meta::ValueKind::String) {
                const int order = current.Value().AsString().Compare(
                    expected.AsString());
                switch (comparison) {
                case Aero::Interactivity::ComparisonCondition::Operator::LessThan:
                    return order < 0;
                case Aero::Interactivity::ComparisonCondition::Operator::LessThanOrEqual:
                    return order <= 0;
                case Aero::Interactivity::ComparisonCondition::Operator::GreaterThan:
                    return order > 0;
                case Aero::Interactivity::ComparisonCondition::Operator::GreaterThanOrEqual:
                    return order >= 0;
                default:
                    return false;
                }
            }
            return false;
        };

        for (const Base::Ref<Base::Object>& behavior : behaviors) {
            if (!behavior) continue;
            if (behavior->RuntimeType() !=
                Aero::Interactivity::ConditionBehavior::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Interaction trigger contains an unsupported behavior");
            }
            const Base::Ref<Aero::Interactivity::ConditionalExpression> expression =
                static_cast<Aero::Interactivity::ConditionBehavior&>(
                    *behavior).GetExpression();
            if (!expression) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior has no expression");
            }
            const bool conjunction = expression->GetChaining() ==
                Aero::Interactivity::ConditionalExpression::ForwardChaining::And;
            bool expressionResult = conjunction;
            bool hasCondition = false;
            for (const Base::Ref<Aero::Interactivity::ComparisonCondition>& condition :
                 expression->GetConditions()) {
                if (!condition) continue;
                hasCondition = true;
                Base::Result<bool> matches = evaluate(*condition);
                if (!matches) return matches.GetStatus();
                expressionResult = matches.Value();
                if (conjunction && !expressionResult) return false;
                if (!conjunction && expressionResult) break;
            }
            if (!hasCondition || !expressionResult) return false;
        }
        return true;
    }

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

Base::Result<bool> InteractivityEngine::DataTemplateTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected) noexcept {
        if (actual.Kind() == Meta::ValueKind::Object &&
            !actual.IsNullObject() &&
            actual.AsObject() &&
            actual.AsObject()->RuntimeType() ==
                ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
            return DataTemplateTriggerValuesMatch(
                static_cast<const ::Aero::Controls::BoxedItemValue&>(
                    *actual.AsObject()).Value(),
                std::move(expected));
        }
        if (expected.Kind() == Meta::ValueKind::String &&
            expected.Type() != actual.Type()) {
            if (metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate Trigger metadata is unavailable");
            }
            Base::Result<Meta::PropertyValue> converted =
                metadata->TryConvertText(
                    actual.Type(), expected.AsString());
            if (!converted) {
                // WPF data conditions simply do not match when the authored
                // value cannot be converted to the source property's type.
                return false;
            }
            expected = std::move(converted).Value();
        }
        return actual == expected;
    }

Base::Object* InteractivityEngine::ResolveDataTemplateConditionSource(
        Aero::Controls::DataTemplateTriggerState& context,
        Aero::Controls::DataTemplateTriggerCondition& condition,
        Base::StringView& path) noexcept {
        path = condition.binding
            ? condition.binding->GetPath().GetPath()
            : Base::StringView{};
        Base::Object* source = nullptr;
        if (condition.binding &&
            !condition.binding->GetElementName().Empty()) {
            source = context.FindName(
                condition.binding->GetElementName());
            if (source == nullptr) {
                source = view->loadedDocument.names.Find(
                    condition.binding->GetElementName());
            }
        } else {
            Base::Ref<Base::Object> retainedSource =
                condition.source.Lock();
            source = retainedSource.Get();
        }

        if (condition.usesDataContext && source != nullptr &&
            metadata != nullptr &&
            metadata->Types().IsDerivedFrom(
                source->RuntimeType(), FrameworkElement::StaticTypeId())) {
            Meta::Value dataContext =
                static_cast<FrameworkElement*>(source)->GetDataContext();
            source = dataContext.Kind() == Meta::ValueKind::Object &&
                    !dataContext.IsNullObject() && dataContext.AsObject()
                ? dataContext.AsObject().Get()
                : nullptr;
        }

        constexpr Base::StringView TemplatedParentPrefix(
            "TemplatedParent.");
        if (source != nullptr &&
            path.SizeBytes() > TemplatedParentPrefix.SizeBytes() &&
            path.Substr(0U, TemplatedParentPrefix.SizeBytes()) ==
                TemplatedParentPrefix &&
            metadata != nullptr &&
            metadata->Types().IsDerivedFrom(
                source->RuntimeType(), FrameworkElement::StaticTypeId())) {
            source = static_cast<FrameworkElement*>(source)->GetTemplatedParent();
            path = path.Substr(
                TemplatedParentPrefix.SizeBytes(),
                path.SizeBytes() - TemplatedParentPrefix.SizeBytes());
        }
        return source;
    }

Base::Result<bool> InteractivityEngine::EvaluateDataTemplateCondition(
        Aero::Controls::DataTemplateTriggerState& context,
        Aero::Controls::DataTemplateTriggerCondition& condition) noexcept {
        Meta::PropertyValue current;
        Base::Ref<DependencyObject> dependencySource =
            condition.dependencySource.Lock();
        if (dependencySource &&
            condition.property.IsValid()) {
            Base::Result<Meta::PropertyValue> value =
                dependencySource->GetValue(condition.property);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        } else {
            if (!condition.binding || metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate DataTrigger Binding is unavailable");
            }
            Base::StringView path;
            Base::Object* source =
                ResolveDataTemplateConditionSource(
                    context, condition, path);
            if (source == nullptr) {
                return false;
            }
            Base::Result<Meta::BindingPathPlan> plan =
                Meta::BindingPathPlan::Compile(
                    *metadata,
                    source->RuntimeType(),
                    path);
            if (!plan) return plan.GetStatus();
            Base::Result<Meta::PropertyValue> value =
                plan.Value().Get(*metadata, *source);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        }
        return DataTemplateTriggerValuesMatch(
            current, condition.value);
    }

Base::Result<void> InteractivityEngine::EnsureDataTemplateProviderTokens(
        Aero::Controls::DataTemplateTriggerState& context) noexcept {
        if (values == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger value engine is unavailable");
        }
        if (context.providerOrigin == 0U) {
            Base::Result<std::uint32_t> origin =
                values->AllocateProviderOrigin();
            if (!origin) return origin.GetStatus();
            context.providerOrigin = origin.Value();
        }

        std::uint64_t ordinal = 0U;
        for (Aero::Controls::DataTemplatePropertyTrigger& trigger :
             context.triggers) {
            for (Aero::Controls::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                if (ordinal > UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "DataTemplate Trigger setter ordinal limit reached");
                }
                const Meta::PropertyProviderToken expected{
                    Meta::PropertyValueRank::TemplateTrigger,
                    context.providerOrigin,
                    static_cast<std::uint32_t>(ordinal)};
                if (setter.token.IsValid() && setter.token != expected) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "DataTemplate Trigger provider token is inconsistent");
                }
                setter.token = expected;
                ++ordinal;
            }
        }
        return {};
    }

Base::Result<void> InteractivityEngine::EvaluateDataTemplateTrigger(
        Aero::Controls::DataTemplateTriggerState& context,
        std::uint32_t triggerIndex) noexcept {
        if (triggerIndex >= context.triggers.Size() ||
            context.root == nullptr ||
            values == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger runtime is unavailable");
        }
        Base::Result<void> providerTokens =
            EnsureDataTemplateProviderTokens(context);
        if (!providerTokens) return providerTokens.GetStatus();

        Aero::Controls::DataTemplatePropertyTrigger& trigger =
            context.triggers[triggerIndex];
        bool active = !trigger.conditions.Empty();
        for (Aero::Controls::DataTemplateTriggerCondition& condition :
             trigger.conditions) {
            Base::Result<bool> matches =
                EvaluateDataTemplateCondition(context, condition);
            if (!matches) return matches.GetStatus();
            if (!matches.Value()) {
                active = false;
                break;
            }
        }
        if (active == trigger.active) return {};

        if (active) {
            for (const Aero::Controls::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                Base::Ref<DependencyObject> target =
                    setter.target.Lock();
                if (!target) continue;
                Base::Result<void> applied =
                    values->SetProviderContribution(
                        *target,
                        setter.property,
                        setter.token,
                        setter.value);
                if (!applied) {
                    return applied.GetStatus();
                }
            }
        } else {
            for (const Aero::Controls::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                Base::Ref<DependencyObject> target =
                    setter.target.Lock();
                if (!target) continue;
                Base::Result<bool> cleared =
                    values->ClearProviderContribution(
                        *target,
                        setter.property,
                        setter.token);
                if (!cleared) {
                    return cleared.GetStatus();
                }
            }
        }

        Base::Span<const Base::Ref<Base::Object>> actions =
            active
            ? trigger.enterActions.AsSpan()
            : trigger.exitActions.AsSpan();
        for (const Base::Ref<Base::Object>& authored :
             actions) {
            if (!authored ||
                !metadata->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    Aero::Interactivity::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "DataTemplate Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                storyboards->ExecuteAnimationAction(
                    static_cast<
                        Aero::Interactivity::TriggerAction&>(
                            *authored),
                    *context.root,
                    &context);
            if (!executed) {
                return executed.GetStatus();
            }
        }
        trigger.active = active;
        return {};
    }

Base::Result<bool> InteractivityEngine::StyleDataTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected) noexcept {
        if (actual.Kind() == Meta::ValueKind::Object &&
            !actual.IsNullObject() && actual.AsObject() &&
            actual.AsObject()->RuntimeType() ==
                ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
            return StyleDataTriggerValuesMatch(
                static_cast<const ::Aero::Controls::BoxedItemValue&>(
                    *actual.AsObject()).Value(),
                std::move(expected));
        }
        if (expected.IsNullObject()) {
            return actual.IsNullObject();
        }
        if (expected.Kind() == Meta::ValueKind::String &&
            expected.Type() != actual.Type()) {
            Base::Result<Meta::PropertyValue> converted =
                metadata->TryConvertText(
                    actual.Type(), expected.AsString());
            if (!converted) return false;
            expected = std::move(converted).Value();
        }
        return actual == expected;
    }

Base::Result<void> InteractivityEngine::EvaluateStyleDataTrigger(
        StyleDataTriggerHandlerState& state) noexcept {
        if (styles == nullptr || state.target == nullptr ||
            state.style == nullptr || state.source == nullptr ||
            !state.property.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style DataTrigger subscription is invalid");
        }
        Base::Result<Meta::PropertyValue> actual =
            state.source->GetValue(state.property);
        if (!actual) return actual.GetStatus();
        Base::Result<bool> matches = StyleDataTriggerValuesMatch(
            actual.Value(), state.expected);
        if (!matches) return matches.GetStatus();
        return styles->SetBindingTriggerState(
            *state.target,
            *state.style,
            state.triggerIndex,
            matches.Value());
    }

void InteractivityEngine::ClearStyleDataTriggersFor(
        Aero::FrameworkElement& target) noexcept {
        for (std::uint32_t index = 0U;
             index < styleDataTriggerSubscriptions.Size();) {
            StyleDataTriggerSubscription& subscription =
                styleDataTriggerSubscriptions[index];
            if (subscription.target != &target) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr &&
                subscription.property.IsValid()) {
                (void)subscription.source->RemoveValueChangedHandler(
                    subscription.property,
                    subscription.handler);
            }
            FreeObject(
                *allocator,
                Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U !=
                styleDataTriggerSubscriptions.Size()) {
                styleDataTriggerSubscriptions[index] =
                    std::move(styleDataTriggerSubscriptions.Back());
            }
            styleDataTriggerSubscriptions.PopBack();
        }
    }

Base::Result<std::uint32_t> InteractivityEngine::StartStyleDataTriggers(
        Aero::FrameworkElement& target,
        const Aero::Style& style) noexcept {
        std::uint32_t started = 0U;
        const Base::Span<const Aero::TriggerPlan> triggers =
            Aero::StylePrivate::RuntimeTriggers(style);
        for (std::uint32_t index = 0U;
             index < triggers.Size(); ++index) {
            const Aero::TriggerPlan& trigger = triggers[index];
            if (!trigger.IsBindingTrigger()) continue;
            bool alreadyAttached = false;
            for (const StyleDataTriggerSubscription& existing :
                 styleDataTriggerSubscriptions) {
                alreadyAttached = alreadyAttached ||
                    (existing.target == &target &&
                     existing.context != nullptr &&
                     existing.context->style == &style &&
                     existing.context->triggerIndex == index);
            }
            if (alreadyAttached) continue;

            const Base::Ref<Data::Binding> binding = trigger.binding;
            Base::Object* sourceObject = nullptr;
            if (!binding->GetElementName().Empty()) {
                sourceObject = target.FindName(
                    binding->GetElementName());
            } else if (binding->GetRelativeSource()) {
                const Data::RelativeSourceMode mode =
                    binding->GetRelativeSource()->GetMode();
                if (mode == Data::RelativeSourceMode::Self) {
                    sourceObject = &target;
                } else if (mode ==
                           Data::RelativeSourceMode::TemplatedParent) {
                    sourceObject = target.GetTemplatedParent();
                } else if (mode ==
                           Data::RelativeSourceMode::FindAncestor) {
                    Base::StringView ancestorName =
                        binding->GetRelativeSource()->GetAncestorType();
                    for (std::uint32_t nameIndex = 0U;
                         nameIndex < ancestorName.SizeBytes(); ++nameIndex) {
                        if (ancestorName[nameIndex] == ':') {
                            ancestorName = ancestorName.Substr(
                                nameIndex + 1U,
                                ancestorName.SizeBytes() - nameIndex - 1U);
                            break;
                        }
                    }
                    const std::uint32_t requestedLevel =
                        binding->GetRelativeSource()->GetAncestorLevel();
                    std::uint32_t matchedLevel = 0U;
                    Aero::Media::Visual* current = target.GetLogicalParent();
                    if (current == nullptr) {
                        current = target.GetVisualParent();
                    }
                    while (current != nullptr) {
                        const Meta::TypeInfo* type =
                            metadata->Types().FindType(
                                current->RuntimeType());
                        const bool matchesType = ancestorName.Empty() ||
                            (type != nullptr && type->Name() == ancestorName);
                        if (matchesType && ++matchedLevel == requestedLevel) {
                            sourceObject = current;
                            break;
                        }
                        Aero::Media::Visual* next = current->GetLogicalParent();
                        if (next == nullptr) {
                            next = current->GetVisualParent();
                        }
                        current = next;
                    }
                }
            }
            if (sourceObject == nullptr ||
                !metadata->Types().IsDerivedFrom(
                    sourceObject->RuntimeType(),
                    ::Aero::DependencyObject::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style DataTrigger Binding source was not found");
            }
            auto* source = static_cast<::Aero::DependencyObject*>(
                sourceObject);
            const Base::StringView path =
                binding->GetPath().GetPath();
            const Meta::DependencyProperty* property =
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata).Find(
                        source->RuntimeType(), path);
            if (property == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style DataTrigger Binding path was not found");
            }

            StyleDataTriggerHandlerState* context = nullptr;
            Base::Result<void> allocated = AllocateObject(
                *allocator,
                Base::MemoryTag::Ui,
                context);
            if (!allocated) return allocated.GetStatus();
            context->runtime = this;
            context->target = &target;
            context->style = &style;
            context->triggerIndex = index;
            context->source = source;
            context->property = property->Handle();
            context->expected = trigger.value;
            auto callback = [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                    context->Invoke(object, args);
                };
            Meta::DependencyPropertyChangedEventHandler handler(callback);
            Base::Result<void> subscribed =
                source->AddValueChangedHandlerChecked(
                    property->Handle(), handler);
            if (!subscribed) {
                FreeObject(
                    *allocator,
                    Base::MemoryTag::Ui,
                    context);
                return subscribed.GetStatus();
            }
            StyleDataTriggerSubscription subscription;
            subscription.target = &target;
            subscription.source = source;
            subscription.property = property->Handle();
            subscription.handler = handler;
            subscription.context = context;
            Base::Result<void> retained =
                styleDataTriggerSubscriptions.PushBack(
                    std::move(subscription));
            if (!retained) {
                (void)source->RemoveValueChangedHandler(
                    property->Handle(), handler);
                FreeObject(
                    *allocator,
                    Base::MemoryTag::Ui,
                    context);
                return retained.GetStatus();
            }
            Base::Result<void> evaluated =
                EvaluateStyleDataTrigger(*context);
            if (!evaluated) return evaluated.GetStatus();
            ++started;
        }
        return started;
    }

Base::Result<std::uint32_t>
 InteractivityEngine::StartDataTemplateTriggers(
        Aero::Controls::DataTemplateTriggerState&
            context) noexcept {
        std::uint32_t count = 0U;
        for (std::uint32_t triggerIndex = 0U;
             triggerIndex < context.triggers.Size();
             ++triggerIndex) {
            Aero::Controls::DataTemplatePropertyTrigger&
                trigger =
                    context.triggers[triggerIndex];
            for (std::uint32_t conditionIndex = 0U;
                 conditionIndex <
                     trigger.conditions.Size();
                 ++conditionIndex) {
                Aero::Controls::DataTemplateTriggerCondition&
                    condition =
                        trigger.conditions[conditionIndex];
                Base::Ref<DependencyObject> dependencySource =
                    condition.dependencySource.Lock();
                if ((!dependencySource ||
                     !condition.property.IsValid()) &&
                    condition.binding) {
                    Base::StringView path;
                    Base::Object* source =
                        ResolveDataTemplateConditionSource(
                            context, condition, path);
                    if (source != nullptr &&
                        metadata->Types().IsDerivedFrom(
                            source->RuntimeType(),
                            ::Aero::DependencyObject::
                                StaticTypeId())) {
                        const Meta::DependencyProperty*
                            property =
                                ::Aero::MetadataPrivate::
                                    DependencyProperties(
                                        *metadata)
                                        .Find(
                                            source->
                                                RuntimeType(),
                                            path);
                        if (property != nullptr) {
                            dependencySource =
                                Base::Ref<DependencyObject>::FromBorrowed(
                                    *static_cast<DependencyObject*>(source));
                            condition.dependencySource =
                                Base::WeakRef<DependencyObject>(
                                    dependencySource);
                            condition.property = property->Handle();
                        }
                    }
                }
                if (!dependencySource ||
                    !condition.property.IsValid()) {
                    continue;
                }
                bool alreadyAttached = false;
                for (const DataTemplateTriggerSubscription&
                         existing :
                     dataTemplateTriggerSubscriptions) {
                    alreadyAttached =
                        alreadyAttached ||
                        (existing.context != nullptr &&
                         existing.context->
                                 triggerContext.Get() ==
                             &context &&
                         existing.context->
                                 triggerIndex ==
                             triggerIndex &&
                         existing.context->
                                 conditionIndex ==
                             conditionIndex);
                }
                if (alreadyAttached) continue;

                DataTemplateTriggerHandlerState*
                    handlerContext = nullptr;
                Base::Result<void> created =
                    AllocateObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        handlerContext);
                if (!created) {
                    return created.GetStatus();
                }
                handlerContext->runtime = this;
                handlerContext->triggerContext =
                    Base::Ref<
                        Aero::Controls::DataTemplateTriggerState>::
                        FromBorrowed(context);
                handlerContext->triggerIndex =
                    triggerIndex;
                handlerContext->conditionIndex =
                    conditionIndex;
                auto callback =
                    [handlerContext](
                        ::Aero::DependencyObject& object,
                        const Meta::
                            DependencyPropertyChangedEventArgs&
                                args) noexcept {
                        handlerContext->Invoke(
                            object, args);
                    };
                Meta::DependencyPropertyChangedEventHandler
                    handler(callback);
                Base::Result<void> subscribed =
                    dependencySource->AddValueChangedHandlerChecked(
                        condition.property, handler);
                if (!subscribed) {
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        handlerContext);
                    return subscribed.GetStatus();
                }
                DataTemplateTriggerSubscription record;
                record.source = dependencySource.Get();
                record.property = condition.property;
                record.handler = handler;
                record.context = handlerContext;
                Base::Result<void> retained =
                    dataTemplateTriggerSubscriptions.
                        PushBack(std::move(record));
                if (!retained) {
                    static_cast<void>(
                        dependencySource->RemoveValueChangedHandler(
                            condition.property, handler));
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        handlerContext);
                    return retained.GetStatus();
                }
                ++count;
            }
            Base::Result<void> evaluated =
                EvaluateDataTemplateTrigger(
                    context, triggerIndex);
            if (!evaluated) {
                return evaluated.GetStatus();
            }
        }
        return count;
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

Base::Result<Base::Ref<Interactivity::Behavior>>
 InteractivityEngine::CloneBehaviorPrototype(
        const Interactivity::Behavior& prototype) noexcept {
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior metadata is unavailable");
        }
        Base::Result<Base::Ref<Base::Object>> created =
            metadata->CreateObject(prototype.RuntimeType());
        if (!created) return created.GetStatus();
        if (!created.Value() ||
            !metadata->Types().IsDerivedFrom(
                created.Value()->RuntimeType(),
                Interactivity::Behavior::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior factory returned an incompatible object");
        }
        Base::Ref<Interactivity::Behavior> clone =
            Base::Ref<Interactivity::Behavior>::FromBorrowed(
                *static_cast<Interactivity::Behavior*>(
                    created.Value().Get()));
        for (const Meta::DependencyProperty& property :
             prototype.PropertyRegistry().Properties()) {
            if (property.MetadataFor(prototype.RuntimeType()) == nullptr ||
                property.MetadataFor(clone->RuntimeType()) == nullptr) {
                continue;
            }
            Meta::PropertyValue local =
                prototype.ReadLocalValue(property.Handle());
            if (local.IsUnset()) continue;
            Base::Result<void> copied = clone->SetValueChecked(
                property.Handle(), local);
            if (!copied) return copied.GetStatus();
        }
        Base::Result<void> bindingsCopied =
            prototype.CopyAuthoredBindingsTo(*clone);
        if (!bindingsCopied) return bindingsCopied.GetStatus();
        return clone;
    }

Base::Object* InteractivityEngine::ResolveBehaviorBindingSource(
        const Data::Binding& binding,
        Interactivity::Behavior& behavior,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        if (binding.GetSource()) return binding.GetSource().Get();
        if (!binding.GetElementName().Empty()) {
            Base::Object* source = owner.FindName(
                binding.GetElementName());
            if (source == nullptr && names != nullptr) {
                source = names->Find(binding.GetElementName());
            }
            if (source == nullptr) {
                source = view->loadedDocument.names.Find(
                    binding.GetElementName());
            }
            return source;
        }
        const Base::Ref<Data::RelativeSource> relative =
            binding.GetRelativeSource();
        if (!relative) return nullptr;
        if (relative->GetMode() == Data::RelativeSourceMode::Self) {
            return &behavior;
        }
        if (relative->GetMode() ==
            Data::RelativeSourceMode::TemplatedParent) {
            return owner.GetTemplatedParent();
        }
        if (relative->GetMode() !=
            Data::RelativeSourceMode::FindAncestor) {
            return nullptr;
        }
        Base::StringView ancestorName = relative->GetAncestorType();
        for (std::uint32_t index = 0U;
             index < ancestorName.SizeBytes(); ++index) {
            if (ancestorName[index] == ':') {
                ancestorName = ancestorName.Substr(
                    index + 1U,
                    ancestorName.SizeBytes() - index - 1U);
                break;
            }
        }
        std::uint32_t matched = 0U;
        Aero::Media::Visual* current = owner.GetLogicalParent();
        if (current == nullptr) current = owner.GetVisualParent();
        while (current != nullptr) {
            const Meta::TypeInfo* type =
                metadata->Types().FindType(current->RuntimeType());
            const bool matches = ancestorName.Empty() ||
                (type != nullptr && type->Name() == ancestorName);
            if (matches && ++matched == relative->GetAncestorLevel()) {
                return current;
            }
            Aero::Media::Visual* next = current->GetLogicalParent();
            if (next == nullptr) next = current->GetVisualParent();
            current = next;
        }
        return nullptr;
    }

Base::Object* InteractivityEngine::ResolveAuthoredBindingSource(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext,
        const Aero::NameScope* names,
        Base::Object* self) noexcept {
        if (binding.GetSource()) {
            return binding.GetSource().Get();
        }
        if (!binding.GetElementName().Empty()) {
            Base::Object* source = dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(binding.GetElementName())
                : nullptr;
            if (source == nullptr) {
                source = owner.FindName(binding.GetElementName());
            }
            if (source == nullptr && names != nullptr) {
                source = names->Find(binding.GetElementName());
            }
            if (source == nullptr) {
                source = view->loadedDocument.names.Find(
                    binding.GetElementName());
            }
            return source;
        }

        const Base::Ref<Data::RelativeSource> relative =
            binding.GetRelativeSource();
        if (relative) {
            if (relative->GetMode() ==
                Data::RelativeSourceMode::Self) {
                return self != nullptr
                    ? self
                    : static_cast<Base::Object*>(&owner);
            }
            if (relative->GetMode() ==
                Data::RelativeSourceMode::TemplatedParent) {
                return owner.GetTemplatedParent();
            }
            if (relative->GetMode() !=
                Data::RelativeSourceMode::FindAncestor) {
                return nullptr;
            }
            Base::StringView ancestorName =
                relative->GetAncestorType();
            for (std::uint32_t index = 0U;
                 index < ancestorName.SizeBytes(); ++index) {
                if (ancestorName[index] != ':') continue;
                ancestorName = ancestorName.Substr(
                    index + 1U,
                    ancestorName.SizeBytes() - index - 1U);
                break;
            }
            std::uint32_t matched = 0U;
            Aero::Media::Visual* current = owner.GetLogicalParent();
            if (current == nullptr) {
                current = owner.GetVisualParent();
            }
            while (current != nullptr) {
                const Meta::TypeInfo* type =
                    metadata != nullptr
                    ? metadata->Types().FindType(
                          current->RuntimeType())
                    : nullptr;
                const bool matches = ancestorName.Empty() ||
                    (type != nullptr &&
                     type->Name() == ancestorName);
                if (matches &&
                    ++matched == relative->GetAncestorLevel()) {
                    return current;
                }
                Aero::Media::Visual* next = current->GetLogicalParent();
                if (next == nullptr) {
                    next = current->GetVisualParent();
                }
                current = next;
            }
            return nullptr;
        }

        Meta::PropertyValue dataContext = owner.GetDataContext();
        if (dataContext.Kind() != Meta::ValueKind::Object ||
            dataContext.IsNullObject() ||
            !dataContext.AsObject()) {
            return nullptr;
        }
        return dataContext.AsObject().Get();
    }

Base::Result<Meta::PropertyValue> InteractivityEngine::EvaluateAuthoredBinding(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext,
        const Aero::NameScope* names,
        Base::Object* self) noexcept {
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Authored Binding metadata is unavailable");
        }
        Base::Object* source = ResolveAuthoredBindingSource(
            binding,
            owner,
            dataTemplateContext,
            names,
            self);
        if (source == nullptr) {
            if (!binding.GetFallbackValue().IsUnset()) {
                return binding.GetFallbackValue();
            }
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Authored Binding source was not found");
        }

        Base::Result<Meta::PropertyValue> value =
            Meta::PropertyValue::FromObject(
                source->RuntimeType(),
                Base::Ref<Base::Object>::FromBorrowed(*source));
        const Base::StringView path = binding.GetPath().GetPath();
        if (!path.Empty()) {
            Meta::BindingPathCompileError pathError;
            Base::Result<Meta::BindingPathPlan> plan =
                Meta::BindingPathPlan::Compile(
                    *metadata,
                    source->RuntimeType(),
                    path,
                    &pathError);
            if (plan) {
                value = plan.Value().Get(*metadata, *source);
            } else {
                value = plan.GetStatus();
            }
        }
        if (!value) {
            if (!binding.GetFallbackValue().IsUnset()) {
                return binding.GetFallbackValue();
            }
            return value.GetStatus();
        }

        Meta::PropertyValue resolved = value.Value();
        if (resolved.Kind() == Meta::ValueKind::Object &&
            !resolved.IsNullObject() && resolved.AsObject() &&
            resolved.AsObject()->RuntimeType() ==
                Controls::BoxedItemValue::StaticTypeId()) {
            resolved = static_cast<const Controls::BoxedItemValue&>(
                *resolved.AsObject()).Value();
        }
        if (resolved.IsNullObject() &&
            !binding.GetTargetNullValue().IsUnset()) {
            resolved = binding.GetTargetNullValue();
        }
        if (binding.GetConverter()) {
            Base::Result<Meta::PropertyValue> converted =
                binding.GetConverter()->Convert(
                    resolved,
                    binding.GetConverterParameter());
            if (!converted) {
                if (!binding.GetFallbackValue().IsUnset()) {
                    return binding.GetFallbackValue();
                }
                return converted.GetStatus();
            }
            resolved = std::move(converted).Value();
        }
        return resolved;
    }

Base::Result<void> InteractivityEngine::ExecuteTriggerActions(
        Base::Span<const Base::Ref<Base::Object>> actions,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        for (const Base::Ref<Base::Object>& authored : actions) {
            if (!authored || metadata == nullptr ||
                !metadata->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    Aero::Interactivity::TriggerAction::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Interaction Trigger contains an invalid action");
            }
            Base::Result<void> executed = storyboards->ExecuteAnimationAction(
                static_cast<Aero::Interactivity::TriggerAction&>(*authored),
                owner,
                nullptr,
                names);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

Base::Result<void> InteractivityEngine::ExecuteTriggerActions(
        Base::Span<const Base::Ref<Aero::Interactivity::TriggerAction>> actions,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        for (const Base::Ref<Aero::Interactivity::TriggerAction>& action :
             actions) {
            if (!action) continue;
            Base::Result<void> executed = storyboards->ExecuteAnimationAction(
                *action, owner, nullptr, names);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

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
            if (property == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Interaction Trigger Binding property was not found");
            }
            resolved.dependencyProperty = property->Handle();
            return resolved;
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
        Base::Result<bool> matches = DataTemplateTriggerValuesMatch(
            actual.Value(), state.trigger->GetAuthoredValue());
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
                key, "Escape")) {
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
        Aero::UIElement* source = owner.AsUIElement();
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

Base::Result<void> InteractivityEngine::AttachBehavior(
        const Interactivity::Behavior& prototype,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names,
        bool clonePrototype) noexcept {
        for (const AttachedBehaviorInstance& existing :
             attachedBehaviorInstances) {
            if (existing.target == &owner &&
                existing.prototype == &prototype) {
                return {};
            }
        }
        Base::Ref<Interactivity::Behavior> instance;
        if (clonePrototype) {
            Base::Result<Base::Ref<Interactivity::Behavior>> cloned =
                CloneBehaviorPrototype(prototype);
            if (!cloned) return cloned.GetStatus();
            instance = std::move(cloned).Value();
        } else {
            instance = Base::Ref<Interactivity::Behavior>::TryFromBorrowed(
                const_cast<Interactivity::Behavior&>(prototype));
            if (!instance) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Direct Behavior instance cannot be retained");
            }
        }
        AttachedBehaviorInstance record;
        record.target = &owner;
        record.prototype = &prototype;
        record.instance = std::move(instance);

        for (const Interactivity::Behavior::AuthoredBinding& authored :
             record.instance->GetAuthoredBindings()) {
            if (!authored.binding) continue;
            Base::Object* source = ResolveBehaviorBindingSource(
                *authored.binding, *record.instance, owner, names);
            if ((!authored.binding->GetElementName().Empty() ||
                 authored.binding->GetSource() ||
                 authored.binding->GetRelativeSource()) &&
                source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Behavior Binding source was not found");
            }
            Data::MetadataBindingDescriptor descriptor;
            descriptor.metadata = metadata;
            descriptor.source = source;
            descriptor.target = record.instance.Get();
            descriptor.targetProperty = authored.property;
            descriptor.dataContextProperty =
                FrameworkElement::DataContextProperty.Handle();
            descriptor.dataContextOwner = &owner;
            descriptor.path = authored.binding->GetPath().GetPath();
            descriptor.stringFormat =
                authored.binding->GetStringFormat();
            descriptor.bindsToSource = descriptor.path.Empty();
            descriptor.mode = authored.binding->GetMode() ==
                    Data::BindingMode::Default
                ? Data::BindingMode::OneWay
                : authored.binding->GetMode();
            descriptor.updateSourceTrigger =
                authored.binding->GetUpdateSourceTrigger() ==
                    Meta::UpdateSourceTrigger::Default
                ? Meta::UpdateSourceTrigger::PropertyChanged
                : authored.binding->GetUpdateSourceTrigger();
            descriptor.fallbackValue =
                authored.binding->GetFallbackValue();
            descriptor.targetNullValue =
                authored.binding->GetTargetNullValue();
            Base::Result<Data::BindingHandle> attached =
                bindings->Attach(descriptor);
            if (!attached) {
                for (const Data::BindingHandle handle : record.bindings) {
                    static_cast<void>(bindings->Detach(handle));
                }
                return attached.GetStatus();
            }
            Base::Result<void> retained = record.bindings.PushBack(
                attached.Value());
            if (!retained) {
                static_cast<void>(bindings->Detach(attached.Value()));
                for (const Data::BindingHandle handle : record.bindings) {
                    static_cast<void>(bindings->Detach(handle));
                }
                return retained.GetStatus();
            }
        }
        Base::Result<void> attached = record.instance->Attach(owner);
        if (!attached) {
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(bindings->Detach(handle));
            }
            return attached.GetStatus();
        }
        Base::Result<void> retained = attachedBehaviorInstances.PushBack(
            std::move(record));
        if (!retained) {
            record.instance->Detach();
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(bindings->Detach(handle));
            }
            return retained.GetStatus();
        }
        return {};
    }

void InteractivityEngine::ClearDataTemplateTriggerProviders(
        Aero::Controls::DataTemplateTriggerState& context) noexcept {
        if (values != nullptr) {
            for (Aero::Controls::DataTemplatePropertyTrigger& trigger :
                 context.triggers) {
                for (Aero::Controls::DataTemplateTriggerSetter& setter :
                     trigger.setters) {
                    Base::Ref<DependencyObject> target =
                        setter.target.Lock();
                    if (!target || !setter.token.IsValid()) continue;
                    static_cast<void>(
                        values->ClearProviderContribution(
                            *target,
                            setter.property,
                            setter.token));
                    setter.token = {};
                }
                trigger.active = false;
            }
        }
        context.providerOrigin = 0U;
    }

void InteractivityEngine::ClearDataTemplateTriggerProvidersInSubtree(
        Aero::Media::Visual& visual) noexcept {
        Aero::FrameworkElement* element =
            visual.AsFrameworkElement();
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authored :
                 AeroGuiInternal::AuthoredTriggers(*element)) {
                if (authored && authored->RuntimeType() ==
                    Aero::Controls::DataTemplateTriggerState::StaticTypeId()) {
                    ClearDataTemplateTriggerProviders(
                        static_cast<Aero::Controls::DataTemplateTriggerState&>(
                            *authored));
                }
            }
        }
        for (Aero::Media::Visual* child : visual.GetVisualChildren()) {
            if (child != nullptr) {
                ClearDataTemplateTriggerProvidersInSubtree(*child);
            }
        }
    }

void InteractivityEngine::DetachBehaviorsInSubtree(Aero::Media::Visual& visual) noexcept {
        for (std::uint32_t index = 0U;
             index < attachedBehaviorInstances.Size();) {
            AttachedBehaviorInstance& record =
                attachedBehaviorInstances[index];
            if (record.target == nullptr ||
                !IsInVisualSubtree(record.target, visual)) {
                ++index;
                continue;
            }
            for (const Data::BindingHandle handle : record.bindings) {
                if (bindings != nullptr) {
                    static_cast<void>(bindings->Detach(handle));
                }
            }
            if (record.instance) record.instance->Detach();
            if (index + 1U != attachedBehaviorInstances.Size()) {
                attachedBehaviorInstances[index] =
                    std::move(attachedBehaviorInstances.Back());
            }
            attachedBehaviorInstances.PopBack();
        }
    }

void InteractivityEngine::ClearAnimationSubscriptionsFor(
        Aero::Media::Visual& fragmentRoot) noexcept {
        DetachBehaviorsInSubtree(fragmentRoot);
        ClearDataTemplateTriggerProvidersInSubtree(fragmentRoot);
        for (std::uint32_t index = 0U;
             index < dataTemplateTriggerSubscriptions.Size();) {
            DataTemplateTriggerSubscription& subscription =
                dataTemplateTriggerSubscriptions[index];
            const bool sourceMatches =
                subscription.source != nullptr &&
                metadata->Types().IsDerivedFrom(
                    subscription.source->RuntimeType(),
                    Aero::Media::Visual::StaticTypeId()) &&
                IsInVisualSubtree(
                    static_cast<Aero::Media::Visual*>(
                        subscription.source), fragmentRoot);
            const bool contextMatches =
                subscription.context != nullptr &&
                subscription.context->triggerContext &&
                subscription.context->triggerContext->root != nullptr &&
                IsInVisualSubtree(
                    subscription.context->
                        triggerContext->root,
                    fragmentRoot);
            const bool matches =
                sourceMatches || contextMatches;
            if (!matches) {
                ++index;
                continue;
            }
            static_cast<void>(
                subscription.source->RemoveValueChangedHandler(
                    subscription.property, subscription.handler));
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            for (std::uint32_t next = index + 1U;
                 next < dataTemplateTriggerSubscriptions.Size(); ++next) {
                dataTemplateTriggerSubscriptions[next - 1U] =
                    std::move(dataTemplateTriggerSubscriptions[next]);
            }
            dataTemplateTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < propertyChangedTriggerSubscriptions.Size();) {
            PropertyChangedTriggerSubscription& subscription =
                propertyChangedTriggerSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U !=
                propertyChangedTriggerSubscriptions.Size()) {
                propertyChangedTriggerSubscriptions[index] =
                    std::move(
                        propertyChangedTriggerSubscriptions.Back());
            }
            propertyChangedTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < interactionDataTriggerSubscriptions.Size();) {
            InteractionDataTriggerSubscription& subscription =
                interactionDataTriggerSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U !=
                interactionDataTriggerSubscriptions.Size()) {
                interactionDataTriggerSubscriptions[index] =
                    std::move(
                        interactionDataTriggerSubscriptions.Back());
            }
            interactionDataTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < keyTriggerSubscriptions.Size();) {
            KeyTriggerSubscription& subscription =
                keyTriggerSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                static_cast<void>(subscription.source->RemoveHandler(
                    Aero::UIElement::KeyDownEvent.Handle(),
                    subscription.handler));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U != keyTriggerSubscriptions.Size()) {
                keyTriggerSubscriptions[index] =
                    std::move(keyTriggerSubscriptions.Back());
            }
            keyTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < animationEventSubscriptions.Size();) {
            AnimationEventSubscription& subscription =
                animationEventSubscriptions[index];
            if (subscription.visualOwner == nullptr ||
                !IsInVisualSubtree(
                    subscription.visualOwner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                if (subscription.contentSource) {
                    static_cast<void>(
                        static_cast<Aero::ContentElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                } else {
                    static_cast<void>(
                        static_cast<Aero::UIElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                }
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            for (std::uint32_t next = index + 1U;
                 next < animationEventSubscriptions.Size(); ++next) {
                animationEventSubscriptions[next - 1U] =
                    std::move(animationEventSubscriptions[next]);
            }
            animationEventSubscriptions.PopBack();
        }
        if (storyboards == nullptr) return;
        for (std::uint32_t index = 0U;
             index < storyboards->storyboardSessions.Size();) {
            StoryboardHost::StoryboardSession& session = storyboards->storyboardSessions[index];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            storyboards->CancelStoryboardCompletionSessions(session.handles.AsSpan());
            if (animations != nullptr) {
                for (Aero::Media::Animation::Model::AnimationHandle handle : session.handles) {
                    static_cast<void>(animations->Remove(handle));
                }
            }
            for (std::uint32_t next = index + 1U;
                 next < storyboards->storyboardSessions.Size(); ++next) {
                storyboards->storyboardSessions[next - 1U] =
                    std::move(storyboards->storyboardSessions[next]);
            }
            storyboards->storyboardSessions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < storyboards->storyboardCompletionSessions.Size();) {
            StoryboardHost::StoryboardCompletionSession& session =
                storyboards->storyboardCompletionSessions[index];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (animations != nullptr) {
                for (Aero::Media::Animation::Model::AnimationHandle handle :
                     session.handles) {
                    static_cast<void>(animations->Remove(handle));
                }
            }
            for (std::uint32_t next = index + 1U;
                 next < storyboards->storyboardCompletionSessions.Size(); ++next) {
                storyboards->storyboardCompletionSessions[next - 1U] =
                    std::move(storyboards->storyboardCompletionSessions[next]);
            }
            storyboards->storyboardCompletionSessions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < storyboards->storyboardCompletedSubscriptions.Size();) {
            const StoryboardHost::StoryboardCompletedSubscription& subscription =
                storyboards->storyboardCompletedSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(
                    subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            for (std::uint32_t next = index + 1U;
                 next < storyboards->storyboardCompletedSubscriptions.Size(); ++next) {
                storyboards->storyboardCompletedSubscriptions[next - 1U] =
                    std::move(storyboards->storyboardCompletedSubscriptions[next]);
            }
            storyboards->storyboardCompletedSubscriptions.PopBack();
        }
    }

void InteractivityEngine::ClearAnimationEventSubscriptions() noexcept {
        for (DataTemplateTriggerSubscription&
                 subscription :
             dataTemplateTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->
                        RemoveValueChangedHandler(
                            subscription.property,
                            subscription.handler));
            }
            FreeObject(
                *allocator,
                Base::MemoryTag::Ui,
                subscription.context);
        }
        dataTemplateTriggerSubscriptions.Clear();
        for (PropertyChangedTriggerSubscription& subscription :
             propertyChangedTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
        }
        propertyChangedTriggerSubscriptions.Clear();
        for (InteractionDataTriggerSubscription& subscription :
             interactionDataTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
        }
        interactionDataTriggerSubscriptions.Clear();
        for (KeyTriggerSubscription& subscription :
             keyTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(subscription.source->RemoveHandler(
                    Aero::UIElement::KeyDownEvent.Handle(),
                    subscription.handler));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
        }
        keyTriggerSubscriptions.Clear();
        for (AnimationEventSubscription& subscription :
             animationEventSubscriptions) {
            if (subscription.source != nullptr) {
                if (subscription.contentSource) {
                    static_cast<void>(
                        static_cast<Aero::ContentElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                } else {
                    static_cast<void>(
                        static_cast<Aero::UIElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                }
            }
            FreeObject(
                *allocator,
                Base::MemoryTag::Ui,
                subscription.context);
        }
        animationEventSubscriptions.Clear();
        if (storyboards != nullptr) {
            storyboards->storyboardCompletionSessions.Clear();
            storyboards->storyboardCompletedSubscriptions.Clear();
        }
        animationEventStatus = Base::Status::Ok();
    }


void InteractivityEngine::StyleDataTriggerHandlerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept
{
    if (runtime == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> evaluated =
        runtime->EvaluateStyleDataTrigger(*this);
    if (!evaluated) {
        runtime->animationEventStatus =
            evaluated.GetStatus();
    }
}

void InteractivityEngine::
DataTemplateTriggerHandlerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&)
    noexcept
{
    if (runtime == nullptr ||
        !triggerContext ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> evaluated =
        runtime->EvaluateDataTemplateTrigger(
            *triggerContext,
            triggerIndex);
    if (!evaluated) {
        runtime->animationEventStatus =
            evaluated.GetStatus();
    }
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
        Aero::UIElement* expected = owner->AsUIElement();
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


} // namespace Aero
