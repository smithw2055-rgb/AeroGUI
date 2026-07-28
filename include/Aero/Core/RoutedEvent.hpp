#pragma once

#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Metadata/MetadataId.hpp>

#include <cstdint>

namespace Aero::Core {

using RoutedEventId = MemberId;

struct RoutedEventHandle final {
    RoutedEventId value = InvalidMemberId;

    constexpr bool IsValid() const noexcept {
        return value != InvalidMemberId;
    }
};

constexpr RoutedEventHandle MakeRoutedEventHandle(
    TypeId ownerType,
    Base::StringView name) noexcept;

template<class TOwner, class TArgs>
class RoutedEventRef final {
public:
    using Owner = TOwner;
    using Args = TArgs;

    constexpr explicit RoutedEventRef(
        Base::StringView name) noexcept
        : name_(name),
          handle_(MakeRoutedEventHandle(
              TOwner::StaticTypeIdValue_, name)) {}

    constexpr Base::StringView Name() const noexcept {
        return name_;
    }

    constexpr RoutedEventHandle Handle() const noexcept {
        return handle_;
    }

    constexpr operator RoutedEventHandle() const noexcept {
        return handle_;
    }

    constexpr MemberId Id() const noexcept {
        return handle_.value;
    }

private:
    Base::StringView name_;
    RoutedEventHandle handle_;
};

constexpr bool operator==(
    RoutedEventHandle left,
    RoutedEventHandle right) noexcept {
    return left.value == right.value;
}

constexpr bool operator!=(
    RoutedEventHandle left,
    RoutedEventHandle right) noexcept {
    return !(left == right);
}

enum class RoutingStrategy : std::uint8_t {
    Direct = 0U,
    Tunnel,
    Bubble
};

constexpr RoutedEventHandle MakeRoutedEventHandle(
    TypeId ownerType,
    Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.MEMBER.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(
        domain,
        static_cast<std::uint32_t>(
            sizeof(domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddByte(2U);
    builder.AddString(name);
    return {builder.Finish()};
}

} // namespace Aero::Core
