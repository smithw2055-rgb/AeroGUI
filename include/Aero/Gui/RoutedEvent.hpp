#pragma once

#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Input {

enum class KeyboardAction : std::uint8_t;
enum class MouseButton : std::uint8_t;
enum class MouseButtonState : std::uint8_t;
enum class DragDropEffects : std::uint8_t;

} // namespace Aero::Input

namespace Aero {

class UIElement;

using RoutedEventId = Meta::MemberId;

struct RoutedEventHandle {
    RoutedEventId value = Meta::InvalidMemberId;
    constexpr bool IsValid() const noexcept {
        return value != Meta::InvalidMemberId;
    }
};

using RoutedEvent = RoutedEventHandle;

enum class RoutingStrategy : std::uint8_t { Direct = 0U, Tunnel, Bubble };

constexpr RoutedEventHandle MakeRoutedEventHandle(
    Meta::TypeId ownerType,
    Base::StringView name) noexcept;

template<class TOwner, class TArgs>
class RoutedEventRef {
public:
    using Owner = TOwner;
    using Args = TArgs;

    constexpr explicit RoutedEventRef(Base::StringView name) noexcept
        : name_(name),
          handle_(MakeRoutedEventHandle(
              TOwner::StaticTypeIdValue_, name)) {}

    constexpr Base::StringView Name() const noexcept { return name_; }
    constexpr RoutedEventHandle Handle() const noexcept { return handle_; }
    constexpr operator RoutedEventHandle() const noexcept { return handle_; }
    constexpr Meta::MemberId Id() const noexcept { return handle_.value; }

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

constexpr RoutedEventHandle MakeRoutedEventHandle(
    Meta::TypeId ownerType,
    Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.MEMBER.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(
        domain,
        static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddByte(2U);
    builder.AddString(name);
    return {builder.Finish()};
}

} // namespace Aero
