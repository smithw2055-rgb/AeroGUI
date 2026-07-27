#pragma once

#include <Aero/Core/Metadata/MetadataDescriptors.hpp>

#include <cstdint>

namespace Aero::Markup {

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

} // namespace Aero::Markup
