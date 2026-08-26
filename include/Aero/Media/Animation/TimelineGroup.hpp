#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Animation/Timeline.hpp>
#include <utility>

namespace Aero::Media::Animation {

class AERO_GUI_API TimelineGroup : public Timeline {
    AERO_DECLARE_TYPE(TimelineGroup, Timeline)
public:
    Result<void> AddChild(Ref<Timeline> value) noexcept;
    Result<void> AddTimeline(Ref<Timeline> value) noexcept {
        return AddChild(std::move(value));
    }
    void Clear() noexcept;
    void ClearTimelines() noexcept { Clear(); }
    Span<const Ref<Timeline>> GetTimelines() const noexcept {
        return {timelines_.Data(), timelines_.Size()};
    }
    Span<const Ref<Timeline>> Children() const noexcept {
        return GetTimelines();
    }
    std::uint32_t Count() const noexcept { return timelines_.Size(); }

protected:
    explicit TimelineGroup(Meta::TypeId runtimeType) noexcept
        : Timeline(runtimeType) {}
    ~TimelineGroup() override;
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnTimelineChanged(Freezable&) noexcept;
    Base::Vector<Ref<Timeline>> timelines_;
    FreezableChangedHandler timelineChangedHandler_;
};

} // namespace Aero::Media::Animation
