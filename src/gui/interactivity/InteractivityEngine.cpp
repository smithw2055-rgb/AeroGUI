#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/meta/ValueConversion.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

#include "gui/controls/ItemsContainers.hpp"
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
    Base::Object& source,
    std::uint32_t triggerIndex,
    std::uint32_t actionIndex) noexcept {
    auto* element = TryCast<FrameworkElement>(&source);
    if (element == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Interaction.Triggers source is not a FrameworkElement");
    }
    Base::Span<const Base::Ref<Base::Object>> triggers =
        AeroGuiInternal::StyleTriggerPrototypes(*element);
    if (triggers.Empty()) {
        triggers = AeroGuiInternal::AuthoredTriggers(*element);
    }
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

InteractivityEngine::InteractivityEngine(ViewState& owner) noexcept
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
    // ElementTree / ViewState services are read on demand; Bind only marks
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
        if (actual.Kind() == Meta::ValueKind::Object &&
            !actual.IsNullObject() &&
            actual.AsObject() &&
            actual.AsObject()->RuntimeType() ==
                BoxedItemValue::StaticTypeId()) {
            return DataTemplateTriggerValuesMatch(
                static_cast<const BoxedItemValue&>(
                    *actual.AsObject()).Value(),
                std::move(expected));
        }
        if (expected.IsNullObject() || expected.IsUnset()) {
            return actual.IsNullObject() || actual.IsUnset();
        }
        if (expected.Kind() == Meta::ValueKind::String &&
            actual.Kind() == Meta::ValueKind::String) {
            return actual.AsString() == expected.AsString();
        }
        if (expected.Kind() == Meta::ValueKind::String &&
            expected.Type() != actual.Type()) {
            if (Metadata() == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate Trigger metadata is unavailable");
            }
            Base::Result<Meta::PropertyValue> converted =
                Metadata()->TryConvertText(
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

Base::Result<bool> InteractivityEngine::EvaluateTriggerComparison(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected,
        Base::StringView comparison) noexcept {
        if (actual.Kind() == Meta::ValueKind::Object &&
            !actual.IsNullObject() &&
            actual.AsObject() &&
            actual.AsObject()->RuntimeType() ==
                BoxedItemValue::StaticTypeId()) {
            return EvaluateTriggerComparison(
                static_cast<const BoxedItemValue&>(
                    *actual.AsObject()).Value(),
                std::move(expected),
                comparison);
        }
        if (comparison.Empty() ||
            Base::ValueConversion::EqualsAsciiInsensitive(
                comparison, "Equal") ||
            Base::ValueConversion::EqualsAsciiInsensitive(
                comparison, "NotEqual")) {
            Base::Result<bool> equal = DataTemplateTriggerValuesMatch(
                actual, expected);
            if (!equal) return equal.GetStatus();
            if (comparison.Empty() ||
                Base::ValueConversion::EqualsAsciiInsensitive(
                    comparison, "Equal")) {
                return equal.Value();
            }
            return !equal.Value();
        }
        Meta::PropertyValue leftValue = actual;
        Meta::PropertyValue rightValue = std::move(expected);
        if (rightValue.Kind() == Meta::ValueKind::String &&
            leftValue.Kind() != Meta::ValueKind::String) {
            if (Metadata() == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Interaction DataTrigger metadata is unavailable");
            }
            Base::Result<Meta::PropertyValue> parsed =
                Metadata()->TryConvertText(
                    leftValue.Type(), rightValue.AsString());
            if (!parsed) return false;
            rightValue = std::move(parsed).Value();
        }
        if (leftValue.Kind() == Meta::ValueKind::String &&
            rightValue.Kind() == Meta::ValueKind::String) {
            const int order = leftValue.AsString().Compare(
                rightValue.AsString());
            if (Base::ValueConversion::EqualsAsciiInsensitive(
                    comparison, "LessThan")) {
                return order < 0;
            }
            if (Base::ValueConversion::EqualsAsciiInsensitive(
                    comparison, "LessThanOrEqual")) {
                return order <= 0;
            }
            if (Base::ValueConversion::EqualsAsciiInsensitive(
                    comparison, "GreaterThan")) {
                return order > 0;
            }
            if (Base::ValueConversion::EqualsAsciiInsensitive(
                    comparison, "GreaterThanOrEqual")) {
                return order >= 0;
            }
            return false;
        }
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
        long double leftNumber = 0.0L;
        long double rightNumber = 0.0L;
        if (!numeric(leftValue, leftNumber) ||
            !numeric(rightValue, rightNumber)) {
            return false;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                comparison, "LessThan")) {
            return leftNumber < rightNumber;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                comparison, "LessThanOrEqual")) {
            return leftNumber <= rightNumber;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                comparison, "GreaterThan")) {
            return leftNumber > rightNumber;
        }
        if (Base::ValueConversion::EqualsAsciiInsensitive(
                comparison, "GreaterThanOrEqual")) {
            return leftNumber >= rightNumber;
        }
        return false;
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
            source = ResolveAuthoredBindingSource(
                *condition.binding,
                *context.root,
                &context,
                nullptr,
                context.root);
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
            Base::Result<Meta::BindingPathPlan> plan =
                Meta::BindingPathPlan::Compile(
                    *Metadata(),
                    source->RuntimeType(),
                    path);
            if (!plan) return plan.GetStatus();
            Base::Result<Meta::PropertyValue> value =
                plan.Value().Get(*Metadata(), *source);
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
                value = ResolveInteractionActionPath(
                    *source, triggerIndex, actionIndex);
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
                 AeroGuiInternal::AuthoredTriggers(*element)) {
                if (authored && authored->RuntimeType() ==
                    DataTemplateTriggerInstance::StaticTypeId()) {
                    ClearDataTemplateTriggerProviders(
                        static_cast<DataTemplateTriggerInstance&>(
                            *authored));
                }
            }
        }
        for (Visual* child : AeroGuiInternal::RenderChildren(visual)) {
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
