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

struct MetaRegistrationContext;
class MetaRegistrationBuilder;

inline constexpr std::uint32_t TypeIdAlgorithmVersion = 1U;
inline constexpr std::uint32_t RegistrySnapshotFormatVersion = 2U;

enum class MemberKind : std::uint8_t {
    Property = 1U,
    Event = 2U,
    Method = 3U
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
    AffectsRender = 1U << 5U,
    AffectsParentMeasure = 1U << 6U,
    AffectsParentArrange = 1U << 7U,
    Structural = 1U << 8U,
    Collection = 1U << 9U,
    WriteOnly = 1U << 10U
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

constexpr PropertyFlags operator|(
    PropertyFlags left, PropertyFlags right) noexcept {
    return static_cast<PropertyFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr EventFlags operator|(
    EventFlags left, EventFlags right) noexcept {
    return static_cast<EventFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
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
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
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
    PropertyAccessKind Access() const noexcept { return access_; }
    PropertyGetCallback Getter() const noexcept { return get_; }
    PropertySetCallback Setter() const noexcept { return set_; }
    PropertyProviderId Provider() const noexcept {
        return provider_;
    }
    void* Context() const noexcept { return context_; }
    Base::StringView Name() const noexcept { return name_.View(); }

private:
    friend class TypeRegistry;

    PropertyInfo() noexcept = default;

    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId valueType_ = InvalidTypeId;
    PropertyFlags flags_ = PropertyFlags::None;
    PropertyAccessKind access_ = PropertyAccessKind::External;
    PropertyGetCallback get_ = nullptr;
    PropertySetCallback set_ = nullptr;
    PropertyProviderId provider_ = InvalidPropertyProviderId;
    void* context_ = nullptr;
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
    MethodInvokeCallback Invoker() const noexcept { return invoke_; }
    void* Context() const noexcept { return context_; }

private:
    friend class TypeRegistry;
    MethodInfo() noexcept = default;
    MemberId id_ = InvalidMemberId;
    TypeId ownerType_ = InvalidTypeId;
    TypeId returnType_ = InvalidTypeId;
    MethodFlags flags_ = MethodFlags::None;
    MethodInvokeCallback invoke_ = nullptr;
    void* context_ = nullptr;
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
    TypeFlags Flags() const noexcept { return flags_; }
    ObjectFactory Factory() const noexcept { return factory_; }
    Base::StringView XamlNamespace() const noexcept {
        return xamlNamespace_.View();
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Span<const PropertyInfo> Properties() const noexcept {
        return {properties_.Data(), properties_.Size()};
    }
    Base::Span<const EventInfo> Events() const noexcept {
        return {events_.Data(), events_.Size()};
    }
    Base::Span<const MethodInfo> Methods() const noexcept {
        return {methods_.Data(), methods_.Size()};
    }
    MemberId ContentMember() const noexcept {
        return contentMember_;
    }

private:
    friend class TypeRegistry;

    TypeInfo() noexcept = default;

    TypeId id_ = InvalidTypeId;
    TypeId baseType_ = InvalidTypeId;
    TypeFlags flags_ = TypeFlags::None;
    ObjectFactory factory_ = nullptr;
    Base::String xamlNamespace_;
    Base::String name_;
    Base::Vector<PropertyInfo> properties_;
    Base::Vector<EventInfo> events_;
    Base::Vector<MethodInfo> methods_;
    MemberId contentMember_ = InvalidMemberId;
};

constexpr TypeId MakeTypeId(
    Base::StringView xamlNamespace,
    Base::StringView name) noexcept {
    return Base::MakeMetaTypeId(xamlNamespace, name);
}

constexpr Base::StringView AeroNamespaceUri() noexcept {
    return Base::DefaultMetadataNamespaceUri();
}

constexpr TypeId MakeTypeId(Base::StringView name) noexcept {
    return Base::MakeMetaTypeId(name);
}

constexpr MemberId MakeMemberId(
    TypeId ownerType,
    MemberKind kind,
    Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.MEMBER.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddByte(static_cast<std::uint8_t>(kind));
    builder.AddString(name);
    return builder.Finish();
}

AERO_API MemberId MakeMethodId(
    TypeId ownerType,
    Base::StringView name,
    Base::Span<const TypeId> parameterTypes) noexcept;

class AERO_API TypeRegistry final {
public:
    TypeRegistry() noexcept;
    ~TypeRegistry() = default;

    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    TypeRegistry(TypeRegistry&&) = delete;
    TypeRegistry& operator=(TypeRegistry&&) = delete;

    Base::Result<TypeId> TryRegisterType(
        const TypeRegistration& registration) noexcept;
    Base::Result<MemberId> TryRegisterProperty(
        TypeId ownerType,
        const PropertyRegistration& registration) noexcept;
    Base::Result<MemberId> TryRegisterEvent(
        TypeId ownerType,
        const EventRegistration& registration) noexcept;
    Base::Result<MemberId> TryRegisterMethod(
        TypeId ownerType,
        const MethodRegistration& registration) noexcept;
    Base::Result<void> TrySetFactory(
        TypeId type,
        ObjectFactory factory) noexcept;
    Base::Result<void> TrySetContentMember(
        TypeId type,
        MemberId member) noexcept;
    Base::Result<void> TryRegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) noexcept;
    Base::Result<void> TryRegisterTextConverter(
        const TextValueConverterRegistration& registration) noexcept;
    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept;
    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept;

    // Seal-time export surface used to transfer runtime behavior into typed
    // facets. Returned registrations remain immutable after Freeze().
    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept;
    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept;

    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t TypeCount() const noexcept {
        return types_.Size();
    }
    Base::Span<const TypeInfo> Types() const noexcept {
        return {types_.Data(), types_.Size()};
    }

    const TypeInfo* FindType(TypeId id) const noexcept;
    const TypeInfo* FindType(
        Base::StringView xamlNamespace,
        Base::StringView name) const noexcept;

    const PropertyInfo* FindProperty(MemberId id) const noexcept;
    const PropertyInfo* FindProperty(
        TypeId ownerType,
        Base::StringView name,
        bool includeBaseTypes = true) const noexcept;

    const EventInfo* FindEvent(MemberId id) const noexcept;
    const EventInfo* FindEvent(
        TypeId ownerType,
        Base::StringView name,
        bool includeBaseTypes = true) const noexcept;

    const MethodInfo* FindMethod(MemberId id) const noexcept;
    const MethodInfo* FindMethod(
        TypeId ownerType,
        Base::StringView name,
        Base::Span<const TypeId> parameterTypes,
        bool includeBaseTypes = true) const noexcept;
    MemberId FindContentMember(TypeId type) const noexcept;

    bool IsDerivedFrom(
        TypeId type,
        TypeId expectedBase) const noexcept;
    bool IsInstanceOf(
        const Base::Object& object,
        TypeId expectedType) const noexcept;

    template<class T>
    T* TryCast(Base::Object& object) const noexcept {
        return IsInstanceOf(object, T::StaticTypeId())
            ? static_cast<T*>(&object) : nullptr;
    }

    template<class T>
    const T* TryCast(const Base::Object& object) const noexcept {
        return IsInstanceOf(object, T::StaticTypeId())
            ? static_cast<const T*>(&object) : nullptr;
    }

    Base::Result<void> BuildSnapshot(
        Base::String& output) const noexcept;
    Base::Result<Base::HashCode> ComputeSnapshotHash()
        const noexcept;

private:
    struct MemberLocation final {
        std::uint32_t typeIndex = 0U;
        std::uint32_t memberIndex = 0U;
        MemberKind kind = MemberKind::Property;
    };

    Base::Vector<TypeInfo> types_;
    Base::HashMap<TypeId, std::uint32_t> typeIndex_;
    Base::HashMap<MemberId, MemberLocation> memberIndex_;
    struct ValueSemanticsEntry final {
        TypeId type = InvalidTypeId;
        Base::Ref<ValueTypeSemantics> semantics;
    };
    Base::Vector<ValueSemanticsEntry> valueSemantics_;
    Base::Vector<TextValueConverterRegistration> textConverters_;
    bool frozen_ = false;

    TypeInfo* MutableType(TypeId id) noexcept;
    const TypeInfo* TypeAt(std::uint32_t index) const noexcept;
    const PropertyInfo* PropertyAt(
        const MemberLocation& location) const noexcept;
    const EventInfo* EventAt(
        const MemberLocation& location) const noexcept;
    const MethodInfo* MethodAt(
        const MemberLocation& location) const noexcept;
};

inline const Base::Ref<ValueTypeSemantics>*
TypeRegistry::FindValueSemantics(TypeId type) const noexcept {
    for (const ValueSemanticsEntry& entry : valueSemantics_) {
        if (entry.type == type) return &entry.semantics;
    }
    return nullptr;
}

inline const TextValueConverterRegistration*
TypeRegistry::FindTextConverter(TypeId type) const noexcept {
    for (const TextValueConverterRegistration& entry : textConverters_) {
        if (entry.type == type) return &entry;
    }
    return nullptr;
}

} // namespace Aero::Core

#define AERO_DECLARE_METADATA_NAMED( \
    classType, parentType, metadataNamespace, metadataName) \
private: \
    inline static constexpr Aero::Core::TypeId StaticTypeIdValue_ = \
        Aero::Core::MakeTypeId( \
            Aero::Base::StringView(metadataNamespace), \
            Aero::Base::StringView(metadataName)); \
public: \
    static constexpr Aero::Core::TypeId StaticTypeId() noexcept { \
        return StaticTypeIdValue_; \
    } \
    static Aero::Base::Result<void> TryRegisterMetadata( \
        Aero::Core::MetaRegistrationContext& context) noexcept; \
private: \
    using SelfClass = classType; \
    using ParentClass = parentType; \
    static constexpr Aero::Base::StringView StaticMetadataNamespace() noexcept { \
        return Aero::Base::StringView(metadataNamespace); \
    } \
    static constexpr Aero::Base::StringView StaticMetadataName() noexcept { \
        return Aero::Base::StringView(metadataName); \
    } \
    static void StaticFillMetadata( \
        Aero::Core::MetaRegistrationBuilder& helper) noexcept;

#define AERO_DECLARE_METADATA(classType, parentType) \
private: \
    inline static constexpr Aero::Core::TypeId StaticTypeIdValue_ = \
        Aero::Core::MakeTypeId(Aero::Base::StringView(#classType)); \
public: \
    static constexpr Aero::Core::TypeId StaticTypeId() noexcept { \
        return StaticTypeIdValue_; \
    } \
    static Aero::Base::Result<void> TryRegisterMetadata( \
        Aero::Core::MetaRegistrationContext& context) noexcept; \
private: \
    using SelfClass = classType; \
    using ParentClass = parentType; \
    static constexpr Aero::Base::StringView StaticMetadataNamespace() noexcept { \
        return Aero::Core::AeroNamespaceUri(); \
    } \
    static constexpr Aero::Base::StringView StaticMetadataName() noexcept { \
        return Aero::Base::StringView(#classType); \
    } \
    static void StaticFillMetadata( \
        Aero::Core::MetaRegistrationBuilder& helper) noexcept;

#define AERO_DECLARE_TYPE_ID(typeName) \
    inline static constexpr Aero::Core::TypeId StaticTypeIdValue_ = \
        Aero::Core::MakeTypeId(Aero::Base::StringView(#typeName)); \
    static constexpr Aero::Core::TypeId StaticTypeId() noexcept { \
        return StaticTypeIdValue_; \
    }
