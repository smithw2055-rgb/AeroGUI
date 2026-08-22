#pragma once

#include <Aero/Media/Animation/EventTrigger.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API TimerTrigger : public EventTrigger {
    AERO_DECLARE_TYPE(TimerTrigger, EventTrigger)
public:
    TimerTrigger() noexcept : EventTrigger(StaticTypeId()) {}
    std::uint32_t GetTotalTicks() const noexcept { return totalTicks_; }
    const Meta::PropertyValue& GetMillisecondsPerTick() const noexcept {
        return millisecondsPerTick_;
    }
    void SetTotalTicks(std::uint32_t value) noexcept { totalTicks_ = value; }
    void SetMillisecondsPerTick(const Meta::PropertyValue& value) noexcept {
        millisecondsPerTick_ = value;
    }
    Ref<Aero::Data::Binding> GetMillisecondsPerTickBinding() const noexcept {
        return millisecondsPerTickBinding_;
    }
    void SetMillisecondsPerTickBinding(
        Ref<Aero::Data::Binding> value) noexcept {
        millisecondsPerTickBinding_ = std::move(value);
    }

private:
    std::uint32_t totalTicks_ = 1U;
    Meta::PropertyValue millisecondsPerTick_;
    Ref<Aero::Data::Binding> millisecondsPerTickBinding_;
};

} // namespace Aero::Media::Animation
