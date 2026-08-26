#pragma once

// Trigger evaluation engine. Owns the per-object trigger application state and
// the deferred trigger-evaluation phase, and evaluates Style/ControlTemplate/
// DataTemplate triggers. Mirrors the trigger orchestration in
// C:\Projects\AeroGUI NsGui/Resources/ (BaseTrigger.cpp and friends), kept as a
// dedicated engine class so the style subsystem delegates trigger behavior to
// it. The runtime action execution (Enter/Exit actions) is still supplied by
// the host via SetTriggerActionHandler (the View wires its callback here).

#include <Aero/Style.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Threading.hpp>

#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

class TriggerEngine {
public:
    using TriggerActionHandler = Base::Result<void>(*)(
        DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    TriggerEngine(
        StyleProviderSession& values,
        DependencyPropertyRegistry& properties,
        Base::Vector<StyleApplication>& applications) noexcept;
    ~TriggerEngine() noexcept;

    // Style application lifecycle (trigger side). The style setter application
    // itself remains in StyleEngine; these only manage trigger state.
    Base::Result<void> SubscribeTriggers(
        DependencyObject& object, const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object, const Style& style) noexcept;
    Base::Result<void> EvaluateTriggers(
        DependencyObject& object, const Style& style) noexcept;
    Base::Result<void> ClearTriggerSetters(
        DependencyObject& object, const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    Base::Result<void> ExecuteTriggerActions(
        DependencyObject& object,
        Base::Span<const Base::Ref<Base::Object>>
            actions) noexcept;
    Base::Result<void> EnsureTriggerPhaseHook(
        DependencyObject& object) noexcept;
    Base::Result<void> QueueTriggerEvaluation(
        DependencyObject& object) noexcept;
    void RemovePendingTriggerEvaluation(
        DependencyObject& object) noexcept;
    Base::Result<std::uint32_t>
        FlushPendingTriggerEvaluations() noexcept;
    static void TriggerPhaseHook(void* context) noexcept;
    Base::Result<void> SetBindingTriggerState(
        DependencyObject& object,
        const Style& style,
        std::uint32_t triggerIndex,
        bool active) noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept;
    const Base::Status& LastActionStatus() const noexcept;

private:
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;

    StyleProviderSession& values_;
    DependencyPropertyRegistry& properties_;
    Base::Vector<StyleApplication>& applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Dispatcher* dispatcher_ = nullptr;
    DispatcherFrameHookHandle triggerPhaseHook_;
    Base::Vector<DependencyObject*>
        pendingTriggerEvaluations_;
    TriggerActionHandler triggerActionHandler_ = nullptr;
    void* triggerActionContext_ = nullptr;
    Base::Status lastActionStatus_;
};

} // namespace Aero
