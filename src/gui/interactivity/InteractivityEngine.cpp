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
        if (storyboards == nullptr) return;
        storyboards->ClearEventTriggersFor(fragmentRoot);
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
        if (storyboards != nullptr) {
            storyboards->ClearEventTriggers();
            storyboards->storyboardCompletionSessions.Clear();
            storyboards->storyboardCompletedSubscriptions.Clear();
        }
        animationEventStatus = Base::Status::Ok();
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

} // namespace Aero
