#include "gui/meta/TypeRegistryDetail.hpp"
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
#include "gui/markup/MarkupCommon.hpp"
#include "gui/markup/XamlObjectWriterCommon.hpp"
#include "gui/markup/MarkupExtensionHost.hpp"
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/ServiceProvider.hpp>
#include <Aero/VisualStateManager.hpp>

// ===== ObjectWriter core / node stack =====

namespace Aero::Markup {
using namespace WriterSupport;
// ===== ObjectWriter node dispatch (Process/Start/End object/member) =====

Base::Result<void> ObjectWriter::ProcessNode(
    const Node& node) noexcept {
    switch (node.Kind()) {
    case NodeKind::NamespaceDeclaration:
        return QueueNamespaceDeclaration(node);
    case NodeKind::StartObject:
        return StartObject(node);
    case NodeKind::EndObject:
        return EndObject(node);
    case NodeKind::StartMember:
        return StartMember(node);
    case NodeKind::EndMember:
        return EndMember(node);
    case NodeKind::Value:
        return WriteText(node);
    case NodeKind::EndOfDocument:
        if (!frames_.Empty() || !root_ || !pendingNamespaces_.Empty() ||
            !namespaceBindings_.Empty()) {
            return Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                node.Source());
        }
        ended_ = true;
        return {};
    case NodeKind::None:
        break;
    }

    return Failure(
        InvalidStateStatus(),
        XamlObjectWriterDiagnosticCodes::InvalidWriterState,
        MessageInvalidWriterState,
        node.Source());
}

Base::Result<void> ObjectWriter::QueueNamespaceDeclaration(
    const Node& node) noexcept {
    if (node.NamespaceUri().Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNamespaceState.Data()),
            XamlObjectWriterDiagnosticCodes::NamespaceState,
            MessageNamespaceState,
            node.Source());
    }

    PendingNamespaceRecord record;
    Base::Result<void> prefixResult = record.prefix.AssignUnchecked(
        node.NamespacePrefix());
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = record.uri.AssignUnchecked(
        node.NamespaceUri());
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    record.source = node.Source();
    pendingNamespaces_.PushBack(std::move(record));
    return {};
}
Base::Result<void> ObjectWriter::StartObject(
    const Node& node) noexcept {
    if (frames_.Empty() && root_) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageMultipleRoots.Data()),
            XamlObjectWriterDiagnosticCodes::MultipleRootObjects,
            MessageMultipleRoots,
            node.Source());
    }

    std::uint32_t bindingStart = InvalidIndex;
    Base::Result<void> namespaceResult =
        ActivatePendingNamespaces(bindingStart);
    if (!namespaceResult) {
        return Failure(
            namespaceResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NamespaceState,
            MessageNamespaceState,
            node.Source());
    }

    if (!frames_.Empty() &&
        frames_.Back().kind == FrameKind::Object &&
        (node.CompiledMemberId() != Meta::InvalidMemberId ||
         HasPropertyElementSyntax(node.Name()))) {
        return StartPropertyElement(
            node,
            frames_.Size() - 1U,
            bindingStart);
    }

    if (IsXamlNullObject(node.Name())) {
        return StartNullObject(node, bindingStart);
    }

    Meta::TypeId typeId = Meta::InvalidTypeId;
    Meta::TypeFlags typeFlags = Meta::TypeFlags::None;
    const CompiledTypeBinding* compiledType = nullptr;
    Base::Status typeStatus;
    if (node.HasCompiledTypeBinding()) {
        const CompiledTypeBinding& binding = node.CompiledType();
        compiledType = &binding;
        typeId = binding.id;
        typeFlags = binding.flags;
    } else if (node.CompiledTypeId() != Meta::InvalidTypeId) {
        const Meta::TypeInfo* type =
            schema_->Types().FindType(node.CompiledTypeId());
        if (type != nullptr) {
            typeId = type->Id();
            typeFlags = type->Flags();
        } else {
            typeStatus = Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "AXB2 type id is absent from the frozen schema");
        }
    } else {
        Base::Result<const Meta::TypeInfo*> typeResult =
            schema_->ResolveType(
                node.Name().NamespaceUri(),
                node.Name().LocalName());
        if (typeResult) {
            typeId = typeResult.Value()->Id();
            typeFlags = typeResult.Value()->Flags();
        } else {
            typeStatus = typeResult.GetStatus();
        }
    }
    if (typeId == Meta::InvalidTypeId) {
        return Failure(
            typeStatus,
            XamlObjectWriterDiagnosticCodes::UnknownType,
            MessageUnknownType,
            node.Source());
    }

    if (HasTypeFlag(typeFlags, Meta::TypeFlags::ValueType)) {
        return StartValueObject(node, bindingStart, typeId);
    }
    Base::Result<Base::Ref<Base::Object>> createResult =
        CreateObject(typeId);
    if (!createResult) {
        const bool nonConstructible =
            createResult.GetStatus().code == Base::ErrorCode::Unsupported;
        // WPF permits abstract object types with a text converter to be used
        // as value elements. ImageSource is the canonical example:
        // <ImageSource>Images/Atlas.png</ImageSource> materializes a
        // BitmapImage through the registered ImageSource converter. Defer
        // construction until the element text is seen; conversion will still
        // reject abstract types that have no converter.
        if (nonConstructible &&
            HasTypeFlag(typeFlags, Meta::TypeFlags::Abstract) &&
            !frames_.Empty()) {
            return StartValueObject(node, bindingStart, typeId);
        }
        return Failure(
            createResult.GetStatus(),
            nonConstructible
                ? XamlObjectWriterDiagnosticCodes::TypeNotConstructible
                : XamlObjectWriterDiagnosticCodes::FactoryFailed,
            nonConstructible
                ? MessageTypeNotConstructible
                : MessageFactoryFailed,
            node.Source());
    }

    CreatedObjectRecord record;
    record.object = std::move(createResult).Value();
    record.type = typeId;
    if (loadContext_ != nullptr && loadContext_->existingRoot &&
        rootObjectIndex_ == InvalidIndex && frames_.Empty() &&
        record.object.Get() == loadContext_->existingRoot.Get()) {
        const Meta::TypeId runtimeType = record.object->RuntimeType();
        if (runtimeType == Meta::InvalidTypeId) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Existing XAML root has no runtime type"),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                node.Source());
        }
        record.type = runtimeType;
    }
    if (compiledType != nullptr &&
        compiledType->HasContentMember()) {
        record.contentMember =
            ResolveCompiledMember(compiledType->contentMember);
        record.contentPolicy =
            ResolveCompiledMemberPolicy(
                compiledType->contentMember);
        record.hasContentMember = true;
        record.contentValueTypeIsObject =
            compiledType->contentMember.ValueTypeIsObject();
        record.contentValueTypeIsValueType =
            compiledType->contentMember.ValueTypeIsValueType();
    } else if (schema_ != nullptr) {
        Base::Result<ResolvedMember> content =
            schema_->ResolveContentMember(typeId);
        if (content) {
            record.contentMember = content.Value();
            record.contentPolicy =
                schema_->ResolveMemberWritePolicy(
                    record.contentMember);
            record.hasContentMember = true;
            if (const Meta::TypeInfo* valueType =
                    schema_->Types().FindType(
                        record.contentMember.valueType)) {
                record.contentValueTypeIsObject =
                    valueType->Kind() ==
                        Meta::MetadataTypeKind::Object;
                record.contentValueTypeIsValueType =
                    valueType->Kind() !=
                        Meta::MetadataTypeKind::Object &&
                    HasTypeFlag(
                        valueType->Flags(),
                        Meta::TypeFlags::ValueType);
            }
        }
    }
    const std::uint32_t objectIndex = created_.Size();
    created_.PushBack(std::move(record));

    if (rootObjectIndex_ == InvalidIndex) {
        rootObjectIndex_ = objectIndex;
    }

    CreatedObjectRecord& stored = created_[objectIndex];
    stored.beginCalled = true;
    Base::Result<void> beginResult = schema_->BeginInit(
        stored.type,
        *stored.object);
    if (!beginResult) {
        return Failure(
            beginResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InitializationFailed,
            MessageInitializationFailed,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Object;
    frame.objectIndex = objectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    Base::Result<void> scopeResult = CreateScopesForObject(
        objectIndex,
        frame,
        node.Source());
    if (!scopeResult) {
        return scopeResult.GetStatus();
    }

    frames_.PushBack(frame);
    return {};
}

Base::Result<void> ObjectWriter::StartValueObject(
    const Node& node,
    std::uint32_t bindingStart,
    Meta::TypeId type) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Object &&
         frames_.Back().kind != FrameKind::Member)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageTypeMismatch.Data()),
            XamlObjectWriterDiagnosticCodes::TypeMismatch,
            MessageTypeMismatch,
            node.Source());
    }

    CreatedObjectRecord record;
    record.type = type;
    record.valueElement = true;
    const std::uint32_t objectIndex = created_.Size();
    created_.PushBack(
        std::move(record));

    Frame frame;
    frame.kind = FrameKind::ValueObject;
    frame.objectIndex = objectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    frames_.PushBack(frame);
    return {};
}

Base::Result<void> ObjectWriter::StartNullObject(
    const Node& node,
    std::uint32_t bindingStart) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Member &&
         frames_.Back().kind != FrameKind::Object)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::NullObject;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    frames_.PushBack(frame);
    return {};
}

Base::Result<void> ObjectWriter::EndObject(
    const Node& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::ValueObject) {
        return CompleteValueObject(node);
    }
    if (frame.kind == FrameKind::NullObject) {
        return CompleteNullObject(node);
    }
    if (frame.kind == FrameKind::Member) {
        if (!frame.propertyElement) {
            return Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                node.Source());
        }
        if (frame.valuesWritten == 0U) {
            const Meta::TypeInfo* valueType =
                schema_->Types().FindType(
                    frame.member.valueType);
            // WPF permits an empty property element for reference-valued
            // properties. It means "leave the property's current/default
            // value in place", which is required by empty
            // Application.Resources declarations.
            if (valueType == nullptr ||
                valueType->Kind() !=
                    Meta::MetadataTypeKind::Object) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageMissingMemberValue.Data()),
                    XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                    MessageMissingMemberValue,
                    node.Source());
            }
        }
        const std::uint32_t bindingStart = frame.namespaceBindingStart;
        frames_.PopBack();
        PopNamespaceBindings(bindingStart);
        return {};
    }
    if (frame.kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    return CompleteObject(node);
}
Base::Result<void> ObjectWriter::StartMember(
    const Node& node) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Object &&
         frames_.Back().kind != FrameKind::ValueObject)) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame& objectFrame = frames_.Back();
    if (objectFrame.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    if (objectFrame.kind == FrameKind::ValueObject) {
        if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {
            if (IsXamlDirective(node.Name(), DirectiveKey)) {
                return StartDirective(
                    node, DirectiveKind::Key, objectFrame.objectIndex);
            }
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        if (!node.IsFromAttribute() ||
            node.Name().LocalName() != Base::StringView("Value")) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    MessageUnknownMember.Data()),
                XamlObjectWriterDiagnosticCodes::UnknownMember,
                MessageUnknownMember,
                node.Source());
        }
        Frame frame;
        frame.kind = FrameKind::ValueMember;
        frame.targetObjectIndex = objectFrame.objectIndex;
        frame.source = node.Source();
        frames_.PushBack(frame);
        return {};
    }

    if (node.IsFromAttribute() &&
        node.Name().LocalName() == DirectiveName) {
        return StartDirective(
            node,
            DirectiveKind::Name,
            objectFrame.objectIndex);
    }

    if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {
        if (IsXamlDirective(node.Name(), DirectiveName)) {
            return StartDirective(
                node,
                DirectiveKind::Name,
                objectFrame.objectIndex);
        }
        if (IsXamlDirective(node.Name(), DirectiveKey)) {
            return StartDirective(
                node,
                DirectiveKind::Key,
                objectFrame.objectIndex);
        }
        if (IsXamlDirective(node.Name(), DirectiveClass)) {
            return StartDirective(
                node,
                DirectiveKind::Class,
                objectFrame.objectIndex);
        }
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    ResolvedMember member;
    MemberWritePolicy memberPolicy;
    bool hasCompiledPolicy = false;
    bool memberValueTypeIsObject = false;
    bool memberValueTypeIsValueType = false;
    if (node.HasCompiledMemberBinding()) {
        member = ResolveCompiledMember(
            node.CompiledMember());
        memberPolicy = ResolveCompiledMemberPolicy(
            node.CompiledMember());
        hasCompiledPolicy = true;
        memberValueTypeIsObject =
            node.CompiledMember().ValueTypeIsObject();
        memberValueTypeIsValueType =
            node.CompiledMember().ValueTypeIsValueType();
        if (!IsCompiledMemberCompatible(
                *schema_,
                created_[objectFrame.objectIndex].type,
                member)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidAttachedMember.Data()),
                XamlObjectWriterDiagnosticCodes::
                    InvalidAttachedMember,
                MessageInvalidAttachedMember,
                node.Source());
        }
    } else {
        Base::Result<ResolvedMember> memberResult =
            node.CompiledMemberId() != Meta::InvalidMemberId
            ? schema_->ResolveMember(
                  created_[objectFrame.objectIndex].type,
                  node.CompiledMemberId())
            : schema_->ResolveMember(
                  created_[objectFrame.objectIndex].type,
                  node.Name(),
                  MemberSyntax::Attribute);
        if (!memberResult) {
            const bool notFound =
                memberResult.GetStatus().code ==
                    Base::ErrorCode::NotFound;
            return Failure(
                memberResult.GetStatus(),
                notFound
                    ? XamlObjectWriterDiagnosticCodes::UnknownMember
                    : XamlObjectWriterDiagnosticCodes::
                        InvalidAttachedMember,
                notFound
                    ? MessageUnknownMember
                    : MessageInvalidAttachedMember,
                node.Source());
        }
        member = memberResult.Value();
        memberPolicy =
            schema_->ResolveMemberWritePolicy(member);
        if (const Meta::TypeInfo* valueType =
                schema_->Types().FindType(
                    member.valueType)) {
            memberValueTypeIsObject =
                valueType->Kind() ==
                    Meta::MetadataTypeKind::Object;
            memberValueTypeIsValueType =
                HasTypeFlag(
                    valueType->Flags(),
                    Meta::TypeFlags::ValueType);
        }
    }

    const bool eventAttribute =
        member.kind == Meta::MemberKind::Event &&
        node.IsFromAttribute();
    if (!eventAttribute &&
        (member.kind != Meta::MemberKind::Property ||
         !memberPolicy.writable)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Member;
    frame.targetObjectIndex = objectFrame.objectIndex;
    frame.member = member;
    frame.memberPolicy = memberPolicy;
    frame.hasMemberPolicy =
        hasCompiledPolicy;
    frame.memberValueTypeIsObject =
        memberValueTypeIsObject;
    frame.memberValueTypeIsValueType =
        memberValueTypeIsValueType;
    frame.source = node.Source();
    frame.propertyElement = false;
    frames_.PushBack(frame);
    return {};
}

Base::Result<void> ObjectWriter::StartDirective(
    const Node& node,
    DirectiveKind directive,
    std::uint32_t targetObjectIndex) noexcept {
    if (!node.IsFromAttribute() || targetObjectIndex >= created_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    const CreatedObjectRecord& record = created_[targetObjectIndex];
    if ((directive == DirectiveKind::Name &&
         (!record.name.Empty() || record.nameRegistered)) ||
        (directive == DirectiveKind::Key &&
         (!record.key.Empty() || record.resourceRegistered))) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Directive;
    frame.directive = directive;
    frame.targetObjectIndex = targetObjectIndex;
    frame.source = node.Source();
    frames_.PushBack(frame);
    return {};
}

Base::Result<void> ObjectWriter::EndMember(
    const Node& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::ValueMember) {
        if (frame.valuesWritten != 1U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageMissingMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                MessageMissingMemberValue,
                node.Source());
        }
        frames_.PopBack();
        return {};
    }
    if (frame.kind == FrameKind::Directive) {
        if (frame.valuesWritten != 1U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageMissingMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                MessageMissingMemberValue,
                node.Source());
        }
        frames_.PopBack();
        return {};
    }

    if (frame.kind != FrameKind::Member || frame.propertyElement) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    if (frame.valuesWritten == 0U && !frame.deferredStaticResource) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageMissingMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::MissingMemberValue,
            MessageMissingMemberValue,
            node.Source());
    }
    frames_.PopBack();
    return {};
}

} // namespace Aero::Markup
