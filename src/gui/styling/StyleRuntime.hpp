#pragma once

#include "gui/styling/ThemeStyleRegistry.hpp"
#include "runtime/RuntimeFwd.hpp"
#include "gui/property/PropertyProviderSession.hpp"

#include <Aero/Styling.hpp>

namespace Aero::Detail {

using namespace Aero::Core;

class AERO_API UiRuntimeAccess::StyleManager final {
public:
    using TriggerActionHandler = Base::Result<void>(*)(
        DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    explicit StyleManager(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept
        : providerSession_(values),
          values_(&providerSession_),
          properties_(&properties),
          applications_(),
          propertyChangedHandler_(
              this, &StyleManager::OnPropertyChanged) {}
    ~StyleManager() noexcept;

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;
    const Style* AppliedStyle(
        const DependencyObject& object)
        const noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept {
        triggerActionHandler_ = handler;
        triggerActionContext_ = context;
    }
    const Base::Status& LastActionStatus() const noexcept {
        return lastActionStatus_;
    }

private:
    Core::Detail::StyleProviderSession providerSession_;
    Core::Detail::StyleProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
        Base::Vector<std::uint8_t> triggerStates;
    };
    Base::Vector<Application> applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Dispatcher* dispatcher_ = nullptr;
    DispatcherFrameHookHandle triggerPhaseHook_;
    Base::Vector<DependencyObject*>
        pendingTriggerEvaluations_;
    TriggerActionHandler triggerActionHandler_ = nullptr;
    void* triggerActionContext_ = nullptr;
    Base::Status lastActionStatus_;

    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> EvaluateTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
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
    Base::Result<void> ClearTriggerSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};
class AERO_API UiRuntimeAccess::ThemeStyleManager final {
public:
    ThemeStyleManager(
        EffectiveValueEngine& values,
        const Aero::Detail::ThemeStyleRegistry& registry) noexcept
        : providerSession_(values),
          values_(&providerSession_),
          registry_(&registry) {}

    Base::Result<bool> ApplyDefault(
        DependencyObject& object) noexcept;
    Base::Result<bool> Clear(
        DependencyObject& object) noexcept;

private:
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
    };
    Core::Detail::ThemeStyleProviderSession providerSession_;
    Core::Detail::ThemeStyleProviderSession* values_ = nullptr;
    const Aero::Detail::ThemeStyleRegistry* registry_ = nullptr;
    Base::Vector<Application> applications_;

    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
};

} // namespace Aero::Detail
