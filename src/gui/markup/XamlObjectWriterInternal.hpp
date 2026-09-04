#pragma once

// Shared ObjectWriter translation-unit support. Helpers that used to live in
// the XamlObjectWriter.cpp anonymous namespace are inline here so each writer
// .cpp can compile independently without amalgamating .inl hosts.

#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include "gui/markup/MarkupCommon.hpp"

#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Documents/Span.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Style.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/VisualStateManager.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace Aero::Markup {

inline constexpr Base::StringView MessageSchemaNotReady(
    "XAML object writer requires a frozen schema context");
inline constexpr Base::StringView MessageUnknownType(
    "XAML object element does not resolve to a registered type");
inline constexpr Base::StringView MessageTypeNotConstructible(
    "XAML object element resolves to a non-constructible type");
inline constexpr Base::StringView MessageUnknownMember(
    "XAML member does not resolve on the target object type");
inline constexpr Base::StringView MessageInvalidAttachedMember(
    "XAML qualified member is not valid for this target object");
inline constexpr Base::StringView MessageUnsupportedMember(
    "XAML member has no supported object-writer adapter");
inline constexpr Base::StringView MessageInvalidValue(
    "XAML value conversion or member assignment failed");
inline constexpr Base::StringView MessageInvalidWriterState(
    "XAML node sequence is invalid for the object writer");
inline constexpr Base::StringView MessageMissingContentProperty(
    "XAML child object requires a registered content property");
inline constexpr Base::StringView MessageDuplicateMemberValue(
    "XAML member is assigned more than once");
inline constexpr Base::StringView MessageInitializationFailed(
    "XAML object initialization callback failed");
inline constexpr Base::StringView MessageUnexpectedText(
    "XAML text is not valid in the current object context");
inline constexpr Base::StringView MessageTypeMismatch(
    "XAML object or scalar value is not assignable to the target member");
inline constexpr Base::StringView MessageFactoryFailed(
    "XAML object factory failed");
inline constexpr Base::StringView MessageMissingMemberValue(
    "XAML member scope does not contain a value");
inline constexpr Base::StringView MessageMultipleRoots(
    "XAML document contains more than one root object");
inline constexpr Base::StringView MessageInvalidDirective(
    "XAML language directive is unsupported or used in an invalid context");
inline constexpr Base::StringView MessageDuplicateName(
    "x:Name is duplicated in the active XAML name scope");
inline constexpr Base::StringView MessageDuplicateResourceKey(
    "x:Key is duplicated in the active XAML resource scope");
inline constexpr Base::StringView MessageStaticResourceNotFound(
    "StaticResource key is not available; forward references are not supported");
inline constexpr Base::StringView MessageMissingResourceScope(
    "x:Key requires an enclosing XAML resource scope");
inline constexpr Base::StringView MessageNullNotAllowed(
    "x:Null is not valid for this XAML value or document root");
inline constexpr Base::StringView MessageInvalidMarkupExtension(
    "XAML markup-extension text is malformed or unsupported");
inline constexpr Base::StringView MessageNamespaceState(
    "XAML namespace declaration state is invalid");
inline constexpr Base::StringView MessageNameRegistrationFailed(
    "XAML name registration callback failed");
inline constexpr Base::StringView MessageResourceRegistrationFailed(
    "XAML resource registration callback failed");
inline constexpr Base::StringView MessageUnknownMarkupExtension(
    "XAML markup-extension type or provider is not registered");
inline constexpr Base::StringView MessageMarkupExtensionFailed(
    "XAML markup-extension value provider failed");

inline constexpr Base::StringView XmlPrefix("xml");
inline constexpr Base::StringView XmlNamespaceUri(
    "http://www.w3.org/XML/1998/namespace");
inline constexpr Base::StringView DirectiveName("Name");
inline constexpr Base::StringView DirectiveKey("Key");
inline constexpr Base::StringView DirectiveClass("Class");
inline constexpr Base::StringView DirectiveNull("Null");
inline constexpr Base::StringView NullMarkup("x:Null");
inline constexpr Base::StringView StaticResourceMarkup("StaticResource");

inline Base::Status InvalidStateStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        MessageInvalidWriterState.Data());
}

inline Base::Status SessionConsumedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML load session is single use");
}

inline Base::Status InvalidContent(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

inline Base::Status InvalidContentState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

inline Base::Result<Base::String> StaticResourceNotFoundMessage(
    Base::StringView key) noexcept {
    Base::String message;
    Base::Result<void> appended = message.Assign(
        "StaticResource key '");
    if (appended) appended = message.Append(key);
    if (appended) appended = message.Append(
        "' is not available; forward references are not supported");
    return appended
        ? Base::Result<Base::String>(std::move(message))
        : Base::Result<Base::String>(appended.GetStatus());
}

inline bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

inline ResolvedMember ResolveCompiledMember(
    const CompiledMemberBinding& binding) noexcept {
    ResolvedMember member;
    member.id = binding.id;
    member.kind = binding.kind;
    member.ownerType = binding.ownerType;
    member.valueType = binding.valueType;
    member.propertyFlags = binding.propertyFlags;
    member.eventFlags = binding.eventFlags;
    member.attached = binding.attached;
    return member;
}

inline MemberWritePolicy ResolveCompiledMemberPolicy(
    const CompiledMemberBinding& binding) noexcept {
    MemberWritePolicy policy;
    policy.mode = binding.writeMode ==
            static_cast<std::uint8_t>(
                MemberWriteMode::Collection)
        ? MemberWriteMode::Collection
        : MemberWriteMode::SetOnce;
    if (binding.id ==
        VisualStateManager::VisualStateGroupsProperty.Handle().value) {
        policy.mode = MemberWriteMode::Collection;
    }
    policy.acceptsAnyValue =
        binding.acceptsAnyValue;
    policy.writable = binding.writable;
    return policy;
}

inline bool IsCompiledMemberCompatible(
    const Schema& schema,
    Meta::TypeId targetType,
    const ResolvedMember& member) noexcept {
    return member.attached ||
        schema.Types().IsDerivedFrom(
            targetType, member.ownerType);
}

} // namespace Aero::Markup
