#pragma once

#include <Aero/DependencyProperty.hpp>
#include <Aero/Media/Animation/ParallelTimeline.hpp>
#include <utility>

namespace Aero::Media::Animation {

class AERO_GUI_API Storyboard : public ParallelTimeline {
    AERO_DECLARE_TYPE(Storyboard, ParallelTimeline)
public:
    Storyboard() noexcept : Storyboard(StaticTypeId()) {}

    inline static constexpr AttachedProperty<String> TargetNameProperty{"TargetName"};
    inline static constexpr AttachedProperty<String> TargetPropertyProperty{"TargetProperty"};

    Result<void> AddTimeline(Ref<Timeline> value) noexcept {
        return AddChild(std::move(value));
    }
    void ClearTimelines() noexcept { Clear(); }

protected:
    explicit Storyboard(Meta::TypeId runtimeType) noexcept
        : ParallelTimeline(runtimeType) {}
};

} // namespace Aero::Media::Animation
