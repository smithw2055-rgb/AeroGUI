#pragma once

#include <Aero/Media/Animation/TimeSpan.hpp>

namespace Aero::Media::Animation {

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
    constexpr bool IsTimeSpan() const noexcept {
        return kind_ == Kind::TimeSpan;
    }
    constexpr bool IsPercent() const noexcept {
        return kind_ == Kind::Percent;
    }
    constexpr bool IsUniform() const noexcept {
        return kind_ == Kind::Uniform;
    }
    constexpr bool IsPaced() const noexcept { return kind_ == Kind::Paced; }
    constexpr TimeSpan GetTimeSpan() const noexcept { return timeSpan_; }
    constexpr double GetPercent() const noexcept { return percent_; }

    static Result<KeyTime> TryParse(StringView text) noexcept;
    std::uint64_t ResolveMicroseconds(
        std::uint64_t durationMicroseconds,
        std::uint32_t index,
        std::uint32_t count) const noexcept;

    friend constexpr bool operator==(KeyTime left, KeyTime right) noexcept {
        return left.kind_ == right.kind_ &&
            left.timeSpan_ == right.timeSpan_ &&
            left.percent_ == right.percent_;
    }
    friend constexpr bool operator!=(KeyTime left, KeyTime right) noexcept {
        return !(left == right);
    }

private:
    Kind kind_ = Kind::TimeSpan;
    TimeSpan timeSpan_{};
    double percent_ = 0.0;
};

} // namespace Aero::Media::Animation

namespace Aero::Meta {

template<>
struct TypeTraits<::Aero::Media::Animation::KeyTime> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("KeyTime"); }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept { return "KeyTime"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta
