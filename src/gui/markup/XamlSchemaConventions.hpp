#pragma once

#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Meta.hpp>
#include "gui/markup/XamlSchema.hpp"

namespace Aero::Markup {

inline constexpr Base::StringView WpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
inline constexpr Base::StringView BehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
inline constexpr Base::StringView BlendInteractivityNamespace(
    "http://schemas.microsoft.com/expression/2010/interactivity");
inline constexpr Base::StringView SystemNamespacePrefix(
    "clr-namespace:System");

inline bool MatchesClrNamespacePrefix(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

inline bool IsExtensionsClrNamespace(Base::StringView value) noexcept {
    if (value.SizeBytes() < 31 ||
        !MatchesClrNamespacePrefix(value, Base::StringView("clr-namespace:"))) {
        return false;
    }
    return MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:AeroGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Aero.GUI.Extensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:NoesisGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Noesis.GUI.Extensions"));
}

inline Base::StringView CanonicalXamlNamespace(
    Base::StringView value) noexcept {
    if (value.SizeBytes() < 31) {
        return value;
    }
    return value == WpfPresentationNamespace ||
            value == BehaviorsNamespace ||
            value == BlendInteractivityNamespace ||
            IsExtensionsClrNamespace(value)
        ? Meta::AeroNamespaceUri()
        : value;
}

inline bool IsSystemNamespace(
    Base::StringView value) noexcept {
    return value.SizeBytes() >=
            SystemNamespacePrefix.SizeBytes() &&
        value.Substr(0U, SystemNamespacePrefix.SizeBytes()) ==
            SystemNamespacePrefix;
}

inline Base::StringView CanonicalXamlTypeName(
    Base::StringView value) noexcept {
    switch (value.SizeBytes()) {
    case 8:
        if (value == Base::StringView("Geometry")) {
            return Base::StringView("StreamGeometry");
        }
        break;
    case 10:
        if (value == Base::StringView("WavesBrush")) {
            return Base::StringView("WavesShader");
        }
        break;
    case 15:
        if (value == Base::StringView("MonochromeBrush")) {
            return Base::StringView("MonochromeShader");
        }
        break;
    case 18:
        if (value == Base::StringView("ConicGradientBrush")) {
            return Base::StringView("ConicGradientShader");
        }
        break;
    case 21:
        if (value == Base::StringView("VisualStateTransition")) {
            return Base::StringView("VisualTransition");
        }
        break;
    default:
        break;
    }
    return value;
}

inline bool IsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    if (!IsExtensionsClrNamespace(xamlNamespace)) {
        return false;
    }
    switch (ownerName.SizeBytes()) {
    case 4:
        return ownerName == Base::StringView("Text") ||
               ownerName == Base::StringView("Path");
    case 5:
        return ownerName == Base::StringView("Brush");
    case 7:
        return ownerName == Base::StringView("Element");
    case 8:
        return ownerName == Base::StringView("RichText");
    default:
        return false;
    }
}

struct ParsedXamlMemberName {
    Base::StringView ownerName;
    Base::StringView memberName;
    Base::StringView ownerNamespace;
    bool hasOwner = false;
    bool isAliasedToBlendMode = false;
};

inline Base::Result<ParsedXamlMemberName> ParseXamlMemberName(
    Base::StringView localName,
    Base::StringView namespaceUri,
    Base::StringView targetXamlNamespace) noexcept {
    if (localName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member name cannot be empty");
    }
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML member contains multiple owner separators");
        }
        dot = index;
    }

    ParsedXamlMemberName result;
    if (dot == localName.SizeBytes()) {
        result.hasOwner = false;
        result.memberName = localName;
        return result;
    }

    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member owner syntax is invalid");
    }

    result.hasOwner = true;
    result.ownerName = localName.Substr(0U, dot);
    result.memberName = localName.Substr(dot + 1U, localName.SizeBytes() - dot - 1U);
    result.ownerNamespace = namespaceUri.Empty()
        ? targetXamlNamespace : namespaceUri;

    if (IsAeroExtensionsFacade(result.ownerNamespace, result.ownerName)) {
        if (result.ownerName == Base::StringView("Element") &&
            result.memberName == Base::StringView("BlendingMode")) {
            result.hasOwner = false;
            result.memberName = Base::StringView("BlendMode");
            result.isAliasedToBlendMode = true;
        }
    }

    return result;
}

inline bool HasPropertyFlag(
    Meta::PropertyFlags flags,
    Meta::PropertyFlags test) noexcept {
    return (static_cast<std::uint32_t>(flags) &
            static_cast<std::uint32_t>(test)) != 0U;
}

inline bool HasEventFlag(
    Meta::EventFlags flags,
    Meta::EventFlags test) noexcept {
    return (static_cast<std::uint32_t>(flags) &
            static_cast<std::uint32_t>(test)) != 0U;
}

inline ResolvedMember MakeResolvedPropertyMember(
    Meta::MemberId id,
    Meta::TypeId ownerType,
    Meta::TypeId valueType,
    Meta::PropertyFlags flags) noexcept {
    ResolvedMember resolved;
    resolved.id = id;
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = ownerType;
    resolved.valueType = valueType;
    resolved.propertyFlags = flags;
    resolved.attached = HasPropertyFlag(flags, Meta::PropertyFlags::Attached);
    return resolved;
}

inline ResolvedMember MakeResolvedEventMember(
    Meta::MemberId id,
    Meta::TypeId ownerType,
    Meta::TypeId eventArgsType,
    Meta::EventFlags flags) noexcept {
    ResolvedMember resolved;
    resolved.id = id;
    resolved.kind = Meta::MemberKind::Event;
    resolved.ownerType = ownerType;
    resolved.valueType = eventArgsType;
    resolved.eventFlags = flags;
    resolved.attached = HasEventFlag(flags, Meta::EventFlags::Attached);
    return resolved;
}

} // namespace Aero::Markup
