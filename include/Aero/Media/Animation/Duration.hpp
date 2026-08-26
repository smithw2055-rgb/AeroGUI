#pragma once

#include <Aero/Media/Animation/TimeSpan.hpp>

namespace Aero::Media::Animation {

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
    constexpr bool IsAutomatic() const noexcept {
        return kind_ == Kind::Automatic;
    }
    constexpr bool IsForever() const noexcept {
        return kind_ == Kind::Forever;
    }
    constexpr bool HasTimeSpan() const noexcept {
        return kind_ == Kind::TimeSpan;
    }
    constexpr TimeSpan GetTimeSpan() const noexcept { return timeSpan_; }

    static Result<Duration> TryParse(StringView text) noexcept;

    friend constexpr bool operator==(Duration left, Duration right) noexcept {
        return left.kind_ == right.kind_ && left.timeSpan_ == right.timeSpan_;
    }
    friend constexpr bool operator!=(Duration left, Duration right) noexcept {
        return !(left == right);
    }

private:
    Kind kind_ = Kind::Automatic;
    TimeSpan timeSpan_{};
};

} // namespace Aero::Media::Animation

namespace Aero::Meta {

template<>
struct TypeTraits<::Aero::Media::Animation::Duration> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Duration"); }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept { return "Duration"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta
