#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlNamesResources.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>

#include <cstdint>

namespace Aero::Markup {

enum class XamlValueKind : std::uint8_t {
    None = 0U,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Double,
    String,
    Object
};

class AERO_API XamlValue final {
public:
    explicit XamlValue(Base::IAllocator* allocator = nullptr) noexcept
        : string_(allocator) {}

    XamlValue(XamlValue&&) noexcept = default;
    XamlValue& operator=(XamlValue&&) noexcept = default;

    XamlValue(const XamlValue&) = delete;
    XamlValue& operator=(const XamlValue&) = delete;

    AERO_NODISCARD static XamlValue FromBoolean(
        Core::TypeId type,
        bool value,
        Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD static XamlValue FromSignedInteger(
        Core::TypeId type,
        std::int64_t value,
        Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD static XamlValue FromUnsignedInteger(
        Core::TypeId type,
        std::uint64_t value,
        Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD static XamlValue FromDouble(
        Core::TypeId type,
        double value,
        Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD static Base::Result<XamlValue> TryFromString(
        Core::TypeId type,
        Base::StringView value,
        Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD static XamlValue FromObject(
        Core::TypeId type,
        Base::Ref<Base::Object> value,
        Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD static XamlValue NullObject(
        Core::TypeId type,
        Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD XamlValueKind Kind() const noexcept { return kind_; }
    AERO_NODISCARD Core::TypeId Type() const noexcept { return type_; }
    AERO_NODISCARD bool IsNullObject() const noexcept {
        return kind_ == XamlValueKind::Object && !object_;
    }
    AERO_NODISCARD bool AsBoolean() const noexcept;
    AERO_NODISCARD std::int64_t AsSignedInteger() const noexcept;
    AERO_NODISCARD std::uint64_t AsUnsignedInteger() const noexcept;
    AERO_NODISCARD double AsDouble() const noexcept;
    AERO_NODISCARD Base::StringView AsString() const noexcept;
    AERO_NODISCARD const Base::Ref<Base::Object>& AsObject() const noexcept;

private:
    union Scalar final {
        Scalar() noexcept : unsignedInteger(0U) {}

        bool boolean;
        std::int64_t signedInteger;
        std::uint64_t unsignedInteger;
        double floatingPoint;
    } scalar_;

    Core::TypeId type_ = Core::InvalidTypeId;
    XamlValueKind kind_ = XamlValueKind::None;
    Base::String string_;
    Base::Ref<Base::Object> object_;
};

enum class XamlScalarKind : std::uint8_t {
    String = 0U,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Double
};

enum class XamlMemberSyntax : std::uint8_t {
    Attribute = 0U,
    PropertyElement,
    Content
};

enum class XamlMemberWriteMode : std::uint8_t {
    SetOnce = 0U,
    Collection
};

struct XamlResolvedMember final {
    Core::MemberId id = Core::InvalidMemberId;
    Core::MemberKind kind = Core::MemberKind::Property;
    Core::TypeId ownerType = Core::InvalidTypeId;
    Core::TypeId valueType = Core::InvalidTypeId;
    Core::PropertyFlags propertyFlags = Core::PropertyFlags::None;
    Core::EventFlags eventFlags = Core::EventFlags::None;
    bool attached = false;

    AERO_NODISCARD bool IsValid() const noexcept {
        return id != Core::InvalidMemberId &&
            ownerType != Core::InvalidTypeId &&
            valueType != Core::InvalidTypeId;
    }
};

struct XamlServiceProvider final {
    Base::Object* targetObject = nullptr;
    Core::TypeId targetObjectType = Core::InvalidTypeId;
    Core::MemberId targetMember = Core::InvalidMemberId;
    Core::TypeId targetValueType = Core::InvalidTypeId;
    Base::Object* rootObject = nullptr;
    Core::SourceSpan source;
    const NameScope* nameScope = nullptr;
    XamlNamespaceScope namespaces;
    XamlResourceResolver resources;
};

using XamlSetMemberCallback = Base::Result<void> (*)(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept;
using XamlSetMemberWithServicesCallback = Base::Result<void> (*)(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept;
using XamlInitializationCallback = Base::Result<void> (*)(
    Base::Object& object,
    void* context) noexcept;
using XamlAbortInitializationCallback = void (*)(
    Base::Object& object,
    void* context) noexcept;
using XamlRegisterNameCallback = Base::Result<void> (*)(
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object,
    void* context) noexcept;
using XamlAddResourceCallback = Base::Result<void> (*)(
    Base::Object& scopeOwner,
    Base::StringView key,
    Core::TypeId valueType,
    const Base::Ref<Base::Object>& value,
    void* context) noexcept;

struct XamlMemberAdapterRegistration final {
    Core::MemberId member = Core::InvalidMemberId;
    XamlMemberWriteMode mode = XamlMemberWriteMode::SetOnce;
    XamlSetMemberCallback set = nullptr;
    void* context = nullptr;
    XamlSetMemberWithServicesCallback setWithServices = nullptr;
};

struct XamlTypeAdapterRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    Core::MemberId contentMember = Core::InvalidMemberId;
    XamlInitializationCallback beginInit = nullptr;
    XamlInitializationCallback endInit = nullptr;
    XamlAbortInitializationCallback abortInit = nullptr;
    void* context = nullptr;
    bool createsNameScope = false;
    bool createsResourceScope = false;
    XamlRegisterNameCallback registerName = nullptr;
    XamlAddResourceCallback addResource = nullptr;
};

class AERO_API XamlSchemaContext final {
public:
    explicit XamlSchemaContext(
        Core::TypeRegistry& types,
        Base::IAllocator* allocator = nullptr) noexcept;

    XamlSchemaContext(const XamlSchemaContext&) = delete;
    XamlSchemaContext& operator=(const XamlSchemaContext&) = delete;

    AERO_NODISCARD Base::Result<void> TryRegisterScalarType(
        Core::TypeId type,
        XamlScalarKind kind) noexcept;
    AERO_NODISCARD Base::Result<void> TryRegisterMemberAdapter(
        const XamlMemberAdapterRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<void> TryRegisterTypeAdapter(
        const XamlTypeAdapterRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<void> Freeze() noexcept;

    AERO_NODISCARD bool IsFrozen() const noexcept { return frozen_; }
    AERO_NODISCARD Core::TypeRegistry& Types() const noexcept { return *types_; }
    AERO_NODISCARD Base::IAllocator& Allocator() const noexcept {
        return *allocator_;
    }

    AERO_NODISCARD Base::Result<const Core::TypeInfo*> ResolveType(
        Base::StringView xamlNamespace,
        Base::StringView localName) const noexcept;
    AERO_NODISCARD Base::Result<XamlResolvedMember> ResolveMember(
        Core::TypeId targetType,
        const XamlQualifiedName& name,
        XamlMemberSyntax syntax) const noexcept;
    AERO_NODISCARD Base::Result<XamlResolvedMember> ResolveContentMember(
        Core::TypeId targetType) const noexcept;

    AERO_NODISCARD Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId type) const noexcept;
    AERO_NODISCARD Base::Result<XamlValue> ConvertText(
        Core::TypeId type,
        Base::StringView text) const noexcept;
    AERO_NODISCARD Base::Result<void> SetMember(
        Base::Object& object,
        Core::TypeId objectType,
        const XamlResolvedMember& member,
        const XamlValue& value,
        const XamlServiceProvider* services = nullptr) const noexcept;

    AERO_NODISCARD Base::Result<void> BeginInit(
        Core::TypeId type,
        Base::Object& object) const noexcept;
    AERO_NODISCARD Base::Result<void> EndInit(
        Core::TypeId type,
        Base::Object& object) const noexcept;
    void AbortInit(Core::TypeId type, Base::Object& object) const noexcept;

    AERO_NODISCARD bool CreatesNameScope(Core::TypeId type) const noexcept;
    AERO_NODISCARD bool CreatesResourceScope(Core::TypeId type) const noexcept;
    AERO_NODISCARD Base::Result<void> RegisterName(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object) const noexcept;
    AERO_NODISCARD Base::Result<void> AddResource(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView key,
        Core::TypeId valueType,
        const Base::Ref<Base::Object>& value) const noexcept;

    AERO_NODISCARD const XamlMemberAdapterRegistration* FindMemberAdapter(
        Core::MemberId member) const noexcept;
    AERO_NODISCARD const XamlTypeAdapterRegistration* FindTypeAdapter(
        Core::TypeId type) const noexcept;

private:
    struct ScalarRegistration final {
        Core::TypeId type = Core::InvalidTypeId;
        XamlScalarKind kind = XamlScalarKind::String;
    };

    Core::TypeRegistry* types_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<ScalarRegistration> scalarTypes_;
    Base::Vector<XamlMemberAdapterRegistration> memberAdapters_;
    Base::Vector<XamlTypeAdapterRegistration> typeAdapters_;
    bool frozen_ = false;

    AERO_NODISCARD const ScalarRegistration* FindScalarType(
        Core::TypeId type) const noexcept;
    AERO_NODISCARD const XamlTypeAdapterRegistration* FindTypeAdapterExact(
        Core::TypeId type) const noexcept;
    AERO_NODISCARD Base::Result<XamlResolvedMember> ResolvePropertyOrEvent(
        Core::TypeId targetType,
        Core::TypeId ownerType,
        Base::StringView memberName,
        XamlMemberSyntax syntax,
        bool ownerWasExplicit) const noexcept;
};

} // namespace Aero::Markup
