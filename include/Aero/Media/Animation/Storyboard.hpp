#pragma once

// TimelineGroup, ParallelTimeline, and Storyboard.
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Media/Animation/Timeline.hpp>
#include <Aero/Media/FreezableCollection.hpp>
#include <Aero/DependencyProperty.hpp>

#include <utility>

namespace Aero::Media::Animation {

class AERO_GUI_API TimelineGroup : public Timeline {
    AERO_DECLARE_TYPE(TimelineGroup, Timeline)

public:
    void AddChild(Ref<Timeline> value) noexcept;
    void AddTimeline(Ref<Timeline> value) noexcept { AddChild(std::move(value)); }
    void Clear() noexcept;
    void ClearTimelines() noexcept { Clear(); }
    Span<const Ref<Timeline>> GetTimelines() const noexcept { return timelines_.AsSpan(); }
    Span<const Ref<Timeline>> Children() const noexcept { return GetTimelines(); }
    std::uint32_t Count() const noexcept { return timelines_.GetCount(); }

protected:
    explicit TimelineGroup(Meta::TypeId runtimeType) noexcept : Timeline(runtimeType) {}
    ~TimelineGroup() override;
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnTimelineChanged(Freezable&) noexcept;
    FreezableCollection<Timeline> timelines_;
    FreezableChangedHandler timelineChangedHandler_;
};

class AERO_GUI_API ParallelTimeline : public TimelineGroup {
    AERO_DECLARE_TYPE(ParallelTimeline, TimelineGroup)

public:
    ParallelTimeline() noexcept : ParallelTimeline(StaticTypeId()) {}

protected:
    explicit ParallelTimeline(Meta::TypeId runtimeType) noexcept : TimelineGroup(runtimeType) {}
};

class AERO_GUI_API Storyboard : public ParallelTimeline {
    AERO_DECLARE_TYPE(Storyboard, ParallelTimeline)

public:
    Storyboard() noexcept : Storyboard(StaticTypeId()) {}

    void AddTimeline(Ref<Timeline> value) noexcept { AddChild(std::move(value)); }

    void ClearTimelines() noexcept { Clear(); }

    AERO_ATTACHED_PROPERTY(String, TargetName);
    AERO_ATTACHED_PROPERTY(String, TargetProperty);

protected:
    explicit Storyboard(Meta::TypeId runtimeType) noexcept : ParallelTimeline(runtimeType) {}
};

} // namespace Aero::Media::Animation
