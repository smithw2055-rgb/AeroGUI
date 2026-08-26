#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Freezable.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

struct TimelineRuntime;

using AnimationTime = std::uint64_t;

enum class FillBehavior : std::uint8_t {
    HoldEnd = 0U,
    Stop
};

class AERO_GUI_API Timeline : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Timeline, ::Aero::Freezable)
public:
    StringView GetBeginTime() const noexcept {
        return beginTimeText_.View();
    }
    StringView GetDuration() const noexcept {
        return durationText_.View();
    }
    StringView GetRepeatBehavior() const noexcept {
        return repeatBehaviorText_.View();
    }
    double GetSpeedRatio() const noexcept { return speedRatio_; }
    bool GetAutoReverse() const noexcept { return autoReverse_; }
    FillBehavior GetFillBehavior() const noexcept {
        return fillBehavior_;
    }

    void SetBeginTime(StringView value) noexcept;
    void SetDuration(StringView value) noexcept;
    Result<void> SetDurationChecked(StringView value) noexcept;
    void SetRepeatBehavior(StringView value) noexcept;
    void SetSpeedRatio(double value) noexcept;
    void SetAutoReverse(bool value) noexcept;
    void SetFillBehavior(FillBehavior value) noexcept;

protected:
    explicit Timeline(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}

private:
    friend struct TimelineRuntime;

    String beginTimeText_;
    String durationText_;
    String repeatBehaviorText_;
    AnimationTime beginTimeMicroseconds_ = 0U;
    AnimationTime durationMicroseconds_ = 0U;
    double repeatCount_ = 1.0;
    double speedRatio_ = 1.0;
    bool repeatForever_ = false;
    bool autoReverse_ = false;
    FillBehavior fillBehavior_ = FillBehavior::HoldEnd;
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(Aero::Media::Animation::FillBehavior)
