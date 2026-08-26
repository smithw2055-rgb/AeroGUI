#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

// Clock-time value used by BeginTime and KeyTime's TimeSpan variant.
// ParseClockTimeMicroseconds accepts 1.5, 2s, 500ms, M:S, and H:M:S.
AERO_GUI_API Result<std::uint64_t> ParseClockTimeMicroseconds(
    StringView text) noexcept;

struct TimeSpan {
    static constexpr TimeSpan Zero() noexcept { return {}; }
    static constexpr TimeSpan FromMicroseconds(
        std::uint64_t microseconds) noexcept {
        TimeSpan value{};
        value.microseconds_ = microseconds;
        return value;
    }

    constexpr std::uint64_t Microseconds() const noexcept {
        return microseconds_;
    }
    constexpr bool IsZero() const noexcept { return microseconds_ == 0U; }

    static Result<TimeSpan> TryParse(StringView text) noexcept;

    friend constexpr bool operator==(TimeSpan left, TimeSpan right) noexcept {
        return left.microseconds_ == right.microseconds_;
    }
    friend constexpr bool operator!=(TimeSpan left, TimeSpan right) noexcept {
        return !(left == right);
    }

private:
    std::uint64_t microseconds_ = 0U;
};

} // namespace Aero::Media::Animation

namespace Aero::Meta {

template<>
struct TypeTraits<::Aero::Media::Animation::TimeSpan> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("TimeSpan"); }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept { return "TimeSpan"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta
