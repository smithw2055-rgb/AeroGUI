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

namespace {

bool TryExtractNumeric(const Meta::PropertyValue& value, long double& output) noexcept {
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
    case Meta::ValueKind::String: {
        Base::StringView str = value.AsString();
        if (str.Empty()) return false;
        char buf[64];
        if (str.SizeBytes() >= sizeof(buf)) return false;
        std::memcpy(buf, str.Data(), str.SizeBytes());
        buf[str.SizeBytes()] = '\0';
        char* endPtr = nullptr;
        output = std::strtold(buf, &endPtr);
        if (endPtr != buf && *endPtr == '\0') {
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

} // namespace

bool ComparePropertyValues(
    const Meta::PropertyValue& actual,
    Meta::PropertyValue expected,
    const Meta::Registry* metadata,
    PropertyComparisonOperator op) noexcept {
    if (actual.Kind() == Meta::ValueKind::Object &&
        !actual.IsNullObject() && actual.AsObject() &&
        actual.AsObject()->RuntimeType() ==
            ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
        return ComparePropertyValues(
            static_cast<const ::Aero::Controls::BoxedItemValue&>(
                *actual.AsObject()).Value(),
            std::move(expected),
            metadata,
            op);
    }
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

    Meta::PropertyValue leftValue = actual;
    Meta::PropertyValue rightValue = std::move(expected);

    if (leftValue.Type() == nullableBoolType || rightValue.Type() == nullableBoolType) {
        auto decodeNullable = [](const Meta::PropertyValue& v) -> ::Aero::Nullable<bool> {
            if (v.Type() == Meta::TypeOf<::Aero::Nullable<bool>>()) {
                auto r = Meta::ValueCodec<::Aero::Nullable<bool>>::Decode(v);
                if (r) return r.Value();
            }
            if (v.Kind() == Meta::ValueKind::Boolean) {
                return ::Aero::Nullable<bool>{v.AsBoolean()};
            }
            if (v.Kind() == Meta::ValueKind::String) {
                const Base::StringView s = v.AsString();
                switch (s.SizeBytes()) {
                case 4:
                    if (Base::ValueConversion::EqualsAsciiInsensitive(s, "True")) return ::Aero::Nullable<bool>{true};
                    if (Base::ValueConversion::EqualsAsciiInsensitive(s, "Null")) return ::Aero::Nullable<bool>{};
                    break;
                case 5:
                    if (Base::ValueConversion::EqualsAsciiInsensitive(s, "False")) return ::Aero::Nullable<bool>{false};
                    break;
                case 8:
                    if (Base::ValueConversion::EqualsAsciiInsensitive(s, "{x:Null}")) return ::Aero::Nullable<bool>{};
                    break;
                default:
                    break;
                }
            }
            return ::Aero::Nullable<bool>{};
        };
        const auto leftNull = decodeNullable(leftValue);
        const auto rightNull = decodeNullable(rightValue);
        const bool eq = (leftNull == rightNull);
        if (op == PropertyComparisonOperator::Equal) return eq;
        if (op == PropertyComparisonOperator::NotEqual) return !eq;
        return false;
    }

    if (rightValue.Kind() == Meta::ValueKind::String &&
        leftValue.Kind() != Meta::ValueKind::String) {
        if (metadata != nullptr) {
            Base::Result<Meta::PropertyValue> converted =
                metadata->TryConvertText(leftValue.Type(), rightValue.AsString());
            if (converted) {
                rightValue = std::move(converted).Value();
            }
        }
    } else if (leftValue.Kind() == Meta::ValueKind::String &&
               rightValue.Kind() != Meta::ValueKind::String) {
        if (metadata != nullptr) {
            Base::Result<Meta::PropertyValue> converted =
                metadata->TryConvertText(rightValue.Type(), leftValue.AsString());
            if (converted) {
                leftValue = std::move(converted).Value();
            }
        }
    }

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
    if (TryExtractNumeric(leftValue, leftNumber) &&
        TryExtractNumeric(rightValue, rightNumber)) {
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
    Meta::PropertyValue expected,
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
    return ComparePropertyValues(actual, std::move(expected), metadata, op);
}

bool TriggerPlan::IsConditionMet(
    const DependencyObject& object) const noexcept {
    Base::Result<PropertyValue> current = object.GetValue(property);
    if (!current) return false;
    const Meta::Registry* metadata = Meta::CurrentObjectFactory().metadata;
    return ComparePropertyValues(current.Value(), value, metadata);
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
    const Base::Span<const TriggerPlan> triggers =
        StyleRuntimeTriggers(style);
    std::uint64_t subscribedMask = 0ULL;
    for (std::uint32_t index = 0U;
         index < triggers.Size();
         ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        if (trigger.IsBindingTrigger()) continue;
        const DependencyPropertyHandle property = trigger.property;
        const std::uint64_t propMask = PropertyTriggerMask(property);
        if ((subscribedMask & propMask) == propMask) {
            bool first = true;
            for (std::uint32_t previous = 0U;
                 previous < index;
                 ++previous) {
                first = first &&
                    (triggers[previous].IsBindingTrigger() ||
                     triggers[previous].property != property);
            }
            if (!first) continue;
        }
        subscribedMask |= propMask;
        object.AddValueChangedHandler(
            property, propertyChangedHandler_);
    }
}

void TriggerEngine::UnsubscribeTriggers(
    DependencyObject& object, const Style& style) noexcept {
    if (StyleRuntimeTriggerMask(style) == 0ULL) {
        return;
    }
    const Base::Span<const TriggerPlan> triggers =
        StyleRuntimeTriggers(style);
    std::uint64_t unsubscribedMask = 0ULL;
    for (std::uint32_t index = 0U;
         index < triggers.Size();
         ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        if (trigger.IsBindingTrigger()) continue;
        const DependencyPropertyHandle property = trigger.property;
        const std::uint64_t propMask = PropertyTriggerMask(property);
        if ((unsubscribedMask & propMask) == propMask) {
            bool first = true;
            for (std::uint32_t previous = 0U;
                 previous < index;
                 ++previous) {
                first = first &&
                    (triggers[previous].IsBindingTrigger() ||
                     triggers[previous].property != property);
            }
            if (!first) continue;
        }
        unsubscribedMask |= propMask;
        (void)object.RemoveValueChangedHandler(
            property, propertyChangedHandler_);
    }
}

void TriggerEngine::EvaluateTriggers(
    DependencyObject& object, const Style& style) noexcept {
    const std::uint32_t applicationIndex =
        FindApplication(object);
    if (applicationIndex == UINT32_MAX) {
        return;
    }
    StyleApplication& application =
        applications_[applicationIndex];
    const Base::Span<const TriggerPlan> triggers =
        StyleRuntimeTriggers(style);
    if (triggers.Empty()) {
        return;
    }

    std::uint64_t activeMask = 0ULL;
    Base::Vector<std::uint8_t> activeStatesFallback;
    if (triggers.Size() > 64U) {
        activeStatesFallback.Resize(triggers.Size(), 0U);
    }
    auto setActive = [&](std::uint32_t idx, bool act) {
        if (idx < 64U) {
            if (act) {
                activeMask |= (UINT64_C(1) << idx);
            }
        } else {
            activeStatesFallback[idx] = act ? 1U : 0U;
        }
    };
    auto getActive = [&](std::uint32_t idx) -> bool {
        if (idx < 64U) {
            return (activeMask & (UINT64_C(1) << idx)) != 0ULL;
        }
        return activeStatesFallback[idx] != 0U;
    };

    bool anyStateChanged = false;
    if (application.triggerStates.Size() < triggers.Size()) {
        anyStateChanged = true;
    }

    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        bool active = false;
        if (trigger.IsBindingTrigger()) {
            active = index < application.bindingTriggerKnown.Size() &&
                index < application.bindingTriggerStates.Size() &&
                application.bindingTriggerKnown[index] != 0U &&
                application.bindingTriggerStates[index] != 0U;
        } else {
            active = trigger.IsConditionMet(object);
        }
        setActive(index, active);
        if (!anyStateChanged) {
            const bool wasActive = index < application.triggerStates.Size() &&
                application.triggerStates[index] != 0U;
            if (active != wasActive) {
                anyStateChanged = true;
            }
        }
    }

    // Short-circuit: if no trigger flipped its active state, no setters or flush needed.
    if (!anyStateChanged) {
        return;
    }

    ClearTriggerSetters(object, style);

    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        if (!getActive(index)) continue;
        const TriggerPlan& trigger =
            triggers[index];
        for (const StyleTriggerSetter& setter :
             trigger.setters) {
            Base::StringView dynamicKey;
            PropertyValue setterValue = setter.value;
            if (IsDynamicResourceSetterValue(setter.value, dynamicKey)) {
                FrameworkElement* element = ::Aero::TryCast<FrameworkElement>(&object);
                ResourceValue lookedUp{};
                if (element != nullptr) {
                    Base::Result<ResourceValue> res = element->FindResource(dynamicKey);
                    if (res) lookedUp = res.Value();
                }
                if (!lookedUp.IsUnset()) {
                    const DependencyProperty* prop =
                        DependencyObjectAccess::PropertyRegistry(object).Find(setter.property);
                    if (prop != nullptr) {
                        Base::Result<PropertyValue> converted =
                            Markup::ConvertDynamicResourceValue(lookedUp, *prop);
                        if (converted) {
                            setterValue = std::move(converted).Value();
                        }
                    }
                }
            } else if (IsDeferredBindingSetterValue(setter.value)) {
                continue;
            }
            (void)values_.SetTriggerValue(
                object, setter.property,
                setterValue);
        }
    }
    (void)values_.Flush();
    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        const bool active = getActive(index);
        const std::uint32_t currentAppIndex = FindApplication(object);
        if (currentAppIndex == UINT32_MAX || currentAppIndex >= applications_.Size()) {
            break;
        }
        StyleApplication& currentApp = applications_[currentAppIndex];
        if (index >= currentApp.triggerStates.Size()) {
            break;
        }
        const bool wasActive =
            currentApp.triggerStates[index] != 0U;
        if (active == wasActive) continue;
        currentApp.triggerStates[index] =
            active ? 1U : 0U;
        ExecuteTriggerActions(
            object,
            active
                ? trigger.enterActions.AsSpan()
                : trigger.exitActions.AsSpan());
    }
}

void TriggerEngine::ExecuteTriggerActions(
    DependencyObject& object,
    Base::Span<const Base::Ref<Base::Object>>
        actions) noexcept {
    if (actions.Empty() ||
        triggerActionHandler_ == nullptr) {
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
    const Base::Span<const TriggerPlan> triggers =
        StyleRuntimeTriggers(style);
    std::uint64_t clearedMask = 0ULL;
    for (std::uint32_t triggerIndex = 0U;
         triggerIndex < triggers.Size();
         ++triggerIndex) {
        const TriggerPlan& trigger =
            triggers[triggerIndex];
        for (std::uint32_t setterIndex = 0U;
             setterIndex < trigger.setters.Size();
             ++setterIndex) {
            Base::StringView dynamicKey;
            if (IsDeferredBindingSetterValue(
                    trigger.setters[setterIndex].value) &&
                !IsDynamicResourceSetterValue(trigger.setters[setterIndex].value, dynamicKey)) {
                continue;
            }
            const DependencyPropertyHandle property =
                trigger.setters[setterIndex].property;
            const std::uint64_t propMask = PropertyTriggerMask(property);
            if ((clearedMask & propMask) == propMask) {
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
            }
            clearedMask |= propMask;
            (void)values_.ClearTriggerValue(object, property);
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
    application.bindingTriggerKnown[triggerIndex] = 1U;
    application.bindingTriggerStates[triggerIndex] = active ? 1U : 0U;
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
