#include "gui/triggers/TriggerEngine.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/data/BindingCommon.hpp"
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Base/Assert.hpp>
#include <cstdlib>
#include <cstring>
#include <new>

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

bool EvaluateTriggerPropertyCondition(
    const DependencyObject& source,
    DependencyPropertyHandle property,
    const Meta::PropertyValue& expected,
    const Meta::Registry* metadata) noexcept {
    const PropertyValue current = source.GetValue(property);
    if (current.IsUnset()) return false;
    return EvaluatePropertyTriggerCondition(current, expected, metadata);
}

bool TriggerPlan::IsConditionMet(
    const DependencyObject& object) const noexcept {
    const Meta::Registry* metadata = Meta::CurrentObjectFactory().metadata;
    return EvaluateTriggerPropertyCondition(object, property, value, metadata);
}

Base::Object* TriggerEngine::ResolveTriggerBindingSource(
    const Meta::Registry* metadata,
    const Data::Binding& binding,
    FrameworkElement& owner,
    Controls::DataTemplateTriggerInstance* dataTemplateContext,
    const NameScope* names,
    Base::Object* self) noexcept {
    if (dataTemplateContext != nullptr && !binding.GetElementName().Empty()) {
        Base::Object* source = dataTemplateContext->FindName(binding.GetElementName());
        if (source != nullptr) return source;
    }
    Base::Result<Base::Object*> codeSource = Data::ResolveCodeBindingSource(
        binding, owner, metadata, names, self, /*startAtTarget=*/true);
    if (codeSource && codeSource.Value() != nullptr) {
        return codeSource.Value();
    }
    if (!binding.GetElementName().Empty()) {
        Base::Object* source = ElementTree::FindInTree(owner, binding.GetElementName());
        if (source != nullptr) return source;
    }
    if (binding.GetSource() || !binding.GetElementName().Empty() || binding.GetRelativeSource()) {
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

Base::Result<TriggerEngine::TriggerBindingSubscription>
TriggerEngine::AttachTriggerBinding(
    BindingEngine& bindings,
    const Data::Binding& binding,
    FrameworkElement& owner,
    Controls::DataTemplateTriggerInstance* dataTemplateContext,
    const NameScope* names,
    Base::Object* explicitSourceOverride,
    FrameworkElement* dataContextOwnerOverride,
    Meta::DependencyPropertyChangedEventHandler handler,
    bool evaluateImmediately,
    bool ignoreInitialValue,
    const Data::CompiledBindingPath* precompiledPath) noexcept {
    if (bindings.Metadata() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Trigger Binding requires an active metadata registry");
    }

    Base::Object* explicitSource = explicitSourceOverride;
    const bool authoredExplicitSource =
        binding.GetSource() ||
        !binding.GetElementName().Empty() ||
        binding.GetRelativeSource();
    if (explicitSource == nullptr && authoredExplicitSource) {
        Base::Object* selfObject = &owner;
        if (dataTemplateContext != nullptr &&
            binding.GetRelativeSource() &&
            binding.GetRelativeSource()->GetMode() == Data::RelativeSourceMode::Self &&
            owner.GetTemplatedParent() != nullptr) {
            selfObject = owner.GetTemplatedParent();
        }
        explicitSource = ResolveTriggerBindingSource(
            bindings.Metadata(),
            binding,
            owner,
            dataTemplateContext,
            names,
            selfObject);
        if (explicitSource == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Trigger Binding explicit source was not found");
        }
    }

    bool pathSliced = false;
    Base::StringView path = binding.GetPath().GetPath();
    constexpr Base::StringView kTemplatedParentPrefix("TemplatedParent.");
    if (explicitSource == nullptr &&
        path.SizeBytes() > kTemplatedParentPrefix.SizeBytes() &&
        path.Substr(0U, kTemplatedParentPrefix.SizeBytes()) ==
            kTemplatedParentPrefix) {
        FrameworkElement* templatedOwner =
            dataContextOwnerOverride != nullptr ? dataContextOwnerOverride : &owner;
        explicitSource = templatedOwner->GetTemplatedParent();
        if (explicitSource == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Trigger Binding TemplatedParent source was not found");
        }
        path = path.Substr(
            kTemplatedParentPrefix.SizeBytes(),
            path.SizeBytes() - kTemplatedParentPrefix.SizeBytes());
        pathSliced = true;
    }

    Base::Result<Base::Ref<Data::MultiBindingProxy>> createdProxy =
        Base::MakeRef<Data::MultiBindingProxy>();
    if (!createdProxy) {
        return createdProxy.GetStatus();
    }
    Base::Ref<Data::MultiBindingProxy> proxy = std::move(createdProxy).Value();
    proxy->SetValue(
        Data::MultiBindingProxy::ValueProperty.Handle(),
        MakeTriggerProxySentinel());

    FrameworkElement* dcOwner =
        dataContextOwnerOverride != nullptr ? dataContextOwnerOverride : &owner;
    Data::MetadataBindingDescriptor descriptor =
        Data::MetadataBindingDescriptor::FromBinding(
            binding,
            *proxy,
            Data::MultiBindingProxy::ValueProperty.Handle(),
            bindings.Metadata(),
            explicitSource,
            dcOwner);
    descriptor.path = path;
    descriptor.precompiledPath = pathSliced ? nullptr : precompiledPath;
    descriptor.bindsToSource = path.Empty();
    descriptor.mode = Data::BindingMode::OneWay;
    descriptor.updateSourceTrigger = Meta::UpdateSourceTrigger::PropertyChanged;

    if (!ignoreInitialValue && !handler.Empty()) {
        proxy->AddValueChangedHandler(
            Data::MultiBindingProxy::ValueProperty.Handle(),
            handler);
    }

    Base::Result<Data::BindingHandle> attached = bindings.Attach(descriptor);
    if (!attached) {
        return attached.GetStatus();
    }

    if (evaluateImmediately) {
        static_cast<void>(bindings.EvaluateNow(attached.Value()));
    }

    if (ignoreInitialValue && !handler.Empty()) {
        proxy->AddValueChangedHandler(
            Data::MultiBindingProxy::ValueProperty.Handle(),
            handler);
    }

    TriggerBindingSubscription result;
    result.proxy = std::move(proxy);
    result.handle = attached.Value();
    return result;
}

Base::Result<bool> TriggerEngine::EvaluateControlTemplateTriggers(
    TemplatedParentProviderSession& values,
    const Controls::ControlTemplate& plan,
    Controls::Control* templatedParent,
    const ResourceDictionary* resources,
    bool& triggersEvaluated,
    std::uint64_t& triggerStatesMask,
    TemplateTargetResolver resolveTarget,
    const void* resolveContext) noexcept {
    const Base::Span<const Controls::TemplatePropertyTrigger> triggers =
        Controls::FrameworkTemplateState::Triggers(plan);
    const bool firstEval = !triggersEvaluated;
    if (firstEval) {
        triggersEvaluated = true;
        triggerStatesMask = 0ULL;
        for (const Controls::TemplatePropertyTrigger& trigger : triggers) {
            for (const Controls::TemplateTriggerSetter& setter : trigger.setters) {
                if (!ShouldApplyOrClearTriggerSetter(setter.value)) {
                    continue;
                }
                DependencyObject* target = resolveTarget != nullptr
                    ? resolveTarget(resolveContext, setter.targetName.View(), setter.targetPartIndex)
                    : nullptr;
                if (target == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Template trigger target name was not found");
                }
            }
        }
    }

    if (triggers.Empty()) {
        Base::Result<std::uint32_t> flushed = values.Flush();
        if (!flushed) return flushed.GetStatus();
        return false;
    }

    TriggerActiveBitset activeStates(triggers.Size());
    bool anyStateChanged = firstEval;
    for (std::uint32_t index = 0U; index < triggers.Size(); ++index) {
        const Controls::TemplatePropertyTrigger& trigger = triggers[index];
        bool active = true;
        for (const Controls::TemplateTriggerCondition& triggerCondition :
             trigger.conditions) {
            DependencyObject* source = resolveTarget != nullptr
                ? resolveTarget(resolveContext, triggerCondition.sourceName.View(), triggerCondition.sourcePartIndex)
                : nullptr;
            if (source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template trigger source name was not found");
            }
            Base::Result<PropertyValue> current =
                source->GetValue(triggerCondition.property);
            if (!current) return current.GetStatus();
            if (!triggerCondition.IsMet(*source, current.Value())) {
                active = false;
                break;
            }
        }
        activeStates.Set(index, active);
        const std::uint64_t bit = index < 64U ? (UINT64_C(1) << index) : 0ULL;
        const bool wasActive = (triggerStatesMask & bit) != 0ULL;
        if (active != wasActive) {
            anyStateChanged = true;
        }
    }

    if (!anyStateChanged) {
        return false;
    }

    for (std::uint32_t index = 0U; index < triggers.Size() && index < 64U; ++index) {
        const std::uint64_t bit = (UINT64_C(1) << index);
        if ((triggerStatesMask & bit) == 0ULL) {
            continue;
        }
        const Controls::TemplatePropertyTrigger& trigger = triggers[index];
        for (const Controls::TemplateTriggerSetter& setter : trigger.setters) {
            if (!ShouldApplyOrClearTriggerSetter(setter.value)) {
                continue;
            }
            DependencyObject* target = resolveTarget != nullptr
                ? resolveTarget(resolveContext, setter.targetName.View(), setter.targetPartIndex)
                : nullptr;
            if (target == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template trigger target name was not found");
            }
            Base::Result<void> cleared =
                values.ClearTriggerValue(*target, setter.property);
            if (!cleared) return cleared.GetStatus();
        }
        triggerStatesMask &= ~bit;
    }

    for (std::uint32_t index = 0U; index < triggers.Size() && index < 64U; ++index) {
        const bool active = activeStates.Get(index);
        if (!active) continue;
        const Controls::TemplatePropertyTrigger& trigger = triggers[index];
        for (const Controls::TemplateTriggerSetter& setter : trigger.setters) {
            DependencyObject* target = resolveTarget != nullptr
                ? resolveTarget(resolveContext, setter.targetName.View(), setter.targetPartIndex)
                : nullptr;
            if (target == nullptr) continue;
            Base::Result<bool> applied = ApplyResolvedTriggerSetter(
                values, *target, setter.property, setter.value, templatedParent, resources);
            if (!applied) return applied.GetStatus();
        }
        triggerStatesMask |= (UINT64_C(1) << index);
    }
    Base::Result<std::uint32_t> flushed = values.Flush();
    if (!flushed) return flushed.GetStatus();
    return !firstEval;
}

Base::Result<void> TriggerEngine::ClearControlTemplateTriggerSetters(
    TemplatedParentProviderSession& values,
    const Controls::ControlTemplate& plan,
    std::uint64_t triggerStatesMask,
    TemplateTargetResolver resolveTarget,
    const void* resolveContext) noexcept {
    const Base::Span<const Controls::TemplatePropertyTrigger> triggers =
        Controls::FrameworkTemplateState::Triggers(plan);
    for (std::uint32_t index = 0U; index < triggers.Size() && index < 64U; ++index) {
        if ((triggerStatesMask & (UINT64_C(1) << index)) == 0ULL) {
            continue;
        }
        const Controls::TemplatePropertyTrigger& trigger = triggers[index];
        for (const Controls::TemplateTriggerSetter& setter : trigger.setters) {
            DependencyObject* target = resolveTarget != nullptr
                ? resolveTarget(resolveContext, setter.targetName.View(), setter.targetPartIndex)
                : nullptr;
            if (target != nullptr) {
                Base::Result<bool> cleared = ClearResolvedTriggerSetter(
                    values, *target, setter.property, setter.value);
                if (!cleared) return cleared.GetStatus();
            }
        }
    }
    return {};
}

TriggerEngine::TriggerEngine(
    StyleProviderSession& values,
    DependencyPropertyRegistry& properties,
    Base::Vector<StyleApplication>& applications) noexcept
    : values_(values),
      properties_(properties),
      applications_(applications),
      propertyChangedHandler_(this, &TriggerEngine::OnPropertyChanged) {}

TriggerEngine::~TriggerEngine() noexcept {
    ClearAllStyleDataTriggers();
    ClearAllDataTemplateTriggers();
}

void TriggerEngine::SubscribeTriggers(
    DependencyObject& object, const Style& style) noexcept {
    if (StyleRuntimeTriggerMask(style) != 0ULL) {
        TriggerSubscriptionFilter filter;
        for (const TriggerPlan& trigger : StyleRuntimeTriggers(style)) {
            if (!trigger.IsBindingTrigger() && filter.TryAcquire(trigger.property)) {
                object.AddValueChangedHandler(trigger.property, propertyChangedHandler_);
            }
        }
    }
    if (auto* element = TryCast<FrameworkElement>(&object)) {
        static_cast<void>(StartStyleDataTriggers(*element, style));
    }
}

void TriggerEngine::UnsubscribeTriggers(
    DependencyObject& object, const Style& style) noexcept {
    ClearStyleDataTriggersFor(object);
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

void TriggerEngine::StyleDataTriggerConditionState::OnProxyValueChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (runtime == nullptr || target == nullptr || style == nullptr) {
        return;
    }
    runtime->EvaluateStyleDataTriggerCondition(*this, args.GetNewValue());
}

void TriggerEngine::EvaluateStyleDataTriggerCondition(
    StyleDataTriggerConditionState& state,
    const Meta::PropertyValue& actual) noexcept {
    if (state.target == nullptr || state.style == nullptr) {
        return;
    }
    const Meta::Registry* metadata = Meta::CurrentObjectFactory().metadata;
    if (BindingEngine* bindings = ElementTree::BindingsOf(*state.target)) {
        if (bindings->Metadata() != nullptr) {
            metadata = bindings->Metadata();
        }
    }
    const bool matches = !IsTriggerProxySentinel(actual) &&
        ComparePropertyValues(actual, state.expected, metadata);
    if (state.aggregate != nullptr) {
        if (state.conditionIndex >= state.aggregate->known.Size() ||
            state.conditionIndex >= state.aggregate->active.Size()) {
            return;
        }
        state.aggregate->known[state.conditionIndex] = 1U;
        state.aggregate->active[state.conditionIndex] = matches ? 1U : 0U;
        bool allKnown = true;
        bool allActive = true;
        for (std::uint32_t i = 0U; i < state.aggregate->known.Size(); ++i) {
            allKnown = allKnown && state.aggregate->known[i] != 0U;
            allActive = allActive && state.aggregate->active[i] != 0U;
        }
        if (!allKnown) {
            return;
        }
        SetBindingTriggerState(
            *state.target,
            *state.style,
            state.triggerIndex,
            allActive);
        return;
    }
    SetBindingTriggerState(
        *state.target,
        *state.style,
        state.triggerIndex,
        matches);
}

Base::Result<std::uint32_t> TriggerEngine::StartStyleDataTriggers(
    FrameworkElement& target,
    const Style& style) noexcept {
    const Base::Span<const TriggerPlan> triggers = StyleRuntimeTriggers(style);
    if (triggers.Empty()) {
        return std::uint32_t{0U};
    }
    BindingEngine* bindings = ElementTree::BindingsOf(target);
    if (bindings == nullptr || bindings->Metadata() == nullptr) {
        return std::uint32_t{0U};
    }

    std::uint32_t started = 0U;
    for (std::uint32_t index = 0U; index < triggers.Size(); ++index) {
        const TriggerPlan& trigger = triggers[index];
        if (!trigger.IsBindingTrigger()) continue;

        bool alreadyAttached = false;
        for (const StyleDataTriggerConditionState* existing : styleDataTriggerSubscriptions_) {
            if (existing != nullptr &&
                existing->target == &target &&
                existing->style == &style &&
                existing->triggerIndex == index) {
                alreadyAttached = true;
                break;
            }
        }
        if (alreadyAttached) continue;
        if (!trigger.binding) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style DataTrigger Binding is incomplete");
        }

        // If any condition uses an explicit source (ElementName / RelativeSource)
        // that is not yet reachable in the tree, wait until the element is mounted.
        auto explicitSourceUnresolved = [&](const Base::Ref<Data::Binding>& b) noexcept {
            if (!b) return true;
            const bool isExplicit = b->GetSource() ||
                !b->GetElementName().Empty() ||
                b->GetRelativeSource();
            if (!isExplicit) return false;
            return ResolveTriggerBindingSource(
                bindings->Metadata(), *b, target) == nullptr;
        };
        bool deferred = explicitSourceUnresolved(trigger.binding);
        for (std::uint32_t extraIndex = 0U;
             extraIndex < trigger.extraBindings.Size() && !deferred;
             ++extraIndex) {
            deferred = explicitSourceUnresolved(
                trigger.extraBindings[extraIndex].binding);
        }
        if (deferred) {
            continue;
        }

        const std::uint32_t conditionCount = 1U + trigger.extraBindings.Size();
        StyleDataTriggerAggregate* aggregate = nullptr;
        if (conditionCount > 1U) {
            aggregate = new (std::nothrow) StyleDataTriggerAggregate();
            if (aggregate == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "Failed to allocate Style MultiDataTrigger aggregate");
            }
            aggregate->known.Resize(conditionCount, 0U);
            aggregate->active.Resize(conditionCount, 0U);
        }

        auto attachCondition =
            [&](const Base::Ref<Data::Binding>& conditionBinding,
                const Meta::PropertyValue& expectedValue,
                std::uint32_t conditionIndex,
                bool ownsAggregate) -> Base::Result<void> {
            auto* state = new (std::nothrow) StyleDataTriggerConditionState();
            if (state == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "Failed to allocate Style DataTrigger condition state");
            }
            state->runtime = this;
            state->target = &target;
            state->style = &style;
            state->triggerIndex = index;
            state->conditionIndex = conditionIndex;
            state->aggregate = aggregate;
            state->ownsAggregate = ownsAggregate;
            state->expected = expectedValue;

            Base::Result<TriggerBindingSubscription> attached = AttachTriggerBinding(
                *bindings,
                *conditionBinding,
                target,
                nullptr,
                nullptr,
                nullptr,
                &target,
                [state](
                    DependencyObject& proxy,
                    const DependencyPropertyChangedEventArgs& args) noexcept {
                    state->OnProxyValueChanged(proxy, args);
                },
                /*evaluateImmediately=*/false,
                /*ignoreInitialValue=*/false);
            if (!attached) {
                delete state;
                return attached.GetStatus();
            }
            state->binding = std::move(attached).Value();
            styleDataTriggerSubscriptions_.PushBack(state);
            return {};
        };

        const std::uint32_t initialSubCount = styleDataTriggerSubscriptions_.Size();
        Base::Result<void> attached = attachCondition(
            trigger.binding,
            trigger.value,
            0U,
            aggregate != nullptr);
        if (!attached) {
            delete aggregate;
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
            if (!attached) {
                while (styleDataTriggerSubscriptions_.Size() > initialSubCount) {
                    StyleDataTriggerConditionState* state =
                        styleDataTriggerSubscriptions_.Back();
                    styleDataTriggerSubscriptions_.PopBack();
                    if (state != nullptr) {
                        state->binding.Detach(bindings);
                        if (state->ownsAggregate && state->aggregate != nullptr) {
                            delete state->aggregate;
                            state->aggregate = nullptr;
                        }
                        delete state;
                    }
                }
                return attached.GetStatus();
            }
            ++started;
        }
    }
    return started;
}

void TriggerEngine::ClearStyleDataTriggersFor(
    DependencyObject& target) noexcept {
    BindingEngine* bindings = ElementTree::BindingsOf(target);
    for (std::uint32_t index = 0U; index < styleDataTriggerSubscriptions_.Size();) {
        StyleDataTriggerConditionState* state = styleDataTriggerSubscriptions_[index];
        if (state == nullptr || state->target != &target) {
            ++index;
            continue;
        }
        state->binding.Detach(bindings);
        if (state->ownsAggregate && state->aggregate != nullptr) {
            delete state->aggregate;
            state->aggregate = nullptr;
        }
        delete state;
        if (index + 1U != styleDataTriggerSubscriptions_.Size()) {
            styleDataTriggerSubscriptions_[index] =
                styleDataTriggerSubscriptions_.Back();
        }
        styleDataTriggerSubscriptions_.PopBack();
    }
}

void TriggerEngine::ClearAllStyleDataTriggers() noexcept {
    for (StyleDataTriggerConditionState* state : styleDataTriggerSubscriptions_) {
        if (state == nullptr) continue;
        BindingEngine* bindings = state->target != nullptr
            ? ElementTree::BindingsOf(*state->target)
            : nullptr;
        state->binding.Detach(bindings);
        if (state->ownsAggregate && state->aggregate != nullptr) {
            delete state->aggregate;
            state->aggregate = nullptr;
        }
        delete state;
    }
    styleDataTriggerSubscriptions_.Clear();
}

void TriggerEngine::SetDataTemplateTriggerCallbacks(
    DataTemplateActionHandler actionHandler,
    DataTemplateEventTriggerHandler eventTriggerHandler,
    TriggerFailureReporter failureReporter,
    void* context) noexcept {
    dataTemplateActionHandler_ = actionHandler;
    dataTemplateEventTriggerHandler_ = eventTriggerHandler;
    failureReporter_ = failureReporter;
    dataTemplateCallbackContext_ = context;
}

void TriggerEngine::DataTemplateTriggerSubscription::Unbind() noexcept {
    if (source != nullptr && property.IsValid()) {
        static_cast<void>(source->RemoveValueChangedHandler(property, handler));
        source = nullptr;
        property = Meta::DependencyPropertyHandle{};
    }
    delete context;
    context = nullptr;
}

void TriggerEngine::DataTemplateTriggerHandlerState::Invoke(
    DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    if (runtime == nullptr || !triggerContext || triggerContext->root == nullptr) {
        return;
    }
    Meta::EffectiveValueEngine* values =
        ElementTree::ValuesOf(*triggerContext->root);
    const Meta::Registry* metadata = Meta::CurrentObjectFactory().metadata;
    if (BindingEngine* bindings = ElementTree::BindingsOf(*triggerContext->root)) {
        if (bindings->Metadata() != nullptr) {
            metadata = bindings->Metadata();
        }
    }
    Base::Result<void> evaluated = runtime->EvaluateDataTemplateTrigger(
        *triggerContext, triggerIndex, values, metadata);
    if (!evaluated && runtime->failureReporter_ != nullptr) {
        runtime->failureReporter_(
            evaluated.GetStatus(), runtime->dataTemplateCallbackContext_);
    }
}

bool TriggerEngine::IsInVisualSubtree(
    Media::Visual* node,
    const Media::Visual& fragmentRoot) noexcept {
    while (node != nullptr) {
        if (node == &fragmentRoot) return true;
        node = TryCast<Media::Visual>(node->GetLogicalParent()) != nullptr
            ? TryCast<Media::Visual>(node->GetLogicalParent())
            : node->GetVisualParent();
    }
    return false;
}

Base::Result<bool> TriggerEngine::EvaluateDataTemplateCondition(
    Controls::DataTemplateTriggerInstance& context,
    Controls::DataTemplateTriggerCondition& condition,
    const Meta::Registry* metadata) noexcept {
    Meta::PropertyValue current;
    if (condition.binding) {
        if (!condition.bindingProxy) {
            return false;
        }
        current = condition.bindingProxy->GetValue(
            Data::MultiBindingProxy::ValueProperty.Handle());
        if (IsTriggerProxySentinel(current)) {
            return false;
        }
        return EvaluatePropertyTriggerCondition(current, condition.value, metadata);
    }

    Base::Ref<DependencyObject> dependencySource =
        condition.dependencySource.Lock();
    if ((!dependencySource || !condition.property.IsValid()) &&
        context.root != nullptr &&
        condition.property.IsValid()) {
        dependencySource =
            Base::Ref<DependencyObject>::FromBorrowed(
                *static_cast<DependencyObject*>(context.root));
        condition.dependencySource =
            Base::WeakRef<DependencyObject>(dependencySource);
    }
    if (!dependencySource || !condition.property.IsValid()) {
        return false;
    }
    Base::Result<Meta::PropertyValue> value =
        dependencySource->GetValue(condition.property);
    if (!value) return value.GetStatus();
    current = std::move(value).Value();
    return EvaluatePropertyTriggerCondition(current, condition.value, metadata);
}

Base::Result<void> TriggerEngine::EnsureDataTemplateProviderTokens(
    Controls::DataTemplateTriggerInstance& context,
    Meta::EffectiveValueEngine* values) noexcept {
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
    for (Controls::DataTemplatePropertyTrigger& trigger : context.triggers) {
        for (Controls::DataTemplateTriggerSetter& setter : trigger.setters) {
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

Base::Result<void> TriggerEngine::EvaluateDataTemplateTrigger(
    Controls::DataTemplateTriggerInstance& context,
    std::uint32_t triggerIndex,
    Meta::EffectiveValueEngine* values,
    const Meta::Registry* metadata) noexcept {
    if (triggerIndex >= context.triggers.Size() || context.root == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate Trigger runtime is unavailable");
    }
    Controls::DataTemplatePropertyTrigger& trigger = context.triggers[triggerIndex];
    if (!trigger.setters.Empty()) {
        if (values == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger runtime is unavailable");
        }
        Base::Result<void> providerTokens =
            EnsureDataTemplateProviderTokens(context, values);
        if (!providerTokens) return providerTokens.GetStatus();
    }
    bool active = !trigger.conditions.Empty();
    for (std::uint32_t condIdx = 0U; condIdx < trigger.conditions.Size(); ++condIdx) {
        Controls::DataTemplateTriggerCondition& condition = trigger.conditions[condIdx];
        Base::Result<bool> matches =
            EvaluateDataTemplateCondition(context, condition, metadata);
        if (!matches) return matches.GetStatus();
        if (!matches.Value()) {
            active = false;
            break;
        }
    }
    if (active == trigger.active) return {};

    if (active) {
        for (const Controls::DataTemplateTriggerSetter& setter : trigger.setters) {
            Base::Ref<DependencyObject> target = setter.target.Lock();
            if (!target) continue;

            Meta::PropertyValue valueToApply;
            if (!ResolveTriggerSetterValue(
                    *target,
                    setter.property,
                    setter.value,
                    valueToApply,
                    context.root,
                    nullptr,
                    /*fallbackToDefaultOnMissingResource=*/true)) {
                continue;
            }

            Base::Result<void> applied =
                values->SetProviderContribution(
                    *target,
                    setter.property,
                    setter.token,
                    valueToApply);
            if (!applied) {
                return applied.GetStatus();
            }
        }
    } else {
        for (const Controls::DataTemplateTriggerSetter& setter : trigger.setters) {
            Base::Ref<DependencyObject> target = setter.target.Lock();
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
    if (values != nullptr && !values->IsFlushing()) {
        static_cast<void>(values->Flush());
    }

    Base::Span<const Base::Ref<Base::Object>> actions =
        active ? trigger.enterActions.AsSpan() : trigger.exitActions.AsSpan();
    if (!actions.Empty() && dataTemplateActionHandler_ != nullptr) {
        Base::Result<void> executed = dataTemplateActionHandler_(
            context, actions, dataTemplateCallbackContext_);
        if (!executed && failureReporter_ != nullptr) {
            failureReporter_(executed.GetStatus(), dataTemplateCallbackContext_);
        }
    }
    trigger.active = active;
    return {};
}

Base::Result<std::uint32_t> TriggerEngine::StartDataTemplateTriggers(
    Controls::DataTemplateTriggerInstance& context,
    BindingEngine* bindings,
    Meta::EffectiveValueEngine* values,
    const Meta::Registry* metadata) noexcept {
    if (context.root == nullptr) {
        return std::uint32_t{0U};
    }
    std::uint32_t count = 0U;
    for (std::uint32_t triggerIndex = 0U;
         triggerIndex < context.triggers.Size();
         ++triggerIndex) {
        Controls::DataTemplatePropertyTrigger& trigger =
            context.triggers[triggerIndex];
        for (std::uint32_t conditionIndex = 0U;
             conditionIndex < trigger.conditions.Size();
             ++conditionIndex) {
            Controls::DataTemplateTriggerCondition& condition =
                trigger.conditions[conditionIndex];

            if (condition.binding) {
                if (condition.bindingHandle.IsValid() && condition.bindingProxy) {
                    continue;
                }
                if (bindings == nullptr || metadata == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotInitialized,
                        "DataTemplate DataTrigger requires an active BindingEngine");
                }

                auto* handlerContext = new (std::nothrow) DataTemplateTriggerHandlerState();
                if (handlerContext == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfMemory,
                        "Failed to allocate DataTemplateTriggerHandlerState");
                }
                handlerContext->runtime = this;
                handlerContext->triggerContext =
                    Base::Ref<Controls::DataTemplateTriggerInstance>::FromBorrowed(context);
                handlerContext->triggerIndex = triggerIndex;
                handlerContext->conditionIndex = conditionIndex;

                FrameworkElement* dcOwner = context.root;
                if (condition.usesDataContext) {
                    if (Base::Ref<Base::Object> lockedSource = condition.source.Lock()) {
                        if (auto* feSource = TryCast<FrameworkElement>(lockedSource.Get())) {
                            dcOwner = feSource;
                        }
                    }
                    if (dcOwner == context.root) {
                        if (auto* templated = TryCast<FrameworkElement>(
                                context.root->GetTemplatedParent())) {
                            dcOwner = templated;
                        }
                    }
                }

                Base::Object* explicitSource = nullptr;
                if (!condition.usesDataContext &&
                    condition.binding->GetElementName().Empty() &&
                    !condition.binding->GetRelativeSource() &&
                    !condition.binding->GetSource()) {
                    if (Base::Ref<Base::Object> lockedSource = condition.source.Lock()) {
                        explicitSource = lockedSource.Get();
                    }
                }

                Base::Result<TriggerBindingSubscription> attached =
                    AttachTriggerBinding(
                        *bindings,
                        *condition.binding,
                        *context.root,
                        &context,
                        nullptr,
                        explicitSource,
                        dcOwner,
                        [handlerContext](
                            DependencyObject& object,
                            const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                            handlerContext->Invoke(object, args);
                        },
                        /*evaluateImmediately=*/true,
                        /*ignoreInitialValue=*/false);
                if (!attached) {
                    delete handlerContext;
                    if (attached.GetStatus().code == Base::ErrorCode::NotFound) {
                        continue;
                    }
                    return attached.GetStatus();
                }

                condition.bindingProxy = attached.Value().proxy;
                condition.bindingHandle = attached.Value().handle;
                DataTemplateTriggerSubscription record;
                record.context = handlerContext;
                dataTemplateTriggerSubscriptions_.PushBack(std::move(record));
                ++count;
                continue;
            }

            Base::Ref<DependencyObject> dependencySource =
                condition.dependencySource.Lock();
            if ((!dependencySource || !condition.property.IsValid()) &&
                context.root != nullptr &&
                condition.property.IsValid()) {
                dependencySource =
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(context.root));
                condition.dependencySource =
                    Base::WeakRef<DependencyObject>(dependencySource);
            }
            if (!dependencySource || !condition.property.IsValid()) {
                continue;
            }
            bool alreadyAttached = false;
            for (const DataTemplateTriggerSubscription& existing :
                 dataTemplateTriggerSubscriptions_) {
                if (existing.context != nullptr &&
                    existing.context->triggerContext.Get() == &context &&
                    existing.context->triggerIndex == triggerIndex &&
                    existing.context->conditionIndex == conditionIndex) {
                    alreadyAttached = true;
                    break;
                }
            }
            if (alreadyAttached) continue;

            auto* handlerContext = new (std::nothrow) DataTemplateTriggerHandlerState();
            if (handlerContext == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "Failed to allocate DataTemplateTriggerHandlerState");
            }
            handlerContext->runtime = this;
            handlerContext->triggerContext =
                Base::Ref<Controls::DataTemplateTriggerInstance>::FromBorrowed(context);
            handlerContext->triggerIndex = triggerIndex;
            handlerContext->conditionIndex = conditionIndex;

            DataTemplateTriggerSubscription record;
            record.source = dependencySource.Get();
            record.property = condition.property;
            record.context = handlerContext;
            record.handler = [handlerContext](
                DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                handlerContext->Invoke(object, args);
            };
            record.source->AddValueChangedHandler(record.property, record.handler);
            dataTemplateTriggerSubscriptions_.PushBack(std::move(record));
            ++count;
        }

        Base::Result<void> evaluated =
            EvaluateDataTemplateTrigger(context, triggerIndex, values, metadata);
        if (!evaluated && failureReporter_ != nullptr) {
            failureReporter_(evaluated.GetStatus(), dataTemplateCallbackContext_);
        }
    }

    if (!context.eventTriggers.Empty() && dataTemplateEventTriggerHandler_ != nullptr) {
        Base::Result<std::uint32_t> started =
            dataTemplateEventTriggerHandler_(context, dataTemplateCallbackContext_);
        if (started) {
            count += started.Value();
        } else if (failureReporter_ != nullptr) {
            failureReporter_(started.GetStatus(), dataTemplateCallbackContext_);
        }
    }
    return count;
}

void TriggerEngine::ClearDataTemplateTriggerProviders(
    Controls::DataTemplateTriggerInstance& context,
    BindingEngine* bindings,
    Meta::EffectiveValueEngine* values) noexcept {
    for (Controls::DataTemplatePropertyTrigger& trigger : context.triggers) {
        for (Controls::DataTemplateTriggerCondition& condition : trigger.conditions) {
            if (condition.bindingHandle.IsValid() && bindings != nullptr) {
                static_cast<void>(bindings->Detach(condition.bindingHandle));
            }
            condition.bindingHandle = {};
            condition.bindingProxy.Reset();
        }
        if (values != nullptr) {
            for (Controls::DataTemplateTriggerSetter& setter : trigger.setters) {
                Base::Ref<DependencyObject> target = setter.target.Lock();
                if (!target || !setter.token.IsValid()) continue;
                static_cast<void>(
                    values->ClearProviderContribution(
                        *target,
                        setter.property,
                        setter.token));
                setter.token = {};
            }
        }
        trigger.active = false;
    }
    context.providerOrigin = 0U;
}

void TriggerEngine::ClearDataTemplateTriggerProvidersInSubtree(
    Media::Visual& visual,
    BindingEngine* bindings,
    Meta::EffectiveValueEngine* values) noexcept {
    if (auto* element = TryCast<FrameworkElement>(&visual)) {
        for (const Base::Ref<Base::Object>& authored : FrameworkElementSeams::AuthoredTriggers(*element)) {
            if (authored && authored->RuntimeType() ==
                Controls::DataTemplateTriggerInstance::StaticTypeId()) {
                ClearDataTemplateTriggerProviders(
                    static_cast<Controls::DataTemplateTriggerInstance&>(*authored),
                    bindings,
                    values);
            }
        }
    }
    for (Media::Visual* child : visual.RenderChildren()) {
        if (child != nullptr) {
            ClearDataTemplateTriggerProvidersInSubtree(*child, bindings, values);
        }
    }
}

void TriggerEngine::ClearDataTemplateTriggersForSubtree(
    Media::Visual& fragmentRoot,
    BindingEngine* bindings,
    Meta::EffectiveValueEngine* values,
    const Meta::Registry* metadata) noexcept {
    ClearDataTemplateTriggerProvidersInSubtree(fragmentRoot, bindings, values);
    std::uint32_t writeIndex = 0U;
    for (std::uint32_t readIndex = 0U;
         readIndex < dataTemplateTriggerSubscriptions_.Size();
         ++readIndex) {
        DataTemplateTriggerSubscription& subscription =
            dataTemplateTriggerSubscriptions_[readIndex];
        const bool sourceMatches =
            subscription.source != nullptr &&
            metadata != nullptr &&
            metadata->Types().IsDerivedFrom(
                subscription.source->RuntimeType(),
                Media::Visual::StaticTypeId()) &&
            IsInVisualSubtree(
                static_cast<Media::Visual*>(subscription.source),
                fragmentRoot);
        DataTemplateTriggerHandlerState* ctx = subscription.context;
        const bool contextMatches =
            ctx != nullptr &&
            ctx->triggerContext &&
            ctx->triggerContext->root != nullptr &&
            IsInVisualSubtree(ctx->triggerContext->root, fragmentRoot);
        if (!sourceMatches && !contextMatches) {
            if (writeIndex != readIndex) {
                dataTemplateTriggerSubscriptions_[writeIndex] =
                    std::move(subscription);
            }
            ++writeIndex;
            continue;
        }
        subscription.Unbind();
    }
    static_cast<void>(dataTemplateTriggerSubscriptions_.Resize(writeIndex));
}

void TriggerEngine::ClearAllDataTemplateTriggers() noexcept {
    for (DataTemplateTriggerSubscription& subscription :
         dataTemplateTriggerSubscriptions_) {
        subscription.Unbind();
    }
    dataTemplateTriggerSubscriptions_.Clear();
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
                (void)ClearResolvedTriggerSetter(
                    values_, object, setter.property, setter.value, &clearedFilter);
            }
        }
    }

    for (std::uint32_t index = 0U; index < triggers.Size(); ++index) {
        if (!activeStates.Get(index)) continue;
        for (const StyleTriggerSetter& setter : triggers[index].setters) {
            (void)ApplyResolvedTriggerSetter(
                values_, object, setter.property, setter.value);
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
            (void)ClearResolvedTriggerSetter(
                values_, object, setter.property, setter.value, &clearedFilter);
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
    ClearStyleDataTriggersFor(object);
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
    const std::uint32_t idx = object.StyleApplicationIndex();
    return (idx < applications_.Size() && applications_[idx].object == &object)
        ? idx
        : UINT32_MAX;
}

} // namespace Aero
