#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>

namespace Aero::Core {

using TypeId = std::uint64_t;
using MemberId = std::uint64_t;

inline constexpr TypeId InvalidTypeId = 0U;
inline constexpr MemberId InvalidMemberId = 0U;
inline constexpr std::uint32_t TypeIdAlgorithmVersion = 1U;
inline constexpr std::uint32_t RegistrySnapshotFormatVersion = 1U;

enum class MemberKind : std::uint8_t {
    Property = 1U,
    Event = 2U
};

enum class TypeFlags : std::uint32_t {
    None = 0U,
    Abstract = 1U << 0U,
    Sealed = 1U << 1U,
    ValueType = 1U << 2U,
    Collection = 1U << 3U,
    MarkupExtension = 1U << 4U
};

enum class PropertyFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U,
    ReadOnly = 1U << 1U,
    Inherits = 1U << 2U,
    AffectsMeasure = 1U << 3U,
    AffectsArrange = 1U << 4U,
    AffectsRender = 1U << 5U
};

enum class EventFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U,
    Routed = 1U << 1U
};

AERO_NODISCARD constexpr TypeFlags operator|(
    TypeFlags left, TypeFlags right) noexcept {
    return static_cast<TypeFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

AERO_NODISCARD constexpr PropertyFlags operator|(
    PropertyFlags left, PropertyFlags right) noexcept {
    return static_cast<PropertyFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

AERO_NODISCARD constexpr EventFlags operator|(
    EventFlags left, EventFlags right) noexcept {
    return static_cast<EventFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

using ObjectFactory = Base::Result<Base::Ref<Base::Object>> (*)(
    Base::IAllocator& allocator) noexcept;

struct TypeRegistration final {
    Base::StringView xamlNamespace;
    Base::StringView name;
    TypeId baseType = InvalidTypeId;
    TypeFlags flags = TypeFlags::None;
    ObjectFactory factory = nullptr;
};

struct PropertyRegistration final {
    Base::StringView name;
    TypeId valueType = InvalidTypeId;
    PropertyFlags flags = PropertyFlags::None;
};

struct EventRegistration final {
    Base::StringView name;
    TypeId eventArgsType = InvalidTypeId;
    EventFlags flags = EventFlags::None;
};

class PropertyInfo final {
public:
    PropertyInfo(PropertyInfo&&) noexcept = default;
    PropertyInfo& operator=(PropertyInfo&&) noexcept = default;

    PropertyInfo(const PropertyInfo&) = delete;
    PropertyInfo& operator=(const PropertyInfo&) = delete;

    AERO_NODISCARD MemberId Id() const noexcept { return id_; }
    AERO_NODISCARD TypeId OwnerType() const noexcept { return ownerType_; }
    AERO_NODISCARD TypeId ValueType() const noexcept { return valueType_; }
    AERO_NODISCARD PropertyFlags Flags() const noexcept { return flags_; }
    AERO_NODISCARD Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class TypeRegistry;

    explicit PropertyInfo(Base::IAllocator* allocator) noexcept
        : name_(allocator) {}

    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    PropertyFlags flags_ = PropertyFlags::None;
    Base::String name_;
};

class EventInfo final {
public:
    EventInfo(EventInfo&&) noexcept = default;
    EventInfo& operator=(EventInfo&&) noexcept = default;

    EventInfo(const EventInfo&) = delete;
    EventInfo& operator=(const EventInfo&) = delete;

    AERO_NODISCARD MemberId Id() const noexcept { return id_; }
    AERO_NODISCARD TypeId OwnerType() const noexcept { return ownerType_; }
    AERO_NODISCARD TypeId EventArgsType() const noexcept { return eventArgsType_; }
    AERO_NODISCARD EventFlags Flags() const noexcept { return flags_; }
    AERO_NODISCARD Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class TypeRegistry;

    explicit EventInfo(Base::IAllocator* allocator) noexcept
        : name_(allocator) {}

    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId eventArgsType_ = InvalidTypeId;
    EventFlags flags_ = EventFlags::None;
    Base::String name_;
};

class TypeInfo final {
public:
    TypeInfo(TypeInfo&&) noexcept = default;
    TypeInfo& operator=(TypeInfo&&) noexcept = default;

    TypeInfo(const TypeInfo&) = delete;
    TypeInfo& operator=(const TypeInfo&) = delete;

    AERO_NODISCARD TypeId Id() const noexcept { return id_; }
    AERO_NODISCARD TypeId BaseType() const noexcept { return baseType_; }
    AERO_NODISCARD TypeFlags Flags() const noexcept { return flags_; }
    AERO_NODISCARD ObjectFactory Factory() const noexcept { return factory_; }
    AERO_NODISCARD Base::StringView XamlNamespace() const noexcept {
        return xamlNamespace_.View();
    }
    AERO_NODISCARD Base::StringView Name() const noexcept {
        return name_.View();
    }
    AERO_NODISCARD Base::Span<const PropertyInfo> Properties() const noexcept {
        return {properties_.Data(), properties_.Size()};
    }
    AERO_NODISCARD Base::Span<const EventInfo> Events() const noexcept {
        return {events_.Data(), events_.Size()};
    }

private:
    friend class TypeRegistry;

    explicit TypeInfo(Base::IAllocator* allocator) noexcept
        : xamlNamespace_(allocator),
          name_(allocator),
          properties_(allocator),
          events_(allocator) {}

    TypeId id_ = InvalidTypeId;
    TypeId baseType_ = InvalidTypeId;
    TypeFlags flags_ = TypeFlags::None;
    ObjectFactory factory_ = nullptr;
    Base::String xamlNamespace_;
    Base::String name_;
    Base::Vector<PropertyInfo> properties_;
    Base::Vector<EventInfo> events_;
};

AERO_NODISCARD AERO_API TypeId MakeTypeId(
    Base::StringView xamlNamespace,
    Base::StringView name) noexcept;

AERO_NODISCARD AERO_API MemberId MakeMemberId(
    TypeId ownerType,
    MemberKind kind,
    Base::StringView name) noexcept;

class AERO_API TypeRegistry final {
public:
    explicit TypeRegistry(Base::IAllocator* allocator = nullptr) noexcept;
    ~TypeRegistry() = default;

    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    TypeRegistry(TypeRegistry&&) = delete;
    TypeRegistry& operator=(TypeRegistry&&) = delete;

    AERO_NODISCARD Base::Result<TypeId> TryRegisterType(
        const TypeRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<MemberId> TryRegisterProperty(
        TypeId ownerType,
        const PropertyRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<MemberId> TryRegisterEvent(
        TypeId ownerType,
        const EventRegistration& registration) noexcept;

    AERO_NODISCARD Base::Result<void> Freeze() noexcept;

    AERO_NODISCARD bool IsFrozen() const noexcept { return frozen_; }
    AERO_NODISCARD std::uint32_t TypeCount() const noexcept {
        return types_.Size();
    }
    AERO_NODISCARD Base::IAllocator& Allocator() const noexcept {
        return *allocator_;
    }
    AERO_NODISCARD Base::Span<const TypeInfo> Types() const noexcept {
        return {types_.Data(), types_.Size()};
    }

    AERO_NODISCARD const TypeInfo* FindType(TypeId id) const noexcept;
    AERO_NODISCARD const TypeInfo* FindType(
        Base::StringView xamlNamespace,
        Base::StringView name) const noexcept;

    AERO_NODISCARD const PropertyInfo* FindProperty(MemberId id) const noexcept;
    AERO_NODISCARD const PropertyInfo* FindProperty(
        TypeId ownerType,
        Base::StringView name,
        bool includeBaseTypes = true) const noexcept;

    AERO_NODISCARD const EventInfo* FindEvent(MemberId id) const noexcept;
    AERO_NODISCARD const EventInfo* FindEvent(
        TypeId ownerType,
        Base::StringView name,
        bool includeBaseTypes = true) const noexcept;

    AERO_NODISCARD bool IsDerivedFrom(
        TypeId type,
        TypeId expectedBase) const noexcept;

    AERO_NODISCARD Base::Result<void> BuildSnapshot(
        Base::String& output) const noexcept;
    AERO_NODISCARD Base::Result<Base::HashCode> ComputeSnapshotHash()
        const noexcept;

private:
    struct MemberLocation final {
        std::uint32_t typeIndex = 0U;
        std::uint32_t memberIndex = 0U;
        MemberKind kind = MemberKind::Property;
    };

    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<TypeInfo> types_;
    Base::HashMap<TypeId, std::uint32_t> typeIndex_;
    Base::HashMap<MemberId, MemberLocation> memberIndex_;
    bool frozen_ = false;

    AERO_NODISCARD TypeInfo* MutableType(TypeId id) noexcept;
    AERO_NODISCARD const TypeInfo* TypeAt(std::uint32_t index) const noexcept;
    AERO_NODISCARD const PropertyInfo* PropertyAt(
        const MemberLocation& location) const noexcept;
    AERO_NODISCARD const EventInfo* EventAt(
        const MemberLocation& location) const noexcept;
};

} // namespace Aero::Core
