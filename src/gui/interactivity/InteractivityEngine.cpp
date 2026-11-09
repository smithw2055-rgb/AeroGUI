// InteractivityEngine: behaviors and interaction triggers (delegating Style,
// ControlTemplate, and DataTemplate trigger evaluation and binding subscriptions
// to TriggerRuntime + BindingEngine).
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
#include "gui/triggers/TriggerEngine.hpp"
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
      attachedBehaviorInstances(owner.allocator),
      propertyChangedTriggerSubscriptions(owner.allocator),
      interactionDataTriggerSubscriptions(owner.allocator),
      pendingInteractionTriggers(owner.allocator),
      keyTriggerSubscriptions(owner.allocator) {}

TriggerRuntime* InteractivityEngine::EnsureTriggerRuntime() noexcept {
    if (Styles() == nullptr || Styles()->Triggers() == nullptr) {
        return nullptr;
    }
    TriggerRuntime* triggers = Styles()->Triggers();
    triggers->SetDataTemplateTriggerCallbacks(
        &InteractivityEngine::ExecuteDataTemplateTriggerActions,
        &InteractivityEngine::StartDataTemplateEventTriggers,
        &InteractivityEngine::ReportTriggerFailure,
        this);
    return triggers;
}

void InteractivityEngine::Bind() noexcept {
    static_cast<void>(EnsureTriggerRuntime());
}

void InteractivityEngine::PropertyChangedTriggerSubscription::Unbind(
    InteractivityEngine& engine) noexcept {
    binding.Detach(engine.Bindings());
    if (context != nullptr && engine.Allocator() != nullptr) {
        FreeObject(*engine.Allocator(), Base::MemoryTag::Ui, context);
        context = nullptr;
    }
}

void InteractivityEngine::InteractionDataTriggerSubscription::Unbind(
    InteractivityEngine& engine) noexcept {
    binding.Detach(engine.Bindings());
    if (context != nullptr && engine.Allocator() != nullptr) {
        FreeObject(*engine.Allocator(), Base::MemoryTag::Ui, context);
        context = nullptr;
    }
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
    return view != nullptr ? view->Tree() : nullptr;
}

StyleEngine* InteractivityEngine::Styles() const noexcept {
    return view != nullptr ? view->Styles() : nullptr;
}

Meta::EffectiveValueEngine* InteractivityEngine::Values() const noexcept {
    return view != nullptr ? view->Values() : nullptr;
}

BindingEngine* InteractivityEngine::Bindings() const noexcept {
    return view != nullptr ? view->Bindings() : nullptr;
}

StoryboardHost* InteractivityEngine::Storyboards() const noexcept {
    return view != nullptr ? view->Storyboards() : nullptr;
}

bool InteractivityEngine::IsInVisualSubtree(
        Visual* node,
        const Visual& fragmentRoot) const noexcept {
    while (node != nullptr) {
        if (node == &fragmentRoot) return true;
        node = TryCast<Visual>(node->GetLogicalParent()) != nullptr
            ? TryCast<Visual>(node->GetLogicalParent())
            : node->GetVisualParent();
    }
    return false;
}

Base::Result<bool> InteractivityEngine::ConditionBehaviorsAllowExecution(
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        FrameworkElement& owner,
        const NameScope* names) noexcept {
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
        return ComparePropertyValues(
            current.Value(),
            condition.GetRightOperand(),
            Metadata(),
            static_cast<PropertyComparisonOperator>(condition.GetComparisonOperator()));
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

bool InteractivityEngine::DataTemplateTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        const Meta::PropertyValue& expected) noexcept {
    return EvaluatePropertyTriggerCondition(actual, expected, Metadata());
}

bool InteractivityEngine::EvaluateTriggerComparison(
        const Meta::PropertyValue& actual,
        const Meta::PropertyValue& expected,
        Base::StringView comparison) noexcept {
    return ComparePropertyValues(
        actual, expected, Metadata(), comparison);
}

Base::Result<void> InteractivityEngine::ExecuteDataTemplateTriggerActions(
    DataTemplateTriggerInstance& instance,
    Base::Span<const Base::Ref<Base::Object>> actions,
    void* context) noexcept {
    auto* runtime = static_cast<InteractivityEngine*>(context);
    if (runtime == nullptr || instance.root == nullptr ||
        runtime->Metadata() == nullptr || runtime->Storyboards() == nullptr) {
        return {};
    }
    for (const Base::Ref<Base::Object>& authored : actions) {
        if (!authored) continue;
        const bool isAction =
            runtime->Metadata()->Types().IsDerivedFrom(
                authored->RuntimeType(),
                TriggerAction::StaticTypeId());
        if (!isAction) continue;
        Base::Result<void> executed =
            runtime->Storyboards()->ExecuteAnimationAction(
                static_cast<TriggerAction&>(*authored),
                *instance.root,
                &instance);
        if (!executed) {
            return executed.GetStatus();
        }
    }
    return {};
}

Base::Result<std::uint32_t> InteractivityEngine::StartDataTemplateEventTriggers(
    DataTemplateTriggerInstance& instance,
    void* context) noexcept {
    auto* runtime = static_cast<InteractivityEngine*>(context);
    if (runtime == nullptr || instance.root == nullptr ||
        runtime->Storyboards() == nullptr) {
        return std::uint32_t{0U};
    }
    std::uint32_t count = 0U;
    for (const Base::Ref<EventTrigger>& trigger : instance.eventTriggers) {
        if (!trigger) continue;
        Base::Result<void> started = runtime->Storyboards()->StartEventTrigger(
            *trigger, *instance.root, *instance.root, nullptr, &instance);
        if (!started) {
            return started.GetStatus();
        }
        ++count;
    }
    return count;
}

void InteractivityEngine::ReportTriggerFailure(
    const Base::Status& status,
    void* context) noexcept {
    auto* runtime = static_cast<InteractivityEngine*>(context);
    if (runtime != nullptr && runtime->view != nullptr && !status.IsOk()) {
        runtime->view->ReportUpdateFailure(status);
    }
}

Base::Result<bool> InteractivityEngine::EvaluateDataTemplateCondition(
        DataTemplateTriggerInstance& context,
        DataTemplateTriggerCondition& condition) noexcept {
    TriggerRuntime* triggers = EnsureTriggerRuntime();
    if (triggers == nullptr) return false;
    return triggers->EvaluateDataTemplateCondition(context, condition, Metadata());
}

Base::Result<void> InteractivityEngine::EnsureDataTemplateProviderTokens(
        DataTemplateTriggerInstance& context) noexcept {
    TriggerRuntime* triggers = EnsureTriggerRuntime();
    if (triggers == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate Trigger runtime is unavailable");
    }
    return triggers->EnsureDataTemplateProviderTokens(context, Values());
}

Base::Result<void> InteractivityEngine::EvaluateDataTemplateTrigger(
        DataTemplateTriggerInstance& context,
        std::uint32_t triggerIndex) noexcept {
    TriggerRuntime* triggers = EnsureTriggerRuntime();
    if (triggers == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate Trigger runtime is unavailable");
    }
    return triggers->EvaluateDataTemplateTrigger(
        context, triggerIndex, Values(), Metadata());
}

Base::Result<std::uint32_t>
InteractivityEngine::StartDataTemplateTriggers(
        DataTemplateTriggerInstance& context) noexcept {
    TriggerRuntime* triggers = EnsureTriggerRuntime();
    if (triggers == nullptr) {
        return std::uint32_t{0U};
    }
    return triggers->StartDataTemplateTriggers(
        context, Bindings(), Values(), Metadata());
}

Base::Result<const Meta::BindingPathPlan*>
InteractivityEngine::FindOrCompileBindingPath(
        Meta::TypeId sourceType,
        Base::StringView path) noexcept {
    if (Metadata() == nullptr || Bindings() == nullptr ||
        sourceType == Meta::InvalidTypeId || path.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Authored Binding path or source type is invalid");
    }
    return Bindings()->FindOrCompilePathPlan(*Metadata(), sourceType, path);
}

Base::Object* InteractivityEngine::ResolveAuthoredBindingSource(
        const Binding& binding,
        FrameworkElement& owner,
        DataTemplateTriggerInstance* dataTemplateContext,
        const NameScope* names,
        Base::Object* self) noexcept {
    return TriggerRuntime::ResolveTriggerBindingSource(
        Metadata(), binding, owner, dataTemplateContext, names, self);
}

Base::Result<Meta::PropertyValue> InteractivityEngine::EvaluateAuthoredBinding(
        const Binding& binding,
        FrameworkElement& owner,
        DataTemplateTriggerInstance* dataTemplateContext,
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
            Base::Result<const Meta::BindingPathPlan*> plan =
                FindOrCompileBindingPath(source->RuntimeType(), path);
            if (plan && plan.Value() != nullptr) {
                value = plan.Value()->Get(*Metadata(), *source);
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
    for (const Base::Ref<TriggerAction>& action : actions) {
        if (!action) continue;
        Base::Result<void> executed = Storyboards()->ExecuteAnimationAction(
            *action, owner, nullptr, names);
        if (!executed) return executed.GetStatus();
    }
    return {};
}

void InteractivityEngine::ClearDataTemplateTriggerProviders(
        DataTemplateTriggerInstance& context) noexcept {
    if (TriggerRuntime* triggers = EnsureTriggerRuntime()) {
        triggers->ClearDataTemplateTriggerProviders(context, Bindings(), Values());
    }
}

void InteractivityEngine::ClearDataTemplateTriggerProvidersInSubtree(
        Visual& visual) noexcept {
    if (TriggerRuntime* triggers = EnsureTriggerRuntime()) {
        triggers->ClearDataTemplateTriggerProvidersInSubtree(
            visual, Bindings(), Values());
    }
}

void InteractivityEngine::ClearAnimationSubscriptionsFor(
        Visual& fragmentRoot) noexcept {
    DetachBehaviorsInSubtree(fragmentRoot);
    if (TriggerRuntime* triggers = EnsureTriggerRuntime()) {
        triggers->ClearDataTemplateTriggersForSubtree(
            fragmentRoot, Bindings(), Values(), Metadata());
    }
    auto clearMatchingTriggers = [this, &fragmentRoot](auto& subscriptions) {
        for (std::uint32_t index = 0U; index < subscriptions.Size();) {
            auto& subscription = subscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            subscription.Unbind(*this);
            if (index + 1U != subscriptions.Size()) {
                subscriptions[index] = std::move(subscriptions.Back());
            }
            subscriptions.PopBack();
        }
    };
    clearMatchingTriggers(propertyChangedTriggerSubscriptions);
    clearMatchingTriggers(interactionDataTriggerSubscriptions);
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
    {
        auto& sessions = Storyboards()->storyboardSessions;
        std::uint32_t writeIndex = 0U;
        for (std::uint32_t readIndex = 0U;
             readIndex < sessions.Size();
             ++readIndex) {
            StoryboardHost::StoryboardSession& session = sessions[readIndex];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                if (writeIndex != readIndex) {
                    sessions[writeIndex] = std::move(session);
                }
                ++writeIndex;
                continue;
            }
            Storyboards()->CancelStoryboardCompletionSessions(session.handles.AsSpan());
            if (Animations() != nullptr) {
                Animations()->RemoveBatch(session.handles.AsSpan());
            }
        }
        while (sessions.Size() > writeIndex) {
            sessions.PopBack();
        }
    }
    {
        auto& completionSessions = Storyboards()->storyboardCompletionSessions;
        std::uint32_t writeIndex = 0U;
        for (std::uint32_t readIndex = 0U;
             readIndex < completionSessions.Size();
             ++readIndex) {
            StoryboardHost::StoryboardCompletionSession& session =
                completionSessions[readIndex];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                if (writeIndex != readIndex) {
                    completionSessions[writeIndex] = std::move(session);
                }
                ++writeIndex;
                continue;
            }
            if (Animations() != nullptr) {
                Animations()->RemoveBatch(session.handles.AsSpan());
            }
        }
        while (completionSessions.Size() > writeIndex) {
            completionSessions.PopBack();
        }
    }
    {
        auto& completedSubs = Storyboards()->storyboardCompletedSubscriptions;
        std::uint32_t writeIndex = 0U;
        for (std::uint32_t readIndex = 0U;
             readIndex < completedSubs.Size();
             ++readIndex) {
            StoryboardHost::StoryboardCompletedSubscription& subscription =
                completedSubs[readIndex];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                if (writeIndex != readIndex) {
                    completedSubs[writeIndex] = std::move(subscription);
                }
                ++writeIndex;
            }
        }
        static_cast<void>(completedSubs.Resize(writeIndex));
    }
}

void InteractivityEngine::ClearAnimationEventSubscriptions() noexcept {
    if (Styles() != nullptr && Styles()->Triggers() != nullptr) {
        Styles()->Triggers()->ClearAllStyleDataTriggers();
        Styles()->Triggers()->ClearAllDataTemplateTriggers();
    }
    auto clearAll = [this](auto& subscriptions) {
        for (auto& subscription : subscriptions) {
            subscription.Unbind(*this);
        }
        subscriptions.Clear();
    };
    clearAll(propertyChangedTriggerSubscriptions);
    clearAll(interactionDataTriggerSubscriptions);
    ClearPendingInteractionTriggers();
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

void InteractivityEngine::NotifyLayoutUpdated() noexcept {
    if (Bindings() != nullptr && Bindings()->HasDirtyBindings()) {
        Aero::BindingEngine::DataBindHook(Bindings());
    }
    RetryPendingInteractionTriggers();
    for (auto& behavior : attachedBehaviorInstances) {
        if (behavior.instance) {
            behavior.instance->NotifyLayoutUpdated();
        }
    }
}

Base::Ref<Interactivity::Behavior>
InteractivityEngine::CloneBehaviorPrototype(
    const Interactivity::Behavior& prototype) noexcept {
    if (Metadata() == nullptr) {
        return {};
    }
    return Interactivity::Behavior::ClonePrototype(prototype, *Metadata());
}

Base::Object* InteractivityEngine::ResolveBehaviorBindingSource(
        const Data::Binding& binding,
        Interactivity::Behavior& behavior,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
    return ResolveAuthoredBindingSource(
        binding, owner, nullptr, names, &behavior);
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
        instance = CloneBehaviorPrototype(prototype);
        if (!instance) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior metadata is unavailable or clone failed");
        }
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
        Data::MetadataBindingDescriptor descriptor =
            Data::MetadataBindingDescriptor::FromBinding(
                *authored.binding,
                *record.instance.Get(),
                authored.property,
                Metadata(),
                source,
                &owner);
        Base::Result<Data::BindingHandle> attached =
            Bindings()->Attach(descriptor);
        if (!attached) {
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(Bindings()->Detach(handle));
            }
            return attached.GetStatus();
        }
        record.bindings.PushBack(attached.Value());
    }
    if (!record.instance->Attach(owner)) {
        for (const Data::BindingHandle handle : record.bindings) {
            static_cast<void>(Bindings()->Detach(handle));
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Behavior is already attached to another object");
    }
    attachedBehaviorInstances.PushBack(std::move(record));
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

Base::Result<bool> InteractivityEngine::EvaluateInteractionDataTrigger(
        InteractionDataTriggerState& state,
        const Meta::PropertyValue& current) noexcept {
    if (state.trigger == nullptr || state.owner == nullptr ||
        !state.trigger->GetBinding()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Interaction DataTrigger state is invalid");
    }
    if (TriggerRuntime::IsTriggerProxySentinel(current) && !state.active) {
        return false;
    }
    const bool active = !TriggerRuntime::IsTriggerProxySentinel(current) &&
        EvaluateTriggerComparison(
            current,
            state.trigger->GetAuthoredValue(),
            state.trigger->GetComparison());
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
    if (!trigger.GetBinding() || Bindings() == nullptr) {
        return false;
    }
    for (const PropertyChangedTriggerSubscription& existing :
         propertyChangedTriggerSubscriptions) {
        if (existing.owner == &owner &&
            existing.context != nullptr &&
            existing.context->trigger == &trigger) {
            return false;
        }
    }
    PropertyChangedTriggerState* context = nullptr;
    Base::Result<void> allocated = Base::AllocateObject(
        *Allocator(), Base::MemoryTag::Ui, context);
    if (!allocated) return allocated.GetStatus();
    context->runtime = this;
    context->trigger = &trigger;
    context->owner = &owner;
    context->names = names;

    Base::Result<TriggerRuntime::TriggerBindingSubscription> attached =
        TriggerRuntime::AttachTriggerBinding(
            *Bindings(),
            *trigger.GetBinding(),
            owner,
            nullptr,
            names,
            nullptr,
            &owner,
            [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                context->Invoke(object, args);
            },
            /*evaluateImmediately=*/true,
            /*ignoreInitialValue=*/true);
    if (!attached) {
        Base::FreeObject(*Allocator(), Base::MemoryTag::Ui, context);
        if (attached.GetStatus().code == Base::ErrorCode::NotFound) {
            PendUntilDataContext(owner, names, nullptr, &trigger);
            return false;
        }
        return attached.GetStatus();
    }

    PropertyChangedTriggerSubscription subscription;
    subscription.owner = &owner;
    subscription.binding = std::move(attached).Value();
    subscription.context = context;
    propertyChangedTriggerSubscriptions.PushBack(std::move(subscription));
    return true;
}

Base::Result<bool> InteractivityEngine::StartInteractionDataTrigger(
        Aero::DataTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
    if (!trigger.GetBinding() || Bindings() == nullptr) {
        return false;
    }
    for (const InteractionDataTriggerSubscription& existing :
         interactionDataTriggerSubscriptions) {
        if (existing.owner == &owner &&
            existing.context != nullptr &&
            existing.context->trigger == &trigger) {
            return false;
        }
    }
    InteractionDataTriggerState* context = nullptr;
    Base::Result<void> allocated = Base::AllocateObject(
        *Allocator(), Base::MemoryTag::Ui, context);
    if (!allocated) return allocated.GetStatus();
    context->runtime = this;
    context->trigger = &trigger;
    context->owner = &owner;
    context->names = names;

    Base::Result<TriggerRuntime::TriggerBindingSubscription> attached =
        TriggerRuntime::AttachTriggerBinding(
            *Bindings(),
            *trigger.GetBinding(),
            owner,
            nullptr,
            names,
            nullptr,
            &owner,
            [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                context->Invoke(object, args);
            },
            /*evaluateImmediately=*/true,
            /*ignoreInitialValue=*/false);
    if (!attached) {
        Base::FreeObject(*Allocator(), Base::MemoryTag::Ui, context);
        if (attached.GetStatus().code == Base::ErrorCode::NotFound) {
            PendUntilDataContext(owner, names, &trigger, nullptr);
            return false;
        }
        return attached.GetStatus();
    }

    InteractionDataTriggerSubscription subscription;
    subscription.owner = &owner;
    subscription.binding = std::move(attached).Value();
    subscription.context = context;
    interactionDataTriggerSubscriptions.PushBack(std::move(subscription));
    return true;
}

std::uint32_t InteractivityEngine::KeyCodeFromName(
        Base::StringView key) noexcept {
    switch (key.SizeBytes()) {
    case 2:
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Up")) return Input::KeyboardKeyUp;
        break;
    case 3:
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Esc")) return Input::KeyboardKeyEscape;
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Tab")) return Input::KeyboardKeyTab;
        break;
    case 4:
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Left")) return Input::KeyboardKeyLeft;
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Down")) return Input::KeyboardKeyDown;
        break;
    case 5:
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Enter")) return Input::KeyboardKeyEnter;
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Space")) return Input::KeyboardKeySpace;
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Right")) return Input::KeyboardKeyRight;
        break;
    case 6:
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Return")) return Input::KeyboardKeyEnter;
        if (Base::ValueConversion::EqualsAsciiInsensitive(key, "Escape")) return Input::KeyboardKeyEscape;
        break;
    default:
        break;
    }
    return 0U;
}

Base::Result<bool> InteractivityEngine::StartKeyTrigger(
        Aero::Interactivity::KeyTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
    Aero::UIElement* source = ::Aero::TryCast<::Aero::UIElement>(&owner);
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
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    // Ignore initial baseline resolution from sentinel -> initial bound value,
    // or reset back to sentinel when DataContext is cleared.
    if (TriggerRuntime::IsTriggerProxySentinel(args.GetOldValue()) ||
        TriggerRuntime::IsTriggerProxySentinel(args.GetNewValue())) {
        return;
    }
    Base::Result<void> executed = runtime->ExecuteTriggerActions(
        trigger->GetActions(), *owner, names);
    if (!executed) {
        runtime->animationEventStatus = executed.GetStatus();
    }
}

void InteractivityEngine::InteractionDataTriggerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<bool> evaluated =
        runtime->EvaluateInteractionDataTrigger(*this, args.GetNewValue());
    if (!evaluated) {
        runtime->animationEventStatus = evaluated.GetStatus();
    }
}

void InteractivityEngine::KeyTriggerState::Invoke(
    Base::Object*,
    Aero::KeyEventArgs& args) noexcept {
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

void InteractivityEngine::PendUntilDataContext(
    Aero::FrameworkElement& owner,
    const Aero::NameScope* names,
    Aero::DataTrigger* dataTrigger,
    Aero::Interactivity::PropertyChangedTrigger* propertyTrigger) noexcept {
    for (const PendingInteractionTrigger& existing :
         pendingInteractionTriggers) {
        if (existing.owner == &owner &&
            existing.dataTrigger == dataTrigger &&
            existing.propertyTrigger == propertyTrigger) {
            return;
        }
    }
    PendingInteractionTrigger pending;
    pending.owner = &owner;
    pending.names = names;
    pending.dataTrigger = dataTrigger;
    pending.propertyTrigger = propertyTrigger;
    pendingInteractionTriggers.PushBack(std::move(pending));
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

Base::Result<void> InteractivityEngine::ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept {
    auto* runtime = static_cast<InteractivityEngine*>(context);
    if (runtime == nullptr ||
        !runtime->Metadata()->Types().IsDerivedFrom(
            owner.RuntimeType(),
            Aero::FrameworkElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style Trigger action owner is not a FrameworkElement");
    }
    auto& element = static_cast<Aero::FrameworkElement&>(owner);
    return runtime->ExecuteTriggerActions(actions, element, nullptr);
}

void InteractivityEngine::FlushPendingStyleDataTriggerEvaluations() noexcept {
    if (Bindings() != nullptr && Bindings()->HasDirtyBindings()) {
        Aero::BindingEngine::DataBindHook(Bindings());
    }
}

void InteractivityEngine::ClearStyleDataTriggersFor(
        Aero::FrameworkElement& target) noexcept {
    if (Styles() != nullptr && Styles()->Triggers() != nullptr) {
        Styles()->Triggers()->ClearStyleDataTriggersFor(target);
    }
}

Base::Result<std::uint32_t> InteractivityEngine::StartStyleDataTriggers(
        Aero::FrameworkElement& target,
        const Aero::Style& style) noexcept {
    if (Styles() == nullptr || Styles()->Triggers() == nullptr) {
        return std::uint32_t{0U};
    }
    return Styles()->Triggers()->StartStyleDataTriggers(target, style);
}

} // namespace Aero
