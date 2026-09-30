// InteractivityEngine: behaviors, interaction triggers, and style triggers.
#include "gui/ViewFrame.hpp"
#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/core/ValueConversion.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include "gui/controls/ItemsContainers.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include <cstdio>
#include <Aero/Interactivity/InteractionTriggers.hpp>

namespace Aero {

using namespace ::Aero;
using namespace Media::Animation;
using namespace Controls;
using namespace Data;
using namespace Interactivity;
using namespace Media;
using Model::AnimationHandle;

namespace {

bool ParseInteractionActionPath(
    Base::StringView path,
    std::uint32_t& triggerIndex,
    std::uint32_t& actionIndex) noexcept {
    constexpr Base::StringView prefix(
        "(b:Interaction.Triggers)[");
    constexpr Base::StringView middle("].Actions[");
    if (path.SizeBytes() <=
            prefix.SizeBytes() + middle.SizeBytes() + 1U ||
        path.Substr(0U, prefix.SizeBytes()) != prefix) {
        return false;
    }
    std::uint32_t cursor = prefix.SizeBytes();
    std::uint64_t parsedTrigger = 0U;
    const std::uint32_t triggerBegin = cursor;
    while (cursor < path.SizeBytes() &&
        path[cursor] >= '0' && path[cursor] <= '9') {
        parsedTrigger = parsedTrigger * 10U +
            static_cast<std::uint64_t>(path[cursor] - '0');
        if (parsedTrigger > UINT32_MAX) return false;
        ++cursor;
    }
    if (cursor == triggerBegin ||
        cursor + middle.SizeBytes() > path.SizeBytes() ||
        path.Substr(cursor, middle.SizeBytes()) != middle) {
        return false;
    }
    cursor += middle.SizeBytes();
    std::uint64_t parsedAction = 0U;
    const std::uint32_t actionBegin = cursor;
    while (cursor < path.SizeBytes() &&
        path[cursor] >= '0' && path[cursor] <= '9') {
        parsedAction = parsedAction * 10U +
            static_cast<std::uint64_t>(path[cursor] - '0');
        if (parsedAction > UINT32_MAX) return false;
        ++cursor;
    }
    if (cursor == actionBegin ||
        cursor + 1U != path.SizeBytes() ||
        path[cursor] != ']') {
        return false;
    }
    triggerIndex = static_cast<std::uint32_t>(parsedTrigger);
    actionIndex = static_cast<std::uint32_t>(parsedAction);
    return true;
}

Base::Result<Meta::PropertyValue> ResolveInteractionActionPath(
    Base::Span<const Base::Ref<Base::Object>> triggers,
    std::uint32_t triggerIndex,
    std::uint32_t actionIndex) noexcept {
    if (triggerIndex >= triggers.Size() ||
        !triggers[triggerIndex]) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Interaction.Triggers index is out of range");
    }
    auto* eventTrigger =
        TryCast<EventTrigger>(
            triggers[triggerIndex].Get());
    if (eventTrigger == nullptr ||
        actionIndex >= eventTrigger->GetActions().Size() ||
        !eventTrigger->GetActions()[actionIndex]) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Interaction Trigger Actions index is out of range");
    }
    Base::Ref<TriggerAction> action =
        eventTrigger->GetActions()[actionIndex];
    const Meta::TypeId actionType = action->RuntimeType();
    return Meta::PropertyValue::FromObject(
        actionType,
        Base::Ref<Base::Object>(std::move(action)));
}

} // namespace

InteractivityEngine::InteractivityEngine(ViewFrame& owner) noexcept
    : view(&owner),
      styleDataTriggerSubscriptions(owner.allocator),
      attachedBehaviorInstances(owner.allocator),
      propertyChangedTriggerSubscriptions(owner.allocator),
      interactionDataTriggerSubscriptions(owner.allocator),
      pendingInteractionTriggers(owner.allocator),
      pendingStyleDataTriggerEvaluations(owner.allocator),
      keyTriggerSubscriptions(owner.allocator),
      dataTemplateTriggerSubscriptions(owner.allocator) {}

void InteractivityEngine::Bind() noexcept {
    // ElementTree / ViewFrame services are read on demand; Bind only marks
    // the host as attached to its owning view (already set in the ctor).
}

Base::IAllocator* InteractivityEngine::Allocator() const noexcept {
    return view != nullptr ? view->allocator : nullptr;
}

Meta::Registry* InteractivityEngine::Metadata() const noexcept {
    return view != nullptr ? view->metadata : nullptr;
}

AnimationEngine* InteractivityEngine::Animations() const noexcept {
    return view != nullptr ? view->Animations() : nullptr;
}

InputRouter* InteractivityEngine::Input() const noexcept {
    return view != nullptr ? view->Input() : nullptr;
}

ElementTree* InteractivityEngine::Tree() const noexcept {
    return view != nullptr ? view->tree : nullptr;
}

StyleEngine* InteractivityEngine::Styles() const noexcept {
    return view != nullptr ? view->Styles() : nullptr;
}

Meta::EffectiveValueEngine* InteractivityEngine::Values() const noexcept {
    return view != nullptr ? view->values : nullptr;
}

BindingEngine* InteractivityEngine::Bindings() const noexcept {
    return view != nullptr ? view->Bindings() : nullptr;
}

StoryboardHost* InteractivityEngine::Storyboards() const noexcept {
    return view != nullptr ? view->storyboards : nullptr;
}

bool InteractivityEngine::IsInVisualSubtree(
        Visual* node,
        const Visual& fragmentRoot) const noexcept {
    while (node != nullptr) {
        if (node == &fragmentRoot) return true;
        node = TryCast<Visual>(node->GetLogicalParent()) != nullptr ? TryCast<Visual>(node->GetLogicalParent()) : node->GetVisualParent();
    }
    return false;
}


Base::Result<bool> InteractivityEngine::ConditionBehaviorsAllowExecution(
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        FrameworkElement& owner,
        const NameScope* names) noexcept {
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
        const auto evaluate = [&](const ComparisonCondition& condition)
            noexcept -> Base::Result<bool> {
            const Base::Ref<Binding> binding = condition.GetLeftOperand();
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
                ComparisonCondition::Operator::Equal) {
                return current.Value().Equals(expected);
            }
            if (comparison ==
                ComparisonCondition::Operator::NotEqual) {
                return !current.Value().Equals(expected);
            }
            long double left = 0.0L;
            long double right = 0.0L;
            if (numeric(current.Value(), left) && numeric(expected, right)) {
                switch (comparison) {
                case ComparisonCondition::Operator::LessThan:
                    return left < right;
                case ComparisonCondition::Operator::LessThanOrEqual:
                    return left <= right;
                case ComparisonCondition::Operator::GreaterThan:
                    return left > right;
                case ComparisonCondition::Operator::GreaterThanOrEqual:
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
                case ComparisonCondition::Operator::LessThan:
                    return order < 0;
                case ComparisonCondition::Operator::LessThanOrEqual:
                    return order <= 0;
                case ComparisonCondition::Operator::GreaterThan:
                    return order > 0;
                case ComparisonCondition::Operator::GreaterThanOrEqual:
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
                ConditionBehavior::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Interaction trigger contains an unsupported behavior");
            }
            const Base::Ref<ConditionalExpression> expression =
                static_cast<ConditionBehavior&>(
                    *behavior).GetExpression();
            if (!expression) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior has no expression");
            }
            const bool conjunction = expression->GetChaining() ==
                ConditionalExpression::ForwardChaining::And;
            bool expressionResult = conjunction;
            bool hasCondition = false;
            for (const Base::Ref<ComparisonCondition>& condition :
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
    return ComparePropertyValues(actual, std::move(expected), Metadata());
}

Base::Result<bool> InteractivityEngine::EvaluateTriggerComparison(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected,
        Base::StringView comparison) noexcept {
    return ComparePropertyValues(
        actual, std::move(expected), Metadata(), comparison);
}

Base::Object* InteractivityEngine::ResolveDataTemplateConditionSource(
        DataTemplateTriggerInstance& context,
        DataTemplateTriggerCondition& condition,
        Base::StringView& path) noexcept {
        path = condition.binding
            ? condition.binding->GetPath().GetPath()
            : Base::StringView{};
        Base::Object* source = nullptr;
        if (condition.binding &&
            !condition.binding->GetElementName().Empty()) {
            const Base::StringView elementName =
                condition.binding->GetElementName();
            source = context.FindName(elementName);
            Visual* current = context.root;
            while (source == nullptr && current != nullptr) {
                if (auto* framework =
                        TryCast<FrameworkElement>(current)) {
                    source = framework->FindName(elementName);
                }
                Visual* logical =
                    TryCast<Visual>(
                        current->GetLogicalParent());
                current = logical != nullptr
                    ? logical
                    : current->GetVisualParent();
            }
            if (source == nullptr && view != nullptr) {
                source = view->loadedDocument.names.Find(elementName);
            }
        } else if (condition.binding &&
                   condition.binding->GetRelativeSource() &&
                   context.root != nullptr) {
            // DataTrigger Bindings with RelativeSource (FindAncestor,
            // TemplatedParent, Self) are resolved from the generated template
            // root, not the item payload compiled into condition.source.
            Base::Object* selfObject = context.root;
            if (condition.binding->GetRelativeSource()->GetMode() ==
                    Data::RelativeSourceMode::Self &&
                context.root->GetTemplatedParent() != nullptr) {
                selfObject = context.root->GetTemplatedParent();
            }
            source = ResolveAuthoredBindingSource(
                *condition.binding,
                *context.root,
                &context,
                nullptr,
                selfObject);
        } else {
            Base::Ref<Base::Object> retainedSource =
                condition.source.Lock();
            source = retainedSource.Get();
            if (source == nullptr && context.root != nullptr) {
                source = context.root;
            }
        }

        if (condition.usesDataContext && source != nullptr &&
            Metadata() != nullptr &&
            Metadata()->Types().IsDerivedFrom(
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
            Metadata() != nullptr &&
            Metadata()->Types().IsDerivedFrom(
                source->RuntimeType(), FrameworkElement::StaticTypeId())) {
            source = static_cast<FrameworkElement*>(source)->GetTemplatedParent();
            path = path.Substr(
                TemplatedParentPrefix.SizeBytes(),
                path.SizeBytes() - TemplatedParentPrefix.SizeBytes());
        }
        return source;
    }

Base::Result<bool> InteractivityEngine::EvaluateDataTemplateCondition(
        DataTemplateTriggerInstance& context,
        DataTemplateTriggerCondition& condition) noexcept {
        Meta::PropertyValue current;
        Base::Ref<DependencyObject> dependencySource =
            condition.dependencySource.Lock();
        if ((!dependencySource || !condition.property.IsValid()) &&
            context.root != nullptr &&
            !condition.binding &&
            condition.property.IsValid()) {
            dependencySource =
                Base::Ref<DependencyObject>::FromBorrowed(
                    *static_cast<DependencyObject*>(context.root));
            condition.dependencySource =
                Base::WeakRef<DependencyObject>(dependencySource);
        }
        if (dependencySource &&
            condition.property.IsValid()) {
            Base::Result<Meta::PropertyValue> value =
                dependencySource->GetValue(condition.property);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        } else {
            if (!condition.binding || Metadata() == nullptr) {
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
            Base::Result<Meta::PropertyValue> value =
                EvaluateAuthoredBinding(
                    *condition.binding,
                    *context.root,
                    &context,
                    nullptr,
                    source);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        }
        return DataTemplateTriggerValuesMatch(current, condition.value);
    }

Base::Result<void> InteractivityEngine::EnsureDataTemplateProviderTokens(
        DataTemplateTriggerInstance& context) noexcept {
        if (Values() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger value engine is unavailable");
        }
        if (context.providerOrigin == 0U) {
            Base::Result<std::uint32_t> origin =
                Values()->AllocateProviderOrigin();
            if (!origin) return origin.GetStatus();
            context.providerOrigin = origin.Value();
        }

        std::uint64_t ordinal = 0U;
        for (DataTemplatePropertyTrigger& trigger :
             context.triggers) {
            for (DataTemplateTriggerSetter& setter :
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
        DataTemplateTriggerInstance& context,
        std::uint32_t triggerIndex) noexcept {
        if (triggerIndex >= context.triggers.Size() ||
            context.root == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger runtime is unavailable");
        }
        Base::Result<void> clrAttached =
            AttachDataTemplateClrSubscription(context, triggerIndex);
        if (!clrAttached) return clrAttached.GetStatus();
        DataTemplatePropertyTrigger& trigger =
            context.triggers[triggerIndex];
        if (!trigger.setters.Empty()) {
            if (Values() == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate Trigger runtime is unavailable");
            }
            Base::Result<void> providerTokens =
                EnsureDataTemplateProviderTokens(context);
            if (!providerTokens) return providerTokens.GetStatus();
        }
        bool active = !trigger.conditions.Empty();
        for (DataTemplateTriggerCondition& condition :
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
            for (const DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                Base::Ref<DependencyObject> target =
                    setter.target.Lock();
                if (!target) continue;
                Base::Result<void> applied =
                    Values()->SetProviderContribution(
                        *target,
                        setter.property,
                        setter.token,
                        setter.value);
                if (!applied) {
                    return applied.GetStatus();
                }
            }
        } else {
            for (const DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                Base::Ref<DependencyObject> target =
                    setter.target.Lock();
                if (!target) continue;
                Base::Result<bool> cleared =
                    Values()->ClearProviderContribution(
                        *target,
                        setter.property,
                        setter.token);
                if (!cleared) {
                    return cleared.GetStatus();
                }
            }
        }
        if (Values() != nullptr && !Values()->IsFlushing()) {
            static_cast<void>(Values()->Flush());
        }

        Base::Span<const Base::Ref<Base::Object>> actions =
            active
            ? trigger.enterActions.AsSpan()
            : trigger.exitActions.AsSpan();
        for (const Base::Ref<Base::Object>& authored :
             actions) {
            if (!authored) continue;
            const bool isAction =
                Metadata()->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    TriggerAction::
                        StaticTypeId());
            if (!isAction) continue;
            Base::Result<void> executed =
                Storyboards()->ExecuteAnimationAction(
                    static_cast<
                        TriggerAction&>(
                            *authored),
                    *context.root,
                    &context);
            if (!executed && view != nullptr) {
                view->ReportUpdateFailure(executed.GetStatus());
            }
        }
        trigger.active = active;
        return {};
    }

Base::Result<void> InteractivityEngine::AttachDataTemplateClrSubscription(
        DataTemplateTriggerInstance& context,
        std::uint32_t triggerIndex) noexcept {
        if (Metadata() == nullptr || triggerIndex >= context.triggers.Size()) {
            return {};
        }
        DataTemplatePropertyTrigger& trigger =
            context.triggers[triggerIndex];
        for (std::uint32_t conditionIndex = 0U;
             conditionIndex < trigger.conditions.Size();
             ++conditionIndex) {
            DataTemplateTriggerCondition& condition =
                trigger.conditions[conditionIndex];
            if (!condition.binding) continue;
            Base::StringView path;
            Base::Object* source = ResolveDataTemplateConditionSource(
                context, condition, path);
            if (source == nullptr) continue;
            if (Metadata()->Types().IsDerivedFrom(
                    source->RuntimeType(),
                    DependencyObject::StaticTypeId())) {
                const Meta::DependencyProperty* property =
                    (*Metadata()).DependencyProperties()
                        .Find(source->RuntimeType(), path);
                if (property != nullptr) continue;
            }
            bool alreadyAttached = false;
            for (std::uint32_t index = 0U;
                 index < dataTemplateTriggerSubscriptions.Size();
                 ++index) {
                DataTemplateTriggerSubscription& existing =
                    dataTemplateTriggerSubscriptions[index];
                if (existing.context == nullptr ||
                    existing.context->triggerContext.Get() != &context ||
                    existing.context->triggerIndex != triggerIndex ||
                    existing.context->conditionIndex != conditionIndex) {
                    continue;
                }
                if (existing.metadataSource == source) {
                    alreadyAttached = true;
                    break;
                }
                if (existing.metadataSource != nullptr &&
                    existing.metadataSubscription != 0U) {
                    static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                        *existing.metadataSource,
                        existing.metadataSubscription));
                    existing.metadataSource = nullptr;
                    existing.metadataSubscription = 0U;
                }
            }
            if (alreadyAttached) continue;

            DataTemplateTriggerHandlerState* handlerContext = nullptr;
            Base::Result<void> created = AllocateObject(
                *Allocator(), Base::MemoryTag::Ui, handlerContext);
            if (!created) return created.GetStatus();
            handlerContext->runtime = this;
            handlerContext->triggerContext =
                Base::Ref<DataTemplateTriggerInstance>::
                    FromBorrowed(context);
            handlerContext->triggerIndex = triggerIndex;
            handlerContext->conditionIndex = conditionIndex;
            if (!path.Empty()) {
                const Meta::PropertyInfo* clrProperty =
                    Metadata()->Types().FindProperty(
                        source->RuntimeType(), path, true);
                if (clrProperty != nullptr) {
                    handlerContext->metadataProperty = clrProperty->Id();
                }
            }
            Base::Result<std::uint64_t> notification =
                Metadata()->SubscribePropertyChanged(
                    *source,
                    &DataTemplateTriggerHandlerState::MetadataInvoke,
                    handlerContext);
            if (!notification) {
                FreeObject(*Allocator(), Base::MemoryTag::Ui, handlerContext);
                return notification.GetStatus();
            }
            if (notification.Value() == 0U) {
                FreeObject(*Allocator(), Base::MemoryTag::Ui, handlerContext);
                continue;
            }
            DataTemplateTriggerSubscription record;
            record.metadataSource = source;
            record.metadataSubscription = notification.Value();
            record.context = handlerContext;
            dataTemplateTriggerSubscriptions.PushBack(std::move(record));
        }
        return {};
    }

Base::Result<std::uint32_t>
 InteractivityEngine::StartDataTemplateTriggers(
        DataTemplateTriggerInstance&
            context) noexcept {
        std::uint32_t count = 0U;
        for (std::uint32_t triggerIndex = 0U;
             triggerIndex < context.triggers.Size();
             ++triggerIndex) {
            DataTemplatePropertyTrigger&
                trigger =
                    context.triggers[triggerIndex];
            for (std::uint32_t conditionIndex = 0U;
                 conditionIndex <
                     trigger.conditions.Size();
                 ++conditionIndex) {
                DataTemplateTriggerCondition&
                    condition =
                        trigger.conditions[conditionIndex];
                Base::Ref<DependencyObject> dependencySource =
                    condition.dependencySource.Lock();
                if ((!dependencySource || !condition.property.IsValid()) &&
                    context.root != nullptr &&
                    !condition.binding &&
                    condition.property.IsValid()) {
                    dependencySource =
                        Base::Ref<DependencyObject>::FromBorrowed(
                            *static_cast<DependencyObject*>(context.root));
                    condition.dependencySource =
                        Base::WeakRef<DependencyObject>(dependencySource);
                }
                if ((!dependencySource ||
                     !condition.property.IsValid()) &&
                    condition.binding) {
                    Base::StringView path;
                    Base::Object* source =
                        ResolveDataTemplateConditionSource(
                            context, condition, path);
                    if (source != nullptr &&
                        Metadata()->Types().IsDerivedFrom(
                            source->RuntimeType(),
                            DependencyObject::
                                StaticTypeId())) {
                        const Meta::DependencyProperty*
                            property =
                                (*Metadata()).DependencyProperties()
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
                        *Allocator(),
                        Base::MemoryTag::Ui,
                        handlerContext);
                if (!created) {
                    return created.GetStatus();
                }
                handlerContext->runtime = this;
                handlerContext->triggerContext =
                    Base::Ref<
                        DataTemplateTriggerInstance>::
                        FromBorrowed(context);
                handlerContext->triggerIndex =
                    triggerIndex;
                handlerContext->conditionIndex =
                    conditionIndex;
                auto callback =
                    [handlerContext](
                        DependencyObject& object,
                        const Meta::
                            DependencyPropertyChangedEventArgs&
                                args) noexcept {
                        handlerContext->Invoke(
                            object, args);
                    };
                Meta::DependencyPropertyChangedEventHandler
                    handler(callback);
                dependencySource->AddValueChangedHandler(
                    condition.property, handler);
                DataTemplateTriggerSubscription record;
                record.source = dependencySource.Get();
                record.property = condition.property;
                record.handler = handler;
                record.context = handlerContext;
                dataTemplateTriggerSubscriptions.
                    PushBack(std::move(record));
                ++count;
            }
            bool watchesDataContext = false;
            for (const DataTemplateTriggerCondition&
                     condition : trigger.conditions) {
                watchesDataContext = watchesDataContext ||
                    condition.usesDataContext;
            }
            if (watchesDataContext && context.root != nullptr) {
                FrameworkElement* dcOwner = context.root;
                if (auto* templated = TryCast<FrameworkElement>(
                        dcOwner->GetTemplatedParent())) {
                    dcOwner = templated;
                }
                bool alreadyWatching = false;
                for (const DataTemplateTriggerSubscription& existing :
                     dataTemplateTriggerSubscriptions) {
                    alreadyWatching = alreadyWatching ||
                        (existing.context != nullptr &&
                         existing.context->triggerContext.Get() ==
                             &context &&
                         existing.context->triggerIndex == triggerIndex &&
                         existing.source == dcOwner &&
                         existing.property ==
                             FrameworkElement::DataContextProperty.Handle());
                }
                if (!alreadyWatching) {
                    DataTemplateTriggerHandlerState* handlerContext = nullptr;
                    Base::Result<void> created = AllocateObject(
                        *Allocator(), Base::MemoryTag::Ui, handlerContext);
                    if (!created) return created.GetStatus();
                    handlerContext->runtime = this;
                    handlerContext->triggerContext =
                        Base::Ref<DataTemplateTriggerInstance>::
                            FromBorrowed(context);
                    handlerContext->triggerIndex = triggerIndex;
                    handlerContext->conditionIndex = 0U;
                    auto callback =
                        [handlerContext](
                            DependencyObject& object,
                            const Meta::DependencyPropertyChangedEventArgs&
                                args) noexcept {
                            handlerContext->Invoke(object, args);
                        };
                    Meta::DependencyPropertyChangedEventHandler handler(
                        callback);
                    dcOwner->AddValueChangedHandler(
                        FrameworkElement::DataContextProperty.Handle(),
                        handler);
                    DataTemplateTriggerSubscription record;
                    record.source = dcOwner;
                    record.property =
                        FrameworkElement::DataContextProperty.Handle();
                    record.handler = handler;
                    record.context = handlerContext;
                    dataTemplateTriggerSubscriptions.PushBack(
                            std::move(record));
                    ++count;
                }
            }
            Base::Result<void> evaluated =
                EvaluateDataTemplateTrigger(
                    context, triggerIndex);
            if (!evaluated && view != nullptr) {
                view->ReportUpdateFailure(evaluated.GetStatus());
            }
        }
        return count;
    }

Base::Object* InteractivityEngine::ResolveAuthoredBindingSource(
        const Binding& binding,
        FrameworkElement& owner,
        DataTemplateTriggerInstance*
            dataTemplateContext,
        const NameScope* names,
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

        const Base::Ref<RelativeSource> relative =
            binding.GetRelativeSource();
        if (relative) {
            if (relative->GetMode() ==
                RelativeSourceMode::Self) {
                return self != nullptr
                    ? self
                    : static_cast<Base::Object*>(&owner);
            }
            if (relative->GetMode() ==
                RelativeSourceMode::TemplatedParent) {
                return owner.GetTemplatedParent();
            }
            if (relative->GetMode() !=
                RelativeSourceMode::FindAncestor) {
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
            // The Binding is authored on a Trigger/Action object. Its
            // inheritance context is the associated element, so that element
            // is the first FindAncestor candidate (unlike a Binding authored
            // directly on a visual, which starts at the visual's parent).
            Visual* current = &owner;
            while (current != nullptr) {
                const Meta::TypeInfo* type =
                    Metadata() != nullptr
                    ? Metadata()->Types().FindType(
                          current->RuntimeType())
                    : nullptr;
                const bool matches = ancestorName.Empty() ||
                    (type != nullptr &&
                     type->Name() == ancestorName);
                if (matches &&
                    ++matched == relative->GetAncestorLevel()) {
                    return current;
                }
                Visual* next = TryCast<Visual>(current->GetLogicalParent());
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
        const Binding& binding,
        FrameworkElement& owner,
        DataTemplateTriggerInstance*
            dataTemplateContext,
        const NameScope* names,
        Base::Object* self) noexcept {
        if (Metadata() == nullptr) {
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
        if (source == nullptr && self != nullptr) {
            source = self;
        }
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
            std::uint32_t triggerIndex = 0U;
            std::uint32_t actionIndex = 0U;
            if (ParseInteractionActionPath(
                    path, triggerIndex, actionIndex)) {
                auto* element = TryCast<FrameworkElement>(source);
                if (element == nullptr) {
                    value = Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Interaction.Triggers source is not a FrameworkElement");
                } else {
                    Base::Span<const Base::Ref<Base::Object>> triggers =
                        element->StyleTriggerPrototypes();
                    if (triggers.Empty()) {
                        triggers = element->AuthoredTriggers();
                    }
                    value = ResolveInteractionActionPath(
                        triggers, triggerIndex, actionIndex);
                }
            } else {
                Meta::BindingPathCompileError pathError;
                Base::Result<Meta::BindingPathPlan> plan =
                    Meta::BindingPathPlan::Compile(
                        *Metadata(),
                        source->RuntimeType(),
                        path,
                        &pathError);
                if (plan) {
                    value = plan.Value().Get(*Metadata(), *source);
                } else {
                    value = plan.GetStatus();
                }
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
                BoxedItemValue::StaticTypeId()) {
            resolved = static_cast<const BoxedItemValue&>(
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
        FrameworkElement& owner,
        const NameScope* names) noexcept {
        for (const Base::Ref<Base::Object>& authored : actions) {
            if (!authored || Metadata() == nullptr ||
                !Metadata()->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    TriggerAction::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Interaction Trigger contains an invalid action");
            }
            Base::Result<void> executed = Storyboards()->ExecuteAnimationAction(
                static_cast<TriggerAction&>(*authored),
                owner,
                nullptr,
                names);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

Base::Result<void> InteractivityEngine::ExecuteTriggerActions(
        Base::Span<const Base::Ref<TriggerAction>> actions,
        FrameworkElement& owner,
        const NameScope* names) noexcept {
        for (const Base::Ref<TriggerAction>& action :
             actions) {
            if (!action) continue;
            Base::Result<void> executed = Storyboards()->ExecuteAnimationAction(
                *action, owner, nullptr, names);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

void InteractivityEngine::ClearDataTemplateTriggerProviders(
        DataTemplateTriggerInstance& context) noexcept {
        if (Values() != nullptr) {
            for (DataTemplatePropertyTrigger& trigger :
                 context.triggers) {
                for (DataTemplateTriggerSetter& setter :
                     trigger.setters) {
                    Base::Ref<DependencyObject> target =
                        setter.target.Lock();
                    if (!target || !setter.token.IsValid()) continue;
                    static_cast<void>(
                        Values()->ClearProviderContribution(
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
        Visual& visual) noexcept {
        FrameworkElement* element =
            TryCast<FrameworkElement>(&(visual));
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authored :
                 (*element).AuthoredTriggers()) {
                if (authored && authored->RuntimeType() ==
                    DataTemplateTriggerInstance::StaticTypeId()) {
                    ClearDataTemplateTriggerProviders(
                        static_cast<DataTemplateTriggerInstance&>(
                            *authored));
                }
            }
        }
        for (Visual* child : (visual).RenderChildren()) {
            if (child != nullptr) {
                ClearDataTemplateTriggerProvidersInSubtree(*child);
            }
        }
    }

void InteractivityEngine::ClearAnimationSubscriptionsFor(
        Visual& fragmentRoot) noexcept {
        DetachBehaviorsInSubtree(fragmentRoot);
        ClearDataTemplateTriggerProvidersInSubtree(fragmentRoot);
        for (std::uint32_t index = 0U;
             index < dataTemplateTriggerSubscriptions.Size();) {
            DataTemplateTriggerSubscription& subscription =
                dataTemplateTriggerSubscriptions[index];
            const bool sourceMatches =
                subscription.source != nullptr &&
                Metadata()->Types().IsDerivedFrom(
                    subscription.source->RuntimeType(),
                    Visual::StaticTypeId()) &&
                IsInVisualSubtree(
                    static_cast<Visual*>(
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
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property, subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       Metadata() != nullptr) {
                static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
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
                       Metadata() != nullptr) {
                static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
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
                       Metadata() != nullptr) {
                static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
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
             index < pendingInteractionTriggers.Size();) {
            PendingInteractionTrigger& pending =
                pendingInteractionTriggers[index];
            if (pending.owner == nullptr ||
                !IsInVisualSubtree(pending.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (index + 1U != pendingInteractionTriggers.Size()) {
                pendingInteractionTriggers[index] =
                    std::move(pendingInteractionTriggers.Back());
            }
            pendingInteractionTriggers.PopBack();
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
                    UIElement::KeyDownEvent.Handle(),
                    subscription.handler));
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U != keyTriggerSubscriptions.Size()) {
                keyTriggerSubscriptions[index] =
                    std::move(keyTriggerSubscriptions.Back());
            }
            keyTriggerSubscriptions.PopBack();
        }
        if (Storyboards() == nullptr) return;
        Storyboards()->ClearEventTriggersFor(fragmentRoot);
        for (std::uint32_t index = 0U;
             index < Storyboards()->storyboardSessions.Size();) {
            StoryboardHost::StoryboardSession& session = Storyboards()->storyboardSessions[index];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            Storyboards()->CancelStoryboardCompletionSessions(session.handles.AsSpan());
            if (Animations() != nullptr) {
                for (AnimationHandle handle : session.handles) {
                    static_cast<void>(Animations()->Remove(handle));
                }
            }
            for (std::uint32_t next = index + 1U;
                 next < Storyboards()->storyboardSessions.Size(); ++next) {
                Storyboards()->storyboardSessions[next - 1U] =
                    std::move(Storyboards()->storyboardSessions[next]);
            }
            Storyboards()->storyboardSessions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < Storyboards()->storyboardCompletionSessions.Size();) {
            StoryboardHost::StoryboardCompletionSession& session =
                Storyboards()->storyboardCompletionSessions[index];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (Animations() != nullptr) {
                for (AnimationHandle handle :
                     session.handles) {
                    static_cast<void>(Animations()->Remove(handle));
                }
            }
            for (std::uint32_t next = index + 1U;
                 next < Storyboards()->storyboardCompletionSessions.Size(); ++next) {
                Storyboards()->storyboardCompletionSessions[next - 1U] =
                    std::move(Storyboards()->storyboardCompletionSessions[next]);
            }
            Storyboards()->storyboardCompletionSessions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < Storyboards()->storyboardCompletedSubscriptions.Size();) {
            const StoryboardHost::StoryboardCompletedSubscription& subscription =
                Storyboards()->storyboardCompletedSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(
                    subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            for (std::uint32_t next = index + 1U;
                 next < Storyboards()->storyboardCompletedSubscriptions.Size(); ++next) {
                Storyboards()->storyboardCompletedSubscriptions[next - 1U] =
                    std::move(Storyboards()->storyboardCompletedSubscriptions[next]);
            }
            Storyboards()->storyboardCompletedSubscriptions.PopBack();
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
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       Metadata() != nullptr) {
                static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *Allocator(),
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
                       Metadata() != nullptr) {
                static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
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
                       Metadata() != nullptr) {
                static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
                subscription.context);
        }
        interactionDataTriggerSubscriptions.Clear();
        ClearPendingInteractionTriggers();
        pendingStyleDataTriggerEvaluations.Clear();
        flushingPendingStyleDataTriggers_ = false;
        for (KeyTriggerSubscription& subscription :
             keyTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(subscription.source->RemoveHandler(
                    UIElement::KeyDownEvent.Handle(),
                    subscription.handler));
            }
            FreeObject(
                *Allocator(), Base::MemoryTag::Ui,
                subscription.context);
        }
        keyTriggerSubscriptions.Clear();
        if (Storyboards() != nullptr) {
            Storyboards()->ClearEventTriggers();
            Storyboards()->storyboardCompletionSessions.Clear();
            Storyboards()->storyboardCompletedSubscriptions.Clear();
        }
        animationEventStatus = Base::Status::Ok();
    }


void InteractivityEngine::
DataTemplateTriggerHandlerState::Invoke(
    DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&)
    noexcept
{
    if (runtime == nullptr || !triggerContext) {
        return;
    }
    Base::Result<void> evaluated =
        runtime->EvaluateDataTemplateTrigger(
            *triggerContext,
            triggerIndex);
    if (!evaluated && runtime->view != nullptr) {
        runtime->view->ReportUpdateFailure(evaluated.GetStatus());
    }
}

void InteractivityEngine::
DataTemplateTriggerHandlerState::MetadataInvoke(
    Base::Object&,
    Meta::MemberId property,
    void* context) noexcept
{
    auto* state = static_cast<DataTemplateTriggerHandlerState*>(context);
    if (state == nullptr ||
        (state->metadataProperty != Meta::InvalidMemberId &&
         property != Meta::InvalidMemberId &&
         property != state->metadataProperty)) {
        return;
    }
    if (state->runtime == nullptr || !state->triggerContext) {
        return;
    }
    Base::Result<void> evaluated =
        state->runtime->EvaluateDataTemplateTrigger(
            *state->triggerContext,
            state->triggerIndex);
    if (!evaluated && state->runtime->view != nullptr) {
        state->runtime->view->ReportUpdateFailure(evaluated.GetStatus());
    }
}

} // namespace Aero

namespace Aero {

using namespace ::Aero;

void InteractivityEngine::NotifyLayoutUpdated() noexcept {
    FlushPendingStyleDataTriggerEvaluations();
    RetryPendingInteractionTriggers();
    for (auto& behavior : attachedBehaviorInstances) {
        if (behavior.instance) {
            behavior.instance->NotifyLayoutUpdated();
        }
    }
}

Base::Result<Base::Ref<Interactivity::Behavior>>
 InteractivityEngine::CloneBehaviorPrototype(
        const Interactivity::Behavior& prototype) noexcept {
        if (Metadata() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior metadata is unavailable");
        }
        return Interactivity::Behavior::ClonePrototype(prototype, *Metadata());
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
        Aero::Media::Visual* current = ::Aero::TryCast<::Aero::Media::Visual>(owner.GetLogicalParent());
        if (current == nullptr) current = owner.GetVisualParent();
        while (current != nullptr) {
            const Meta::TypeInfo* type =
                Metadata()->Types().FindType(current->RuntimeType());
            const bool matches = ancestorName.Empty() ||
                (type != nullptr && type->Name() == ancestorName);
            if (matches && ++matched == relative->GetAncestorLevel()) {
                return current;
            }
            Aero::Media::Visual* next = ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent());
            if (next == nullptr) next = current->GetVisualParent();
            current = next;
        }
        return nullptr;
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
            descriptor.metadata = Metadata();
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
            descriptor.mode = Bindings()->ResolveBindingMode(
                *record.instance.Get(),
                authored.property,
                authored.binding->GetMode());
            descriptor.updateSourceTrigger =
                Bindings()->ResolveUpdateSourceTrigger(
                    *record.instance.Get(),
                    authored.property,
                    authored.binding->GetUpdateSourceTrigger());
            descriptor.fallbackValue =
                authored.binding->GetFallbackValue();
            descriptor.targetNullValue =
                authored.binding->GetTargetNullValue();
            Base::Result<Data::BindingHandle> attached =
                Bindings()->Attach(descriptor);
            if (!attached) {
                for (const Data::BindingHandle handle : record.bindings) {
                    static_cast<void>(Bindings()->Detach(handle));
                }
                return attached.GetStatus();
            }
            record.bindings.PushBack(
                attached.Value());
        }
        Base::Result<void> attached = record.instance->Attach(owner);
        if (!attached) {
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(Bindings()->Detach(handle));
            }
            return attached.GetStatus();
        }
        attachedBehaviorInstances.PushBack(
            std::move(record));
        return {};
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
                if (Bindings() != nullptr) {
                    static_cast<void>(Bindings()->Detach(handle));
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

} // namespace Aero

namespace Aero {

using namespace ::Aero;

Base::Result<InteractivityEngine::InteractionTriggerProperty>
InteractivityEngine::ResolveInteractionTriggerProperty(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        Base::Object* sourceObject = ResolveAuthoredBindingSource(
            binding, owner, nullptr, names, nullptr);
        if (sourceObject == nullptr || Metadata() == nullptr) {
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
        if (Metadata()->Types().IsDerivedFrom(
                sourceObject->RuntimeType(),
                ::Aero::DependencyObject::StaticTypeId())) {
            resolved.dependencySource =
                static_cast<::Aero::DependencyObject*>(sourceObject);
            const Meta::DependencyProperty* property =
                (
                    *Metadata()).DependencyProperties().Find(sourceObject->RuntimeType(), path);
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
        const Meta::PropertyInfo* property = Metadata()->Types().FindProperty(
            sourceObject->RuntimeType(), rootPath, true);
        if (property == nullptr ||
            !Metadata()->CanReadProperty(property->Id())) {
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
            *Allocator(), Base::MemoryTag::Ui, context);
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
            property.Value().dependencySource
                ->AddValueChangedHandler(
                    property.Value().dependencyProperty, handler);
        } else {
            Base::Result<std::uint64_t> notification =
                Metadata()->SubscribePropertyChanged(
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
                *Allocator(), Base::MemoryTag::Ui, context);
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
        propertyChangedTriggerSubscriptions.PushBack(
                std::move(subscription));
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
            *Allocator(), Base::MemoryTag::Ui, context);
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
            property.Value().dependencySource
                ->AddValueChangedHandler(
                    property.Value().dependencyProperty, handler);
        } else {
            Base::Result<std::uint64_t> notification =
                Metadata()->SubscribePropertyChanged(
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
                *Allocator(), Base::MemoryTag::Ui, context);
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
        interactionDataTriggerSubscriptions.PushBack(
                std::move(subscription));
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
            *Allocator(), Base::MemoryTag::Ui, context);
        if (!allocated) return allocated.GetStatus();
        context->runtime = this;
        context->trigger = &trigger;
        context->owner = &owner;
        context->names = names;
        Aero::KeyEventHandler handler(
            [context](Base::Object* sender, Aero::KeyEventArgs& args) noexcept {
                context->Invoke(sender, args);
            });
        source->AddHandler(
            Aero::UIElement::KeyDownEvent.Handle(), handler);
        keyTriggerSubscriptions.PushBack({
            &owner, source, handler, context});
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
        if (expected == nullptr || runtime->Input() == nullptr ||
            runtime->Input()->GetFocusedElement() != expected) {
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
    pendingInteractionTriggers.PushBack(std::move(pending));
    return {};
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
    Base::Vector<PendingInteractionTrigger> snapshot(Allocator());
    for (PendingInteractionTrigger& pending : pendingInteractionTriggers) {
        snapshot.PushBack(std::move(pending));
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

namespace Aero {

using namespace ::Aero;

Base::Result<void> InteractivityEngine::ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>>
            actions,
        void* context) noexcept {
        auto* runtime = static_cast<InteractivityEngine*>(context);
        if (runtime == nullptr ||
            !runtime->Metadata()->Types().IsDerivedFrom(
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
                !runtime->Metadata()->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    Aero::Interactivity::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Style Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                runtime->Storyboards()->ExecuteAnimationAction(
                    static_cast<Aero::Interactivity::TriggerAction&>(
                        *authored),
                    element);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

Base::Result<bool> InteractivityEngine::StyleDataTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected) noexcept {
    return ComparePropertyValues(actual, std::move(expected), Metadata());
}

Base::Result<void> InteractivityEngine::EvaluateStyleDataTrigger(
        StyleDataTriggerHandlerState& state) noexcept {
        if (Styles() == nullptr || state.target == nullptr ||
            state.style == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style DataTrigger subscription is invalid");
        }
        const bool hasDependency =
            state.source != nullptr && state.property.IsValid();
        const bool hasMetadata =
            state.metadataSource != nullptr &&
            state.metadataProperty != Meta::InvalidMemberId &&
            Metadata() != nullptr;
        if (!hasDependency && !hasMetadata) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style DataTrigger subscription is invalid");
        }
        Base::Result<Meta::PropertyValue> actual = hasDependency
            ? state.source->GetValue(state.property)
            : Metadata()->GetProperty(
                  *state.metadataSource, state.metadataProperty);
        if (!actual) return actual.GetStatus();
        Base::Result<bool> matches = StyleDataTriggerValuesMatch(
            actual.Value(), state.expected);
        if (!matches) return matches.GetStatus();
        if (state.aggregate != nullptr) {
            if (state.conditionIndex >= state.aggregate->known.Size() ||
                state.conditionIndex >= state.aggregate->active.Size()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style MultiDataTrigger condition index is out of range");
            }
            state.aggregate->known[state.conditionIndex] = 1U;
            state.aggregate->active[state.conditionIndex] =
                matches.Value() ? 1U : 0U;
            bool allKnown = true;
            bool allActive = true;
            for (std::uint32_t index = 0U;
                 index < state.aggregate->known.Size();
                 ++index) {
                allKnown = allKnown &&
                    state.aggregate->known[index] != 0U;
                allActive = allActive &&
                    state.aggregate->active[index] != 0U;
            }
            if (!allKnown) {
                return {};
            }
            return Styles()->SetBindingTriggerState(
                *state.target,
                *state.style,
                state.triggerIndex,
                allActive);
        }
        return Styles()->SetBindingTriggerState(
            *state.target,
            *state.style,
            state.triggerIndex,
            matches.Value());
    }

void InteractivityEngine::FlushPendingStyleDataTriggerEvaluations() noexcept {
        if (flushingPendingStyleDataTriggers_ ||
            pendingStyleDataTriggerEvaluations.Empty()) {
            return;
        }
        flushingPendingStyleDataTriggers_ = true;
        Base::Vector<StyleDataTriggerHandlerState*> snapshot(Allocator());
        for (StyleDataTriggerHandlerState* context :
             pendingStyleDataTriggerEvaluations) {
            snapshot.PushBack(context);
        }
        pendingStyleDataTriggerEvaluations.Clear();
        for (StyleDataTriggerHandlerState* context : snapshot) {
            if (context == nullptr || context->target == nullptr) {
                continue;
            }
            bool live = false;
            for (const StyleDataTriggerSubscription& subscription :
                 styleDataTriggerSubscriptions) {
                live = live || subscription.context == context;
            }
            if (!live) continue;
            Base::Result<void> evaluated =
                EvaluateStyleDataTrigger(*context);
            if (!evaluated && view != nullptr) {
                view->ReportUpdateFailure(evaluated.GetStatus());
            }
        }
        flushingPendingStyleDataTriggers_ = false;
    }

void InteractivityEngine::ClearStyleDataTriggersFor(
        Aero::FrameworkElement& target) noexcept {
        for (std::uint32_t index = 0U;
             index < pendingStyleDataTriggerEvaluations.Size();) {
            StyleDataTriggerHandlerState* context =
                pendingStyleDataTriggerEvaluations[index];
            if (context == nullptr || context->target != &target) {
                ++index;
                continue;
            }
            if (index + 1U != pendingStyleDataTriggerEvaluations.Size()) {
                pendingStyleDataTriggerEvaluations[index] =
                    pendingStyleDataTriggerEvaluations.Back();
            }
            pendingStyleDataTriggerEvaluations.PopBack();
        }
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
            if (subscription.metadataSource != nullptr &&
                subscription.metadataSubscription != 0U &&
                Metadata() != nullptr) {
                static_cast<void>(Metadata()->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            if (subscription.context != nullptr) {
                if (subscription.context->ownsAggregate &&
                    subscription.context->aggregate != nullptr) {
                    FreeObject(
                        *Allocator(),
                        Base::MemoryTag::Ui,
                        subscription.context->aggregate);
                }
                FreeObject(
                    *Allocator(),
                    Base::MemoryTag::Ui,
                    subscription.context);
            }
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
            Aero::StyleRuntimeTriggers(style);
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
            if (!trigger.binding) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style DataTrigger Binding is incomplete");
            }

            const std::uint32_t conditionCount =
                1U + trigger.extraBindings.Size();
            StyleDataTriggerAggregate* aggregate = nullptr;
            if (conditionCount > 1U) {
                Base::Result<void> allocated = AllocateObject(
                    *Allocator(),
                    Base::MemoryTag::Ui,
                    aggregate);
                if (!allocated) return allocated.GetStatus();
                aggregate->known.Resize(conditionCount, 0U);
                aggregate->active.Resize(conditionCount, 0U);
            }

            // {Binding Path} DataTriggers use DataContext. Item containers often
            // receive Style before PrepareContainer assigns the item, so a
            // missing DataContext is a retry, not a hard error. Skip the
            // whole trigger so MultiDataTrigger extras are not attached alone.
            auto bindingWaitsForDataContext =
                [](const Base::Ref<Data::Binding>& binding) noexcept {
                    return binding &&
                        binding->GetElementName().Empty() &&
                        !binding->GetRelativeSource() &&
                        !binding->GetSource();
                };
            auto dataContextReady = [&target]() noexcept {
                const Base::Value dataContext = target.GetDataContext();
                return !dataContext.IsNullObject() &&
                    dataContext.AsObject().Get() != nullptr;
            };
            bool deferred = bindingWaitsForDataContext(trigger.binding) &&
                !dataContextReady();
            for (std::uint32_t extraIndex = 0U;
                 extraIndex < trigger.extraBindings.Size() && !deferred;
                 ++extraIndex) {
                deferred = bindingWaitsForDataContext(
                    trigger.extraBindings[extraIndex].binding) &&
                    !dataContextReady();
            }
            if (deferred) {
                if (aggregate != nullptr) {
                    FreeObject(
                        *Allocator(),
                        Base::MemoryTag::Ui,
                        aggregate);
                }
                continue;
            }

            auto attachCondition =
                [this, &target, &style, index, aggregate](
                    const Base::Ref<Data::Binding>& binding,
                    const Meta::PropertyValue& expected,
                    std::uint32_t conditionIndex,
                    bool ownsAggregate) -> Base::Result<void> {
                Base::Object* sourceObject = ResolveAuthoredBindingSource(
                    *binding, target, nullptr, nullptr, nullptr);
                if (sourceObject == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Style DataTrigger Binding source was not found");
                }
                const Base::StringView path =
                    binding->GetPath().GetPath();
                ::Aero::DependencyObject* dependencySource = nullptr;
                const Meta::DependencyProperty* dependencyProperty = nullptr;
                Meta::MemberId metadataProperty = Meta::InvalidMemberId;
                if (Metadata()->Types().IsDerivedFrom(
                        sourceObject->RuntimeType(),
                        ::Aero::DependencyObject::StaticTypeId())) {
                    dependencySource =
                        static_cast<::Aero::DependencyObject*>(sourceObject);
                    dependencyProperty =
                        (*Metadata()).DependencyProperties().Find(
                                sourceObject->RuntimeType(), path);
                }
                if (dependencyProperty == nullptr) {
                    Base::StringView rootPath = path;
                    for (std::uint32_t pathIndex = 0U;
                         pathIndex < path.SizeBytes(); ++pathIndex) {
                        if (path[pathIndex] == '.') {
                            rootPath = path.Substr(0U, pathIndex);
                            break;
                        }
                    }
                    const Meta::PropertyInfo* clrProperty =
                        Metadata()->Types().FindProperty(
                            sourceObject->RuntimeType(), rootPath, true);
                    if (clrProperty == nullptr ||
                        !Metadata()->CanReadProperty(clrProperty->Id())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Style DataTrigger Binding path was not found");
                    }
                    metadataProperty = clrProperty->Id();
                    dependencySource = nullptr;
                }

                StyleDataTriggerHandlerState* context = nullptr;
                Base::Result<void> allocated = AllocateObject(
                    *Allocator(),
                    Base::MemoryTag::Ui,
                    context);
                if (!allocated) return allocated.GetStatus();
                context->runtime = this;
                context->target = &target;
                context->style = &style;
                context->triggerIndex = index;
                context->conditionIndex = conditionIndex;
                context->aggregate = aggregate;
                context->ownsAggregate = ownsAggregate;
                context->source = dependencySource;
                context->metadataSource =
                    dependencyProperty == nullptr ? sourceObject : nullptr;
                context->property = dependencyProperty != nullptr
                    ? dependencyProperty->Handle()
                    : Meta::DependencyPropertyHandle{};
                context->metadataProperty = metadataProperty;
                context->expected = expected;
                auto callback = [context](
                    ::Aero::DependencyObject& object,
                    const Meta::DependencyPropertyChangedEventArgs& args)
                    noexcept {
                        context->Invoke(object, args);
                    };
                Meta::DependencyPropertyChangedEventHandler handler(callback);
                std::uint64_t metadataSubscription = 0U;
                Base::Result<void> subscribed;
                if (dependencyProperty != nullptr) {
                    dependencySource->AddValueChangedHandler(
                        dependencyProperty->Handle(), handler);
                } else {
                    Base::Result<std::uint64_t> notification =
                        Metadata()->SubscribePropertyChanged(
                            *sourceObject,
                            &StyleDataTriggerHandlerState::MetadataInvoke,
                            context);
                    if (notification) {
                        metadataSubscription = notification.Value();
                    } else {
                        subscribed = notification.GetStatus();
                    }
                }
                if (!subscribed) {
                    FreeObject(
                        *Allocator(),
                        Base::MemoryTag::Ui,
                        context);
                    return subscribed.GetStatus();
                }
                StyleDataTriggerSubscription subscription;
                subscription.target = &target;
                subscription.source = dependencySource;
                subscription.metadataSource =
                    dependencyProperty == nullptr ? sourceObject : nullptr;
                subscription.property = dependencyProperty != nullptr
                    ? dependencyProperty->Handle()
                    : Meta::DependencyPropertyHandle{};
                subscription.metadataSubscription = metadataSubscription;
                subscription.handler = handler;
                subscription.context = context;
                styleDataTriggerSubscriptions.PushBack(
                        std::move(subscription));
                // Defer the first evaluation until DataBind. Evaluating
                // here runs SetBindingTriggerState inside ApplyViewUi /
                // item generation, which can re-enter the property engine
                // and prevent the first frame from completing.
                pendingStyleDataTriggerEvaluations.PushBack(context);
                return {};
            };

            Base::Result<void> attached = attachCondition(
                trigger.binding,
                trigger.value,
                0U,
                aggregate != nullptr);
            if (!attached) {
                if (aggregate != nullptr) {
                    bool owned = false;
                    for (const StyleDataTriggerSubscription& existing :
                         styleDataTriggerSubscriptions) {
                        owned = owned ||
                            (existing.context != nullptr &&
                             existing.context->aggregate == aggregate);
                    }
                    if (!owned) {
                        FreeObject(
                            *Allocator(),
                            Base::MemoryTag::Ui,
                            aggregate);
                    }
                }
                return attached.GetStatus();
            }
            ++started;
            for (std::uint32_t extraIndex = 0U;
                 extraIndex < trigger.extraBindings.Size();
                 ++extraIndex) {
                attached = attachCondition(
                    trigger.extraBindings[extraIndex].binding,
                    trigger.extraBindings[extraIndex].value,
                    extraIndex + 1U,
                    false);
                if (!attached) return attached.GetStatus();
                ++started;
            }
        }
        return started;
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

void InteractivityEngine::StyleDataTriggerHandlerState::MetadataInvoke(
    Base::Object&,
    Meta::MemberId property,
    void* context) noexcept
{
    auto* state = static_cast<StyleDataTriggerHandlerState*>(context);
    if (state == nullptr ||
        (state->metadataProperty != Meta::InvalidMemberId &&
         property != Meta::InvalidMemberId &&
         property != state->metadataProperty)) {
        return;
    }
    if (state->runtime == nullptr ||
        !state->runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> evaluated =
        state->runtime->EvaluateStyleDataTrigger(*state);
    if (!evaluated) {
        state->runtime->animationEventStatus =
            evaluated.GetStatus();
    }
}

} // namespace Aero
