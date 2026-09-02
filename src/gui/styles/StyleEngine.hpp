#pragma once

#include "gui/styles/StyleState.hpp"
#include <Aero/Data/BindingExpression.hpp>

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;

class TriggerEngine;

// Per-object style-application record. Lifted to namespace scope so the
// dedicated TriggerEngine can reference the same vector as StyleEngine.
struct StyleApplication {
    DependencyObject* object = nullptr;
    const Style* style = nullptr;
    Base::Vector<std::uint8_t> triggerStates;
    Base::Vector<std::uint8_t> bindingTriggerStates;
    Base::Vector<std::uint8_t> bindingTriggerKnown;
};

class StyleEngine {
public:
    using TriggerActionHandler = Base::Result<void>(*)(
        DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    explicit StyleEngine(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept;
    ~StyleEngine() noexcept;

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SetBindingTriggerState(
        DependencyObject& object,
        const Style& style,
        std::uint32_t triggerIndex,
        bool active) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;
    const Style* AppliedStyle(
        const DependencyObject& object)
        const noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept;
    const Base::Status& LastActionStatus() const noexcept;

private:
    ::Aero::StyleProviderSession providerSession_;
    ::Aero::StyleProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    Base::Vector<StyleApplication> applications_;
    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> AttachSetterBindings(
        DependencyObject& object,
        const Style& style) noexcept;
    void DetachSetterBindings(DependencyObject& object) noexcept;

    struct SetterBinding {
        DependencyObject* object = nullptr;
        Data::BindingHandle handle;
    };
    Base::Vector<SetterBinding> setterBindings_;

    // Owned trigger-evaluation engine. StyleEngine delegates all trigger
    // state, subscription and deferred-evaluation behavior to it.
    TriggerEngine* triggerEngine_ = nullptr;
};

} // namespace Aero
