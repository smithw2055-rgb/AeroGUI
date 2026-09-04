#include "gui/triggers/TriggerEngine.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"
#include "gui/triggers/TriggerValueCompare.hpp"

namespace Aero {

TriggerEngine::TriggerEngine(
    StyleProviderSession& values,
    DependencyPropertyRegistry& properties,
    Base::Vector<StyleApplication>& applications) noexcept
    : values_(values),
      properties_(properties),
      applications_(applications),
      propertyChangedHandler_(this, &TriggerEngine::OnPropertyChanged) {}

TriggerEngine::~TriggerEngine() noexcept {
    // P3.2: no frame-hook registration; nothing to unregister.
}

Base::Result<void> TriggerEngine::SubscribeTriggers(
    DependencyObject& object, const Style& style) noexcept {
    for (std::uint32_t index = 0U;
         index < StylePrivate::RuntimeTriggers(style).Size();
         ++index) {
        const TriggerPlan& trigger =
            StylePrivate::RuntimeTriggers(style)[index];
        if (trigger.IsBindingTrigger()) continue;
        const DependencyPropertyHandle property = trigger.property;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                (StylePrivate::RuntimeTriggers(style)[previous].IsBindingTrigger() ||
                 StylePrivate::RuntimeTriggers(style)[previous].property != property);
        }
        if (!first) continue;
        object.AddValueChangedHandler(
            property, propertyChangedHandler_);
    }
    return {};
}

void TriggerEngine::UnsubscribeTriggers(
    DependencyObject& object, const Style& style) noexcept {
    for (std::uint32_t index = 0U;
         index < StylePrivate::RuntimeTriggers(style).Size();
         ++index) {
        const TriggerPlan& trigger =
            StylePrivate::RuntimeTriggers(style)[index];
        if (trigger.IsBindingTrigger()) continue;
        const DependencyPropertyHandle property = trigger.property;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                (StylePrivate::RuntimeTriggers(style)[previous].IsBindingTrigger() ||
                 StylePrivate::RuntimeTriggers(style)[previous].property != property);
        }
        if (first) {
            (void)object.RemoveValueChangedHandler(
                property, propertyChangedHandler_);
        }
    }
}

Base::Result<void> TriggerEngine::EvaluateTriggers(
    DependencyObject& object, const Style& style) noexcept {
    const std::uint32_t applicationIndex =
        FindApplication(object);
    if (applicationIndex == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style application was not found while evaluating triggers");
    }
    StyleApplication& application =
        applications_[applicationIndex];
    Base::Result<void> cleared =
        ClearTriggerSetters(object, style);
    if (!cleared) return cleared.GetStatus();
    const Base::Span<const TriggerPlan> triggers =
        StylePrivate::RuntimeTriggers(style);
    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        bool active = false;
        if (trigger.IsBindingTrigger()) {
            active = application.bindingTriggerKnown[index] != 0U &&
                application.bindingTriggerStates[index] != 0U;
        } else {
            Base::Result<bool> met =
                IsTriggerConditionMet(object, trigger);
            if (!met) return met.GetStatus();
            active = met.Value();
        }
        if (active) {
            for (const StyleTriggerSetter& setter :
                 trigger.setters) {
                if (IsDeferredBindingSetterValue(setter.value)) {
                    continue;
                }
                Base::Result<void> applied =
                    values_.SetTriggerValue(
                        object, setter.property,
                        setter.value);
                if (!applied) {
                    return applied.GetStatus();
                }
            }
        }
    }
    Base::Result<std::uint32_t> flushed =
        values_.Flush();
    if (!flushed) return flushed.GetStatus();
    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        bool active = false;
        if (trigger.IsBindingTrigger()) {
            active = application.bindingTriggerKnown[index] != 0U &&
                application.bindingTriggerStates[index] != 0U;
        } else {
            Base::Result<bool> met =
                IsTriggerConditionMet(object, trigger);
            if (!met) return met.GetStatus();
            active = met.Value();
        }
        const bool wasActive =
            application.triggerStates[index] != 0U;
        if (active == wasActive) continue;
        application.triggerStates[index] =
            active ? 1U : 0U;
        Base::Result<void> actions =
            ExecuteTriggerActions(
                object,
                active
                    ? trigger.enterActions.AsSpan()
                    : trigger.exitActions.AsSpan());
        if (!actions) {
            lastActionStatus_ = actions.GetStatus();
            return actions.GetStatus();
        }
    }
    return {};
}

Base::Result<void> TriggerEngine::ExecuteTriggerActions(
    DependencyObject& object,
    Base::Span<const Base::Ref<Base::Object>>
        actions) noexcept {
    if (actions.Empty() ||
        triggerActionHandler_ == nullptr) {
        return {};
    }
    return triggerActionHandler_(
        object, actions, triggerActionContext_);
}

Base::Result<void> TriggerEngine::ClearTriggerSetters(
    DependencyObject& object, const Style& style) noexcept {
    const Base::Span<const TriggerPlan> triggers =
        StylePrivate::RuntimeTriggers(style);
    for (std::uint32_t triggerIndex = 0U;
         triggerIndex < triggers.Size();
         ++triggerIndex) {
        const TriggerPlan& trigger =
            triggers[triggerIndex];
        for (std::uint32_t setterIndex = 0U;
             setterIndex < trigger.setters.Size();
             ++setterIndex) {
            if (IsDeferredBindingSetterValue(
                    trigger.setters[setterIndex].value)) {
                continue;
            }
            const DependencyPropertyHandle property =
                trigger.setters[setterIndex].property;
            bool first = true;
            for (std::uint32_t earlierTrigger = 0U;
                 earlierTrigger <= triggerIndex;
                 ++earlierTrigger) {
                const TriggerPlan& earlier =
                    triggers[earlierTrigger];
                const std::uint32_t limit =
                    earlierTrigger == triggerIndex
                        ? setterIndex : earlier.setters.Size();
                for (std::uint32_t earlierSetter = 0U;
                     earlierSetter < limit;
                     ++earlierSetter) {
                    first = first &&
                        earlier.setters[earlierSetter].property !=
                            property;
                }
            }
            if (!first) continue;
            Base::Result<void> cleared =
                values_.ClearTriggerValue(object, property);
            if (!cleared) return cleared.GetStatus();
        }
    }
    return {};
}

void TriggerEngine::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const std::uint32_t index = FindApplication(object);
    if (index == UINT32_MAX) return;
    const Style& style = *applications_[index].style;
    for (const TriggerPlan& trigger : StylePrivate::RuntimeTriggers(style)) {
        if (!trigger.IsBindingTrigger() &&
            trigger.property == args.GetProperty()) {
            if (values_.IsFlushing()) {
                Base::Result<void> queued =
                    QueueTriggerEvaluation(object);
                if (!queued) {
                    lastActionStatus_ =
                        queued.GetStatus();
                }
            } else {
                Base::Result<void> evaluated =
                    EvaluateTriggers(object, style);
                if (!evaluated) {
                    lastActionStatus_ =
                        evaluated.GetStatus();
                }
            }
            return;
        }
    }
}

Base::Result<void> TriggerEngine::EnableDataBindPhase(
    DependencyObject& object) noexcept {
    Dispatcher& dispatcher = object.GetDispatcher();
    if (dataBindPhaseEnabled_) {
        return dispatcher_ == &dispatcher
            ? Base::Result<void>()
            : Base::Result<void>(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StyleEngine objects must share one Dispatcher"));
    }
    // P3.2: no hook registration; ViewFrame drives TriggerPhaseHook().
    dispatcher_ = &dispatcher;
    dataBindPhaseEnabled_ = true;
    return {};
}

Base::Result<void> TriggerEngine::QueueTriggerEvaluation(
    DependencyObject& object) noexcept {
    for (DependencyObject* pending :
         pendingTriggerEvaluations_) {
        if (pending == &object) return {};
    }
    return pendingTriggerEvaluations_.PushBack(
        &object);
}

void TriggerEngine::RemovePendingTriggerEvaluation(
    DependencyObject& object) noexcept {
    for (std::uint32_t index = 0U;
         index < pendingTriggerEvaluations_.Size();) {
        if (pendingTriggerEvaluations_[index] != &object) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < pendingTriggerEvaluations_.Size();
             ++next) {
            pendingTriggerEvaluations_[next - 1U] =
                pendingTriggerEvaluations_[next];
        }
        pendingTriggerEvaluations_.PopBack();
    }
}

Base::Result<std::uint32_t>
TriggerEngine::FlushPendingTriggerEvaluations() noexcept {
    Base::Vector<DependencyObject*> pending =
        std::move(pendingTriggerEvaluations_);
    std::uint32_t evaluatedCount = 0U;
    for (DependencyObject* object : pending) {
        if (object == nullptr) continue;
        const std::uint32_t index =
            FindApplication(*object);
        if (index == UINT32_MAX) continue;
        Base::Result<void> evaluated =
            EvaluateTriggers(
                *object,
                *applications_[index].style);
        if (!evaluated) return evaluated.GetStatus();
        ++evaluatedCount;
    }
    return evaluatedCount;
}

void TriggerEngine::TriggerPhaseHook(
    void* context) noexcept {
    auto* manager =
        static_cast<TriggerEngine*>(context);
    if (manager == nullptr ||
        !manager->lastActionStatus_.IsOk()) {
        return;
    }
    Base::Result<std::uint32_t> flushed =
        manager->FlushPendingTriggerEvaluations();
    if (!flushed) {
        manager->lastActionStatus_ =
            flushed.GetStatus();
    }
}

Base::Result<void> TriggerEngine::SetBindingTriggerState(
    DependencyObject& object,
    const Style& style,
    std::uint32_t triggerIndex,
    bool active) noexcept {
    const std::uint32_t applicationIndex = FindApplication(object);
    if (applicationIndex == UINT32_MAX ||
        applications_[applicationIndex].style != &style ||
        triggerIndex >= StylePrivate::RuntimeTriggers(style).Size() ||
        !StylePrivate::RuntimeTriggers(style)[triggerIndex].IsBindingTrigger()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style DataTrigger application was not found");
    }
    StyleApplication& application = applications_[applicationIndex];
    application.bindingTriggerKnown[triggerIndex] = 1U;
    application.bindingTriggerStates[triggerIndex] = active ? 1U : 0U;
    return EvaluateTriggers(object, style);
}

void TriggerEngine::SetTriggerActionHandler(
    TriggerActionHandler handler, void* context) noexcept {
    triggerActionHandler_ = handler;
    triggerActionContext_ = context;
}

const Base::Status& TriggerEngine::LastActionStatus() const noexcept {
    return lastActionStatus_;
}

std::uint32_t TriggerEngine::FindApplication(
    const DependencyObject& object) const noexcept {
    for (std::uint32_t index = 0U;
         index < applications_.Size(); ++index) {
        if (applications_[index].object == &object) {
            return index;
        }
    }
    return UINT32_MAX;
}

} // namespace Aero
