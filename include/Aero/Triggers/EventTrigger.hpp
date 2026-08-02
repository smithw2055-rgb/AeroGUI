#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_API EventTrigger : public Base::Object {
    AERO_DECLARE_TYPE(EventTrigger, Base::Object)
public:
    EventTrigger() noexcept : EventTrigger(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    Base::StringView GetRoutedEvent() const noexcept { return routedEvent_.View(); }
    Base::StringView GetEventName() const noexcept { return routedEvent_.View(); }
    Base::StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetRoutedEvent(Base::StringView value) noexcept;
    void SetEventName(Base::StringView value) noexcept { SetRoutedEvent(value); }
    void SetSourceName(Base::StringView value) noexcept;
    Base::Result<void> AddAction(Base::Ref<TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Base::Span<const Base::Ref<TriggerAction>> GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }
    Base::Result<void> AddConditionBehavior(Base::Ref<Base::Object> value) noexcept {
        return behaviors_.PushBack(std::move(value));
    }
    void ClearConditionBehaviors() noexcept { behaviors_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> GetBehaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

protected:
    explicit EventTrigger(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
    Base::String routedEvent_;
    Base::String sourceName_;
    Base::Vector<Base::Ref<TriggerAction>> actions_;
    Base::Vector<Base::Ref<Base::Object>> behaviors_;
};

} // namespace Aero::Media::Animation
