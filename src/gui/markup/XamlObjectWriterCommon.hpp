#pragma once

// Shared internals for XamlObjectWriter translation units.

#include "gui/markup/MarkupWriterState.hpp"
#include "gui/markup/MarkupCommon.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/ServiceProvider.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/VisualStateManager.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Markup {

class NodeCursor {
public:
    virtual ~NodeCursor() = default;
    virtual Base::Result<const Node*> Read(
        Node& scratch) noexcept = 0;
};

namespace WriterDetail {

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

class StreamingXamlNodeCursor final : public NodeCursor {
public:
    explicit StreamingXamlNodeCursor(NodeReader& reader) noexcept
        : reader_(&reader) {}

    Base::Result<const Node*> Read(Node& scratch) noexcept override {
        Base::Result<NodeKind> read = reader_->Read(scratch);
        return read
            ? Base::Result<const Node*>(&scratch)
            : Base::Result<const Node*>(read.GetStatus());
    }

private:
    NodeReader* reader_ = nullptr;
};

class CompiledXamlNodeCursor final : public NodeCursor {
public:
    explicit CompiledXamlNodeCursor(
        const CompiledDocument& document) noexcept
        : nodes_(document.Nodes()) {}

    Base::Result<const Node*> Read(Node&) noexcept override {
        if (index_ >= nodes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML node stream ended unexpectedly");
        }
        return &nodes_[index_++];
    }

private:
    Base::Span<const Node> nodes_;
    std::uint32_t index_ = 0U;
};

class XamlEventConnection final : public Base::Object {
public:
    XamlEventConnection(
        Base::WeakRef<Base::Object> target,
        Meta::EventHandlerThunk thunk) noexcept
        : target_(std::move(target)),
          thunk_(thunk) {}

    void Invoke(
        Base::Object* sender,
        RoutedEventArgs& args) noexcept {
        Base::Ref<Base::Object> target = target_.Lock();
        if (target && thunk_ != nullptr) {
            thunk_(target.Get(), sender, args);
        }
    }

private:
    Base::WeakRef<Base::Object> target_;
    Meta::EventHandlerThunk thunk_ = nullptr;
};

struct XamlEventInvoker {
    Base::Ref<XamlEventConnection> connection;

    void operator()(
        Base::Object* sender,
        RoutedEventArgs& args) const noexcept {
        if (connection) connection->Invoke(sender, args);
    }

    bool operator==(const XamlEventInvoker& other) const noexcept {
        return connection.Get() == other.connection.Get();
    }
};

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

inline bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<Base::String> StaticResourceNotFoundMessage(
    Base::StringView key) noexcept;

ResolvedMember ResolveCompiledMember(
    const CompiledMemberBinding& binding) noexcept;

MemberWritePolicy ResolveCompiledMemberPolicy(
    const CompiledMemberBinding& binding) noexcept;

bool IsCompiledMemberCompatible(
    const Schema& schema,
    Meta::TypeId targetType,
    const ResolvedMember& member) noexcept;

} // namespace WriterDetail

using WriterDetail::MessageSchemaNotReady;
using WriterDetail::MessageUnknownType;
using WriterDetail::MessageTypeNotConstructible;
using WriterDetail::MessageUnknownMember;
using WriterDetail::MessageInvalidAttachedMember;
using WriterDetail::MessageUnsupportedMember;
using WriterDetail::MessageInvalidValue;
using WriterDetail::MessageInvalidWriterState;
using WriterDetail::MessageMissingContentProperty;
using WriterDetail::MessageDuplicateMemberValue;
using WriterDetail::MessageInitializationFailed;
using WriterDetail::MessageUnexpectedText;
using WriterDetail::MessageTypeMismatch;
using WriterDetail::MessageFactoryFailed;
using WriterDetail::MessageMissingMemberValue;
using WriterDetail::MessageMultipleRoots;
using WriterDetail::MessageInvalidDirective;
using WriterDetail::MessageDuplicateName;
using WriterDetail::MessageDuplicateResourceKey;
using WriterDetail::MessageStaticResourceNotFound;
using WriterDetail::MessageMissingResourceScope;
using WriterDetail::MessageNullNotAllowed;
using WriterDetail::MessageInvalidMarkupExtension;
using WriterDetail::MessageNamespaceState;
using WriterDetail::MessageNameRegistrationFailed;
using WriterDetail::MessageResourceRegistrationFailed;
using WriterDetail::MessageUnknownMarkupExtension;
using WriterDetail::MessageMarkupExtensionFailed;
using WriterDetail::XmlPrefix;
using WriterDetail::XmlNamespaceUri;
using WriterDetail::DirectiveName;
using WriterDetail::DirectiveKey;
using WriterDetail::DirectiveClass;
using WriterDetail::DirectiveNull;
using WriterDetail::NullMarkup;
using WriterDetail::StaticResourceMarkup;
using WriterDetail::StreamingXamlNodeCursor;
using WriterDetail::CompiledXamlNodeCursor;
using WriterDetail::XamlEventConnection;
using WriterDetail::XamlEventInvoker;
using WriterDetail::InvalidStateStatus;
using WriterDetail::SessionConsumedStatus;
using WriterDetail::IsAsciiWhitespace;
using WriterDetail::HasTypeFlag;
using WriterDetail::StaticResourceNotFoundMessage;
using WriterDetail::ResolveCompiledMember;
using WriterDetail::ResolveCompiledMemberPolicy;
using WriterDetail::IsCompiledMemberCompatible;

} // namespace Aero::Markup
