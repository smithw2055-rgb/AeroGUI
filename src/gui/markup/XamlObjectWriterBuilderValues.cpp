#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include "gui/markup/MarkupCommon.hpp"
#include "gui/markup/XamlObjectWriterCommon.hpp"
#include "gui/markup/MarkupExtensionHost.hpp"
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/ServiceProvider.hpp>
#include <Aero/VisualStateManager.hpp>

// ===== ObjectWriter core / node stack =====

namespace Aero::Markup {
// ===== ObjectWriter markup-extension and text values =====

ObjectWriter::MarkupValueKind ObjectWriter::ParseMarkupValue(
    Base::StringView text,
    Base::StringView& extensionName,
    Base::StringView& argument) const noexcept {
    extensionName = {};
    argument = {};
    const Base::StringView value = TrimAscii(text);
    if (value.Empty() || value[0] != '{') {
        return MarkupValueKind::Literal;
    }
    if (value.SizeBytes() >= 2U && value[1] == '}') {
        argument = value.Substr(2U, value.SizeBytes() - 2U);
        return MarkupValueKind::EscapedLiteral;
    }
    if (value.SizeBytes() < 2U ||
        value[value.SizeBytes() - 1U] != '}') {
        return MarkupValueKind::Invalid;
    }

    const Base::StringView inner = TrimAscii(value.Substr(
        1U,
        value.SizeBytes() - 2U));
    if (inner == NullMarkup) {
        return MarkupValueKind::Null;
    }

    std::uint32_t nestedDepth = 0U;
    char quote = '\0';
    for (char character : inner) {
        if (quote != '\0') {
            if (character == quote) quote = '\0';
            continue;
        }
        if (character == '\'' || character == '"') {
            quote = character;
        } else if (character == '{') {
            ++nestedDepth;
        } else if (character == '}') {
            if (nestedDepth == 0U) {
                return MarkupValueKind::Invalid;
            }
            --nestedDepth;
        }
    }
    if (nestedDepth != 0U || quote != '\0') {
        return MarkupValueKind::Invalid;
    }

    std::uint32_t nameEnd = 0U;
    while (nameEnd < inner.SizeBytes() &&
           !IsAsciiWhitespace(inner[nameEnd]) && inner[nameEnd] != ',') {
        ++nameEnd;
    }
    if (nameEnd == 0U) {
        return MarkupValueKind::Invalid;
    }
    extensionName = inner.Substr(0U, nameEnd);

    argument = TrimAscii(inner.Substr(nameEnd, inner.SizeBytes() - nameEnd));
    if (!argument.Empty() && argument[0] == ',') {
        argument = TrimAscii(argument.Substr(1U, argument.SizeBytes() - 1U));
    }
    if (extensionName == StaticResourceMarkup) {
        if (argument.Empty()) {
            return MarkupValueKind::Invalid;
        }
        constexpr Base::StringView resourceKeyPrefix("ResourceKey=");
        if (argument.SizeBytes() > resourceKeyPrefix.SizeBytes() &&
            argument.Substr(0U, resourceKeyPrefix.SizeBytes()) ==
                resourceKeyPrefix) {
            argument = TrimAscii(argument.Substr(
                resourceKeyPrefix.SizeBytes(),
                argument.SizeBytes() - resourceKeyPrefix.SizeBytes()));
            if (argument.Empty()) return MarkupValueKind::Invalid;
        }
        return MarkupValueKind::StaticResource;
    }
    return MarkupValueKind::Extension;
}

Base::Result<ProvidedValue> ObjectWriter::EvaluateMarkupExtension(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    Base::StringView extensionName,
    Base::StringView arguments,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    std::uint32_t colon = extensionName.SizeBytes();
    for (std::uint32_t index = 0U;
         index < extensionName.SizeBytes();
         ++index) {
        if (extensionName[index] != ':') {
            continue;
        }
        if (colon != extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        colon = index;
    }

    Base::StringView prefix;
    Base::StringView localName = extensionName;
    if (colon != extensionName.SizeBytes()) {
        if (colon == 0U || colon + 1U >= extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        prefix = extensionName.Substr(0U, colon);
        localName = extensionName.Substr(
            colon + 1U,
            extensionName.SizeBytes() - colon - 1U);
    }

    Base::Result<Base::StringView> namespaceResult = LookupNamespace(prefix);
    if (!namespaceResult) {
        return Failure(
            namespaceResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }
    Base::Result<const Meta::TypeInfo*> typeResult =
        schema_->ResolveType(
            namespaceResult.Value(),
            localName);
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        source);
    Base::Result<ProvidedValue> provided =
        schema_->ProvideMarkupExtensionValue(
            typeResult.Value()->Id(),
            arguments,
            services);
    if (!provided) {
        const bool missing =
            provided.GetStatus().code == Base::ErrorCode::Unsupported ||
            provided.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            provided.GetStatus(),
            missing
                ? XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension
                : XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,
            missing
                ? MessageUnknownMarkupExtension
                : MessageMarkupExtensionFailed,
            source);
    }
    return provided;
}

// ===== ObjectWriter property / content apply =====



Base::Result<void> ObjectWriter::WriteText(
    const Node& node) noexcept {
    if (!node.HasCompiledValue() &&
        !node.IsFromAttribute() &&
        IsWhitespaceOnly(node.Value())) {
        // WPF TextBlock/Span mixed content collapses XML whitespace between
        // inlines to a single space so sibling Runs do not mash together
        // ("Release, Press or Hover" not "Release,PressorHover").
        if (frames_.Empty()) {
            return {};
        }
        Frame& spaceFrame = frames_.Back();
        std::uint32_t objectIndex = InvalidIndex;
        if (spaceFrame.kind == FrameKind::Member ||
            spaceFrame.kind == FrameKind::ValueMember) {
            objectIndex = spaceFrame.targetObjectIndex;
        } else if (spaceFrame.kind == FrameKind::Object ||
                   spaceFrame.kind == FrameKind::ValueObject) {
            objectIndex = spaceFrame.objectIndex;
        }
        if (objectIndex >= created_.Size() ||
            !created_[objectIndex].object) {
            return {};
        }
        const Meta::TypeId hostType = created_[objectIndex].type;
        const bool inlineHost =
            schema_->Types().IsDerivedFrom(
                hostType, Controls::TextBlock::StaticTypeId()) ||
            schema_->Types().IsDerivedFrom(
                hostType, Documents::Span::StaticTypeId());
        if (!inlineHost) {
            return {};
        }
        std::uint32_t inlineCount = 0U;
        Base::Object& host = *created_[objectIndex].object;
        if (schema_->Types().IsDerivedFrom(
                hostType, Controls::TextBlock::StaticTypeId())) {
            inlineCount = static_cast<Controls::TextBlock&>(host)
                .GetInlineCount();
        } else {
            inlineCount = static_cast<Documents::Span&>(host)
                .GetInlines().GetCount();
        }
        if (inlineCount == 0U) {
            return {};
        }
        Base::Result<Meta::Value> space =
            Meta::Value::TryFromString(
                Meta::TypeOf<Base::String>(),
                Base::StringView(" "));
        if (!space) {
            return space.GetStatus();
        }
        if (spaceFrame.kind == FrameKind::Member) {
            return WriteValueToMember(
                spaceFrame, std::move(space).Value(), node.Source());
        }
        if (spaceFrame.kind == FrameKind::Object &&
            created_[objectIndex].hasContentMember) {
            return WriteValue(
                objectIndex,
                created_[objectIndex].contentMember,
                std::move(space).Value(),
                node.Source(),
                &created_[objectIndex].contentPolicy);
        }
        return {};
    }
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    Frame& frame = frames_.Back();
    if (node.HasCompiledValue()) {
        Meta::Value value = node.CompiledValue();
        if (frame.kind == FrameKind::ValueMember ||
            frame.kind == FrameKind::ValueObject) {
            const std::uint32_t objectIndex =
                frame.kind == FrameKind::ValueMember
                    ? frame.targetObjectIndex
                    : frame.objectIndex;
            if (objectIndex >= created_.Size() ||
                !created_[objectIndex].valueElement ||
                !created_[objectIndex].value.IsUnset() ||
                frame.valuesWritten != 0U) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        MessageDuplicateMemberValue.Data()),
                    XamlObjectWriterDiagnosticCodes::
                        DuplicateMemberValue,
                    MessageDuplicateMemberValue,
                    node.Source());
            }
            created_[objectIndex].value = std::move(value);
            ++frame.valuesWritten;
            return {};
        }
        if (frame.kind == FrameKind::Member) {
            return WriteValueToMember(
                frame, std::move(value), node.Source());
        }
        if (frame.kind == FrameKind::Object &&
            frame.objectIndex < created_.Size()) {
            const CreatedObjectRecord& object =
                created_[frame.objectIndex];
            if (!object.hasContentMember) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        MessageMissingContentProperty.Data()),
                    XamlObjectWriterDiagnosticCodes::
                        MissingContentProperty,
                    MessageMissingContentProperty,
                    node.Source());
            }
            return WriteValue(
                frame.objectIndex,
                object.contentMember,
                std::move(value),
                node.Source(),
                &object.contentPolicy);
        }
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    if (frame.kind == FrameKind::Directive) {
        return WriteDirectiveText(frame, node);
    }

    if (frame.kind == FrameKind::ValueMember ||
        frame.kind == FrameKind::ValueObject) {
        const std::uint32_t objectIndex =
            frame.kind == FrameKind::ValueMember
                ? frame.targetObjectIndex
                : frame.objectIndex;
        if (objectIndex >= created_.Size() ||
            !created_[objectIndex].valueElement ||
            !created_[objectIndex].value.IsUnset() ||
            frame.valuesWritten != 0U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    MessageDuplicateMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
                MessageDuplicateMemberValue,
                node.Source());
        }
        Base::StringView extensionName;
        Base::StringView argument;
        const MarkupValueKind markup = ParseMarkupValue(
            node.Value(), extensionName, argument);
        if (markup != MarkupValueKind::Literal &&
            markup != MarkupValueKind::EscapedLiteral) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                node.Source());
        }
        ResolvedMember valueMember;
        valueMember.valueType = created_[objectIndex].type;
        const ExtensionServices services = BuildExtensionServices(
            objectIndex,
            valueMember,
            node.Source());
        Base::Result<Meta::Value> converted = schema_->ConvertText(
            created_[objectIndex].type,
            markup == MarkupValueKind::EscapedLiteral
                ? argument : node.Value(),
            &services);
        if (!converted) {
            return Failure(
                converted.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                node.Source());
        }
        created_[objectIndex].value = std::move(converted).Value();
        ++frame.valuesWritten;
        return {};
    }

    if (frame.kind == FrameKind::Member) {
        if (frame.member.kind == Meta::MemberKind::Event) {
            if (frame.valuesWritten != 0U ||
                node.HasCompiledValue()) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        MessageDuplicateMemberValue.Data()),
                    XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
                    MessageDuplicateMemberValue,
                    node.Source());
            }
            Base::Result<void> connected = ConnectEvent(
                frame, TrimAscii(node.Value()), node.Source());
            if (!connected) return connected.GetStatus();
            ++frame.valuesWritten;
            return {};
        }
        const MemberWritePolicy policy =
            frame.hasMemberPolicy
            ? frame.memberPolicy
            : schema_->ResolveMemberWritePolicy(
                  frame.member);
        const bool acceptsAnyValue =
            policy.acceptsAnyValue;
        Base::StringView extensionName;
        Base::StringView argument;
        const MarkupValueKind markup = ParseMarkupValue(
            node.Value(),
            extensionName,
            argument);
        if (markup == MarkupValueKind::Invalid) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                node.Source());
        }
        if (markup == MarkupValueKind::Null) {
            if (frame.targetObjectIndex < created_.Size() &&
                frame.member.id ==
                    Controls::Primitives::ToggleButton::
                        IsCheckedProperty.Handle().value &&
                schema_->Types().IsDerivedFrom(
                    created_[frame.targetObjectIndex].type,
                    Controls::Primitives::ToggleButton::
                        StaticTypeId())) {
                Base::Result<Meta::Value> nullable =
                    Meta::ValueCodec<Nullable<bool>>::Encode(
                        Nullable<bool>{});
                if (!nullable) return nullable.GetStatus();
                Base::Result<void> written = WriteValueToMember(
                    frame, std::move(nullable).Value(), node.Source());
                if (!written) return written.GetStatus();
                return {};
            }
            if (frame.member.valueType == Meta::TypeOf<Base::String>()) {
                Base::Result<Meta::Value> empty =
                    Meta::Value::TryFromString(
                        Meta::TypeOf<Base::String>(), {});
                if (!empty) return empty.GetStatus();
                return WriteValueToMember(
                    frame, std::move(empty).Value(), node.Source());
            }
            if (frame.memberValueTypeIsValueType &&
                !acceptsAnyValue) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageNullNotAllowed.Data()),
                    XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                    MessageNullNotAllowed,
                    node.Source());
            }
            Meta::Value value = Meta::Value::NullObject(frame.member.valueType);
            return WriteValueToMember(frame, std::move(value), node.Source());
        }
        if (markup == MarkupValueKind::StaticResource) {
            Base::Result<Aero::ResourceValue> resource = LookupResource(argument);
            if (!resource) {
                if (loadContext_ != nullptr &&
                    loadContext_->deferUnresolvedStaticResources) {
                    frame.deferredStaticResource = true;
                    hasDeferredStaticResources_ = true;
                    if (frame.targetObjectIndex < created_.Size()) {
                        created_[frame.targetObjectIndex]
                            .deferredStaticResource = true;
                    }
                    DeferredStaticResourceRecord deferred;
                    deferred.targetObjectIndex =
                        frame.targetObjectIndex;
                    deferred.member = frame.member;
                    deferred.policy = policy;
                    deferred.hasPolicy = true;
                    deferred.source = node.Source();
                    Base::Result<void> key = deferred.key.Assign(
                        argument);
                    if (!key) return key.GetStatus();
                    deferredStaticResources_.PushBack(
                            std::move(deferred));
                    return {};
                }
                Base::Result<Base::String> message =
                    StaticResourceNotFoundMessage(argument);
                if (!message) return message.GetStatus();
                return Failure(
                    resource.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                    message.Value().View(),
                    node.Source());
            }
            return WriteValueToMember(
                frame, std::move(resource).Value(), node.Source());
        }

        if (markup == MarkupValueKind::Extension) {
            Base::Result<ProvidedValue> value =
                EvaluateMarkupExtension(
                    frame.targetObjectIndex,
                    frame.member,
                    extensionName,
                    argument,
                    node.Source());
            if (!value) return value.GetStatus();
            return WriteProvidedValueToMember(
                frame,
                std::move(value).Value(),
                node.Source());
        }

        const ExtensionServices services = BuildExtensionServices(
            frame.targetObjectIndex,
            frame.member,
            node.Source());
        Base::Result<Meta::Value> convertResult = schema_->ConvertText(
            frame.member.valueType,
            markup == MarkupValueKind::EscapedLiteral
                ? argument
                : node.Value(),
            &services);
        if (!convertResult) {
            return Failure(
                convertResult.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                node.Source());
        }
        return WriteValueToMember(
            frame,
            std::move(convertResult).Value(),
            node.Source());
    }

    if (frame.kind != FrameKind::Object ||
        frame.objectIndex >= created_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    const CreatedObjectRecord& contentOwner =
        created_[frame.objectIndex];
    if (!contentOwner.hasContentMember) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }
    const ResolvedMember& contentMember =
        contentOwner.contentMember;

    Base::StringView extensionName;
    Base::StringView argument;
    const MarkupValueKind markup = ParseMarkupValue(
        node.Value(),
        extensionName,
        argument);
    if (markup == MarkupValueKind::Invalid) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidMarkupExtension.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
            MessageInvalidMarkupExtension,
            node.Source());
    }
    if (markup == MarkupValueKind::Null) {
        if (contentOwner.contentValueTypeIsValueType) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                node.Source());
        }
        Meta::Value value = Meta::Value::NullObject(
            contentMember.valueType);
        return WriteValue(
            frame.objectIndex,
            contentMember,
            std::move(value),
            node.Source(),
            &contentOwner.contentPolicy);
    }
    if (markup == MarkupValueKind::StaticResource) {
        Base::Result<Aero::ResourceValue> resource = LookupResource(argument);
        if (!resource) {
            if (loadContext_ != nullptr &&
                loadContext_->deferUnresolvedStaticResources) {
                frame.deferredStaticResource = true;
                hasDeferredStaticResources_ = true;
                if (frame.objectIndex < created_.Size()) {
                    created_[frame.objectIndex].deferredStaticResource =
                        true;
                }
                DeferredStaticResourceRecord deferred;
                deferred.targetObjectIndex = frame.objectIndex;
                deferred.member = contentMember;
                deferred.policy = contentOwner.contentPolicy;
                deferred.hasPolicy = true;
                deferred.source = node.Source();
                Base::Result<void> key = deferred.key.Assign(
                    argument);
                if (!key) return key.GetStatus();
                deferredStaticResources_.PushBack(
                        std::move(deferred));
                return {};
            }
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(argument);
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                node.Source());
        }
        return WriteValue(
            frame.objectIndex,
            contentMember,
            std::move(resource).Value(),
            node.Source(),
            &contentOwner.contentPolicy);
    }

    if (markup == MarkupValueKind::Extension) {
        Base::Result<ProvidedValue> value =
            EvaluateMarkupExtension(
                frame.objectIndex,
                contentMember,
                extensionName,
                argument,
                node.Source());
        if (!value) return value.GetStatus();
        return WriteProvidedValue(
            frame.objectIndex,
            contentMember,
            std::move(value).Value(),
            node.Source(),
            &contentOwner.contentPolicy);
    }

    const ExtensionServices services = BuildExtensionServices(
        frame.objectIndex,
        contentMember,
        node.Source());
    const bool itemsControlScalarContent =
        schema_->Types().IsDerivedFrom(
            contentOwner.type,
            Controls::ItemsControl::StaticTypeId()) &&
        contentOwner.hasContentMember &&
        contentOwner.contentMember.id == contentMember.id;
    Base::Result<Meta::Value> convertResult = schema_->ConvertText(
        itemsControlScalarContent
            ? Meta::TypeOf<Base::String>()
            : contentMember.valueType,
        markup == MarkupValueKind::EscapedLiteral
            ? argument
            : node.Value(),
        &services);
    if (!convertResult) {
        return Failure(
            convertResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            node.Source());
    }
    return WriteValue(
        frame.objectIndex,
        contentMember,
        std::move(convertResult).Value(),
        node.Source(),
        &contentOwner.contentPolicy);
}

Base::Result<void> ObjectWriter::WriteDirectiveText(
    Frame& frame,
    const Node& node) noexcept {
    if (frame.targetObjectIndex >= created_.Size() ||
        frame.valuesWritten != 0U || !node.IsFromAttribute()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    CreatedObjectRecord& object = created_[frame.targetObjectIndex];
    if (frame.directive == DirectiveKind::Name) {
        const Meta::PropertyInfo* nameProperty =
            schema_->Metadata()->Types().FindProperty(
                object.type, Base::StringView("Name"), false);
        const bool validName =
            Aero::NameScope::IsValidName(node.Value());
        if (!validName && nameProperty == nullptr) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        if (validName) {
            Base::Result<void> assignResult = object.name.Assign(node.Value());
            if (!assignResult) {
                return assignResult.GetStatus();
            }
            Base::Result<void> registerResult = RegisterObjectName(
                frame.targetObjectIndex,
                node.Source());
            if (!registerResult) {
                return registerResult.GetStatus();
            }
        }
        // VisualState and related non-visual authoring objects expose an
        // ordinary Name property in addition to participating in x:Name
        // scopes. Types such as Scoreboard's Game also expose Name as a
        // display string that is not a runtime name (spaces are allowed).
        if (nameProperty != nullptr) {
            Base::Result<Meta::Value> nameValue =
                schema_->ConvertText(
                    nameProperty->ValueType(), node.Value(), nullptr);
            if (!nameValue) return nameValue.GetStatus();
            ResolvedMember nameMember;
            nameMember.id = nameProperty->Id();
            nameMember.kind = Meta::MemberKind::Property;
            nameMember.ownerType = nameProperty->OwnerType();
            nameMember.valueType = nameProperty->ValueType();
            nameMember.propertyFlags = nameProperty->Flags();
            Base::Result<void> assignedName = WriteValue(
                frame.targetObjectIndex, nameMember,
                std::move(nameValue).Value(), node.Source());
            if (!assignedName) return assignedName.GetStatus();
        }
    } else if (frame.directive == DirectiveKind::Key) {
        if (node.Value().Empty()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        Base::Result<void> assignResult = object.key.Assign(node.Value());
        if (!assignResult) {
            return assignResult.GetStatus();
        }
    } else if (frame.directive == DirectiveKind::Class) {
        if (node.Value().Empty() ||
            frame.targetObjectIndex != rootObjectIndex_) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        const Base::StringView className = TrimAscii(node.Value());
        std::uint32_t separator = className.SizeBytes();
        for (std::uint32_t index = 0U;
             index < className.SizeBytes(); ++index) {
            if (className[index] == '.') separator = index;
        }
        if (separator == 0U ||
            separator + 1U >= className.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "x:Class must use Namespace.Type syntax"),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        Base::String xamlNamespace;
        Base::Result<void> namespaceAssigned =
            xamlNamespace.Assign("clr-namespace:");
        if (namespaceAssigned) {
            namespaceAssigned = xamlNamespace.Append(
                className.Substr(0U, separator));
        }
        if (!namespaceAssigned) return namespaceAssigned.GetStatus();
        const Base::StringView localName = className.Substr(
            separator + 1U,
            className.SizeBytes() - separator - 1U);
        Base::Result<const Meta::TypeInfo*> classType =
            schema_->ResolveType(xamlNamespace.View(), localName);
        if (!classType) {
            if (loadContext_ != nullptr && loadContext_->existingRoot) {
                return Failure(
                    classType.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::UnknownType,
                    MessageUnknownType,
                    node.Source());
            }
            if (classType.GetStatus().code ==
                Base::ErrorCode::NotFound) {
                // A pure-XAML host may intentionally omit the code-behind
                // class. Keep the authored root type in that case while
                // activating a registered derived class when one exists.
                frame.valuesWritten = 1U;
                return {};
            }
            return classType.GetStatus();
        }
        if (!schema_->Types().IsDerivedFrom(
                classType.Value()->Id(), object.type)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "x:Class type does not derive from the authored root type"),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                node.Source());
        }
        if (loadContext_ != nullptr && loadContext_->existingRoot &&
            frame.targetObjectIndex == rootObjectIndex_ &&
            classType.Value()->Id() !=
                loadContext_->existingRoot->RuntimeType()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "x:Class does not match the existing root runtime type"),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                node.Source());
        }
        if (classType.Value()->Id() != object.type) {
            Base::Result<Base::Ref<Base::Object>> replacement =
                CreateObject(classType.Value()->Id());
            if (!replacement) {
                return Failure(
                    replacement.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::FactoryFailed,
                    MessageFactoryFailed,
                    node.Source());
            }
            Base::Result<void> initialized = schema_->BeginInit(
                classType.Value()->Id(), *replacement.Value());
            if (!initialized) return initialized.GetStatus();
            if (object.beginCalled && object.object) {
                schema_->AbortInit(object.type, *object.object);
            }
            object.object = std::move(replacement).Value();
            object.type = classType.Value()->Id();
            object.beginCalled = true;
            object.endCalled = false;
            object.hasContentMember = false;
            object.contentMember = {};
            object.contentPolicy = {};
            object.contentValueTypeIsObject = false;
            object.contentValueTypeIsValueType = false;
            Base::Result<ResolvedMember> content =
                schema_->ResolveContentMember(object.type);
            if (content) {
                object.contentMember = content.Value();
                object.contentPolicy =
                    schema_->ResolveMemberWritePolicy(
                        object.contentMember);
                object.hasContentMember = true;
                if (const Meta::TypeInfo* valueType =
                        schema_->Types().FindType(
                            object.contentMember.valueType)) {
                    object.contentValueTypeIsObject =
                        valueType->Kind() ==
                            Meta::MetadataTypeKind::Object;
                    object.contentValueTypeIsValueType =
                        valueType->Kind() !=
                            Meta::MetadataTypeKind::Object &&
                        HasTypeFlag(
                            valueType->Flags(),
                            Meta::TypeFlags::ValueType);
                }
            } else if (content.GetStatus().code !=
                       Base::ErrorCode::NotFound) {
                return content.GetStatus();
            }
            const std::uint32_t objectFrame =
                FindObjectFrameIndex(frame.targetObjectIndex);
            if (objectFrame != InvalidIndex &&
                frames_[objectFrame].resourceScopeIndex != InvalidIndex &&
                frames_[objectFrame].resourceScopeIndex <
                    resourceScopes_.Size()) {
                resourceScopes_[
                    frames_[objectFrame].resourceScopeIndex].external =
                        schema_->ResolveResourceScope(
                            object.type, *object.object);
            }
        }
    } else {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    frame.valuesWritten = 1U;
    return {};
}

} // namespace Aero::Markup
