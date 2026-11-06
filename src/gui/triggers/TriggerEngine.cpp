#include "gui/triggers/TriggerEngine.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Base/Assert.hpp>
#include <cstdlib>
#include <cstring>

namespace Aero {

bool ComparePropertyValues(
    const Meta::PropertyValue& rawActual,
    const Meta::PropertyValue& expected,
    const Meta::Registry* metadata,
    PropertyComparisonOperator op) noexcept {
    // Fast path for exact-type non-object scalar equality (Boolean, Int64, Double, Enum, etc.)
    if (op == PropertyComparisonOperator::Equal &&
        rawActual.Kind() == expected.Kind() &&
        rawActual.Type() == expected.Type() &&
        rawActual.Kind() != Meta::ValueKind::Object &&
        rawActual.Kind() != Meta::ValueKind::Custom &&
        !rawActual.IsUnset()) {
        return rawActual == expected;
    }

    Meta::PropertyValue unboxedStorage;
    const Meta::PropertyValue* actualPtr = &rawActual;
    if (rawActual.Kind() == Meta::ValueKind::Object && !rawActual.IsNullObject()) {
        unboxedStorage = Base::ValueConversion::UnboxItemValue(rawActual);
        actualPtr = &unboxedStorage;
    }
    const Meta::PropertyValue& actual = *actualPtr;
    const Meta::TypeId nullableBoolType = Meta::TypeOf<::Aero::Nullable<bool>>();
    if (expected.IsNullObject() || expected.IsUnset()) {
        bool eq = actual.IsNullObject() || actual.IsUnset();
        if (!eq && actual.Type() == nullableBoolType) {
            auto decoded = Meta::ValueCodec<::Aero::Nullable<bool>>::Decode(actual);
            eq = decoded && !decoded.Value().GetHasValue();
        }
        if (op == PropertyComparisonOperator::Equal) return eq;
        if (op == PropertyComparisonOperator::NotEqual) return !eq;
        return false;
    }

    if (actual.Type() == nullableBoolType || expected.Type() == nullableBoolType) {
        auto decodeNullable = [](const Meta::PropertyValue& v) -> ::Aero::Nullable<bool> {
            if (v.Type() == Meta::TypeOf<::Aero::Nullable<bool>>()) {
                auto r = Meta::ValueCodec<::Aero::Nullable<bool>>::Decode(v);
                if (r) return r.Value();
            }
            if (v.Kind() == Meta::ValueKind::Boolean) {
                return ::Aero::Nullable<bool>{v.AsBoolean()};
            }
            if (v.Kind() == Meta::ValueKind::String) {
                auto parsed = Base::ValueConversion::ConvertNullableBoolean(v.AsString());
                if (parsed) return parsed.Value();
            }
            return ::Aero::Nullable<bool>{};
        };
        const auto leftNull = decodeNullable(actual);
        const auto rightNull = decodeNullable(expected);
        const bool eq = (leftNull == rightNull);
        if (op == PropertyComparisonOperator::Equal) return eq;
        if (op == PropertyComparisonOperator::NotEqual) return !eq;
        return false;
    }

    const Meta::PropertyValue* leftPtr = &actual;
    const Meta::PropertyValue* rightPtr = &expected;
    Meta::PropertyValue convertedStorage;

    const Meta::Registry* activeMetadata = metadata != nullptr
        ? metadata
        : Meta::CurrentObjectFactory().metadata;
    if (expected.Kind() == Meta::ValueKind::String &&
        actual.Kind() != Meta::ValueKind::String) {
        if (activeMetadata != nullptr) {
            Base::Result<Meta::PropertyValue> converted =
                activeMetadata->TryConvertText(actual.Type(), expected.AsString());
            if (converted) {
                convertedStorage = std::move(converted).Value();
                rightPtr = &convertedStorage;
            }
        }
    } else if (actual.Kind() == Meta::ValueKind::String &&
               expected.Kind() != Meta::ValueKind::String) {
        if (activeMetadata != nullptr) {
            Base::Result<Meta::PropertyValue> converted =
                activeMetadata->TryConvertText(expected.Type(), actual.AsString());
            if (converted) {
                convertedStorage = std::move(converted).Value();
                leftPtr = &convertedStorage;
            }
        }
    }

    const Meta::PropertyValue& leftValue = *leftPtr;
    const Meta::PropertyValue& rightValue = *rightPtr;

    if (leftValue == rightValue) {
        if (op == PropertyComparisonOperator::Equal) return true;
        if (op == PropertyComparisonOperator::NotEqual) return false;
    }

    if (leftValue.Kind() == Meta::ValueKind::String &&
        rightValue.Kind() == Meta::ValueKind::String) {
        const int order = leftValue.AsString().Compare(rightValue.AsString());
        switch (op) {
        case PropertyComparisonOperator::Equal:
            return order == 0;
        case PropertyComparisonOperator::NotEqual:
            return order != 0;
        case PropertyComparisonOperator::LessThan:
            return order < 0;
        case PropertyComparisonOperator::LessThanOrEqual:
            return order <= 0;
        case PropertyComparisonOperator::GreaterThan:
            return order > 0;
        case PropertyComparisonOperator::GreaterThanOrEqual:
            return order >= 0;
        default:
            return false;
        }
    }

    long double leftNumber = 0.0L;
    long double rightNumber = 0.0L;
    if (Base::ValueConversion::TryExtractNumber(leftValue, leftNumber, true) &&
        Base::ValueConversion::TryExtractNumber(rightValue, rightNumber, true)) {
        switch (op) {
        case PropertyComparisonOperator::Equal:
            return leftNumber == rightNumber;
        case PropertyComparisonOperator::NotEqual:
            return leftNumber != rightNumber;
        case PropertyComparisonOperator::LessThan:
            return leftNumber < rightNumber;
        case PropertyComparisonOperator::LessThanOrEqual:
            return leftNumber <= rightNumber;
        case PropertyComparisonOperator::GreaterThan:
            return leftNumber > rightNumber;
        case PropertyComparisonOperator::GreaterThanOrEqual:
            return leftNumber >= rightNumber;
        default:
            return false;
        }
    }

    if (op == PropertyComparisonOperator::Equal) return false;
    if (op == PropertyComparisonOperator::NotEqual) return true;

    return false;
}

bool ComparePropertyValues(
    const Meta::PropertyValue& actual,
    const Meta::PropertyValue& expected,
    const Meta::Registry* metadata,
    Base::StringView comparison) noexcept {
    PropertyComparisonOperator op = PropertyComparisonOperator::Equal;
    if (!comparison.Empty()) {
        switch (comparison.SizeBytes()) {
        case 5:
            if (Base::ValueConversion::EqualsAsciiInsensitive(comparison, "Equal")) {
                op = PropertyComparisonOperator::Equal;
            } else {
                return false;
            }
            break;
        case 8: {
            const char c = comparison[0];
            if ((c == 'N' || c == 'n') &&
                Base::ValueConversion::EqualsAsciiInsensitive(comparison, "NotEqual")) {
                op = PropertyComparisonOperator::NotEqual;
            } else if ((c == 'L' || c == 'l') &&
                       Base::ValueConversion::EqualsAsciiInsensitive(comparison, "LessThan")) {
                op = PropertyComparisonOperator::LessThan;
            } else {
                return false;
            }
            break;
        }
        case 11:
            if (Base::ValueConversion::EqualsAsciiInsensitive(comparison, "GreaterThan")) {
                op = PropertyComparisonOperator::GreaterThan;
            } else {
                return false;
            }
            break;
        case 15:
            if (Base::ValueConversion::EqualsAsciiInsensitive(comparison, "LessThanOrEqual")) {
                op = PropertyComparisonOperator::LessThanOrEqual;
            } else {
                return false;
            }
            break;
        case 18:
            if (Base::ValueConversion::EqualsAsciiInsensitive(comparison, "GreaterThanOrEqual")) {
                op = PropertyComparisonOperator::GreaterThanOrEqual;
            } else {
                return false;
            }
            break;
        default:
            return false;
        }
    }
    return ComparePropertyValues(actual, expected, metadata, op);
}

bool TriggerPlan::IsConditionMet(
    const DependencyObject& object) const noexcept {
    Base::Result<PropertyValue> current = object.GetValue(property);
    if (!current) return false;
    const Meta::Registry* metadata = Meta::CurrentObjectFactory().metadata;
    return EvaluatePropertyTriggerCondition(current.Value(), value, metadata);
}

TriggerEngine::TriggerEngine(
    StyleProviderSession& values,
    DependencyPropertyRegistry& properties,
    Base::Vector<StyleApplication>& applications,
    const Base::HashMap<const DependencyObject*, std::uint32_t>& objectIndexMap) noexcept
    : values_(values),
      properties_(properties),
      applications_(applications),
      objectIndexMap_(objectIndexMap),
      propertyChangedHandler_(this, &TriggerEngine::OnPropertyChanged) {}

TriggerEngine::~TriggerEngine() noexcept {
    // P3.2: no frame-hook registration; nothing to unregister.
}

void TriggerEngine::SubscribeTriggers(
    DependencyObject& object, const Style& style) noexcept {
    if (StyleRuntimeTriggerMask(style) == 0ULL) {
        return;
    }
    TriggerSubscriptionFilter filter;
    for (const TriggerPlan& trigger : StyleRuntimeTriggers(style)) {
        if (!trigger.IsBindingTrigger() && filter.TryAcquire(trigger.property)) {
            object.AddValueChangedHandler(trigger.property, propertyChangedHandler_);
        }
    }
}

void TriggerEngine::UnsubscribeTriggers(
    DependencyObject& object, const Style& style) noexcept {
    if (StyleRuntimeTriggerMask(style) == 0ULL) {
        return;
    }
    TriggerSubscriptionFilter filter;
    for (const TriggerPlan& trigger : StyleRuntimeTriggers(style)) {
        if (!trigger.IsBindingTrigger() && filter.TryAcquire(trigger.property)) {
            (void)object.RemoveValueChangedHandler(trigger.property, propertyChangedHandler_);
        }
    }
}

void TriggerEngine::EvaluateTriggers(
    DependencyObject& object, const Style& style) noexcept {
    const std::uint32_t applicationIndex = FindApplication(object);
    if (applicationIndex == UINT32_MAX) {
        return;
    }
    StyleApplication& application = applications_[applicationIndex];
    const Base::Span<const TriggerPlan> triggers = StyleRuntimeTriggers(style);
    if (triggers.Empty()) {
        return;
    }

    TriggerActiveBitset activeStates(triggers.Size());
    bool anyStateChanged = !application.triggersInitialized;
    application.triggersInitialized = true;

    for (std::uint32_t index = 0U; index < triggers.Size(); ++index) {
        const TriggerPlan& trigger = triggers[index];
        bool active = false;
        const std::uint64_t bit = index < 64U ? (UINT64_C(1) << index) : 0ULL;
        if (trigger.IsBindingTrigger()) {
            active = (application.bindingTriggerKnownMask & bit) != 0ULL &&
                (application.bindingTriggerStatesMask & bit) != 0ULL;
        } else {
            active = trigger.IsConditionMet(object);
        }
        activeStates.Set(index, active);
        if (!anyStateChanged) {
            const bool wasActive = (application.triggerStatesMask & bit) != 0ULL;
            if (active != wasActive) {
                anyStateChanged = true;
            }
        }
    }

    if (!anyStateChanged) {
        return;
    }

    const std::uint64_t previousActiveMask = application.triggerStatesMask;
    if (previousActiveMask != 0ULL) {
        TriggerSubscriptionFilter clearedFilter;
        for (std::uint32_t triggerIndex = 0U;
             triggerIndex < triggers.Size() && triggerIndex < 64U;
             ++triggerIndex) {
            if ((previousActiveMask & (UINT64_C(1) << triggerIndex)) == 0ULL) {
                continue;
            }
            for (const StyleTriggerSetter& setter : triggers[triggerIndex].setters) {
                if (ShouldApplyOrClearTriggerSetter(setter.value) &&
                    clearedFilter.TryAcquire(setter.property)) {
                    (void)values_.ClearTriggerValue(object, setter.property);
                }
            }
        }
    }

    for (std::uint32_t index = 0U; index < triggers.Size(); ++index) {
        if (!activeStates.Get(index)) continue;
        for (const StyleTriggerSetter& setter : triggers[index].setters) {
            PropertyValue setterValue;
            if (ResolveTriggerSetterValue(object, setter.property, setter.value, setterValue)) {
                (void)values_.SetTriggerValue(object, setter.property, setterValue);
            }
        }
    }
    (void)values_.Flush();
    for (std::uint32_t index = 0U; index < triggers.Size() && index < 64U; ++index) {
        const TriggerPlan& trigger = triggers[index];
        const bool active = activeStates.Get(index);
        const std::uint32_t currentAppIndex = FindApplication(object);
        if (currentAppIndex == UINT32_MAX || currentAppIndex >= applications_.Size()) {
            break;
        }
        StyleApplication& currentApp = applications_[currentAppIndex];
        const std::uint64_t bit = (UINT64_C(1) << index);
        const bool wasActive = (currentApp.triggerStatesMask & bit) != 0ULL;
        if (active == wasActive) continue;
        if (active) {
            currentApp.triggerStatesMask |= bit;
        } else {
            currentApp.triggerStatesMask &= ~bit;
        }
        ExecuteTriggerActions(
            object,
            active ? trigger.enterActions.AsSpan() : trigger.exitActions.AsSpan());
    }
}

void TriggerEngine::ExecuteTriggerActions(
    DependencyObject& object,
    Base::Span<const Base::Ref<Base::Object>> actions) noexcept {
    if (actions.Empty() || triggerActionHandler_ == nullptr) {
        return;
    }
    Base::Result<void> res = triggerActionHandler_(
        object, actions, triggerActionContext_);
    if (!res) {
        lastActionStatus_ = res.GetStatus();
    }
}

void TriggerEngine::ClearTriggerSetters(
    DependencyObject& object, const Style& style) noexcept {
    TriggerSubscriptionFilter clearedFilter;
    for (const TriggerPlan& trigger : StyleRuntimeTriggers(style)) {
        for (const StyleTriggerSetter& setter : trigger.setters) {
            if (ShouldApplyOrClearTriggerSetter(setter.value) &&
                clearedFilter.TryAcquire(setter.property)) {
                (void)values_.ClearTriggerValue(object, setter.property);
            }
        }
    }
}

void TriggerEngine::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const std::uint32_t index = FindApplication(object);
    if (index == UINT32_MAX) return;
    const StyleApplication& application = applications_[index];
    const std::uint64_t propMask = PropertyTriggerMask(args.GetProperty());
    if ((application.triggerPropertyMask & propMask) != propMask) {
        return;
    }
    const Style& style = *application.style;
    for (const TriggerPlan& trigger : StyleRuntimeTriggers(style)) {
        if (!trigger.IsBindingTrigger() &&
            trigger.property == args.GetProperty()) {
            if (values_.IsFlushing()) {
                QueueTriggerEvaluation(object);
            } else {
                EvaluateTriggers(object, style);
            }
            return;
        }
    }
}

void TriggerEngine::EnableDataBindPhase(
    DependencyObject& object) noexcept {
    Dispatcher& dispatcher = object.GetDispatcher();
    if (dataBindPhaseEnabled_) {
        AERO_ASSERT(dispatcher_ == &dispatcher && "StyleEngine objects must share one Dispatcher");
        return;
    }
    dispatcher_ = &dispatcher;
    dataBindPhaseEnabled_ = true;
}

void TriggerEngine::QueueTriggerEvaluation(
    DependencyObject& object) noexcept {
    for (DependencyObject* pending :
         pendingTriggerEvaluations_) {
        if (pending == &object) return;
    }
    pendingTriggerEvaluations_.PushBack(
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

std::uint32_t
TriggerEngine::FlushPendingTriggerEvaluations() noexcept {
    Base::Vector<DependencyObject*> pending =
        std::move(pendingTriggerEvaluations_);
    std::uint32_t evaluatedCount = 0U;
    for (DependencyObject* object : pending) {
        if (object == nullptr) continue;
        const std::uint32_t index =
            FindApplication(*object);
        if (index == UINT32_MAX) continue;
        EvaluateTriggers(
            *object,
            *applications_[index].style);
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
    manager->FlushPendingTriggerEvaluations();
}

void TriggerEngine::SetBindingTriggerState(
    DependencyObject& object,
    const Style& style,
    std::uint32_t triggerIndex,
    bool active) noexcept {
    const std::uint32_t applicationIndex = FindApplication(object);
    if (applicationIndex == UINT32_MAX ||
        applications_[applicationIndex].style != &style ||
        triggerIndex >= StyleRuntimeTriggers(style).Size() ||
        !StyleRuntimeTriggers(style)[triggerIndex].IsBindingTrigger()) {
        return;
    }
    StyleApplication& application = applications_[applicationIndex];
    if (triggerIndex < 64U) {
        const std::uint64_t bit = (UINT64_C(1) << triggerIndex);
        application.bindingTriggerKnownMask |= bit;
        if (active) {
            application.bindingTriggerStatesMask |= bit;
        } else {
            application.bindingTriggerStatesMask &= ~bit;
        }
    }
    EvaluateTriggers(object, style);
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
    const std::uint32_t* found = objectIndexMap_.Find(&object);
    return found != nullptr ? *found : UINT32_MAX;
}

} // namespace Aero
