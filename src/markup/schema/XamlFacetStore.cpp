#include <Aero/Markup/Schema/XamlFacetStore.hpp>

#include <cstdint>

namespace Aero::Markup {
namespace {

bool HasTypeFlag(
    Core::TypeFlags value,
    Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status FrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML facet store is frozen");
}

} // namespace

Base::Result<void> XamlFacetStore::TryAdd(
    const XamlMemberFacet& facet,
    const Core::MetadataDescriptorStore& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.member == Core::InvalidMemberId ||
        (facet.set == nullptr && facet.setWithServices == nullptr) ||
        descriptors.FindProperty(facet.member) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member facet is invalid");
    }
    if (FindMember(facet.member) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML member facet is already registered");
    }
    return members_.TryPushBack(facet);
}

Base::Result<void> XamlFacetStore::TryAdd(
    const XamlMemberProviderFacet& facet) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.handles == nullptr || facet.set == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member-provider facet is invalid");
    }
    for (const XamlMemberProviderFacet& current : memberProviders_) {
        if (current.handles == facet.handles &&
            current.set == facet.set &&
            current.context == facet.context) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML member-provider facet is already registered");
        }
    }
    return memberProviders_.TryPushBack(facet);
}

Base::Result<void> XamlFacetStore::TryAdd(
    const XamlTypeFacet& facet,
    const Core::MetadataDescriptorStore& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    const Core::MetadataTypeDescriptor* type =
        descriptors.FindType(facet.type);
    if (type == nullptr ||
        HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML type facet is invalid");
    }
    if (FindTypeExact(facet.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML type facet is already registered");
    }
    return types_.TryPushBack(facet);
}

Base::Result<void> XamlFacetStore::TryAdd(
    const XamlMarkupExtensionFacet& facet,
    const Core::MetadataDescriptorStore& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    const Core::MetadataTypeDescriptor* type =
        descriptors.FindType(facet.type);
    if (type == nullptr || facet.provideValue == nullptr ||
        !HasTypeFlag(type->Flags(), Core::TypeFlags::MarkupExtension)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML markup-extension facet requires a flagged type and provider");
    }
    if (FindMarkupExtension(facet.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML markup-extension facet is already registered");
    }
    return markupExtensions_.TryPushBack(facet);
}

Base::Result<void> XamlFacetStore::Freeze() noexcept {
    frozen_ = true;
    return {};
}

const XamlMemberFacet* XamlFacetStore::FindMember(
    Core::MemberId member) const noexcept {
    for (const XamlMemberFacet& facet : members_) {
        if (facet.member == member) return &facet;
    }
    return nullptr;
}

const XamlMemberProviderFacet* XamlFacetStore::FindMemberProvider(
    const XamlResolvedMember& member) const noexcept {
    if (!member.IsValid()) return nullptr;
    for (const XamlMemberProviderFacet& facet : memberProviders_) {
        if (facet.handles != nullptr &&
            facet.handles(member, facet.context)) {
            return &facet;
        }
    }
    return nullptr;
}

const XamlTypeFacet* XamlFacetStore::FindType(
    Core::TypeId type,
    const Core::MetadataDescriptorStore& descriptors) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        const XamlTypeFacet* facet = FindTypeExact(current);
        if (facet != nullptr) return facet;
        const Core::MetadataTypeDescriptor* descriptor =
            descriptors.FindType(current);
        if (descriptor == nullptr) break;
        current = descriptor->BaseType();
    }
    return nullptr;
}

const XamlMarkupExtensionFacet* XamlFacetStore::FindMarkupExtension(
    Core::TypeId type) const noexcept {
    for (const XamlMarkupExtensionFacet& facet : markupExtensions_) {
        if (facet.type == type) return &facet;
    }
    return nullptr;
}

const XamlTypeFacet* XamlFacetStore::FindTypeExact(
    Core::TypeId type) const noexcept {
    for (const XamlTypeFacet& facet : types_) {
        if (facet.type == type) return &facet;
    }
    return nullptr;
}

} // namespace Aero::Markup
