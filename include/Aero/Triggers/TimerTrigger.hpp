#pragma once

#include <Aero/Triggers/EventTrigger.hpp>

namespace Aero::Media::Animation {

class AERO_API TimerTrigger : public EventTrigger {
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

private:
    std::uint32_t totalTicks_ = 1U;
    Meta::PropertyValue millisecondsPerTick_;
};

} // namespace Aero::Media::Animation
