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
#include <Aero/Core/MetadataId.hpp>
#include <Aero/Core/Value.hpp>

#include <cstdint>

namespace Aero::Core {

class MetaRegistrationContext;
class MetaRegistrationBuilder;
class MetadataBehaviorRegistrationStore;
class MetadataRegistrationTypes;
class MetadataRuntime;

inline constexpr std::uint32_t TypeIdAlgorithmVersion = 1U;
inline constexpr std::uint32_t RegistrySnapshotFormatVersion = 4U;

enum class MetadataTypeKind : std::uint8_t {
    Object = 0U,
    Interface,
    Struct,
    Enum,
    Primitive
};

enum class MemberKind : std::uint8_t {
    Property = 1U,
    Event = 2U,
    Method = 3U,
    Field = 4U,
    EnumValue = 5U
};

enum class TypeFlags : std::uint32_t {
    None = 0U,
    Abstract = 1U << 0U,
    Sealed = 1U << 1U,
    ValueType = 1U << 2U,
    Collection = 1U << 3U,
    MarkupExtension = 1U << 4U,
    FlagsEnum = 1U << 5U,
    TriviallyCopyable = 1U << 6U,
    SignedEnum = 1U << 7U
};

enum class PropertyFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U,
    ReadOnly = 1U << 1U,
    Inherits = 1U << 2U,
    AffectsMeasure = 1U << 3U,
    AffectsArrange = 1U << 4U,
    AffectsRender = 1U << 5U,
    AffectsParentMeasure = 1U << 6U,
    AffectsParentArrange = 1U << 7U,
    Structural = 1U << 8U,
    Collection = 1U << 9U,
    WriteOnly = 1U << 10U
};

enum class FieldFlags : std::uint32_t {
    None = 0U,
    ReadOnly = 1U << 0U,
    Transient = 1U << 1U
};

enum class PropertyAccessKind : std::uint8_t {
    External = 0U,
    Ordinary = 1U,
    Provider = 2U
};

using PropertyProviderId = std::uint64_t;
inline constexpr PropertyProviderId InvalidPropertyProviderId = 0U;
inline constexpr PropertyProviderId DependencyPropertyProviderId =
    UINT64_C(0x445050524F564944);

enum class MethodFlags : std::uint32_t {
    None = 0U,
    Const = 1U << 0U
};

enum class EventFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U,
    Routed = 1U << 1U
};

constexpr TypeFlags operator|(
    TypeFlags left, TypeFlags right) noexcept {
    return static_cast<TypeFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr TypeFlags operator&(
    TypeFlags left, TypeFlags right) noexcept {
    return static_cast<TypeFlags>(
        static_cast<std::uint32_t>(left) &
        static_cast<std::uint32_t>(right));
}

constexpr PropertyFlags operator|(
    PropertyFlags left, PropertyFlags right) noexcept {
    return static_cast<PropertyFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr FieldFlags operator|(
    FieldFlags left, FieldFlags right) noexcept {
    return static_cast<FieldFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr EventFlags operator|(
    EventFlags left, EventFlags right) noexcept {
    return static_cast<EventFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr bool HasTypeFlag(TypeFlags value, TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr bool HasFieldFlag(FieldFlags value, FieldFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

using ObjectFactory = Base::Result<Base::Ref<Base::Object>> (*)() noexcept;
using PropertyGetCallback = Base::Result<Value> (*)(
    const Base::Object& object,
    void* context) noexcept;
using PropertySetCallback = Base::Result<void> (*)(
    Base::Object& object,
    const Value& value,
    void* context) noexcept;
using MethodInvokeCallback = Base::Result<Value> (*)(
    Base::Object& object,
    Base::Span<const Value> arguments,
    void* context) noexcept;
using ValueMemberGetCallback = Base::Result<Value> (*)(
    const void* object,
    MetadataRuntime& runtime,
    void* context) noexcept;
using ValueMemberSetCallback = Base::Result<void> (*)(
    void* object,
    const Value& value,
    MetadataRuntime& runtime,
    void* context) noexcept;

struct TypeRegistration final {
    Base::StringView xamlNamespace;
    Base::StringView name;
    TypeId baseType = InvalidTypeId;
    TypeFlags flags = TypeFlags::None;
    ObjectFactory factory = nullptr;
    MetadataTypeKind kind = MetadataTypeKind::Object;
    TypeId underlyingType = InvalidTypeId;
    Base::Span<const TypeId> interfaces;
};

struct PropertyRegistration final {
    Base::StringView name;
    TypeId valueType = InvalidTypeId;
    PropertyFlags flags = PropertyFlags::None;
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
};

struct FieldRegistration final {
    Base::StringView name;
    TypeId valueType = InvalidTypeId;
    FieldFlags flags = FieldFlags::None;
    ValueMemberGetCallback get = nullptr;
    ValueMemberSetCallback set = nullptr;
    void* context = nullptr;
};

struct EnumValueRegistration final {
    Base::StringView name;
    std::uint64_t rawValue = 0U;
};

struct EventRegistration final {
    Base::StringView name;
    TypeId eventArgsType = InvalidTypeId;
    EventFlags flags = EventFlags::None;
};

struct MethodParameterRegistration final {
    Base::StringView name;
    TypeId type = InvalidTypeId;
};

struct MethodRegistration final {
    Base::StringView name;
    TypeId returnType = InvalidTypeId;
    Base::Span<const MethodParameterRegistration> parameters;
    MethodFlags flags = MethodFlags::None;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

struct TypeFactoryRegistration final {
    TypeId type = InvalidTypeId;
    ObjectFactory factory = nullptr;
};

struct PropertyAccessorRegistration final {
    MemberId member = InvalidMemberId;
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
};

struct ValueMemberAccessorRegistration final {
    MemberId member = InvalidMemberId;
    ValueMemberGetCallback get = nullptr;
    ValueMemberSetCallback set = nullptr;
    void* context = nullptr;
};

struct MethodInvokerRegistration final {
    MemberId member = InvalidMemberId;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

class PropertyInfo final {
public:
    PropertyInfo(PropertyInfo&&) noexcept = default;
    PropertyInfo& operator=(PropertyInfo&&) noexcept = default;
    PropertyInfo(const PropertyInfo&) = delete;
    PropertyInfo& operator=(const PropertyInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ValueType() const noexcept { return valueType_; }
    PropertyFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    PropertyInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    PropertyFlags flags_ = PropertyFlags::None;
    Base::String name_;
};

class FieldInfo final {
public:
    FieldInfo(FieldInfo&&) noexcept = default;
    FieldInfo& operator=(FieldInfo&&) noexcept = default;
    FieldInfo(const FieldInfo&) = delete;
    FieldInfo& operator=(const FieldInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ValueType() const noexcept { return valueType_; }
    FieldFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    FieldInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    FieldFlags flags_ = FieldFlags::None;
    Base::String name_;
};

class EnumValueInfo final {
public:
    EnumValueInfo(EnumValueInfo&&) noexcept = default;
    EnumValueInfo& operator=(EnumValueInfo&&) noexcept = default;
    EnumValueInfo(const EnumValueInfo&) = delete;
    EnumValueInfo& operator=(const EnumValueInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    std::uint64_t RawValue() const noexcept { return rawValue_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    EnumValueInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    std::uint64_t rawValue_ = 0U;
    Base::String name_;
};

class MethodParameterInfo final {
public:
    MethodParameterInfo(MethodParameterInfo&&) noexcept = default;
    MethodParameterInfo& operator=(MethodParameterInfo&&) noexcept = default;
    MethodParameterInfo(const MethodParameterInfo&) = delete;
    MethodParameterInfo& operator=(const MethodParameterInfo&) = delete;
    Base::StringView Name() const noexcept { return name_.View(); }
    TypeId Type() const noexcept { return type_; }
private:
    friend class TypeRegistry;
    MethodParameterInfo() noexcept = default;
    TypeId type_ = InvalidTypeId;
    Base::String name_;
};

class MethodInfo final {
public:
    MethodInfo(MethodInfo&&) noexcept = default;
    MethodInfo& operator=(MethodInfo&&) noexcept = default;
    MethodInfo(const MethodInfo&) = delete;
    MethodInfo& operator=(const MethodInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId ReturnType() const noexcept { return returnType_; }
    MethodFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
    Base::Span<const MethodParameterInfo> Parameters() const noexcept {
        return {parameters_.Data(), parameters_.Size()};
    }
private:
    friend class TypeRegistry;
    MethodInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId returnType_ = InvalidTypeId;
    MethodFlags flags_ = MethodFlags::None;
    Base::String name_;
    Base::Vector<MethodParameterInfo> parameters_;
};

class EventInfo final {
public:
    EventInfo(EventInfo&&) noexcept = default;
    EventInfo& operator=(EventInfo&&) noexcept = default;
    EventInfo(const EventInfo&) = delete;
    EventInfo& operator=(const EventInfo&) = delete;
    MemberId Id() const noexcept { return id_; }
    TypeId OwnerType() const noexcept { return ownerType_; }
    TypeId EventArgsType() const noexcept { return eventArgsType_; }
    EventFlags Flags() const noexcept { return flags_; }
    Base::StringView Name() const noexcept { return name_.View(); }
private:
    friend class TypeRegistry;
    EventInfo() noexcept = default;
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
    TypeId Id() const noexcept { return id_; }
    TypeId BaseType() const noexcept { return baseType_; }
    TypeId UnderlyingType() const noexcept { return underlyingType_; }
    MetadataTypeKind Kind() const noexcept { return kind_; }
    TypeFlags Flags() const noexcept { return flags_; }
    bool IsFlagsEnum() const noexcept {
        return kind_ == MetadataTypeKind::Enum &&
            HasTypeFlag(flags_, TypeFlags::FlagsEnum);
    }
    Base::StringView XamlNamespace() const noexcept { return xamlNamespace_.View(); }
    Base::StringView Name() const noexcept { return name_.View(); }
    Base::Span<const TypeId> Interfaces() const noexcept { return {interfaces_.Data(), interfaces_.Size()}; }
    Base::Span<const PropertyInfo> Properties() const noexcept { return {properties_.Data(), properties_.Size()}; }
    Base::Span<const FieldInfo> Fields() const noexcept { return {fields_.Data(), fields_.Size()}; }
    Base::Span<const EnumValueInfo> EnumValues() const noexcept { return {enumValues_.Data(), enumValues_.Size()}; }
    Base::Span<const EventInfo> Events() const noexcept { return {events_.Data(), events_.Size()}; }
    Base::Span<const MethodInfo> Methods() const noexcept { return {methods_.Data(), methods_.Size()}; }
    MemberId ContentMember() const noexcept { return contentMember_; }
private:
    friend class TypeRegistry;
    TypeInfo() noexcept = default;
    TypeId id_ = InvalidTypeId;
    TypeId baseType_ = InvalidTypeId;
    TypeId underlyingType_ = InvalidTypeId;
    MetadataTypeKind kind_ = MetadataTypeKind::Object;
    TypeFlags flags_ = TypeFlags::None;
    Base::String xamlNamespace_;
    Base::String name_;
    Base::Vector<TypeId> interfaces_;
    Base::Vector<PropertyInfo> properties_;
    Base::Vector<FieldInfo> fields_;
    Base::Vector<EnumValueInfo> enumValues_;
    Base::Vector<EventInfo> events_;
    Base::Vector<MethodInfo> methods_;
    MemberId contentMember_ = InvalidMemberId;
};

constexpr TypeId MakeTypeId(Base::StringView xamlNamespace, Base::StringView name) noexcept { return Base::MakeMetaTypeId(xamlNamespace, name); }
constexpr Base::StringView AeroNamespaceUri() noexcept { return Base::DefaultMetadataNamespaceUri(); }
constexpr TypeId MakeTypeId(Base::StringView name) noexcept { return Base::MakeMetaTypeId(name); }

template<class T> struct MetaTypeTraits {
    static constexpr TypeId Id() noexcept { return T::StaticTypeId(); }
    static constexpr Base::StringView Namespace() noexcept { return T::StaticMetadataNamespace(); }
    static constexpr Base::StringView Name() noexcept { return T::StaticMetadataName(); }
    static constexpr TypeId BaseType() noexcept { return T::ParentClass::StaticTypeId(); }
};
template<class T> constexpr TypeId TypeOf() noexcept { return MetaTypeTraits<T>::Id(); }

constexpr MemberId MakeMemberId(TypeId ownerType, MemberKind kind, Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.MEMBER.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddByte(static_cast<std::uint8_t>(kind));
    builder.AddString(name);
    return builder.Finish();
}
AERO_API MemberId MakeMethodId(TypeId ownerType, Base::StringView name, Base::Span<const TypeId> parameterTypes) noexcept;

class AERO_API TypeRegistry final {
public:
    TypeRegistry() noexcept;
    ~TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    TypeRegistry(TypeRegistry&&) = delete;
    TypeRegistry& operator=(TypeRegistry&&) = delete;
    Base::Result<void> Freeze() noexcept;
    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t TypeCount() const noexcept { return types_.Size(); }
    Base::Span<const TypeInfo> Types() const noexcept { return {types_.Data(), types_.Size()}; }
    const TypeInfo* FindType(TypeId id) const noexcept;
    const TypeInfo* FindType(Base::StringView xamlNamespace, Base::StringView name) const noexcept;
    const PropertyInfo* FindProperty(MemberId id) const noexcept;
    const PropertyInfo* FindProperty(TypeId ownerType, Base::StringView name, bool includeBaseTypes = true) const noexcept;
    const FieldInfo* FindField(MemberId id) const noexcept;
    const FieldInfo* FindField(TypeId ownerType, Base::StringView name) const noexcept;
    const EnumValueInfo* FindEnumValue(MemberId id) const noexcept;
    const EnumValueInfo* FindEnumValue(TypeId ownerType, Base::StringView name) const noexcept;
    const EnumValueInfo* FindEnumValue(TypeId ownerType, std::uint64_t rawValue) const noexcept;
    const EventInfo* FindEvent(MemberId id) const noexcept;
    const EventInfo* FindEvent(TypeId ownerType, Base::StringView name, bool includeBaseTypes = true) const noexcept;
    const MethodInfo* FindMethod(MemberId id) const noexcept;
    const MethodInfo* FindMethod(TypeId ownerType, Base::StringView name, Base::Span<const TypeId> parameterTypes, bool includeBaseTypes = true) const noexcept;
    MemberId FindContentMember(TypeId type) const noexcept;
    bool IsDerivedFrom(TypeId type, TypeId expectedBase) const noexcept;
    bool Implements(TypeId type, TypeId interfaceType) const noexcept;
    bool IsAssignableFrom(TypeId targetType, TypeId sourceType) const noexcept;
    bool IsInstanceOf(const Base::Object& object, TypeId expectedType) const noexcept;
    template<class T> T* TryCast(Base::Object& object) const noexcept { return IsInstanceOf(object, TypeOf<T>()) ? static_cast<T*>(&object) : nullptr; }
    template<class T> const T* TryCast(const Base::Object& object) const noexcept { return IsInstanceOf(object, TypeOf<T>()) ? static_cast<const T*>(&object) : nullptr; }
    Base::Result<void> BuildSnapshot(Base::String& output) const noexcept;
    Base::Result<Base::HashCode> ComputeSnapshotHash() const noexcept;
private:
    friend class MetadataRegistrationTypes;
    Base::Result<TypeId> TryRegisterType(MetadataBehaviorRegistrationStore& behaviors, const TypeRegistration& registration) noexcept;
    Base::Result<void> TryRegisterInterface(TypeId ownerType, TypeId interfaceType) noexcept;
    Base::Result<MemberId> TryRegisterProperty(MetadataBehaviorRegistrationStore& behaviors, TypeId ownerType, const PropertyRegistration& registration) noexcept;
    Base::Result<MemberId> TryRegisterField(MetadataBehaviorRegistrationStore& behaviors, TypeId ownerType, const FieldRegistration& registration) noexcept;
    Base::Result<MemberId> TryRegisterEnumValue(TypeId ownerType, const EnumValueRegistration& registration) noexcept;
    Base::Result<MemberId> TryRegisterEvent(TypeId ownerType, const EventRegistration& registration) noexcept;
    Base::Result<MemberId> TryRegisterMethod(MetadataBehaviorRegistrationStore& behaviors, TypeId ownerType, const MethodRegistration& registration) noexcept;
    Base::Result<void> TrySetFactory(MetadataBehaviorRegistrationStore& behaviors, TypeId type, ObjectFactory factory) noexcept;
    Base::Result<void> TrySetContentMember(TypeId type, MemberId member) noexcept;
    struct MemberLocation final { std::uint32_t typeIndex = 0U; std::uint32_t memberIndex = 0U; MemberKind kind = MemberKind::Property; };
    Base::Vector<TypeInfo> types_;
    Base::HashMap<TypeId, std::uint32_t> typeIndex_;
    Base::HashMap<MemberId, MemberLocation> memberIndex_;
    bool frozen_ = false;
    TypeInfo* MutableType(TypeId id) noexcept;
    const TypeInfo* TypeAt(std::uint32_t index) const noexcept;
    const PropertyInfo* PropertyAt(const MemberLocation& location) const noexcept;
    const FieldInfo* FieldAt(const MemberLocation& location) const noexcept;
    const EnumValueInfo* EnumValueAt(const MemberLocation& location) const noexcept;
    const EventInfo* EventAt(const MemberLocation& location) const noexcept;
    const MethodInfo* MethodAt(const MemberLocation& location) const noexcept;
};

} // namespace Aero::Core

#define AERO_DECLARE_METADATA_NAMED(classType, parentType, metadataNamespace, metadataName) \
private: \
    inline static constexpr Aero::Core::TypeId StaticTypeIdValue_ = Aero::Core::MakeTypeId(Aero::Base::StringView(metadataNamespace), Aero::Base::StringView(metadataName)); \
public: \
    using SelfClass = classType; using ParentClass = parentType; \
    static constexpr Aero::Core::TypeId StaticTypeId() noexcept { return StaticTypeIdValue_; } \
    static constexpr Aero::Base::StringView StaticMetadataNamespace() noexcept { return Aero::Base::StringView(metadataNamespace); } \
    static constexpr Aero::Base::StringView StaticMetadataName() noexcept { return Aero::Base::StringView(metadataName); } \
    static Aero::Base::Result<void> TryRegisterMetadata(Aero::Core::MetaRegistrationContext& context) noexcept; \
private: \
    static void StaticFillMetadata(Aero::Core::MetaRegistrationBuilder& helper) noexcept;

#define AERO_DECLARE_METADATA(classType, parentType) \
private: \
    inline static constexpr Aero::Core::TypeId StaticTypeIdValue_ = Aero::Core::MakeTypeId(Aero::Base::StringView(#classType)); \
public: \
    using SelfClass = classType; using ParentClass = parentType; \
    static constexpr Aero::Core::TypeId StaticTypeId() noexcept { return StaticTypeIdValue_; } \
    static constexpr Aero::Base::StringView StaticMetadataNamespace() noexcept { return Aero::Core::AeroNamespaceUri(); } \
    static constexpr Aero::Base::StringView StaticMetadataName() noexcept { return Aero::Base::StringView(#classType); } \
    static Aero::Base::Result<void> TryRegisterMetadata(Aero::Core::MetaRegistrationContext& context) noexcept; \
private: \
    static void StaticFillMetadata(Aero::Core::MetaRegistrationBuilder& helper) noexcept;

#define AERO_DECLARE_TYPE_ID(typeName) \
    inline static constexpr Aero::Core::TypeId StaticTypeIdValue_ = Aero::Core::MakeTypeId(Aero::Base::StringView(#typeName)); \
    static constexpr Aero::Core::TypeId StaticTypeId() noexcept { return StaticTypeIdValue_; }
