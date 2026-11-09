#pragma once

// Unified trigger runtime (TriggerEngine / TriggerRuntime). Consolidates Style
// property/data triggers, ControlTemplate property/data triggers, DataTemplate
// property/data triggers, and Interaction DataTrigger / PropertyChangedTrigger
// binding subscriptions on top of BindingEngine::BindingExpression
// (via Data::MultiBindingProxy).

#include <Aero/Style.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Threading.hpp>

#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/templates/TemplateProgram.hpp"
#include "gui/triggers/TriggerPlan.hpp"

namespace Aero {

class TriggerEngine {
public:
    using TriggerActionHandler = ::Aero::TriggerActionHandler;

    using TemplateTargetResolver = DependencyObject* (*)(
        const void* resolveContext,
        Base::StringView name,
        std::int32_t& inOutPartIndex) noexcept;

    using DataTemplateActionHandler = Base::Result<void> (*)(
        Controls::DataTemplateTriggerInstance& instance,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    using DataTemplateEventTriggerHandler = Base::Result<std::uint32_t> (*)(
        Controls::DataTemplateTriggerInstance& instance,
        void* context) noexcept;

    using TriggerFailureReporter = void (*)(
        const Base::Status& status,
        void* context) noexcept;

    struct TriggerBindingSubscription {
        Base::Ref<Data::MultiBindingProxy> proxy;
        Data::BindingHandle handle;

        void Detach(BindingEngine* bindings) noexcept {
            if (bindings != nullptr && handle.IsValid()) {
                static_cast<void>(bindings->Detach(handle));
            }
            handle = {};
            proxy.Reset();
        }
    };

    static Meta::PropertyValue MakeTriggerProxySentinel() noexcept {
        return Meta::PropertyValue::FromUnsignedInteger(
            Data::MultiBindingProxy::StaticTypeId(), 0U);
    }

    static bool IsTriggerProxySentinel(const Meta::PropertyValue& value) noexcept {
        return value.Kind() == Meta::ValueKind::UnsignedInteger &&
            value.Type() == Data::MultiBindingProxy::StaticTypeId();
    }

    static Base::Object* ResolveTriggerBindingSource(
        const Meta::Registry* metadata,
        const Data::Binding& binding,
        FrameworkElement& owner,
        Controls::DataTemplateTriggerInstance* dataTemplateContext = nullptr,
        const NameScope* names = nullptr,
        Base::Object* self = nullptr) noexcept;

    static Base::Result<TriggerBindingSubscription> AttachTriggerBinding(
        BindingEngine& bindings,
        const Data::Binding& binding,
        FrameworkElement& owner,
        Controls::DataTemplateTriggerInstance* dataTemplateContext,
        const NameScope* names,
        Base::Object* explicitSourceOverride,
        FrameworkElement* dataContextOwnerOverride,
        Meta::DependencyPropertyChangedEventHandler handler,
        bool evaluateImmediately,
        bool ignoreInitialValue = false,
        const Data::CompiledBindingPath* precompiledPath = nullptr) noexcept;

    // Unified ControlTemplate property/multi-property trigger evaluation & teardown.
    static Base::Result<bool> EvaluateControlTemplateTriggers(
        TemplatedParentProviderSession& values,
        const Controls::ControlTemplate& plan,
        Controls::Control* templatedParent,
        const ResourceDictionary* resources,
        bool& triggersEvaluated,
        std::uint64_t& triggerStatesMask,
        TemplateTargetResolver resolveTarget,
        const void* resolveContext) noexcept;

    static Base::Result<void> ClearControlTemplateTriggerSetters(
        TemplatedParentProviderSession& values,
        const Controls::ControlTemplate& plan,
        std::uint64_t triggerStatesMask,
        TemplateTargetResolver resolveTarget,
        const void* resolveContext) noexcept;

    TriggerEngine(
        StyleProviderSession& values,
        DependencyPropertyRegistry& properties,
        Base::Vector<StyleApplication>& applications) noexcept;
    ~TriggerEngine() noexcept;

    // Style application lifecycle (property + data triggers).
    void SubscribeTriggers(
        DependencyObject& object, const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object, const Style& style) noexcept;
    void EvaluateTriggers(
        DependencyObject& object, const Style& style) noexcept;
    void ClearTriggerSetters(
        DependencyObject& object, const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void ExecuteTriggerActions(
        DependencyObject& object,
        Base::Span<const Base::Ref<Base::Object>>
            actions) noexcept;

    // Unified Style DataTrigger / MultiDataTrigger management backed by BindingEngine.
    Base::Result<std::uint32_t> StartStyleDataTriggers(
        FrameworkElement& target,
        const Style& style) noexcept;
    void ClearStyleDataTriggersFor(
        DependencyObject& target) noexcept;
    void ClearAllStyleDataTriggers() noexcept;

    // Unified DataTemplate / ControlTemplate action & data trigger runtime.
    void SetDataTemplateTriggerCallbacks(
        DataTemplateActionHandler actionHandler,
        DataTemplateEventTriggerHandler eventTriggerHandler,
        TriggerFailureReporter failureReporter,
        void* context) noexcept;

    Base::Result<bool> EvaluateDataTemplateCondition(
        Controls::DataTemplateTriggerInstance& context,
        Controls::DataTemplateTriggerCondition& condition,
        const Meta::Registry* metadata) noexcept;
    Base::Result<void> EnsureDataTemplateProviderTokens(
        Controls::DataTemplateTriggerInstance& context,
        Meta::EffectiveValueEngine* values) noexcept;
    Base::Result<void> EvaluateDataTemplateTrigger(
        Controls::DataTemplateTriggerInstance& context,
        std::uint32_t triggerIndex,
        Meta::EffectiveValueEngine* values,
        const Meta::Registry* metadata) noexcept;
    Base::Result<std::uint32_t> StartDataTemplateTriggers(
        Controls::DataTemplateTriggerInstance& context,
        BindingEngine* bindings,
        Meta::EffectiveValueEngine* values,
        const Meta::Registry* metadata) noexcept;
    void ClearDataTemplateTriggerProviders(
        Controls::DataTemplateTriggerInstance& context,
        BindingEngine* bindings,
        Meta::EffectiveValueEngine* values) noexcept;
    void ClearDataTemplateTriggerProvidersInSubtree(
        Media::Visual& visual,
        BindingEngine* bindings,
        Meta::EffectiveValueEngine* values) noexcept;
    void ClearDataTemplateTriggersForSubtree(
        Media::Visual& fragmentRoot,
        BindingEngine* bindings,
        Meta::EffectiveValueEngine* values,
        const Meta::Registry* metadata) noexcept;
    void ClearAllDataTemplateTriggers() noexcept;

    void EnableDataBindPhase(
        DependencyObject& object) noexcept;
    bool DataBindPhaseEnabled() const noexcept {
        return dataBindPhaseEnabled_;
    }
    void QueueTriggerEvaluation(
        DependencyObject& object) noexcept;
    void RemovePendingTriggerEvaluation(
        DependencyObject& object) noexcept;
    std::uint32_t FlushPendingTriggerEvaluations() noexcept;
    static void TriggerPhaseHook(void* context) noexcept;
    void SetBindingTriggerState(
        DependencyObject& object,
        const Style& style,
        std::uint32_t triggerIndex,
        bool active) noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept;
    const Base::Status& LastActionStatus() const noexcept;

private:
    struct StyleDataTriggerAggregate {
        Base::Vector<std::uint8_t> known;
        Base::Vector<std::uint8_t> active;
    };

    struct StyleDataTriggerConditionState {
        TriggerEngine* runtime = nullptr;
        FrameworkElement* target = nullptr;
        const Style* style = nullptr;
        std::uint32_t triggerIndex = 0U;
        std::uint32_t conditionIndex = 0U;
        StyleDataTriggerAggregate* aggregate = nullptr;
        bool ownsAggregate = false;
        Meta::PropertyValue expected;
        TriggerBindingSubscription binding;

        void OnProxyValueChanged(
            DependencyObject& proxy,
            const DependencyPropertyChangedEventArgs& args) noexcept;
    };

    struct DataTemplateTriggerHandlerState {
        TriggerEngine* runtime = nullptr;
        Base::Ref<Controls::DataTemplateTriggerInstance> triggerContext;
        std::uint32_t triggerIndex = 0U;
        std::uint32_t conditionIndex = 0U;

        void Invoke(
            DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs&) noexcept;
    };

    struct DataTemplateTriggerSubscription {
        DependencyObject* source = nullptr;
        Meta::DependencyPropertyHandle property;
        Meta::DependencyPropertyChangedEventHandler handler;
        DataTemplateTriggerHandlerState* context = nullptr;

        void Unbind() noexcept;
    };

    static bool IsInVisualSubtree(
        Media::Visual* node,
        const Media::Visual& fragmentRoot) noexcept;

    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    void EvaluateStyleDataTriggerCondition(
        StyleDataTriggerConditionState& state,
        const Meta::PropertyValue& actual) noexcept;

    StyleProviderSession& values_;
    DependencyPropertyRegistry& properties_;
    Base::Vector<StyleApplication>& applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Dispatcher* dispatcher_ = nullptr;
    bool dataBindPhaseEnabled_ = false;
    Base::Vector<DependencyObject*>
        pendingTriggerEvaluations_;
    Base::Vector<StyleDataTriggerConditionState*>
        styleDataTriggerSubscriptions_;
    Base::Vector<DataTemplateTriggerSubscription>
        dataTemplateTriggerSubscriptions_;
    TriggerActionHandler triggerActionHandler_ = nullptr;
    void* triggerActionContext_ = nullptr;
    DataTemplateActionHandler dataTemplateActionHandler_ = nullptr;
    DataTemplateEventTriggerHandler dataTemplateEventTriggerHandler_ = nullptr;
    TriggerFailureReporter failureReporter_ = nullptr;
    void* dataTemplateCallbackContext_ = nullptr;
    Base::Status lastActionStatus_;
};

using TriggerRuntime = TriggerEngine;

} // namespace Aero
