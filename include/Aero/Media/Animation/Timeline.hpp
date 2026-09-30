#pragma once

// Clock values and Timeline. Storyboard stays in Storyboard.hpp.
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>
#include <Aero/Animatable.hpp>

#include <cstdint>

namespace Aero::Media::Animation {

// Clock-time value used by BeginTime and KeyTime's TimeSpan variant.
// ParseClockTimeMicroseconds accepts 1.5, 2s, 500ms, M:S, and H:M:S.
AERO_GUI_API Result<std::uint64_t> ParseClockTimeMicroseconds(StringView text) noexcept;

struct TimeSpan {
    static constexpr TimeSpan Zero() noexcept { return {}; }
    static constexpr TimeSpan FromMicroseconds(std::uint64_t microseconds) noexcept {
        TimeSpan value{};
        value.microseconds_ = microseconds;
        return value;
    }

    constexpr std::uint64_t Microseconds() const noexcept { return microseconds_; }
    constexpr bool IsZero() const noexcept { return microseconds_ == 0U; }

    static Result<TimeSpan> TryParse(StringView text) noexcept;

    friend constexpr bool operator==(TimeSpan left, TimeSpan right) noexcept {
        return left.microseconds_ == right.microseconds_;
    }
    friend constexpr bool operator!=(TimeSpan left, TimeSpan right) noexcept { return !(left == right); }

private:
    std::uint64_t microseconds_ = 0U;
};

struct Duration {
    enum class Kind : std::uint8_t {
        Automatic = 0U,
        Forever,
        TimeSpan
    };

    static constexpr Duration Automatic() noexcept { return {}; }
    static constexpr Duration Forever() noexcept {
        Duration value{};
        value.kind_ = Kind::Forever;
        return value;
    }
    static constexpr Duration FromTimeSpan(TimeSpan time) noexcept {
        Duration value{};
        value.kind_ = Kind::TimeSpan;
        value.timeSpan_ = time;
        return value;
    }

    constexpr Kind GetKind() const noexcept { return kind_; }
    constexpr bool IsAutomatic() const noexcept { return kind_ == Kind::Automatic; }
    constexpr bool IsForever() const noexcept { return kind_ == Kind::Forever; }
    constexpr bool HasTimeSpan() const noexcept { return kind_ == Kind::TimeSpan; }
    constexpr TimeSpan GetTimeSpan() const noexcept { return timeSpan_; }

    static Result<Duration> TryParse(StringView text) noexcept;

    friend constexpr bool operator==(Duration left, Duration right) noexcept {
        return left.kind_ == right.kind_ && left.timeSpan_ == right.timeSpan_;
    }
    friend constexpr bool operator!=(Duration left, Duration right) noexcept { return !(left == right); }

private:
    Kind kind_ = Kind::Automatic;
    TimeSpan timeSpan_{};
};

struct KeyTime {
    enum class Kind : std::uint8_t {
        TimeSpan = 0U,
        Percent,
        Uniform,
        Paced
    };

    static constexpr KeyTime FromTimeSpan(TimeSpan time) noexcept {
        KeyTime value{};
        value.kind_ = Kind::TimeSpan;
        value.timeSpan_ = time;
        return value;
    }
    static constexpr KeyTime FromPercent(double percent) noexcept {
        KeyTime value{};
        value.kind_ = Kind::Percent;
        value.percent_ = percent;
        return value;
    }
    static constexpr KeyTime Uniform() noexcept {
        KeyTime value{};
        value.kind_ = Kind::Uniform;
        return value;
    }
    static constexpr KeyTime Paced() noexcept {
        KeyTime value{};
        value.kind_ = Kind::Paced;
        return value;
    }

    constexpr Kind GetKind() const noexcept { return kind_; }
    constexpr bool IsTimeSpan() const noexcept { return kind_ == Kind::TimeSpan; }
    constexpr bool IsPercent() const noexcept { return kind_ == Kind::Percent; }
    constexpr bool IsUniform() const noexcept { return kind_ == Kind::Uniform; }
    constexpr bool IsPaced() const noexcept { return kind_ == Kind::Paced; }
    constexpr TimeSpan GetTimeSpan() const noexcept { return timeSpan_; }
    constexpr double GetPercent() const noexcept { return percent_; }

    static Result<KeyTime> TryParse(StringView text) noexcept;
    std::uint64_t ResolveMicroseconds(std::uint64_t durationMicroseconds, std::uint32_t index,
        std::uint32_t count) const noexcept;

    friend constexpr bool operator==(KeyTime left, KeyTime right) noexcept {
        return left.kind_ == right.kind_ && left.timeSpan_ == right.timeSpan_ && left.percent_ == right.percent_;
    }
    friend constexpr bool operator!=(KeyTime left, KeyTime right) noexcept { return !(left == right); }

private:
    Kind kind_ = Kind::TimeSpan;
    TimeSpan timeSpan_{};
    double percent_ = 0.0;
};

struct RepeatBehavior {
    enum class Kind : std::uint8_t {
        Count = 0U,
        Forever,
        Duration
    };

    static constexpr RepeatBehavior Count(double value) noexcept {
        RepeatBehavior result{};
        result.kind_ = Kind::Count;
        result.count_ = value;
        return result;
    }
    static constexpr RepeatBehavior Forever() noexcept {
        RepeatBehavior result{};
        result.kind_ = Kind::Forever;
        result.count_ = 1.0;
        return result;
    }
    static constexpr RepeatBehavior FromDuration(TimeSpan time) noexcept {
        RepeatBehavior result{};
        result.kind_ = Kind::Duration;
        result.duration_ = time;
        return result;
    }
    static constexpr RepeatBehavior Once() noexcept { return Count(1.0); }

    constexpr Kind GetKind() const noexcept { return kind_; }
    constexpr bool HasCount() const noexcept { return kind_ == Kind::Count; }
    constexpr bool IsForever() const noexcept { return kind_ == Kind::Forever; }
    constexpr bool HasDuration() const noexcept { return kind_ == Kind::Duration; }
    constexpr double GetCount() const noexcept { return count_; }
    constexpr TimeSpan GetDuration() const noexcept { return duration_; }

    static Result<RepeatBehavior> TryParse(StringView text) noexcept;

    friend constexpr bool operator==(RepeatBehavior left, RepeatBehavior right) noexcept {
        return left.kind_ == right.kind_ && left.count_ == right.count_ && left.duration_ == right.duration_;
    }
    friend constexpr bool operator!=(RepeatBehavior left, RepeatBehavior right) noexcept { return !(left == right); }

private:
    Kind kind_ = Kind::Count;
    double count_ = 1.0;
    TimeSpan duration_{};
};

using AnimationTime = std::uint64_t;

enum class FillBehavior : std::uint8_t {
    HoldEnd = 0U,
    Stop
};

class AERO_GUI_API Timeline : public ::Aero::Animatable {
    AERO_DECLARE_TYPE(Timeline, ::Aero::Animatable)

public:
    TimeSpan GetBeginTime() const noexcept { return GetValue(BeginTimeProperty); }
    void SetBeginTime(TimeSpan value) noexcept;
    void SetBeginTime(StringView value) noexcept;
    Duration GetDuration() const noexcept { return GetValue(DurationProperty); }
    void SetDuration(Duration value) noexcept;
    void SetDuration(StringView value) noexcept;
    RepeatBehavior GetRepeatBehavior() const noexcept { return GetValue(RepeatBehaviorProperty); }
    void SetRepeatBehavior(RepeatBehavior value) noexcept;
    void SetRepeatBehavior(StringView value) noexcept;
    double GetSpeedRatio() const noexcept { return GetValue(SpeedRatioProperty); }
    void SetSpeedRatio(double value) noexcept;
    bool GetAutoReverse() const noexcept { return GetValue(AutoReverseProperty); }
    void SetAutoReverse(bool value) noexcept;
    FillBehavior GetFillBehavior() const noexcept { return GetValue(FillBehaviorProperty); }
    void SetFillBehavior(FillBehavior value) noexcept;

    AERO_DEPENDENCY_PROPERTY(TimeSpan, BeginTime);
    AERO_DEPENDENCY_PROPERTY(Duration, Duration);
    AERO_DEPENDENCY_PROPERTY(RepeatBehavior, RepeatBehavior);
    AERO_DEPENDENCY_PROPERTY(double, SpeedRatio);
    AERO_DEPENDENCY_PROPERTY(bool, AutoReverse);
    AERO_DEPENDENCY_PROPERTY(FillBehavior, FillBehavior);

protected:
    explicit Timeline(Meta::TypeId runtimeType) noexcept : Animatable(runtimeType) {}
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_VALUE(::Aero::Media::Animation::TimeSpan, "TimeSpan")
AERO_DECLARE_TYPE_VALUE(::Aero::Media::Animation::Duration, "Duration")
AERO_DECLARE_TYPE_VALUE(::Aero::Media::Animation::KeyTime, "KeyTime")
AERO_DECLARE_TYPE_VALUE(::Aero::Media::Animation::RepeatBehavior, "RepeatBehavior")

AERO_DECLARE_TYPE_ENUM(Aero::Media::Animation::FillBehavior)
