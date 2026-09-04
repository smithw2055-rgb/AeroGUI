#include "gui/markup/XamlObjectWriterInternal.hpp"

// Property apply / content write / event connection.

namespace Aero::Markup {
namespace {

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

} // namespace

Base::Result<void> ObjectBuilder::WriteText(
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
                    Base::Result<void> stored =
                        deferredStaticResources_.PushBack(
                            std::move(deferred));
                    if (!stored) return stored.GetStatus();
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
                Base::Result<void> stored =
                    deferredStaticResources_.PushBack(
                        std::move(deferred));
                if (!stored) return stored.GetStatus();
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

Base::Result<void> ObjectBuilder::WriteDirectiveText(
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


Base::Result<void> ObjectBuilder::WriteValueToParent(
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageTypeMismatch.Data()),
            XamlObjectWriterDiagnosticCodes::TypeMismatch,
            MessageTypeMismatch,
            source);
    }
    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object ||
        parent.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    Base::Result<ResolvedMember> content =
        schema_->ResolveContentMember(
            created_[parent.objectIndex].type);
    if (!content) {
        return Failure(
            content.GetStatus(),
            XamlObjectWriterDiagnosticCodes::MissingContentProperty,
            MessageMissingContentProperty,
            source);
    }
    return WriteValue(
        parent.objectIndex,
        content.Value(),
        std::move(value),
        source);
}

Base::Result<void> ObjectBuilder::WriteObjectToParent(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    if (frames_.Empty()) {
        if (root_) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    MessageMultipleRoots.Data()),
                XamlObjectWriterDiagnosticCodes::MultipleRootObjects,
                MessageMultipleRoots,
                source);
        }
        root_ = created_[objectIndex].object;
        return {};
    }

    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        Meta::Value value = Meta::Value::FromObject(
            created_[objectIndex].type,
            created_[objectIndex].object);
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    return WriteObjectToContent(
        parent.objectIndex,
        objectIndex,
        source);
}

Base::Result<void> ObjectBuilder::WriteObjectToContent(
    std::uint32_t parentObjectIndex,
    std::uint32_t childObjectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (parentObjectIndex >= created_.Size() ||
        childObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const CreatedObjectRecord& contentOwner =
        created_[parentObjectIndex];
    if (!contentOwner.hasContentMember) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageMissingContentProperty.Data()),
            XamlObjectWriterDiagnosticCodes::MissingContentProperty,
            MessageMissingContentProperty,
            source);
    }

    Meta::Value value = Meta::Value::FromObject(
        created_[childObjectIndex].type,
        created_[childObjectIndex].object);
    return WriteValue(
        parentObjectIndex,
        contentOwner.contentMember,
        std::move(value),
        source,
        &contentOwner.contentPolicy);
}

Base::Result<void> ObjectBuilder::WriteNullToParent(
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            source);
    }

    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        if (parent.targetObjectIndex < created_.Size() &&
            parent.member.id ==
                Controls::Primitives::ToggleButton::
                    IsCheckedProperty.Handle().value &&
            schema_->Types().IsDerivedFrom(
                created_[parent.targetObjectIndex].type,
                Controls::Primitives::ToggleButton::
                    StaticTypeId())) {
            Base::Result<Meta::Value> nullable =
                Meta::ValueCodec<Nullable<bool>>::Encode(
                    Nullable<bool>{});
            if (!nullable) return nullable.GetStatus();
            Base::Result<void> written = WriteValueToMember(
                parent, std::move(nullable).Value(), source);
            if (!written) return written.GetStatus();
            return {};
        }
        const MemberWritePolicy policy =
            parent.hasMemberPolicy
            ? parent.memberPolicy
            : schema_->ResolveMemberWritePolicy(
                  parent.member);
        const bool acceptsAnyValue =
            policy.acceptsAnyValue;
        if (parent.member.valueType == Meta::TypeOf<Base::String>()) {
            Base::Result<Meta::Value> empty =
                Meta::Value::TryFromString(
                    Meta::TypeOf<Base::String>(), {});
            if (!empty) return empty.GetStatus();
            return WriteValueToMember(
                parent, std::move(empty).Value(), source);
        }
        if (parent.memberValueTypeIsValueType &&
            !acceptsAnyValue) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                source);
        }
        Meta::Value value = Meta::Value::NullObject(parent.member.valueType);
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object ||
        parent.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const CreatedObjectRecord& contentOwner =
        created_[parent.objectIndex];
    if (!contentOwner.hasContentMember ||
        contentOwner.contentValueTypeIsValueType) {
        return Failure(
            Base::Status::Failure(
                contentOwner.hasContentMember
                    ? Base::ErrorCode::ValidationFailed
                    : Base::ErrorCode::NotFound,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            source);
    }

    Meta::Value value = Meta::Value::NullObject(
        contentOwner.contentMember.valueType);
    return WriteValue(
        parent.objectIndex,
        contentOwner.contentMember,
        std::move(value),
        source,
        &contentOwner.contentPolicy);
}

Base::Result<void> ObjectBuilder::WriteValueToMember(
    Frame& memberFrame,
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<void> result = WriteValue(
        memberFrame.targetObjectIndex,
        memberFrame.member,
        std::move(value),
        source,
        memberFrame.hasMemberPolicy
            ? &memberFrame.memberPolicy
            : nullptr);
    if (!result) {
        return result.GetStatus();
    }
    if (memberFrame.valuesWritten == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++memberFrame.valuesWritten;
    return {};
}

Base::Result<void> ObjectBuilder::WriteProvidedValueToMember(
    Frame& memberFrame,
    ProvidedValue&& provided,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<void> result = WriteProvidedValue(
        memberFrame.targetObjectIndex,
        memberFrame.member,
        std::move(provided),
        source,
        memberFrame.hasMemberPolicy
            ? &memberFrame.memberPolicy
            : nullptr);
    if (!result) return result.GetStatus();
    if (memberFrame.valuesWritten == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++memberFrame.valuesWritten;
    return {};
}

Base::Result<void> ObjectBuilder::WriteProvidedValue(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    ProvidedValue&& provided,
    ::Aero::Diagnostics::SourceSpan source,
    const MemberWritePolicy* compiledPolicy) noexcept {
    if (provided.kind == ProvidedValueKind::Value) {
        return WriteValue(
            targetObjectIndex,
            member,
            std::move(provided.value),
            source,
            compiledPolicy);
    }
    if (targetObjectIndex >= created_.Size()) {
        provided.Discard();
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const MemberWritePolicy policy =
        compiledPolicy != nullptr
        ? *compiledPolicy
        : schema_->ResolveMemberWritePolicy(member);
    AssignmentRecord* assignment = FindAssignment(
        targetObjectIndex, member.id);
    if (!policy.writable ||
        (assignment != nullptr && assignment->count != 0U &&
         policy.mode == MemberWriteMode::SetOnce)) {
        provided.Discard();
        return Failure(
            Base::Status::Failure(
                policy.writable
                    ? Base::ErrorCode::AlreadyExists
                    : Base::ErrorCode::Unsupported,
                policy.writable
                    ? MessageDuplicateMemberValue.Data()
                    : MessageUnsupportedMember.Data()),
            policy.writable
                ? XamlObjectWriterDiagnosticCodes::DuplicateMemberValue
                : XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            policy.writable
                ? MessageDuplicateMemberValue
                : MessageUnsupportedMember,
            source);
    }
    if (assignment == nullptr) {
        Base::Result<void> appended = assignments_.PushBack({
            targetObjectIndex, member.id, 0U});
        if (!appended) {
            provided.Discard();
            return appended.GetStatus();
        }
        assignment = &assignments_.Back();
    }

    CommittedEffect effect;
    if (loadContext_ != nullptr) {
        effect.lifetime = loadContext_->effectLifetime;
    }
    if (provided.kind == ProvidedValueKind::Expression) {
        if (provided.effectiveValues == nullptr ||
            !provided.expression.IsValid()) {
            provided.Discard();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Markup extension returned an invalid property expression");
        }
        Base::Result<::Aero::DependencyObject*> target =
            schema_->ResolvePropertyTarget(
                *created_[targetObjectIndex].object);
        if (!target) {
            provided.Discard();
            return target.GetStatus();
        }
        effect.effectiveValues = provided.effectiveValues;
        effect.targetOwner =
            Base::Ref<::Aero::DependencyObject>::TryFromBorrowed(
                *target.Value());
        if (!effect.targetOwner) {
            provided.Discard();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Markup expression target is not reference-counted");
        }
        effect.target = effect.targetOwner.Get();
        effect.property = Meta::DependencyPropertyHandle{member.id};
        effect.pendingExpression = provided.expression;
        provided.expression = {};
    } else if (provided.kind == ProvidedValueKind::Handled) {
        effect.context = provided.rollbackContext;
        effect.token = provided.rollbackToken;
        effect.rollback = provided.rollback;
        effect.committed = true;
        provided.rollbackContext = nullptr;
        provided.rollback = nullptr;
    } else if (provided.kind == ProvidedValueKind::Deferred) {
        effect.context = provided.rollbackContext;
        effect.prepare = provided.prepare;
        effect.commit = provided.commit;
        effect.rollback = provided.rollback;
        effect.cleanup = provided.cleanup;
        effect.bind = provided.bind;
        provided.rollbackContext = nullptr;
        provided.prepare = nullptr;
        provided.commit = nullptr;
        provided.rollback = nullptr;
        provided.cleanup = nullptr;
        provided.bind = nullptr;
    } else {
        provided.Discard();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Markup extension returned an unknown value kind");
    }

    const bool immediate = loadContext_ == nullptr ||
        loadContext_->effectCommitMode == EffectCommitMode::Immediate;
    if (immediate && !effect.committed) {
        Base::Result<void> committed = effect.Commit();
        if (!committed) {
            effect.Rollback();
            return committed.GetStatus();
        }
    }
    Base::Result<void> effectStored =
        extensionEffects_.PushBack(std::move(effect));
    if (!effectStored) {
        effect.Rollback();
        return effectStored.GetStatus();
    }
    if (assignment->count == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            MessageDuplicateMemberValue.Data());
    }
    ++assignment->count;
    return {};
}

Base::Result<void> ObjectBuilder::WriteValue(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source,
    const MemberWritePolicy* compiledPolicy) noexcept {
    if (targetObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        if (MarkupExtension* extension =
                ::Aero::TryCast<MarkupExtension>(value.AsObject().Get())) {
            Base::Result<Meta::Value> provided = extension->ProvideValue();
            if (!provided) {
                return Failure(
                    provided.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,
                    MessageMarkupExtensionFailed,
                    source);
            }
            value = std::move(provided).Value();
        }
    }
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject() &&
        value.AsObject()->RuntimeType() ==
            Data::MultiBinding::StaticTypeId()) {
        const ExtensionServices services =
            BuildExtensionServices(
                targetObjectIndex,
                member,
                source);
        Base::Result<ProvidedValue> provided =
            CreateMultiBindingValue(
                static_cast<Data::MultiBinding&>(
                    *value.AsObject()),
                services);
        if (!provided) {
            return Failure(
                provided.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                source);
        }
        return WriteProvidedValue(
            targetObjectIndex,
            member,
            std::move(provided).Value(),
            source,
            compiledPolicy);
    }


    const MemberWritePolicy policy =
        compiledPolicy != nullptr
        ? *compiledPolicy
        : schema_->ResolveMemberWritePolicy(member);
    if (!policy.writable) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            source);
    }

    AssignmentRecord* assignment = FindAssignment(
        targetObjectIndex,
        member.id);
    if (assignment != nullptr && assignment->count != 0U &&
        policy.mode == MemberWriteMode::SetOnce) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }

    if (assignment == nullptr) {
        Base::Result<void> appendResult = assignments_.PushBack({
            targetObjectIndex,
            member.id,
            0U});
        if (!appendResult) {
            return Failure(
                appendResult.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        assignment = &assignments_.Back();
    }

    CreatedObjectRecord& targetRecord =
        created_[targetObjectIndex];
    if (value.Kind() != Meta::ValueKind::Object &&
        targetRecord.hasContentMember &&
        targetRecord.contentMember.id == member.id &&
        schema_->Types().IsDerivedFrom(
            targetRecord.type,
            Controls::ItemsControl::StaticTypeId())) {
        Base::Result<Base::Ref<
            Controls::BoxedItemValue>> boxed =
                Base::MakeRef<Controls::BoxedItemValue>(
                    value);
        if (!boxed) return boxed.GetStatus();
        value = Meta::Value::FromObject(
            member.valueType,
            Base::Ref<Base::Object>(
                std::move(boxed).Value()));
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        source);
    if ((member.valueType == Meta::TypeOf<Aero::Length>() ||
         member.valueType == Meta::TypeOf<::Aero::GridLength>()) &&
        value.Type() != member.valueType &&
        (value.Kind() == Meta::ValueKind::SignedInteger ||
         value.Kind() == Meta::ValueKind::UnsignedInteger ||
         value.Kind() == Meta::ValueKind::Double)) {
        Base::Result<Meta::Value> converted =
            ConvertConstantBindingValue(
                value, member.valueType);
        if (!converted) {
            return Failure(
                converted.GetStatus(),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                source);
        }
        value = std::move(converted).Value();
    }
    if (member.valueType ==
            Media::Brush::StaticTypeId() &&
        value.Type() ==
            Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Meta::ValueCodec<Base::Color>::Decode(
                value);
        if (!color) {
            return Failure(
                color.GetStatus(),
                XamlObjectWriterDiagnosticCodes::
                    InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        Base::Result<
            Base::Ref<Media::Brush>>
            brush =
                Media::MakeSolidColorBrush(
                    color.Value());
        if (!brush) {
            return Failure(
                brush.GetStatus(),
                XamlObjectWriterDiagnosticCodes::
                    InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        value = Meta::Value::FromObject(
            Media::Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    Base::Result<Meta::ContentInfo> content =
        schema_->Metadata()->GetContentInfo(member.id);
    const bool hasWritableContent =
        content && content.Value().writable;
    const bool hasVisualContent =
        hasWritableContent && content.Value().IsVisual();
    const Meta::PropertyInfo* memberProperty =
        schema_->Types().FindProperty(member.id);
    const auto isUIElementValue =
        [this](const Meta::Value& candidate) noexcept {
            return candidate.Kind() == Meta::ValueKind::Object &&
                !candidate.IsNullObject() &&
                candidate.AsObject() &&
                schema_->Types().IsDerivedFrom(
                    candidate.AsObject()->RuntimeType(),
                    Aero::UIElement::StaticTypeId());
        };
    const bool hasVisualStructuralProperty =
        memberProperty != nullptr &&
        (static_cast<std::uint32_t>(
             memberProperty->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Structural)) !=
            0U &&
        schema_->Metadata()->
            CanWriteProperty(member.id);
    const bool isDependencyObjectValue =
        value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() && value.AsObject() &&
        schema_->Types().IsDerivedFrom(
            value.AsObject()->RuntimeType(),
            Aero::DependencyObject::StaticTypeId());
    const bool stagesVisualContent =
        (hasVisualContent ||
         hasVisualStructuralProperty) &&
        isUIElementValue(value);
    const bool parentIsVisual =
        schema_->Types().IsDerivedFrom(
            created_[targetObjectIndex].type,
            Aero::Media::Visual::StaticTypeId());
    const bool parentIsFreezable =
        schema_->Types().IsDerivedFrom(
            created_[targetObjectIndex].type,
            Aero::Freezable::StaticTypeId());
    const bool parentIsTimeline =
        schema_->Types().IsDerivedFrom(
            created_[targetObjectIndex].type,
            Media::Animation::Timeline::StaticTypeId());
    const bool stagesDeferredObjectContent =
        services.deferredContentOwner != nullptr &&
        (hasWritableContent || hasVisualStructuralProperty) &&
        isDependencyObjectValue &&
        (services.deferredContentOwner ==
             created_[targetObjectIndex].object.Get() ||
         parentIsVisual ||
         (parentIsFreezable && !parentIsTimeline));
    Base::Result<void> setResult =
        (stagesVisualContent || stagesDeferredObjectContent)
        ? ObjectWriter::StageContent(
              *schema_,
              *created_[targetObjectIndex].object,
              value,
              services)
        : schema_->SetMember(
              *created_[targetObjectIndex].object,
              created_[targetObjectIndex].type,
              member,
              value);
    if (setResult &&
        hasVisualContent &&
        !stagesVisualContent &&
        schema_->Metadata()->CanReadProperty(member.id)) {
        Base::Result<Meta::Value> materialized =
            schema_->Metadata()->GetProperty(
                *created_[targetObjectIndex].object,
                member.id);
        if (materialized &&
            isUIElementValue(materialized.Value())) {
            setResult = ObjectWriter::StageContent(
                *schema_,
                *created_[targetObjectIndex].object,
                materialized.Value(),
                services);
        }
    }
    if (!setResult) {
        ::Aero::Diagnostics::DiagnosticCode code =
            XamlObjectWriterDiagnosticCodes::InvalidValue;
        Base::StringView message = MessageInvalidValue;
        if (setResult.GetStatus().code == Base::ErrorCode::Unsupported) {
            code = XamlObjectWriterDiagnosticCodes::UnsupportedMember;
            message = MessageUnsupportedMember;
        } else if (setResult.GetStatus().code ==
            Base::ErrorCode::InvalidArgument) {
            code = XamlObjectWriterDiagnosticCodes::TypeMismatch;
            message = MessageTypeMismatch;
        }
        return Failure(setResult.GetStatus(), code, message, source);
    }

    if (assignment->count == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++assignment->count;
    return {};
}

Base::Result<void> ObjectBuilder::ConnectEvent(
    Frame& memberFrame,
    Base::StringView handlerName,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (handlerName.Empty() ||
        memberFrame.targetObjectIndex >= created_.Size() ||
        rootObjectIndex_ >= created_.Size() ||
        memberFrame.member.kind != Meta::MemberKind::Event) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event attribute requires a handler name"),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            source);
    }

    CreatedObjectRecord& eventSource =
        created_[memberFrame.targetObjectIndex];
    CreatedObjectRecord& codeBehind =
        created_[rootObjectIndex_];
    if (!eventSource.object || !codeBehind.object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    Meta::EventHandlerThunk thunk =
        schema_->Metadata()->Types().FindEventHandler(
            codeBehind.type,
            handlerName);
    if (thunk == nullptr) {
        // LoadComponentInto a real code-behind instance must resolve the
        // handler. A pure-XAML host (x:Class omitted or unregistered) stores
        // Click="OnFoo" the same way CommandBinding Executed stores a name:
        // do not abort document construction.
        if (loadContext_ != nullptr && loadContext_->existingRoot) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "XAML event handler is not registered on the x:Class type"),
                XamlObjectWriterDiagnosticCodes::UnknownMember,
                MessageUnknownMember,
                source);
        }
        return {};
    }

    Base::Result<Base::Ref<XamlEventConnection>> connection =
        Base::MakeRef<XamlEventConnection>(
            Base::WeakRef<Base::Object>(root_),
            thunk);
    if (!connection) return connection.GetStatus();
    Base::Delegate<void(Base::Object*, RoutedEventArgs&)> handler(
        XamlEventInvoker{std::move(connection).Value()});
    const RoutedEventHandle routedEvent{memberFrame.member.id};

    Base::Result<void> connected;
    if (schema_->Types().IsDerivedFrom(
            eventSource.type, UIElement::StaticTypeId())) {
        connected = static_cast<UIElement*>(
            eventSource.object.Get())->AddHandlerChecked(
                routedEvent, handler);
    } else if (schema_->Types().IsDerivedFrom(
                   eventSource.type,
                   ContentElement::StaticTypeId())) {
        connected = static_cast<ContentElement*>(
            eventSource.object.Get())->AddHandlerChecked(
                routedEvent, handler);
    } else {
        connected = Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML event source does not support routed handlers");
    }
    return connected
        ? Base::Result<void>{}
        : Base::Result<void>(Failure(
              connected.GetStatus(),
              XamlObjectWriterDiagnosticCodes::UnsupportedMember,
              MessageUnsupportedMember,
              source));
}

ObjectBuilder::AssignmentRecord* ObjectBuilder::FindAssignment(
    std::uint32_t objectIndex,
    Meta::MemberId member) noexcept {
    for (AssignmentRecord& assignment : assignments_) {
        if (assignment.objectIndex == objectIndex &&
            assignment.member == member) {
            return &assignment;
        }
    }
    return nullptr;
}

Base::Result<Aero::Media::Visual*> ObjectWriter::ResolveVisual(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Meta::TypeId type) noexcept {
    if (!schema.Types().IsDerivedFrom(object.RuntimeType(), type) ||
        !schema.Types().IsDerivedFrom(
            type, Aero::Media::Visual::StaticTypeId())) {
        return InvalidContent(
            "XAML object metadata is not compatible with Visual");
    }
    return static_cast<Aero::Media::Visual*>(&object);
}

Base::Result<Aero::UIElement*> ObjectWriter::ResolveUIElement(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Meta::TypeId type) noexcept {
    Base::Result<Aero::Media::Visual*> visual =
        ResolveVisual(schema, object, type);
    if (!visual) return visual.GetStatus();
    Aero::UIElement* element =
        ::Aero::TryCast<::Aero::UIElement>(visual.Value());
    if (element == nullptr) {
        return InvalidContent("XAML object is not a UIElement");
    }
    return element;
}

Base::Result<void> ObjectWriter::StageContent(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    const Meta::Value& value,
    const ExtensionServices& services) noexcept {
    VisualContentPlan* plan = services.visualContent;
    if (services.targetObject != &object ||
        value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return InvalidContentState(
            "XAML content requires a non-null object");
    }

    ::Aero::Meta::Registry* metadata = schema.Metadata();
    if (metadata == nullptr) {
        return InvalidContentState(
            "XAML content metadata is unavailable");
    }
    Base::Result<Meta::ContentInfo> contentResult =
        metadata->GetContentInfo(services.targetMember);
    const Meta::PropertyInfo* property =
        schema.Types().FindProperty(
            services.targetMember);
    const bool attachedMember =
        property != nullptr &&
        (static_cast<std::uint32_t>(property->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Attached)) != 0U;
    const bool structuralProperty =
        property != nullptr &&
        (static_cast<std::uint32_t>(
             property->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Structural)) !=
            0U &&
        metadata->CanWriteProperty(
            services.targetMember);
    if (!contentResult && !structuralProperty) {
        return InvalidContent(
            "XAML content target has no content metadata");
    }
    if (!structuralProperty) {
        const Meta::ContentInfo& content =
            contentResult.Value();
        if (!content.writable ||
            !content.clearable ||
            (!attachedMember &&
             !schema.Types().IsDerivedFrom(
                 services.targetObjectType,
                 content.ownerType)) ||
            (services.deferredContentOwner == nullptr &&
             !content.IsVisual())) {
            return InvalidContent(
                "XAML content target has no compatible content facet");
        }
    }

    Base::Object* childObject = value.AsObject().Get();
    const bool childIsDependencyObject =
        schema.Types().IsDerivedFrom(
            childObject->RuntimeType(),
            Aero::DependencyObject::StaticTypeId());

    // A deferred template owns a visual root without itself being a ::Aero::Media::Visual.
    // Commit that root through the template's content accessor; descendant
    // visual edges are staged below once their actual visual parent exists.
    if (services.deferredContentOwner == &object &&
        !schema.Types().IsDerivedFrom(
            services.targetObjectType,
            Aero::Media::Visual::StaticTypeId())) {
        if (structuralProperty) {
            return InvalidContent(
                "A non-visual template root cannot use a visual structural property");
        }
        return metadata->WriteContent(
            object,
            services.targetMember,
            value.AsObject());
    }

    if (services.deferredContentOwner != nullptr) {
        const bool parentIsVisual =
            schema.Types().IsDerivedFrom(
                object.RuntimeType(),
                Aero::Media::Visual::StaticTypeId());
        const bool parentIsFreezable =
            schema.Types().IsDerivedFrom(
                object.RuntimeType(),
                Aero::Freezable::StaticTypeId());
        const bool parentIsTimeline =
            schema.Types().IsDerivedFrom(
                object.RuntimeType(),
                Media::Animation::Timeline::StaticTypeId());
        const bool deferParent =
            services.deferredContentOwner == &object ||
            parentIsVisual ||
            (parentIsFreezable && !parentIsTimeline);
        if (!deferParent) {
            // DataTemplate.Resources Storyboards and Trigger.EnterActions are
            // live shared objects. Visual-tree Freezables (TransformGroup)
            // still clone through the deferred graph.
            return metadata->WriteContent(
                object,
                services.targetMember,
                value.AsObject());
        }
        if (services.deferredContent == nullptr ||
            !childIsDependencyObject) {
            return InvalidContentState(
                "Deferred XAML content requires a DependencyObject child");
        }
        return structuralProperty
            ? services.deferredContent->
                  StageProperty(
                      *services.deferredContentOwner,
                      object,
                      value.AsObject(),
                      *metadata,
                      services.targetMember)
            : services.deferredContent->Stage(
                  *services.deferredContentOwner,
                  object,
                  value.AsObject(),
                  *metadata,
                  services.targetMember);
    }

    if (plan == nullptr) {
        return InvalidContentState(
            "XAML visual content plan is unavailable");
    }
    Base::Result<Aero::UIElement*> childResult =
        ResolveUIElement(schema, *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();
    Base::Result<Aero::UIElement*> parentResult =
        ResolveUIElement(
            schema, object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();

    Base::Result<void> reserved = plan->Reserve(
        plan->contentEdges.Size() + 1U,
        plan->mountEdges.Size() + 1U,
        plan->nodes.Size() + 2U);
    if (!reserved) return reserved.GetStatus();

    Base::Result<Aero::Media::Visual*> parentNode =
        ResolveVisual(
            schema, object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<Aero::Media::Visual*> childNode =
        ResolveVisual(schema, *childObject, value.Type());
    if (!childNode) return childNode.GetStatus();

    Base::Result<void> parentAdded =
        plan->AddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<void> childAdded =
        plan->AddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();

    Base::Ref<Base::Object> parentOwner =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Base::Result<void> tracked =
        plan->contentEdges.PushBack({
            std::move(parentOwner), value.AsObject(),
            metadata, services.targetMember,
            structuralProperty});
    if (!tracked) return tracked.GetStatus();

    tracked = plan->mountEdges.PushBack({
        parentResult.Value(), childResult.Value(), {}});
    if (!tracked) {
        plan->contentEdges.PopBack();
        return tracked.GetStatus();
    }

    Base::Result<void> written =
        structuralProperty
        ? metadata->SetProperty(
              object,
              services.targetMember,
              value)
        : metadata->WriteContent(
              object,
              services.targetMember,
              value.AsObject());
    if (!written) {
        plan->mountEdges.PopBack();
        plan->contentEdges.PopBack();
        return written.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup
