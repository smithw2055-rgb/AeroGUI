#pragma once

#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
using Aero::Interactivity::TriggerAction;

namespace Aero::Media::Animation {

class AERO_GUI_API EventTrigger : public ::Aero::TriggerBase {
    AERO_DECLARE_TYPE(EventTrigger, ::Aero::TriggerBase)
public:
    EventTrigger() noexcept : EventTrigger(StaticTypeId()) {}
    StringView GetRoutedEvent() const noexcept { return routedEvent_.View(); }
    StringView GetEventName() const noexcept { return routedEvent_.View(); }
    StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetRoutedEvent(StringView value) noexcept;
    void SetEventName(StringView value) noexcept { SetRoutedEvent(value); }
    void SetSourceName(StringView value) noexcept;
    Result<void> AddAction(Ref<TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Span<const Ref<TriggerAction>> GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }
    Result<void> AddConditionBehavior(Ref<Base::Object> value) noexcept {
        return behaviors_.PushBack(std::move(value));
    }
    void ClearConditionBehaviors() noexcept { behaviors_.Clear(); }
    Span<const Ref<Base::Object>> GetBehaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

protected:
    explicit EventTrigger(Meta::TypeId runtimeType) noexcept
        : ::Aero::TriggerBase(runtimeType) {}

private:
    String routedEvent_;
    String sourceName_;
    Base::Vector<Ref<TriggerAction>> actions_;
    Base::Vector<Ref<Base::Object>> behaviors_;
};

} // namespace Aero::Media::Animation
