#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/Extensions/XamlExtensionContext.hpp>
#include <Aero/Markup/Parsing/XamlNode.hpp>
#include <Aero/Markup/Schema/XamlResolvedMember.hpp>
#include <Aero/Markup/Schema/XamlFacetStore.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlSchemaContext;

struct XamlDeferredContentEdge final {
    Base::Object* owner = nullptr;
    Base::Object* parent = nullptr;
    Base::Ref<Base::Object> child;
    Core::ContentWriteCallback write = nullptr;
    Core::ContentClearCallback clear = nullptr;
    void* contentContext = nullptr;
};

class XamlDeferredContentPlan final {
public:
    Base::Result<void> Stage(
        Base::Object& owner,
        Base::Object& parent,
        const Base::Ref<Base::Object>& child,
        Core::ContentWriteCallback write,
        Core::ContentClearCallback clear,
        void* contentContext) noexcept;
    Base::Result<void> CopyForOwner(
        const Base::Object& owner,
        Base::Vector<XamlDeferredContentEdge>& output) const noexcept;
    void ReleaseOwner(Base::Object& owner) noexcept;
    void ReleaseAll() noexcept;
    bool Empty() const noexcept {
        return edges_.Empty();
    }

private:
    Base::Vector<XamlDeferredContentEdge> edges_;
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

    Base::Result<void> TryAddFacet(
        const XamlMemberFacet& facet) noexcept {
        return TryRegisterMemberAdapter(facet);
    }
    Base::Result<void> TryAddFacet(
        const XamlMemberProviderFacet& facet) noexcept {
        return TryRegisterMemberProvider(facet);
    }
    Base::Result<void> TryAddFacet(
        const XamlTypeFacet& facet) noexcept {
        return TryRegisterTypeAdapter(facet);
    }
    Base::Result<void> TryAddFacet(
        const XamlMarkupExtensionFacet& facet) noexcept {
        return TryRegisterMarkupExtension(facet);
    }

    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    bool UsesRuntime() const noexcept { return runtime_ != nullptr; }
    const Core::MetadataDescriptorStore& Descriptors() const noexcept {
        return runtime_->Descriptors();
    }
    const Core::MetadataFacetStore& Facets() const noexcept {
        return runtime_->Facets();
    }
    const XamlFacetStore& XamlFacets() const noexcept {
        return xamlFacets_;
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
    Base::Result<Core::DependencyObject*> ResolvePropertyTarget(
        Base::Object& object) const noexcept;
    Base::Result<XamlValue> ConvertText(
        Core::TypeId type,
        Base::StringView text) const noexcept;
    Base::Result<void> SetMember(
        Base::Object& object,
        Core::TypeId objectType,
        const XamlResolvedMember& member,
        const XamlValue& value,
        const XamlServiceProvider* services = nullptr) const noexcept;
    Base::Result<XamlProvidedValue> ProvideMarkupExtensionValue(
        Core::TypeId type,
        Base::StringView arguments,
        const XamlServiceProvider& services) const noexcept;

    Base::Result<void> BeginInit(
        Core::TypeId type,
        Base::Object& object) const noexcept;
    Base::Result<void> EndInit(
        Core::TypeId type,
        Base::Object& object,
        const XamlServiceProvider& services) const noexcept;
    void AbortInit(Core::TypeId type, Base::Object& object) const noexcept;

    bool CreatesNameScope(Core::TypeId type) const noexcept;
    bool CreatesResourceScope(Core::TypeId type) const noexcept;
    bool DefersVisualContent(Core::TypeId type) const noexcept;
    Base::Result<void> RegisterName(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object) const noexcept;
    Base::Result<void> AddResource(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView key,
        const XamlValue& value) const noexcept;
    ResourceDictionary* ResolveResourceScope(
        Core::TypeId scopeType,
        Base::Object& scopeOwner) const noexcept;
    Base::Result<ResourceKey> ResolveImplicitResourceKey(
        Core::TypeId type,
        const Base::Object& object) const noexcept;

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
    XamlFacetStore xamlFacets_;
    bool frozen_ = false;

    const XamlMemberProviderRegistration* FindMemberProvider(
        const XamlResolvedMember& member) const noexcept;
    Base::Result<XamlResolvedMember> ResolvePropertyOrEventRuntime(
        Core::TypeId targetType,
        Core::TypeId ownerType,
        Base::StringView memberName,
        XamlMemberSyntax syntax,
        bool ownerWasExplicit) const noexcept;
};

} // namespace Aero::Markup
