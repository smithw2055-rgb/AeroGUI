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

#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/triggers/TriggerPlan.hpp"

namespace Aero {

class TriggerEngine {
public:
    using TriggerActionHandler = ::Aero::TriggerActionHandler;

    TriggerEngine(
        StyleProviderSession& values,
        DependencyPropertyRegistry& properties,
        Base::Vector<StyleApplication>& applications,
        const Base::HashMap<const DependencyObject*, std::uint32_t>& objectIndexMap) noexcept;
    ~TriggerEngine() noexcept;

    // Style application lifecycle (trigger side). The style setter application
    // itself remains in StyleEngine; these only manage trigger state.
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
    // P3.2: opts the deferred evaluations into ViewFrame's explicit DataBind
    // phase (formerly registered a frame hook). ViewFrame calls
    // TriggerPhaseHook() directly when phase participation is enabled.
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
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;

    StyleProviderSession& values_;
    DependencyPropertyRegistry& properties_;
    Base::Vector<StyleApplication>& applications_;
    const Base::HashMap<const DependencyObject*, std::uint32_t>& objectIndexMap_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Dispatcher* dispatcher_ = nullptr;
    bool dataBindPhaseEnabled_ = false;
    Base::Vector<DependencyObject*>
        pendingTriggerEvaluations_;
    TriggerActionHandler triggerActionHandler_ = nullptr;
    void* triggerActionContext_ = nullptr;
    Base::Status lastActionStatus_;
};

} // namespace Aero
