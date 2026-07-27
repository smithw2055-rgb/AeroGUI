#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>

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
    return {MakeMemberId(ownerType, MemberKind::Event, name)};
}

struct RoutedEventRegistration final {
    Base::StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

class AERO_API RoutedEventCatalog final {
public:
    struct Definition final {
        RoutedEventHandle handle;
        TypeId ownerType = InvalidTypeId;
        TypeId eventArgsType = InvalidTypeId;
        RoutingStrategy strategy = RoutingStrategy::Bubble;
        Base::String name;
        Definition() noexcept : name() {}
    };

    RoutedEventCatalog(
        TypeRegistry& types,
        MetadataBehaviorRegistrationStore& behaviors) noexcept;

    RoutedEventCatalog(const RoutedEventCatalog&) = delete;
    RoutedEventCatalog& operator=(const RoutedEventCatalog&) = delete;

    Base::Result<RoutedEventHandle> TryRegister(
        const RoutedEventRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }
    const Definition* Find(RoutedEventHandle event) const noexcept;

private:
    TypeRegistry* types_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviorRegistrations_ = nullptr;
    Base::Vector<Definition> definitions_;
    bool frozen_ = false;
};

} // namespace Aero::Core
