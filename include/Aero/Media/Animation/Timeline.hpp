#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Media/Animation/Duration.hpp>
#include <Aero/Media/Animation/RepeatBehavior.hpp>
#include <Aero/Media/Animation/TimeSpan.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

using AnimationTime = std::uint64_t;

enum class FillBehavior : std::uint8_t {
    HoldEnd = 0U,
    Stop
};

class AERO_GUI_API Timeline : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Timeline, ::Aero::Freezable)
public:
    TimeSpan GetBeginTime() const noexcept {
        return GetValueOr(BeginTimeProperty, TimeSpan::Zero());
    }
    Duration GetDuration() const noexcept {
        return GetValueOr(DurationProperty, Duration::Automatic());
    }
    RepeatBehavior GetRepeatBehavior() const noexcept {
        return GetValueOr(RepeatBehaviorProperty, RepeatBehavior::Once());
    }
    double GetSpeedRatio() const noexcept {
        return GetValueOr(SpeedRatioProperty, 1.0);
    }
    bool GetAutoReverse() const noexcept {
        return GetValueOr(AutoReverseProperty, false);
    }
    FillBehavior GetFillBehavior() const noexcept {
        return GetValueOr(FillBehaviorProperty, FillBehavior::HoldEnd);
    }

    void SetBeginTime(TimeSpan value) noexcept;
    void SetBeginTime(StringView value) noexcept;
    void SetDuration(Duration value) noexcept;
    void SetDuration(StringView value) noexcept;
    Result<void> SetDurationChecked(StringView value) noexcept;
    void SetRepeatBehavior(RepeatBehavior value) noexcept;
    void SetRepeatBehavior(StringView value) noexcept;
    void SetSpeedRatio(double value) noexcept;
    void SetAutoReverse(bool value) noexcept;
    void SetFillBehavior(FillBehavior value) noexcept;

    inline static constexpr DependencyProperty<TimeSpan> BeginTimeProperty{
        "BeginTime"};
    inline static constexpr DependencyProperty<Duration> DurationProperty{
        "Duration"};
    inline static constexpr DependencyProperty<RepeatBehavior>
        RepeatBehaviorProperty{"RepeatBehavior"};
    inline static constexpr DependencyProperty<double> SpeedRatioProperty{
        "SpeedRatio"};
    inline static constexpr DependencyProperty<bool> AutoReverseProperty{
        "AutoReverse"};
    inline static constexpr DependencyProperty<FillBehavior>
        FillBehaviorProperty{"FillBehavior"};

protected:
    explicit Timeline(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(Aero::Media::Animation::FillBehavior)
