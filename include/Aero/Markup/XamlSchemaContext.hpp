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
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/XamlNamesResources.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlSchemaContext;

using XamlValueKind = Core::ValueKind;
using XamlValue = Core::Value;

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
    bool IsValid() const noexcept {
        return id != Core::InvalidMemberId &&
            ownerType != Core::InvalidTypeId &&
            valueType != Core::InvalidTypeId;
    }
};

struct XamlServiceProvider final {
    const XamlSchemaContext* schema = nullptr;
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
using XamlHandlesMemberCallback = bool (*)(
    const XamlResolvedMember& member,
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
using XamlProvideValueCallback = Base::Result<XamlValue> (*)(
    Base::StringView arguments,
    const XamlServiceProvider& services,
    void* context) noexcept;

struct XamlMemberAdapterRegistration final {
    Core::MemberId member = Core::InvalidMemberId;
    XamlMemberWriteMode mode = XamlMemberWriteMode::SetOnce;
    XamlSetMemberCallback set = nullptr;
    void* context = nullptr;
    XamlSetMemberWithServicesCallback setWithServices = nullptr;
    // The adapter takes responsibility for validating the concrete XamlValue.
    // This is intentionally opt-in for members such as Setter.Value, whose
    // declared text type is needed for literals but which can also receive a
    // StaticResource object or x:Null.
    bool acceptsAnyValue = false;
};

// A member provider handles a family of properties from shared metadata. It is
// consulted only when a property has no exact member adapter, so control- or
// property-specific behavior can still override the generic path. This is the
// extensibility seam used by dependency properties and other custom property
// systems; they register one provider instead of one adapter per property.
struct XamlMemberProviderRegistration final {
    XamlHandlesMemberCallback handles = nullptr;
    XamlSetMemberWithServicesCallback set = nullptr;
    void* context = nullptr;
    XamlMemberWriteMode mode = XamlMemberWriteMode::SetOnce;
    bool acceptsAnyValue = false;
};

struct XamlMemberWritePolicy final {
    XamlMemberWriteMode mode = XamlMemberWriteMode::SetOnce;
    bool acceptsAnyValue = false;
    bool writable = false;
};

struct XamlTypeAdapterRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlInitializationCallback beginInit = nullptr;
    XamlInitializationCallback endInit = nullptr;
    XamlAbortInitializationCallback abortInit = nullptr;
    void* context = nullptr;
    bool createsNameScope = false;
    bool createsResourceScope = false;
    XamlRegisterNameCallback registerName = nullptr;
    XamlAddResourceCallback addResource = nullptr;
};

struct XamlMarkupExtensionRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlProvideValueCallback provideValue = nullptr;
    void* context = nullptr;
};

class AERO_API XamlSchemaContext final {
public:
    XamlSchemaContext(
        Core::MetadataDomain& domain,
        Core::MetadataRuntime& runtime) noexcept;

    XamlSchemaContext(const XamlSchemaContext&) = delete;
    XamlSchemaContext& operator=(const XamlSchemaContext&) = delete;

    Base::Result<void> TryRegisterMemberAdapter(
        const XamlMemberAdapterRegistration& registration) noexcept;
    Base::Result<void> TryRegisterMemberProvider(
        const XamlMemberProviderRegistration& registration) noexcept;
    Base::Result<void> TryRegisterTypeAdapter(
        const XamlTypeAdapterRegistration& registration) noexcept;
    Base::Result<void> TryRegisterMarkupExtension(
        const XamlMarkupExtensionRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    bool UsesRuntime() const noexcept { return runtime_ != nullptr; }
    const Core::MetadataDescriptorStore& Descriptors() const noexcept {
        return runtime_->Descriptors();
    }
    const Core::MetadataFacetStore& Facets() const noexcept {
        return runtime_->Facets();
    }
    Core::MetadataRuntime* Runtime() const noexcept { return runtime_; }
    const Core::MetadataDomain& Domain() const noexcept {
        return *domain_;
    }
    Base::Result<const Core::MetadataTypeDescriptor*> ResolveType(
        Base::StringView xamlNamespace,
        Base::StringView localName) const noexcept;
    Base::Result<XamlResolvedMember> ResolveMember(
        Core::TypeId targetType,
        const XamlQualifiedName& name,
        XamlMemberSyntax syntax) const noexcept;
    Base::Result<XamlResolvedMember> ResolveContentMember(
        Core::TypeId targetType) const noexcept;

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId type) const noexcept;
    // Internal activation seam used by the XAML object-writer translation unit.
    // Direct callers should normally use CreateObject().
    Base::Result<Base::Ref<Base::Object>> CreateObjectActivated(
        Core::TypeId type) const noexcept;
    Base::Result<XamlValue> ConvertText(
        Core::TypeId type,
        Base::StringView text) const noexcept;
    Base::Result<void> SetMember(
        Base::Object& object,
        Core::TypeId objectType,
        const XamlResolvedMember& member,
        const XamlValue& value,
        const XamlServiceProvider* services = nullptr) const noexcept;
    Base::Result<XamlValue> ProvideMarkupExtensionValue(
        Core::TypeId type,
        Base::StringView arguments,
        const XamlServiceProvider& services) const noexcept;

    Base::Result<void> BeginInit(
        Core::TypeId type,
        Base::Object& object) const noexcept;
    Base::Result<void> EndInit(
        Core::TypeId type,
        Base::Object& object) const noexcept;
    void AbortInit(Core::TypeId type, Base::Object& object) const noexcept;

    bool CreatesNameScope(Core::TypeId type) const noexcept;
    bool CreatesResourceScope(Core::TypeId type) const noexcept;
    Base::Result<void> RegisterName(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object) const noexcept;
    Base::Result<void> AddResource(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView key,
        Core::TypeId valueType,
        const Base::Ref<Base::Object>& value) const noexcept;

    const XamlMemberAdapterRegistration* FindMemberAdapter(
        Core::MemberId member) const noexcept;
    XamlMemberWritePolicy ResolveMemberWritePolicy(
        const XamlResolvedMember& member) const noexcept;
    const XamlTypeAdapterRegistration* FindTypeAdapter(
        Core::TypeId type) const noexcept;
    const XamlMarkupExtensionRegistration*
    FindMarkupExtension(Core::TypeId type) const noexcept;

private:
    Core::MetadataDomain* domain_ = nullptr;
    Core::MetadataRuntime* runtime_ = nullptr;
    Base::Vector<XamlMemberAdapterRegistration> memberAdapters_;
    Base::Vector<XamlMemberProviderRegistration> memberProviders_;
    Base::Vector<XamlTypeAdapterRegistration> typeAdapters_;
    Base::Vector<XamlMarkupExtensionRegistration> markupExtensions_;
    bool frozen_ = false;

    const XamlMemberProviderRegistration* FindMemberProvider(
        const XamlResolvedMember& member) const noexcept;
    const XamlTypeAdapterRegistration* FindTypeAdapterExact(
        Core::TypeId type) const noexcept;
    Base::Result<XamlResolvedMember> ResolvePropertyOrEventRuntime(
        Core::TypeId targetType,
        Core::TypeId ownerType,
        Base::StringView memberName,
        XamlMemberSyntax syntax,
        bool ownerWasExplicit) const noexcept;
};

} // namespace Aero::Markup
