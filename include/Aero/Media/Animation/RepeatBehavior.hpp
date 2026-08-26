#pragma once

#include <Aero/Media/Animation/TimeSpan.hpp>

namespace Aero::Media::Animation {

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
    constexpr bool IsForever() const noexcept {
        return kind_ == Kind::Forever;
    }
    constexpr bool HasDuration() const noexcept {
        return kind_ == Kind::Duration;
    }
    constexpr double GetCount() const noexcept { return count_; }
    constexpr TimeSpan GetDuration() const noexcept { return duration_; }

    static Result<RepeatBehavior> TryParse(StringView text) noexcept;

    friend constexpr bool operator==(
        RepeatBehavior left,
        RepeatBehavior right) noexcept {
        return left.kind_ == right.kind_ &&
            left.count_ == right.count_ &&
            left.duration_ == right.duration_;
    }
    friend constexpr bool operator!=(
        RepeatBehavior left,
        RepeatBehavior right) noexcept {
        return !(left == right);
    }

private:
    Kind kind_ = Kind::Count;
    double count_ = 1.0;
    TimeSpan duration_{};
};

} // namespace Aero::Media::Animation

namespace Aero::Meta {

template<>
struct TypeTraits<::Aero::Media::Animation::RepeatBehavior> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("RepeatBehavior");
    }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept { return "RepeatBehavior"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta
