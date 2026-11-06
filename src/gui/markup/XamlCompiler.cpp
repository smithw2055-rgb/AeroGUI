#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/data/BindingCommon.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/media/AnimationModel.hpp"

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>
#include <Aero/VisualStateManager.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>


// ============================================================================
// SECTION: XamlCompiledSchema / Manifest / Validation
// Source: src/gui/markup/XamlCompiledSchema.cpp
// ============================================================================

// ===== CompiledSchema =====


// Canonical compiled-schema bridge used by Loader.



namespace Aero::Markup {
namespace {

Base::Status SchemaNodeFailure(
    Base::Status status,
    const Node& node) noexcept {
    thread_local char message[512];
    const Base::StringView localName = node.Name().LocalName();
    const ::Aero::Diagnostics::SourcePosition position = node.Source().begin;
    std::snprintf(
        message,
        sizeof(message),
        "Compiled XAML schema error at %u:%u for '%.*s': %s",
        position.line,
        position.column,
        static_cast<int>(localName.SizeBytes()),
        localName.Data(),
        status.message != nullptr ? status.message : "operation failed");
    return Base::Status::Failure(status.code, message);
}

Base::Result<SchemaTypeInfo> ResolveTypeInfo(
    const Schema& schema,
    Base::StringView xamlNamespace,
    Base::StringView localName) noexcept {
    Base::Result<const Meta::TypeInfo*> type =
        schema.ResolveType(xamlNamespace, localName);
    if (!type) return type.GetStatus();
    return SchemaTypeInfo{
        type->Id(),
        type->Kind(),
        type->Flags()};
}

Base::Result<SchemaTypeInfo> ResolveTypeInfo(
    const SchemaManifest& schema,
    Base::StringView xamlNamespace,
    Base::StringView localName) noexcept {
    return schema.ResolveType(xamlNamespace, localName);
}

inline Base::StringView TrimCompiledText(
    Base::StringView value) noexcept {
    return ::Aero::Base::ValueConversion::Trim(value);
}

inline bool IsWhitespaceOnly(
    Base::StringView value) noexcept {
    return TrimCompiledText(value).Empty();
}

inline bool EqualsAsciiInsensitive(
    Base::StringView value,
    Base::StringView expected) noexcept {
    return ::Aero::Base::ValueConversion::EqualsAsciiInsensitive(value, expected);
}

bool TryGetCompiledLiteral(
    Base::StringView authored,
    Base::StringView& literal) noexcept {
    const Base::StringView trimmed =
        TrimCompiledText(authored);
    if (trimmed.Empty() || trimmed[0] != '{') {
        literal = authored;
        return true;
    }
    if (trimmed.SizeBytes() >= 2U &&
        trimmed[1] == '}') {
        literal = trimmed.Substr(
            2U, trimmed.SizeBytes() - 2U);
        return true;
    }
    return false;
}

bool IsPersistableCompiledCustomType(
    Meta::TypeId type) noexcept {
    return type == Meta::TypeOf<::Aero::Length>() ||
        type == Meta::TypeOf<Base::Thickness>() ||
        type == Meta::TypeOf<Base::CornerRadius>() ||
        type == Meta::TypeOf<Base::Color>() ||
        type == Meta::TypeOf<Base::Point>() ||
        type == Meta::TypeOf<Base::Transform2D>() ||
        type == Meta::TypeOf<::Aero::GridLength>();
}

bool IsPersistableCompiledValue(
    const Meta::Value& value) noexcept {
    if (value.Type() == Meta::InvalidTypeId) return false;
    switch (value.Kind()) {
    case Meta::ValueKind::Boolean:
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double:
    case Meta::ValueKind::String:
        return true;
    case Meta::ValueKind::Custom:
        return IsPersistableCompiledCustomType(value.Type());
    case Meta::ValueKind::Unset:
    case Meta::ValueKind::Object:
    case Meta::ValueKind::DynamicResource:
        return false;
    }
    return false;
}

template<class T>
Base::Result<Meta::Value> MakeManifestCustomValue(
    Meta::TypeId type,
    const T& value) noexcept {
    static const Meta::ValueTypeRegistration s_reg =
        Meta::MakeValueTypeRegistration<T>();
    return Meta::Value::TryFromCustom(
        type, &value, &s_reg);
}



Base::Result<Meta::Value>
ConvertManifestPrimitive(
    Meta::TypeId type,
    Base::StringView literal) noexcept {
    if (type == Meta::TypeOf<Base::String>()) {
        return Meta::Value::TryFromString(type, literal);
    }

    const Base::StringView text =
        TrimCompiledText(literal);
    if (type == Meta::TypeOf<bool>()) {
        Base::Result<bool> parsed =
            ::Aero::Base::ValueConversion::ConvertBoolean(text);
        if (!parsed) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Boolean literal is invalid");
        }
        return Meta::Value::FromBoolean(type, parsed.Value());
    }

    if (type == Meta::TypeOf<double>()) {
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::ConvertDouble(text);
        if (!parsed) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Double literal is invalid");
        }
        return Meta::Value::FromDouble(type, parsed.Value());
    }

    auto convertSigned = [&](auto dummy) -> Base::Result<Meta::Value> {
        using T = decltype(dummy);
        Base::Result<T> parsed = ::Aero::Base::ValueConversion::ConvertInteger<T>(text);
        if (!parsed) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB2 signed integer literal is invalid");
        return Meta::Value::FromSignedInteger(type, parsed.Value());
    };
    if (type == Meta::TypeOf<std::int8_t>()) return convertSigned(std::int8_t{});
    if (type == Meta::TypeOf<std::int16_t>()) return convertSigned(std::int16_t{});
    if (type == Meta::TypeOf<std::int32_t>()) return convertSigned(std::int32_t{});
    if (type == Meta::TypeOf<std::int64_t>()) return convertSigned(std::int64_t{});

    auto convertUnsigned = [&](auto dummy) -> Base::Result<Meta::Value> {
        using T = decltype(dummy);
        Base::Result<T> parsed = ::Aero::Base::ValueConversion::ConvertInteger<T>(text);
        if (!parsed) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB2 unsigned integer literal is invalid");
        return Meta::Value::FromUnsignedInteger(type, parsed.Value());
    };
    if (type == Meta::TypeOf<std::uint8_t>()) return convertUnsigned(std::uint8_t{});
    if (type == Meta::TypeOf<std::uint16_t>()) return convertUnsigned(std::uint16_t{});
    if (type == Meta::TypeOf<std::uint32_t>()) return convertUnsigned(std::uint32_t{});
    if (type == Meta::TypeOf<std::uint64_t>()) return convertUnsigned(std::uint64_t{});

    if (type == Meta::TypeOf<::Aero::Length>()) {
        if (EqualsAsciiInsensitive(text, "auto")) {
            return MakeManifestCustomValue(
                type, ::Aero::Length::Auto());
        }
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::
                ParseDouble(text);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Length must be Auto or nonnegative");
        }
        return MakeManifestCustomValue(
            type,
            ::Aero::Length::Pixels(parsed.Value()));
    }

    if (type == Meta::TypeOf<Base::Thickness>()) {
        Base::Result<Base::Thickness> parsed =
            ::Aero::Base::ValueConversion::ConvertThickness(text);
        if (!parsed) return parsed.GetStatus();
        return MakeManifestCustomValue(type, parsed.Value());
    }

    if (type == Meta::TypeOf<Base::CornerRadius>()) {
        Base::Result<Base::CornerRadius> parsed =
            ::Aero::Base::ValueConversion::ConvertCornerRadius(text);
        if (!parsed) return parsed.GetStatus();
        return MakeManifestCustomValue(type, parsed.Value());
    }

    if (type == Meta::TypeOf<Base::Point>()) {
        Base::Result<Base::Point> parsed =
            ::Aero::Base::ValueConversion::ConvertPoint(text);
        if (!parsed) return parsed.GetStatus();
        return MakeManifestCustomValue(type, parsed.Value());
    }

    if (type == Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            ::Aero::Base::ValueConversion::ConvertColor(text);
        if (!color) return color.GetStatus();
        return MakeManifestCustomValue(type, color.Value());
    }

    if (type == Meta::TypeOf<Base::Transform2D>()) {
        Base::Result<Base::Transform2D> matrix =
            ::Aero::Base::ValueConversion::ConvertMatrix(text);
        if (!matrix) return matrix.GetStatus();
        return MakeManifestCustomValue(type, matrix.Value());
    }

    if (type == Meta::TypeOf<::Aero::GridLength>()) {
        Base::Result<::Aero::GridLength> length =
            ::Aero::Base::ValueConversion::ConvertGridLength(text);
        if (!length) return length.GetStatus();
        return MakeManifestCustomValue(type, length.Value());
    }

    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "AXB2 manifest cannot preconvert this value type");
}

Base::Result<void> BindCompiledValue(
    const Schema& schema,
    Node& node,
    Meta::TypeId valueType) noexcept {
    if (node.HasCompiledValue() &&
        node.CompiledValue().Type() == valueType) {
        return {};
    }
    Base::StringView literal;
    if (valueType == Meta::InvalidTypeId ||
        !TryGetCompiledLiteral(node.Value(), literal)) {
        return {};
    }
    if (valueType != Meta::TypeOf<Meta::Value>() &&
        valueType != Meta::TypeOf<Base::String>() &&
        !IsPersistableCompiledCustomType(valueType)) {
        if (const Meta::TypeInfo* typeInfo =
                schema.Types().FindType(valueType)) {
            const Meta::MetadataTypeKind kind = typeInfo->Kind();
            if (kind == Meta::MetadataTypeKind::Object ||
                kind == Meta::MetadataTypeKind::Interface ||
                kind == Meta::MetadataTypeKind::Struct) {
                return {};
            }
        }
    }
    Base::Result<Meta::Value> converted =
        schema.ConvertText(valueType, literal);
    if (converted &&
        IsPersistableCompiledValue(converted.Value())) {
        node.BindCompiledValue(
            std::move(converted).Value());
    }
    return {};
}

Base::Result<void> BindCompiledValue(
    const SchemaManifest&,
    Node& node,
    Meta::TypeId valueType) noexcept {
    if (node.HasCompiledValue() &&
        node.CompiledValue().Type() == valueType) {
        return {};
    }
    Base::StringView literal;
    if (valueType == Meta::InvalidTypeId ||
        !TryGetCompiledLiteral(node.Value(), literal)) {
        return {};
    }
    Base::Result<Meta::Value> converted =
        ConvertManifestPrimitive(valueType, literal);
    if (converted &&
        IsPersistableCompiledValue(converted.Value())) {
        node.BindCompiledValue(
            std::move(converted).Value());
    }
    return {};
}

CompiledMemberBinding BuildCompiledMemberBinding(
    const Schema& schema,
    const ResolvedMember& member) noexcept;

void BindCompiledMemberInstruction(
    const Schema& schema,
    Node& node,
    const ResolvedMember& member) noexcept {
    node.BindCompiledMember(
        BuildCompiledMemberBinding(schema, member));
}

CompiledMemberBinding BuildCompiledMemberBinding(
    const Schema& schema,
    const ResolvedMember& member) noexcept {
    const MemberWritePolicy policy =
        schema.ResolveMemberWritePolicy(member);
    CompiledMemberBinding binding;
    binding.id = member.id;
    binding.kind = member.kind;
    binding.ownerType = member.ownerType;
    binding.valueType = member.valueType;
    if (const Meta::TypeInfo* valueType =
            schema.Types().FindType(member.valueType)) {
        binding.valueTypeKind = valueType->Kind();
        binding.valueTypeFlags = valueType->Flags();
    }
    binding.propertyFlags = member.propertyFlags;
    binding.eventFlags = member.eventFlags;
    binding.writeMode =
        static_cast<std::uint8_t>(policy.mode);
    binding.attached = member.attached;
    binding.acceptsAnyValue =
        policy.acceptsAnyValue;
    binding.writable = policy.writable;
    return binding;
}

void BindCompiledMemberInstruction(
    const SchemaManifest&,
    Node& node,
    const ResolvedMember& member) noexcept {
    // A manifest intentionally carries no runtime accessor/facet callbacks.
    // Persist the stable id; the target runtime expands it after identity
    // validation during AXB2 deserialization.
    node.BindCompiledMember(member.id);
}

Base::Result<void> BindCompiledTypeInstruction(
    const Schema& schema,
    Node& node,
    const SchemaTypeInfo& type) noexcept {
    CompiledTypeBinding binding;
    binding.id = type.id;
    binding.kind = type.kind;
    binding.flags = type.flags;

    Base::Result<ResolvedMember> content =
        schema.ResolveContentMember(type.id);
    if (content) {
        binding.contentMember =
            BuildCompiledMemberBinding(
                schema, content.Value());
        binding.hasContentMember = true;
    } else if (content.GetStatus().code !=
               Base::ErrorCode::NotFound) {
        return content.GetStatus();
    }

    node.BindCompiledType(binding);
    return {};
}

Base::Result<void> BindCompiledTypeInstruction(
    const SchemaManifest&,
    Node& node,
    const SchemaTypeInfo& type) noexcept {
    // Manifest files intentionally carry no executable content/accessor
    // policy. The target runtime expands the stable TypeId after validating
    // the AXB2 identity.
    node.BindCompiledType(type.id);
    return {};
}

template<class TSchema>
Base::Result<void> ValidateSchemaCore(
    const CompiledDocument& document,
    const TSchema& schema,
    bool bindInstructions = false) noexcept {
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML schema validation requires a valid document");
    }
    enum class FrameKind : std::uint8_t {
        Object = 0U,
        ValueObject,
        Member,
        PropertyElement,
        NullObject
    };
    struct Frame {
        FrameKind kind = FrameKind::Object;
        Meta::TypeId type = Meta::InvalidTypeId;
        Meta::TypeId contentValueType = Meta::InvalidTypeId;
        Base::Status contentStatus{};
        bool hasKnownContent = false;
    };
    auto ensureFrameContent = [&schema](Frame& frame) noexcept -> Base::Status {
        if (!frame.hasKnownContent) {
            frame.hasKnownContent = true;
            Base::Result<ResolvedMember> content =
                schema.ResolveContentMember(frame.type);
            if (content) {
                frame.contentValueType = content.Value().valueType;
                frame.contentStatus = Base::Status::Ok();
            } else {
                frame.contentValueType = Meta::InvalidTypeId;
                frame.contentStatus = content.GetStatus();
            }
        }
        return frame.contentStatus;
    };
    Base::Vector<Frame> frames;
    bool rootSeen = false;
    for (const Node& node : document.Nodes()) {
        switch (node.Kind()) {
        case NodeKind::NamespaceDeclaration:
            break;
        case NodeKind::Value: {
            if (!bindInstructions || frames.Empty() ||
                (!node.IsFromAttribute() &&
                 IsWhitespaceOnly(node.Value()))) {
                break;
            }
            Frame& frame = frames.Back();
            Meta::TypeId valueType = Meta::InvalidTypeId;
            if (frame.kind == FrameKind::Member ||
                frame.kind == FrameKind::PropertyElement ||
                frame.kind == FrameKind::ValueObject) {
                valueType = frame.type;
            } else if (frame.kind == FrameKind::Object) {
                if (ensureFrameContent(frame).IsOk()) {
                    valueType = frame.contentValueType;
                }
            }
            Base::Result<void> bound = BindCompiledValue(
                schema,
                const_cast<Node&>(node),
                valueType);
            if (!bound) return bound.GetStatus();
            break;
        }
        case NodeKind::StartObject: {
            const bool nullObject =
                node.Name().NamespaceUri() == LanguageNamespaceUri() &&
                node.Name().LocalName() == Base::StringView("Null");
            bool propertyElement = false;
            for (std::uint32_t index = 0U;
                 index < node.Name().LocalName().SizeBytes(); ++index) {
                propertyElement = propertyElement ||
                    node.Name().LocalName()[index] == '.';
            }
            if (propertyElement && !frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                Base::Result<ResolvedMember> member = schema.ResolveMember(
                    frames.Back().type,
                    node.Name(),
                    MemberSyntax::PropertyElement);
                if (!member) {
                    return SchemaNodeFailure(member.GetStatus(), node);
                }
                if (bindInstructions) {
                    BindCompiledMemberInstruction(
                        schema,
                        const_cast<Node&>(node),
                        member.Value());
                }
                frames.PushBack({
                    FrameKind::PropertyElement,
                    member.Value().valueType});
                break;
            }
            if (nullObject) {
                frames.PushBack({
                    FrameKind::NullObject,
                    Meta::InvalidTypeId});
                break;
            }
            Base::Result<SchemaTypeInfo> type = ResolveTypeInfo(
                schema,
                node.Name().NamespaceUri(),
                node.Name().LocalName());
            if (!type) {
                return SchemaNodeFailure(type.GetStatus(), node);
            }
            if (bindInstructions) {
                Base::Result<void> boundType =
                    BindCompiledTypeInstruction(
                        schema,
                        const_cast<Node&>(node),
                        type.Value());
                if (!boundType) {
                    return boundType.GetStatus();
                }
            }
            if (!frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                const Base::Status contentStatus =
                    ensureFrameContent(frames.Back());
                if (!contentStatus.IsOk()) {
                    return contentStatus;
                }
            } else if (!frames.Empty() &&
                frames.Back().kind != FrameKind::Member &&
                frames.Back().kind != FrameKind::PropertyElement) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML object nesting is invalid");
            } else if (frames.Empty()) {
                if (rootSeen) {
                    return Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        "Compiled XAML contains multiple roots");
                }
                rootSeen = true;
            }
            Frame nextFrame{};
            nextFrame.kind = Meta::HasTypeFlag(
                type.Value().flags,
                Meta::TypeFlags::ValueType)
                ? FrameKind::ValueObject
                : FrameKind::Object;
            nextFrame.type = type.Value().id;
            if constexpr (std::is_same_v<TSchema, Schema>) {
                if (bindInstructions && node.HasCompiledTypeBinding()) {
                    nextFrame.hasKnownContent = true;
                    if (node.CompiledType().HasContentMember()) {
                        nextFrame.contentValueType =
                            node.CompiledType().contentMember.valueType;
                        nextFrame.contentStatus = Base::Status::Ok();
                    } else {
                        nextFrame.contentValueType = Meta::InvalidTypeId;
                        nextFrame.contentStatus = Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "XAML type has no content property");
                    }
                }
            }
            frames.PushBack(nextFrame);
            break;
        }
        case NodeKind::EndObject:
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
                 frames.Back().kind != FrameKind::ValueObject &&
                 frames.Back().kind != FrameKind::PropertyElement &&
                 frames.Back().kind != FrameKind::NullObject)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML object frame is unbalanced");
            }
            frames.PopBack();
            break;
        case NodeKind::StartMember: {
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
                 frames.Back().kind != FrameKind::ValueObject)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member has no object owner");
            }
            Meta::TypeId memberValueType = Meta::InvalidTypeId;
            if (frames.Back().kind == FrameKind::ValueObject &&
                node.Name().NamespaceUri() != LanguageNamespaceUri()) {
                if (!node.IsFromAttribute() ||
                    node.Name().LocalName() != Base::StringView("Value")) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Compiled XAML value-type member was not found");
                }
                memberValueType = frames.Back().type;
            } else if (
                node.IsFromAttribute() &&
                node.Name().LocalName() == Base::StringView("Name")) {
                // WPF treats the ordinary Name attribute as the x:Name
                // directive. The object writer performs the name-scope
                // registration and also initializes a real Name property
                // when the target type exposes one.
            } else if (node.Name().NamespaceUri() !=
                       LanguageNamespaceUri()) {
                Base::Result<ResolvedMember> member = schema.ResolveMember(
                    frames.Back().type,
                    node.Name(),
                    MemberSyntax::Attribute);
                if (!member) {
                    return SchemaNodeFailure(member.GetStatus(), node);
                }
                memberValueType = member.Value().valueType;
                if (bindInstructions) {
                    BindCompiledMemberInstruction(
                        schema,
                        const_cast<Node&>(node),
                        member.Value());
                }
            } else if (
                (frames.Back().kind == FrameKind::ValueObject &&
                 node.Name().LocalName() != Base::StringView("Key")) ||
                (frames.Back().kind == FrameKind::Object &&
                 node.Name().LocalName() != Base::StringView("Name") &&
                 node.Name().LocalName() != Base::StringView("Key") &&
                 node.Name().LocalName() != Base::StringView("Class"))) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Compiled XAML directive is not supported");
            }
            frames.PushBack({
                FrameKind::Member,
                memberValueType});
            break;
        }
        case NodeKind::EndMember:
            if (frames.Empty() ||
                frames.Back().kind != FrameKind::Member) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member frame is unbalanced");
            }
            frames.PopBack();
            break;
        case NodeKind::EndOfDocument:
            if (!frames.Empty() || !rootSeen) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML document is incomplete");
            }
            return {};
        case NodeKind::None:
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML contains an empty node");
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Compiled XAML has no end-of-document node");
}

void PrewarmCompiledDocumentBytecode(
    const CompiledDocument& document,
    const Schema& schema) noexcept {
    if (document.BytecodeProgram()) {
        return;
    }
    if (!Meta::HasObjectFactory()) {
        return;
    }
    const Meta::ObjectFactoryState factory =
        Meta::CurrentObjectFactory();
    if (!factory.IsValid() ||
        schema.Metadata() == nullptr ||
        (factory.metadata != nullptr &&
         factory.metadata != schema.Metadata()) ||
        factory.dependencyProperties !=
            &schema.Metadata()->DependencyProperties()) {
        return;
    }
    const Base::Ref<Meta::RegistryLifetime>& lifetime =
        factory.dependencyProperties->Lifetime();
    if (lifetime && !lifetime->isAlive) {
        return;
    }

    Base::Result<const XamlBytecodeProgram*> compiled =
        EnsureDocumentBytecode(document, schema);
    if (!compiled || compiled.Value() == nullptr ||
        !compiled.Value()->IsValid() || !document.BytecodeProgram()) {
        document.SetBytecodeProgram({});
    }
}

} // namespace

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const Schema& schema) noexcept {
    return Compile(reader, schema, {});
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const Schema& schema,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledDocument> compiled =
        Compile(reader, schema.Domain(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<void> valid = compiled.Value().BindSchema(schema);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const SchemaManifest& manifest) noexcept {
    return Compile(reader, manifest, {});
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const SchemaManifest& manifest,
    const Base::ResourceUri& originUri) noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML requires a valid schema manifest");
    }
    Base::Result<CompiledDocument> compiled =
        CompileWithIdentity(reader, manifest.Identity(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<void> valid = compiled.Value().BindSchema(manifest);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<void> CompiledDocument::ValidateSchema(
    const Schema& schema) const noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML validation requires a frozen runtime schema");
    }
    return ValidateSchemaCore(*this, schema);
}

Base::Result<void> CompiledDocument::BindSchema(
    const Schema& schema) noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 binding requires a frozen runtime schema");
    }
    bytecodeProgram_.Reset();
    Base::Result<void> valid = ValidateSchemaCore(*this, schema, true);
    if (!valid) return valid.GetStatus();
    PrewarmCompiledDocumentBytecode(*this, schema);
    return {};
}

Base::Result<void> CompiledDocument::ValidateSchema(
    const SchemaManifest& manifest) const noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML validation requires a valid schema manifest");
    }
    if (CompareCompiledCacheIdentity(identity_, manifest.Identity()) !=
        CompiledCacheCompatibility::Compatible) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML identity does not match the schema manifest");
    }
    return ValidateSchemaCore(*this, manifest);
}

Base::Result<void> CompiledDocument::BindSchema(
    const SchemaManifest& manifest) noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 binding requires a valid schema manifest");
    }
    if (CompareCompiledCacheIdentity(identity_, manifest.Identity()) !=
        CompiledCacheCompatibility::Compatible) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 identity does not match the schema manifest");
    }
    bytecodeProgram_.Reset();
    return ValidateSchemaCore(*this, manifest, true);
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: XamlCompiledDocument / AXB2 Serialization
// Source: src/gui/markup/XamlCompiledDocument.cpp
// ============================================================================


// ===== CompiledDocument =====


// Canonical compiled-document implementation.

namespace Aero::Markup {
namespace {

constexpr std::uint32_t CompiledDocumentMagic =
    UINT32_C(0x32425841); // "AXB2"
constexpr std::uint32_t InvalidTableIndex = UINT32_MAX;

enum class AxbSectionKind : std::uint32_t {
    Dependencies = 1U,
    Strings = 2U,
    Types = 3U,
    Members = 4U,
    Values = 5U,
    Instructions = 6U,
    SourceMap = 7U,
    SourceFallback = 8U
};

struct AxbSection {
    AxbSectionKind kind = AxbSectionKind::Strings;
    std::uint32_t count = 0U;
    Base::Vector<std::uint8_t> bytes;
};

struct AxbSectionDirectoryEntry {
    AxbSectionKind kind = AxbSectionKind::Strings;
    std::uint32_t offset = 0U;
    std::uint32_t size = 0U;
    std::uint32_t count = 0U;
};

enum class AxbValueKind : std::uint8_t {
    Text = 0U,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Double,
    String,
    Custom
};

inline constexpr std::uint8_t AxbMaxValuePayloads = 6U;

struct AxbValueRecord {
    AxbValueKind kind = AxbValueKind::Text;
    std::uint8_t payloadCount = 0U;
    std::uint32_t typeIndex = InvalidTableIndex;
    std::uint64_t payload[AxbMaxValuePayloads]{};
};

enum AxbInstructionFlag : std::uint8_t {
    AxbInstructionFromAttribute = 1U << 0U,
    AxbInstructionHasType = 1U << 1U,
    AxbInstructionHasMember = 1U << 2U,
    AxbInstructionHasValue = 1U << 3U,
    AxbInstructionHasQualifiedName = 1U << 4U,
    AxbInstructionHasNamespace = 1U << 5U
};

inline constexpr std::uint8_t AxbInstructionKnownFlags =
    AxbInstructionFromAttribute |
    AxbInstructionHasType |
    AxbInstructionHasMember |
    AxbInstructionHasValue |
    AxbInstructionHasQualifiedName |
    AxbInstructionHasNamespace;

bool HasInstructionFlag(
    std::uint8_t flags,
    AxbInstructionFlag flag) noexcept {
    return (flags & static_cast<std::uint8_t>(flag)) != 0U;
}

bool NeedsInstructionQualifiedName(
    const Node& node) noexcept {
    if (node.Kind() == NodeKind::StartObject) {
        return node.CompiledTypeId() == Meta::InvalidTypeId &&
            node.CompiledMemberId() == Meta::InvalidMemberId;
    }
    if (node.Kind() == NodeKind::StartMember) {
        return node.CompiledMemberId() == Meta::InvalidMemberId;
    }
    return false;
}

bool IsInstructionShapeValid(
    NodeKind kind,
    std::uint8_t flags) noexcept {
    if ((flags & ~AxbInstructionKnownFlags) != 0U) {
        return false;
    }

    const bool hasValue = HasInstructionFlag(
        flags, AxbInstructionHasValue);
    const bool hasNamespace = HasInstructionFlag(
        flags, AxbInstructionHasNamespace);

    if (hasNamespace !=
        (kind == NodeKind::NamespaceDeclaration)) {
        return false;
    }

    return hasValue == (kind == NodeKind::Value);
}


template<class TDestination, class TSource>
TDestination CopyValueBits(
    const TSource& source) noexcept {
    static_assert(
        sizeof(TDestination) == sizeof(TSource),
        "AXB2 value bit copy requires equal sizes");
    TDestination destination{};
    std::memcpy(
        &destination, &source, sizeof(destination));
    return destination;
}

std::uint64_t PackFloatPair(
    float low,
    float high) noexcept {
    const std::uint32_t lowBits =
        CopyValueBits<std::uint32_t>(low);
    const std::uint32_t highBits =
        CopyValueBits<std::uint32_t>(high);
    return static_cast<std::uint64_t>(lowBits) |
        (static_cast<std::uint64_t>(highBits) << 32U);
}

float UnpackFloat(
    std::uint64_t payload,
    bool high) noexcept {
    const std::uint32_t bits = high
        ? static_cast<std::uint32_t>(payload >> 32U)
        : static_cast<std::uint32_t>(payload);
    return CopyValueBits<float>(bits);
}

Base::Result<void> AppendPosition(
    Base::Vector<std::uint8_t>& output,
    ::Aero::Diagnostics::SourcePosition position) noexcept {
    Base::Result<void> result =
        AppendVarU32(output, position.line);
    if (!result) return result.GetStatus();
    result = AppendVarU32(output, position.column);
    if (!result) return result.GetStatus();
    return AppendVarU64(output, position.byteOffset);
}

Base::Result<::Aero::Diagnostics::SourcePosition> ReadPosition(
    Decoder& decoder) noexcept {
    Base::Result<std::uint32_t> line =
        decoder.ReadVarU32();
    if (!line) return line.GetStatus();
    Base::Result<std::uint32_t> column =
        decoder.ReadVarU32();
    if (!column) return column.GetStatus();
    Base::Result<std::uint64_t> offset =
        decoder.ReadVarU64();
    if (!offset) return offset.GetStatus();
    return ::Aero::Diagnostics::SourcePosition{
        line.Value(), column.Value(), offset.Value()};
}

struct AxbValueEqual {
    bool operator()(const AxbValueRecord& a, const AxbValueRecord& b) const noexcept {
        if (a.kind != b.kind || a.payloadCount != b.payloadCount || a.typeIndex != b.typeIndex) {
            return false;
        }
        for (std::uint8_t i = 0; i < a.payloadCount; ++i) {
            if (a.payload[i] != b.payload[i]) return false;
        }
        return true;
    }
};

struct AxbValueHash {
    Base::HashCode operator()(const AxbValueRecord& v, Base::HashCode seed = 0U) const noexcept {
        Base::HashCode h = Base::MixHash64(static_cast<Base::HashCode>(v.kind) ^ seed);
        h = Base::MixHash64(h ^ static_cast<Base::HashCode>(v.payloadCount));
        h = Base::MixHash64(h ^ static_cast<Base::HashCode>(v.typeIndex));
        for (std::uint8_t i = 0; i < v.payloadCount; ++i) {
            h = Base::MixHash64(h ^ v.payload[i]);
        }
        return h;
    }
};

struct SerializeInterner {
    explicit SerializeInterner(
        Base::Vector<Base::String>& stringsRef,
        Base::Vector<std::uint64_t>& typesRef,
        Base::Vector<std::uint64_t>& membersRef,
        Base::Vector<AxbValueRecord>& valuesRef) noexcept
        : strings(stringsRef),
          types(typesRef),
          members(membersRef),
          values(valuesRef) {}

    Base::Vector<Base::String>& strings;
    Base::Vector<std::uint64_t>& types;
    Base::Vector<std::uint64_t>& members;
    Base::Vector<AxbValueRecord>& values;

    Base::HashMap<Base::StringView, std::uint32_t> stringMap;
    Base::HashMap<std::uint64_t, std::uint32_t> typeMap;
    Base::HashMap<std::uint64_t, std::uint32_t> memberMap;
    Base::HashMap<AxbValueRecord, std::uint32_t, AxbValueHash, AxbValueEqual> valueMap;

    Base::Result<std::uint32_t> InternString(Base::StringView value) noexcept {
        if (const std::uint32_t* found = stringMap.Find(value)) {
            return *found;
        }
        const std::uint32_t index = strings.Size();
        Base::String s;
        Base::Result<void> assigned = s.Assign(value);
        if (!assigned) return assigned.GetStatus();
        strings.PushBack(std::move(s));
        stringMap.Insert(value, index);
        return index;
    }

    Base::Result<std::uint32_t> InternType(std::uint64_t id) noexcept {
        if (id == 0U || id == Meta::InvalidTypeId) return InvalidTableIndex;
        if (const std::uint32_t* found = typeMap.Find(id)) {
            return *found;
        }
        const std::uint32_t index = types.Size();
        types.PushBack(id);
        typeMap.Insert(id, index);
        return index;
    }

    Base::Result<std::uint32_t> InternMember(std::uint64_t id) noexcept {
        if (id == 0U || id == Meta::InvalidMemberId) return InvalidTableIndex;
        if (const std::uint32_t* found = memberMap.Find(id)) {
            return *found;
        }
        const std::uint32_t index = members.Size();
        members.PushBack(id);
        memberMap.Insert(id, index);
        return index;
    }

    Base::Result<std::uint32_t> InternValue(const AxbValueRecord& record) noexcept {
        if (const std::uint32_t* found = valueMap.Find(record)) {
            return *found;
        }
        const std::uint32_t index = values.Size();
        values.PushBack(record);
        valueMap.Insert(record, index);
        return index;
    }

    Base::Result<std::uint32_t> FindString(Base::StringView value) const noexcept {
        if (const std::uint32_t* found = stringMap.Find(value)) {
            return *found;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 instruction string was not interned");
    }

    Base::Result<std::uint32_t> FindType(std::uint64_t id) const noexcept {
        if (id == 0U || id == Meta::InvalidTypeId) return InvalidTableIndex;
        if (const std::uint32_t* found = typeMap.Find(id)) {
            return *found;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 instruction type id was not interned");
    }

    Base::Result<std::uint32_t> FindMember(std::uint64_t id) const noexcept {
        if (id == 0U || id == Meta::InvalidMemberId) return InvalidTableIndex;
        if (const std::uint32_t* found = memberMap.Find(id)) {
            return *found;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 instruction member id was not interned");
    }
};

Base::Result<AxbValueRecord> MakeAxbValueRecord(
    const Node& node,
    SerializeInterner& interner) noexcept {
    const auto makeText = [&]() noexcept
        -> Base::Result<AxbValueRecord> {
        Base::Result<std::uint32_t> stringIndex =
            interner.InternString(node.Value());
        if (!stringIndex) return stringIndex.GetStatus();
        AxbValueRecord record;
        record.kind = AxbValueKind::Text;
        record.payloadCount = 1U;
        record.payload[0] = stringIndex.Value();
        return record;
    };

    if (!node.HasCompiledValue()) return makeText();
    const Meta::Value& value = node.CompiledValue();
    Base::Result<std::uint32_t> typeIndex =
        interner.InternType(value.Type());
    if (!typeIndex ||
        typeIndex.Value() == InvalidTableIndex) {
        return typeIndex
            ? makeText()
            : Base::Result<AxbValueRecord>(
                  typeIndex.GetStatus());
    }

    AxbValueRecord record;
    record.typeIndex = typeIndex.Value();
    switch (value.Kind()) {
    case Meta::ValueKind::Boolean:
        record.kind = AxbValueKind::Boolean;
        record.payloadCount = 1U;
        record.payload[0] =
            value.AsBoolean() ? 1U : 0U;
        return record;
    case Meta::ValueKind::SignedInteger: {
        record.kind = AxbValueKind::SignedInteger;
        record.payloadCount = 1U;
        const std::int64_t stored =
            value.AsSignedInteger();
        record.payload[0] =
            CopyValueBits<std::uint64_t>(stored);
        return record;
    }
    case Meta::ValueKind::UnsignedInteger:
        record.kind = AxbValueKind::UnsignedInteger;
        record.payloadCount = 1U;
        record.payload[0] =
            value.AsUnsignedInteger();
        return record;
    case Meta::ValueKind::Double: {
        record.kind = AxbValueKind::Double;
        record.payloadCount = 1U;
        const double stored = value.AsDouble();
        record.payload[0] =
            CopyValueBits<std::uint64_t>(stored);
        return record;
    }
    case Meta::ValueKind::String: {
        record.kind = AxbValueKind::String;
        record.payloadCount = 1U;
        Base::Result<std::uint32_t> stringIndex =
            interner.InternString(value.AsString());
        if (!stringIndex) return stringIndex.GetStatus();
        record.payload[0] = stringIndex.Value();
        return record;
    }
    case Meta::ValueKind::Custom: {
        record.kind = AxbValueKind::Custom;
        if (value.Type() ==
            Meta::TypeOf<::Aero::Length>()) {
            Base::Result<::Aero::Length> decoded =
                Meta::ValueCodec<::Aero::Length>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().value);
            record.payload[1] =
                decoded.Value().isAuto ? 1U : 0U;
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Thickness>()) {
            Base::Result<Base::Thickness> decoded =
                Meta::ValueCodec<Base::Thickness>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] = PackFloatPair(
                decoded.Value().left,
                decoded.Value().top);
            record.payload[1] = PackFloatPair(
                decoded.Value().right,
                decoded.Value().bottom);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::CornerRadius>()) {
            Base::Result<Base::CornerRadius> decoded =
                Meta::ValueCodec<Base::CornerRadius>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] = PackFloatPair(
                decoded.Value().topLeft,
                decoded.Value().topRight);
            record.payload[1] = PackFloatPair(
                decoded.Value().bottomRight,
                decoded.Value().bottomLeft);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Color>()) {
            Base::Result<Base::Color> decoded =
                Meta::ValueCodec<Base::Color>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] = PackFloatPair(
                decoded.Value().red,
                decoded.Value().green);
            record.payload[1] = PackFloatPair(
                decoded.Value().blue,
                decoded.Value().alpha);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Point>()) {
            Base::Result<Base::Point> decoded =
                Meta::ValueCodec<Base::Point>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 1U;
            record.payload[0] = PackFloatPair(
                decoded.Value().x,
                decoded.Value().y);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Transform2D>()) {
            Base::Result<Base::Transform2D> decoded =
                Meta::ValueCodec<Base::Transform2D>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 6U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m11);
            record.payload[1] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m12);
            record.payload[2] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m21);
            record.payload[3] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m22);
            record.payload[4] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().dx);
            record.payload[5] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().dy);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<::Aero::GridLength>()) {
            Base::Result<::Aero::GridLength> decoded =
                Meta::ValueCodec<::Aero::GridLength>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    static_cast<double>(decoded.Value().value));
            record.payload[1] =
                static_cast<std::uint64_t>(
                    decoded.Value().unit);
            return record;
        }
        return makeText();
    }
    case Meta::ValueKind::Unset:
    case Meta::ValueKind::Object:
    case Meta::ValueKind::DynamicResource:
        return makeText();
    }
    return makeText();
}

Base::Result<Meta::Value> DecodeCompiledValue(
    const AxbValueRecord& record,
    Base::Span<const std::uint64_t> types,
    Base::Span<const Base::String> strings) noexcept {
    if (record.kind == AxbValueKind::Text ||
        record.typeIndex == InvalidTableIndex ||
        record.typeIndex >= types.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 typed value has an invalid type");
    }
    const Meta::TypeId type = types[record.typeIndex];
    const auto requirePayloads =
        [&record](std::uint8_t count) noexcept {
            return record.payloadCount == count;
        };

    switch (record.kind) {
    case AxbValueKind::Boolean:
        if (!requirePayloads(1U) ||
            record.payload[0] > 1U) break;
        return Meta::Value::FromBoolean(
            type, record.payload[0] != 0U);
    case AxbValueKind::SignedInteger: {
        if (!requirePayloads(1U)) break;
        const std::int64_t value =
            CopyValueBits<std::int64_t>(
                record.payload[0]);
        return Meta::Value::FromSignedInteger(
            type, value);
    }
    case AxbValueKind::UnsignedInteger:
        if (!requirePayloads(1U)) break;
        return Meta::Value::FromUnsignedInteger(
            type, record.payload[0]);
    case AxbValueKind::Double: {
        if (!requirePayloads(1U)) break;
        const double value =
            CopyValueBits<double>(
                record.payload[0]);
        if (!std::isfinite(value)) break;
        return Meta::Value::FromDouble(type, value);
    }
    case AxbValueKind::String:
        if (!requirePayloads(1U) ||
            record.payload[0] >= strings.Size()) {
            break;
        }
        return Meta::Value::TryFromString(
            type,
            strings[static_cast<std::uint32_t>(
                record.payload[0])].View());
    case AxbValueKind::Custom: {
        if (type == Meta::TypeOf<::Aero::Length>() &&
            requirePayloads(2U)) {
            const double value =
                CopyValueBits<double>(
                    record.payload[0]);
            if (!std::isfinite(value) ||
                record.payload[1] > 1U ||
                (record.payload[1] == 0U &&
                 value < 0.0)) {
                break;
            }
            return MakeManifestCustomValue(
                type,
                record.payload[1] != 0U
                    ? ::Aero::Length::Auto()
                    : ::Aero::Length::Pixels(value));
        }
        if (type == Meta::TypeOf<Base::Thickness>() &&
            requirePayloads(2U)) {
            const Base::Thickness value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true),
                UnpackFloat(record.payload[1], false),
                UnpackFloat(record.payload[1], true)};
            if (!::Aero::IsFinite(value)) break;
            return MakeManifestCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::CornerRadius>() &&
            requirePayloads(2U)) {
            const Base::CornerRadius value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true),
                UnpackFloat(record.payload[1], false),
                UnpackFloat(record.payload[1], true)};
            if (!std::isfinite(value.topLeft) ||
                !std::isfinite(value.topRight) ||
                !std::isfinite(value.bottomRight) ||
                !std::isfinite(value.bottomLeft)) {
                break;
            }
            return MakeManifestCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::Color>() &&
            requirePayloads(2U)) {
            const Base::Color value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true),
                UnpackFloat(record.payload[1], false),
                UnpackFloat(record.payload[1], true)};
            if (!Base::IsFiniteColor(value) ||
                value.red < 0.0F || value.red > 1.0F ||
                value.green < 0.0F || value.green > 1.0F ||
                value.blue < 0.0F || value.blue > 1.0F ||
                value.alpha < 0.0F || value.alpha > 1.0F) {
                break;
            }
            return MakeManifestCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::Point>() &&
            requirePayloads(1U)) {
            const Base::Point value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true)};
            if (!::Aero::IsFinite(value)) break;
            return MakeManifestCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::Transform2D>() &&
            requirePayloads(6U)) {
            const Base::Transform2D value{
                CopyValueBits<double>(record.payload[0]),
                CopyValueBits<double>(record.payload[1]),
                CopyValueBits<double>(record.payload[2]),
                CopyValueBits<double>(record.payload[3]),
                CopyValueBits<double>(record.payload[4]),
                CopyValueBits<double>(record.payload[5])};
            if (!Base::IsFiniteTransform(value)) break;
            return MakeManifestCustomValue(type, value);
        }
        if (type == Meta::TypeOf<::Aero::GridLength>() &&
            requirePayloads(2U)) {
            const double value =
                CopyValueBits<double>(
                    record.payload[0]);
            if (!std::isfinite(value) ||
                value < 0.0 ||
                record.payload[1] >
                    static_cast<std::uint64_t>(
                        ::Aero::GridUnitType::Star)) {
                break;
            }
            return MakeManifestCustomValue(
                type,
                ::Aero::GridLength{
                    static_cast<float>(value),
                    static_cast<::Aero::GridUnitType>(
                        record.payload[1])});
        }
        break;
    }
    case AxbValueKind::Text:
        break;
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "AXB2 typed value payload is invalid");
}

Base::Result<CompiledMemberBinding>
ResolveCompiledMemberBinding(
    const ::Aero::Meta::Registry& domain,
    Meta::MemberId memberId) noexcept {
    CompiledMemberBinding binding;
    binding.id = memberId;

    if (const Meta::PropertyInfo* property =
            domain.Types().FindProperty(memberId)) {
        binding.kind = Meta::MemberKind::Property;
        binding.ownerType = property->OwnerType();
        binding.valueType = property->ValueType();
        if (const Meta::TypeInfo* valueType =
                domain.Types().FindType(
                    property->ValueType())) {
            binding.valueTypeKind =
                valueType->Kind();
            binding.valueTypeFlags =
                valueType->Flags();
        }
        binding.propertyFlags = property->Flags();
        binding.attached =
            (static_cast<std::uint32_t>(
                 property->Flags()) &
             static_cast<std::uint32_t>(
                 Meta::PropertyFlags::Attached)) != 0U;

        const bool runtimeWritable =
            domain.CanWriteProperty(memberId);
        Base::Result<Meta::ContentInfo> content =
            domain.GetContentInfo(memberId);
        const bool contentWritable =
            content && content.Value().writable;
        binding.writable =
            runtimeWritable || contentWritable;
        binding.writeMode = static_cast<std::uint8_t>(
            (contentWritable &&
                    content.Value().kind ==
                        Meta::ContentKind::Collection) ||
                    memberId ==
                        VisualStateManager::VisualStateGroupsProperty
                            .Handle().value
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce);
        binding.acceptsAnyValue =
            runtimeWritable &&
            (((static_cast<std::uint32_t>(
                   property->Flags()) &
               static_cast<std::uint32_t>(
                   Meta::PropertyFlags::AnyValue)) != 0U) ||
             property->ValueType() ==
                 Meta::TypeOf<Meta::Value>());
        return binding;
    }

    if (const Meta::EventInfo* event =
            domain.Types().FindEvent(memberId)) {
        binding.kind = Meta::MemberKind::Event;
        binding.ownerType = event->OwnerType();
        binding.valueType = event->EventArgsType();
        if (const Meta::TypeInfo* valueType =
                domain.Types().FindType(
                    event->EventArgsType())) {
            binding.valueTypeKind =
                valueType->Kind();
            binding.valueTypeFlags =
                valueType->Flags();
        }
        binding.eventFlags = event->Flags();
        binding.attached =
            (static_cast<std::uint32_t>(
                 event->Flags()) &
             static_cast<std::uint32_t>(
                 Meta::EventFlags::Attached)) != 0U;
        return binding;
    }

    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "AXB2 member table references an unknown frozen member id");
}

Base::Result<CompiledTypeBinding>
ResolveCompiledTypeBinding(
    const ::Aero::Meta::Registry& domain,
    Meta::TypeId typeId) noexcept {
    const Meta::TypeInfo* type =
        domain.Types().FindType(typeId);
    if (type == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 type table references an unknown frozen type id");
    }

    CompiledTypeBinding binding;
    binding.id = typeId;
    binding.kind = type->Kind();
    binding.flags = type->Flags();

    const Meta::MemberId contentId = domain.FindContentMember(typeId);
    if (contentId != Meta::InvalidMemberId) {
        Base::Result<CompiledMemberBinding> content =
            ResolveCompiledMemberBinding(
                domain,
                contentId);
        if (!content) return content.GetStatus();
        binding.contentMember = content.Value();
        binding.hasContentMember = true;
    }
    return binding;
}

const AxbSectionDirectoryEntry* FindSection(
    Base::Span<const AxbSectionDirectoryEntry> directory,
    AxbSectionKind kind) noexcept {
    for (const AxbSectionDirectoryEntry& entry : directory) {
        if (entry.kind == kind) return &entry;
    }
    return nullptr;
}

Base::Result<Decoder> OpenSection(
    Base::Span<const std::uint8_t> document,
    const AxbSectionDirectoryEntry& section) noexcept {
    if (section.offset > document.Size() ||
        section.size > document.Size() - section.offset) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 section range is outside the document");
    }
    return Decoder({document.Data() + section.offset, section.size});
}

Base::Result<void> AssignInternedString(
    Base::String& destination,
    Base::Span<const Base::String> strings,
    std::uint32_t index) noexcept {
    if (index == InvalidTableIndex) {
        destination.Clear();
        return {};
    }
    if (index >= strings.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 instruction references an invalid string index");
    }
    return destination.Assign(strings[index].View());
}

Base::Result<void> ReconstructInstructionName(
    NodeKind nodeKind,
    bool fromAttribute,
    Base::String& localName,
    Base::String& namespaceUri,
    const ::Aero::Meta::Registry& domain,
    Meta::TypeId typeId,
    const CompiledMemberBinding* member) noexcept {
    if (nodeKind == NodeKind::StartObject &&
        typeId != Meta::InvalidTypeId) {
        if (const Meta::TypeInfo* typeInfo =
                domain.Types().FindType(typeId)) {
            Base::Result<void> assigned =
                localName.Assign(typeInfo->Name());
            if (!assigned) return assigned.GetStatus();
            return namespaceUri.Assign(
                typeInfo->XamlNamespace());
        }
        return {};
    }
    if ((nodeKind == NodeKind::StartObject ||
         nodeKind == NodeKind::StartMember) &&
        member != nullptr) {
        const Meta::TypeInfo* ownerType =
            domain.Types().FindType(member->ownerType);
        Base::StringView memberName;
        if (member->kind == Meta::MemberKind::Property) {
            if (const Meta::PropertyInfo* prop =
                    domain.Types().FindProperty(member->id)) {
                memberName = prop->Name();
            }
        } else if (member->kind == Meta::MemberKind::Event) {
            if (const Meta::EventInfo* evt =
                    domain.Types().FindEvent(member->id)) {
                memberName = evt->Name();
            }
        }
        if (memberName.Empty()) {
            return {};
        }
        if (nodeKind == NodeKind::StartObject ||
            member->attached) {
            if (ownerType == nullptr) {
                return {};
            }
            Base::Result<void> assigned =
                localName.Assign(ownerType->Name());
            if (!assigned) return assigned.GetStatus();
            assigned = localName.Append(".");
            if (!assigned) return assigned.GetStatus();
            assigned = localName.Append(memberName);
            if (!assigned) return assigned.GetStatus();
            return namespaceUri.Assign(
                ownerType->XamlNamespace());
        }
        Base::Result<void> assigned =
            localName.Assign(memberName);
        if (!assigned) return assigned.GetStatus();
        if (!fromAttribute && ownerType != nullptr) {
            return namespaceUri.Assign(
                ownerType->XamlNamespace());
        }
    }
    return {};
}

Base::Result<void> FormatDecodedCompiledValueText(
    Base::String& destination,
    const AxbValueRecord& record,
    const ::Aero::Meta::Registry& domain,
    Base::Span<const Meta::TypeId> types,
    Base::Span<const Base::String> strings) noexcept {
    if (record.kind != AxbValueKind::Text &&
        record.typeIndex >= types.Size()) {
        return {};
    }
    const Meta::TypeId type =
        record.kind != AxbValueKind::Text
        ? types[record.typeIndex]
        : Meta::InvalidTypeId;
    char buf[160]{};
    const auto assignFormatted =
        [&destination, &buf](int len) noexcept -> Base::Result<void> {
            if (len <= 0 ||
                static_cast<std::size_t>(len) >= sizeof(buf)) {
                return {};
            }
            return destination.Assign(
                Base::StringView(
                    buf,
                    static_cast<std::uint32_t>(len)));
        };
    switch (record.kind) {
    case AxbValueKind::Boolean:
        return destination.Assign(
            record.payload[0] != 0U
                ? Base::StringView("True")
                : Base::StringView("False"));
    case AxbValueKind::SignedInteger:
        return assignFormatted(std::snprintf(
            buf,
            sizeof(buf),
            "%lld",
            static_cast<long long>(
                CopyValueBits<std::int64_t>(record.payload[0]))));
    case AxbValueKind::UnsignedInteger:
        if (const Meta::EnumValueInfo* enumVal =
                domain.Types().FindEnumValue(type, record.payload[0])) {
            return destination.Assign(enumVal->Name());
        }
        return assignFormatted(std::snprintf(
            buf,
            sizeof(buf),
            "%llu",
            static_cast<unsigned long long>(record.payload[0])));
    case AxbValueKind::Double:
        return assignFormatted(std::snprintf(
            buf,
            sizeof(buf),
            "%.17g",
            CopyValueBits<double>(record.payload[0])));
    case AxbValueKind::String:
        return AssignInternedString(
            destination,
            strings,
            static_cast<std::uint32_t>(record.payload[0]));
    case AxbValueKind::Custom:
        if (type == Meta::TypeOf<::Aero::Length>()) {
            if (record.payload[1] != 0U) {
                return destination.Assign("Auto");
            }
            return assignFormatted(std::snprintf(
                buf,
                sizeof(buf),
                "%.9g",
                CopyValueBits<double>(record.payload[0])));
        }
        if (type == Meta::TypeOf<Base::Thickness>() ||
            type == Meta::TypeOf<Base::CornerRadius>()) {
            return assignFormatted(std::snprintf(
                buf,
                sizeof(buf),
                "%.9g,%.9g,%.9g,%.9g",
                static_cast<double>(UnpackFloat(record.payload[0], false)),
                static_cast<double>(UnpackFloat(record.payload[0], true)),
                static_cast<double>(UnpackFloat(record.payload[1], false)),
                static_cast<double>(UnpackFloat(record.payload[1], true))));
        }
        if (type == Meta::TypeOf<Base::Color>()) {
            const auto toByte = [](float c) noexcept -> unsigned int {
                const float clamped =
                    c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
                return static_cast<unsigned int>(clamped * 255.0f + 0.5f);
            };
            return assignFormatted(std::snprintf(
                buf,
                sizeof(buf),
                "#%02X%02X%02X%02X",
                toByte(UnpackFloat(record.payload[1], true)),
                toByte(UnpackFloat(record.payload[0], false)),
                toByte(UnpackFloat(record.payload[0], true)),
                toByte(UnpackFloat(record.payload[1], false))));
        }
        if (type == Meta::TypeOf<Base::Point>()) {
            return assignFormatted(std::snprintf(
                buf,
                sizeof(buf),
                "%.9g,%.9g",
                static_cast<double>(UnpackFloat(record.payload[0], false)),
                static_cast<double>(UnpackFloat(record.payload[0], true))));
        }
        if (type == Meta::TypeOf<Base::Transform2D>()) {
            return assignFormatted(std::snprintf(
                buf,
                sizeof(buf),
                "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g",
                CopyValueBits<double>(record.payload[0]),
                CopyValueBits<double>(record.payload[1]),
                CopyValueBits<double>(record.payload[2]),
                CopyValueBits<double>(record.payload[3]),
                CopyValueBits<double>(record.payload[4]),
                CopyValueBits<double>(record.payload[5])));
        }
        if (type == Meta::TypeOf<::Aero::GridLength>()) {
            const auto unit =
                static_cast<::Aero::GridUnitType>(record.payload[1]);
            if (unit == ::Aero::GridUnitType::Auto) {
                return destination.Assign("Auto");
            }
            if (unit == ::Aero::GridUnitType::Star) {
                return assignFormatted(std::snprintf(
                    buf,
                    sizeof(buf),
                    "%.9g*",
                    CopyValueBits<double>(record.payload[0])));
            }
            return assignFormatted(std::snprintf(
                buf,
                sizeof(buf),
                "%.9g",
                CopyValueBits<double>(record.payload[0])));
        }
        return {};
    case AxbValueKind::Text:
        return {};
    }
    return {};
}

} // namespace

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    NodeReader& reader,
    const ::Aero::Meta::Registry& domain) noexcept {
    return Compile(reader, domain, {});
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    NodeReader& reader,
    const ::Aero::Meta::Registry& domain,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(domain);
    if (!identity) return identity.GetStatus();
    return CompileWithIdentity(reader, identity.Value(), originUri);
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    Base::Span<const Node> nodes,
    const ::Aero::Meta::Registry& domain,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(domain);
    if (!identity) return identity.GetStatus();

    CompiledDocument document;
    document.identity_ = identity.Value();
    document.originUri_ = originUri;
    if (!originUri.Empty()) {
        document.dependencies_.PushBack(originUri);
    }
    document.nodes_.Reserve(nodes.Size());
    for (const Node& node : nodes) {
        Base::Result<Node> cloned = Node::Clone(node);
        if (!cloned) return cloned.GetStatus();
        document.nodes_.PushBack(
            std::move(cloned).Value());
    }
    return document;
}

Base::Result<CompiledDocument>
CompiledDocument::Clone() const noexcept {
    CompiledDocument document;
    document.identity_ = identity_;
    document.originUri_ = originUri_;
    document.dependencies_ = dependencies_;
    document.bytecodeProgram_ = bytecodeProgram_;
    document.nodes_.Reserve(nodes_.Size());
    for (const Node& node : nodes_) {
        Base::Result<Node> cloned = Node::Clone(node);
        if (!cloned) return cloned.GetStatus();
        document.nodes_.PushBack(std::move(cloned).Value());
    }
    return document;
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    Base::Span<const Node> nodes,
    const Schema& schema,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledDocument> compiled =
        Compile(nodes, schema.Domain(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<void> valid = compiled.Value().BindSchema(schema);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<CompiledDocument>
CompiledDocument::CompileWithIdentity(
    NodeReader& reader,
    const CompiledCacheIdentity& identity,
    const Base::ResourceUri& originUri) noexcept {
    CompiledDocument document;
    document.identity_ = identity;
    document.originUri_ = originUri;
    if (!originUri.Empty()) {
        document.dependencies_.PushBack(originUri);
    }
    Node node;
    while (true) {
        Base::Result<NodeKind> read = reader.Read(node);
        if (!read) return read.GetStatus();
        const NodeKind kind = read.Value();
        document.nodes_.PushBack(std::move(node));
        if (kind == NodeKind::EndOfDocument) break;
    }
    return document;
}

Base::Result<void> CompiledDocument::AddDependency(
    const Base::ResourceUri& dependency) noexcept {
    if (dependency.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Compiled XAML dependency URI is empty");
    }
    for (const Base::ResourceUri& existing :
         dependencies_) {
        if (existing == dependency) {
            return {};
        }
    }
    dependencies_.PushBack(dependency);
    return {};
}

Base::Result<Base::Vector<std::uint8_t>>
CompiledDocument::Serialize(
    const CompiledDocumentSerializeOptions& options) const noexcept {
    if (!IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 document is not valid");
    }

    Base::Vector<Base::String> strings;
    Base::Vector<std::uint64_t> types;
    Base::Vector<std::uint64_t> members;
    Base::Vector<AxbValueRecord> values;
    Base::Vector<std::uint32_t> nodeValueIndices;

    SerializeInterner interner(strings, types, members, values);

    Base::Result<std::uint32_t> originString = interner.InternString(
        originUri_.Canonical());
    if (!originString) return originString.GetStatus();
    for (const Base::ResourceUri& dependency : dependencies_) {
        Base::Result<std::uint32_t> index = interner.InternString(
            dependency.Canonical());
        if (!index) return index.GetStatus();
    }
    nodeValueIndices.Reserve(nodes_.Size());

    for (const Node& node : nodes_) {
        if (NeedsInstructionQualifiedName(node)) {
            const Base::StringView qualifiedName[] = {
                node.name_.prefix_.View(),
                node.name_.localName_.View(),
                node.name_.namespaceUri_.View()};
            for (Base::StringView value : qualifiedName) {
                Base::Result<std::uint32_t> index =
                    interner.InternString(value);
                if (!index) return index.GetStatus();
            }
        }
        if (node.kind_ == NodeKind::NamespaceDeclaration) {
            Base::Result<std::uint32_t> prefix =
                interner.InternString(
                    node.namespacePrefix_.View());
            if (!prefix) return prefix.GetStatus();
            Base::Result<std::uint32_t> uri =
                interner.InternString(
                    node.namespaceUri_.View());
            if (!uri) return uri.GetStatus();
        }

        std::uint32_t valueIndex = InvalidTableIndex;
        if (node.kind_ == NodeKind::Value) {
            Base::Result<AxbValueRecord> record =
                MakeAxbValueRecord(node, interner);
            if (!record) return record.GetStatus();
            Base::Result<std::uint32_t> interned =
                interner.InternValue(record.Value());
            if (!interned) return interned.GetStatus();
            valueIndex = interned.Value();
        }
        nodeValueIndices.PushBack(valueIndex);

        Base::Result<std::uint32_t> typeIndex = interner.InternType(
            node.compiledTypeId_);
        if (!typeIndex) return typeIndex.GetStatus();
        Base::Result<std::uint32_t> memberIndex = interner.InternMember(
            node.compiledMemberId_);
        if (!memberIndex) return memberIndex.GetStatus();
    }

    Base::Vector<AxbSection> sections;
    auto addSection = [&sections](
        AxbSectionKind kind,
        std::uint32_t count,
        Base::Vector<std::uint8_t>&& bytes) noexcept -> Base::Result<void> {
        AxbSection section;
        section.kind = kind;
        section.count = count;
        section.bytes = std::move(bytes);
        sections.PushBack(std::move(section));
        return {};
    };

    Base::Vector<std::uint8_t> dependencyBytes;
    Base::Result<void> result = AppendU32(
        dependencyBytes, originString.Value());
    if (!result) return result.GetStatus();
    result = AppendU32(dependencyBytes, dependencies_.Size());
    if (!result) return result.GetStatus();
    for (const Base::ResourceUri& dependency : dependencies_) {
        Base::Result<std::uint32_t> index = interner.FindString(
            dependency.Canonical());
        if (!index) return index.GetStatus();
        result = AppendU32(dependencyBytes, index.Value());
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Dependencies,
        dependencies_.Size(),
        std::move(dependencyBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> stringBytes;
    result = AppendU32(stringBytes, strings.Size());
    if (!result) return result.GetStatus();
    for (const Base::String& string : strings) {
        result = AppendString(stringBytes, string.View());
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Strings, strings.Size(), std::move(stringBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> typeBytes;
    result = AppendU32(typeBytes, types.Size());
    if (!result) return result.GetStatus();
    for (std::uint64_t type : types) {
        result = AppendU64(typeBytes, type);
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Types, types.Size(), std::move(typeBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> memberBytes;
    result = AppendU32(memberBytes, members.Size());
    if (!result) return result.GetStatus();
    for (std::uint64_t member : members) {
        result = AppendU64(memberBytes, member);
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Members, members.Size(), std::move(memberBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> valueBytes;
    result = AppendU32(valueBytes, values.Size());
    if (!result) return result.GetStatus();
    for (const AxbValueRecord& value : values) {
        if (value.payloadCount == 0U ||
            value.payloadCount > AxbMaxValuePayloads) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "AXB2 value payload count is invalid");
        }
        result = AppendU8(
            valueBytes,
            static_cast<std::uint8_t>(value.kind));
        if (!result) return result.GetStatus();
        result = AppendU8(
            valueBytes, value.payloadCount);
        if (!result) return result.GetStatus();
        result = AppendU16(valueBytes, 0U);
        if (!result) return result.GetStatus();
        result = AppendU32(
            valueBytes, value.typeIndex);
        if (!result) return result.GetStatus();
        for (std::uint32_t payloadIndex = 0U;
             payloadIndex < value.payloadCount;
             ++payloadIndex) {
            result = AppendU64(
                valueBytes,
                value.payload[payloadIndex]);
            if (!result) return result.GetStatus();
        }
    }
    result = addSection(
        AxbSectionKind::Values, values.Size(), std::move(valueBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> instructionBytes;
    result = AppendVarU32(
        instructionBytes, nodes_.Size());
    if (!result) return result.GetStatus();
    for (std::uint32_t nodeIndex = 0U;
         nodeIndex < nodes_.Size(); ++nodeIndex) {
        const Node& node = nodes_[nodeIndex];
        Base::Result<std::uint32_t> typeIndex =
            interner.FindType(node.compiledTypeId_);
        if (!typeIndex) return typeIndex.GetStatus();
        Base::Result<std::uint32_t> memberIndex =
            interner.FindMember(node.compiledMemberId_);
        if (!memberIndex) return memberIndex.GetStatus();
        const std::uint32_t valueIndex =
            nodeValueIndices[nodeIndex];
        const bool hasQualifiedName =
            NeedsInstructionQualifiedName(node);
        const bool hasNamespace =
            node.kind_ == NodeKind::NamespaceDeclaration;

        std::uint8_t flags = node.fromAttribute_
            ? static_cast<std::uint8_t>(AxbInstructionFromAttribute)
            : static_cast<std::uint8_t>(0U);
        if (typeIndex.Value() != InvalidTableIndex) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasType);
        }
        if (memberIndex.Value() != InvalidTableIndex) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasMember);
        }
        if (valueIndex != InvalidTableIndex) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasValue);
        }
        if (hasQualifiedName) {
            flags = static_cast<std::uint8_t>(
                flags |
                AxbInstructionHasQualifiedName);
        }
        if (hasNamespace) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasNamespace);
        }
        if (!IsInstructionShapeValid(
                node.kind_, flags)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "AXB2 instruction shape is invalid");
        }

        result = AppendU8(
            instructionBytes,
            static_cast<std::uint8_t>(
                node.kind_));
        if (!result) return result.GetStatus();
        result = AppendU8(
            instructionBytes, flags);
        if (!result) return result.GetStatus();

        if (HasInstructionFlag(
                flags, AxbInstructionHasType)) {
            result = AppendVarU32(
                instructionBytes,
                typeIndex.Value());
            if (!result) return result.GetStatus();
        }
        if (HasInstructionFlag(
                flags, AxbInstructionHasMember)) {
            result = AppendVarU32(
                instructionBytes,
                memberIndex.Value());
            if (!result) return result.GetStatus();
        }
        if (HasInstructionFlag(
                flags, AxbInstructionHasValue)) {
            result = AppendVarU32(
                instructionBytes, valueIndex);
            if (!result) return result.GetStatus();
        }
        if (hasQualifiedName) {
            const Base::StringView qualifiedName[] = {
                node.name_.prefix_.View(),
                node.name_.localName_.View(),
                node.name_.namespaceUri_.View()};
            for (Base::StringView value :
                 qualifiedName) {
                Base::Result<std::uint32_t> index =
                    interner.FindString(value);
                if (!index) return index.GetStatus();
                result = AppendVarU32(
                    instructionBytes,
                    index.Value());
                if (!result) {
                    return result.GetStatus();
                }
            }
        }
        if (hasNamespace) {
            Base::Result<std::uint32_t> prefix =
                interner.FindString(
                    node.namespacePrefix_.View());
            if (!prefix) return prefix.GetStatus();
            Base::Result<std::uint32_t> uri =
                interner.FindString(
                    node.namespaceUri_.View());
            if (!uri) return uri.GetStatus();
            result = AppendVarU32(
                instructionBytes,
                prefix.Value());
            if (!result) return result.GetStatus();
            result = AppendVarU32(
                instructionBytes,
                uri.Value());
            if (!result) return result.GetStatus();
        }
    }
    result = addSection(
        AxbSectionKind::Instructions,
        nodes_.Size(),
        std::move(instructionBytes));
    if (!result) return result.GetStatus();

    if (options.includeSourceMap) {
        Base::Vector<std::uint8_t> sourceMapBytes;
        result = AppendVarU32(
            sourceMapBytes, nodes_.Size());
        if (!result) return result.GetStatus();
        for (const Node& node : nodes_) {
            result = AppendPosition(
                sourceMapBytes, node.source_.begin);
            if (!result) return result.GetStatus();
            result = AppendPosition(
                sourceMapBytes, node.source_.end);
            if (!result) return result.GetStatus();
        }
        result = addSection(
            AxbSectionKind::SourceMap,
            nodes_.Size(),
            std::move(sourceMapBytes));
        if (!result) return result.GetStatus();
    }

    Base::Vector<std::uint8_t> output;
    result = AppendU32(output, CompiledDocumentMagic);
    if (!result) return result.GetStatus();
    result = AppendU32(output, XamlCompiledDocumentEncodingVersion);
    if (!result) return result.GetStatus();
    result = AppendCompiledCacheIdentity(output, identity_);
    if (!result) return result.GetStatus();
    result = AppendU32(output, sections.Size());
    if (!result) return result.GetStatus();

    std::uint32_t sectionOffset =
        40U + sections.Size() * 16U;
    for (const AxbSection& section : sections) {
        if (sectionOffset > UINT32_MAX - section.bytes.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 document exceeds the 32-bit section range");
        }
        result = AppendU32(output, static_cast<std::uint32_t>(section.kind));
        if (!result) return result.GetStatus();
        result = AppendU32(output, sectionOffset);
        if (!result) return result.GetStatus();
        result = AppendU32(output, section.bytes.Size());
        if (!result) return result.GetStatus();
        result = AppendU32(output, section.count);
        if (!result) return result.GetStatus();
        sectionOffset += section.bytes.Size();
    }
    for (const AxbSection& section : sections) {
        for (std::uint8_t byte : section.bytes) {
            output.PushBack(byte);
        }
    }
    return output;
}

Base::Result<CompiledDocument>
CompiledDocument::Deserialize(
    Base::Span<const std::uint8_t> bytes,
    const ::Aero::Meta::Registry& domain,
    const CompiledDocumentLimits& limits) noexcept {
    if (limits.maxNodes == 0U ||
        limits.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Compiled XAML limits must be positive");
    }
    Decoder decoder(bytes);
    Base::Result<std::uint32_t> magic = decoder.ReadU32();
    if (!magic) return magic.GetStatus();
    Base::Result<std::uint32_t> encoding = decoder.ReadU32();
    if (!encoding) return encoding.GetStatus();
    if (magic.Value() != CompiledDocumentMagic ||
        encoding.Value() != XamlCompiledDocumentEncodingVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "AXB2 encoding is not supported");
    }

    CompiledDocument document;
    Base::Result<CompiledCacheIdentity> readIdentity = ReadCompiledCacheIdentity(decoder);
    if (!readIdentity) return readIdentity.GetStatus();
    document.identity_ = readIdentity.Value();
    Base::Result<void> compatible = ValidateCompiledCacheIdentity(
        document.identity_, domain);
    if (!compatible) return compatible.GetStatus();

    Base::Result<std::uint32_t> sectionCount = decoder.ReadU32();
    if (!sectionCount) return sectionCount.GetStatus();
    if (sectionCount.Value() < 6U || sectionCount.Value() > 8U) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 section count is invalid");
    }
    Base::Vector<AxbSectionDirectoryEntry> directory;
    directory.Reserve(sectionCount.Value());
    const std::uint32_t payloadBegin =
        40U + sectionCount.Value() * 16U;
    for (std::uint32_t index = 0U; index < sectionCount.Value(); ++index) {
        Base::Result<std::uint32_t> kind = decoder.ReadU32();
        if (!kind) return kind.GetStatus();
        Base::Result<std::uint32_t> offset = decoder.ReadU32();
        if (!offset) return offset.GetStatus();
        Base::Result<std::uint32_t> size = decoder.ReadU32();
        if (!size) return size.GetStatus();
        Base::Result<std::uint32_t> count = decoder.ReadU32();
        if (!count) return count.GetStatus();
        if (kind.Value() < static_cast<std::uint32_t>(
                AxbSectionKind::Dependencies) ||
            kind.Value() > static_cast<std::uint32_t>(
                AxbSectionKind::SourceFallback) ||
            offset.Value() < payloadBegin ||
            offset.Value() > bytes.Size() ||
            size.Value() > bytes.Size() - offset.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 section directory is invalid");
        }
        for (const AxbSectionDirectoryEntry& existing : directory) {
            const bool duplicate = existing.kind ==
                static_cast<AxbSectionKind>(kind.Value());
            const bool overlap =
                offset.Value() < existing.offset + existing.size &&
                existing.offset < offset.Value() + size.Value();
            if (duplicate || overlap) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 sections are duplicated or overlapping");
            }
        }
        directory.PushBack({
            static_cast<AxbSectionKind>(kind.Value()),
            offset.Value(), size.Value(), count.Value()});
    }

    const Base::Span<const AxbSectionDirectoryEntry> directorySpan{
        directory.Data(), directory.Size()};
    const AxbSectionDirectoryEntry* dependencySection = FindSection(
        directorySpan, AxbSectionKind::Dependencies);
    const AxbSectionDirectoryEntry* stringSection = FindSection(
        directorySpan, AxbSectionKind::Strings);
    const AxbSectionDirectoryEntry* typeSection = FindSection(
        directorySpan, AxbSectionKind::Types);
    const AxbSectionDirectoryEntry* memberSection = FindSection(
        directorySpan, AxbSectionKind::Members);
    const AxbSectionDirectoryEntry* valueSection = FindSection(
        directorySpan, AxbSectionKind::Values);
    const AxbSectionDirectoryEntry* instructionSection = FindSection(
        directorySpan, AxbSectionKind::Instructions);
    const AxbSectionDirectoryEntry* sourceMapSection = FindSection(
        directorySpan, AxbSectionKind::SourceMap);
    if (dependencySection == nullptr || stringSection == nullptr ||
        typeSection == nullptr || memberSection == nullptr ||
        valueSection == nullptr || instructionSection == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 is missing a required section");
    }

    Base::Result<Decoder> opened = OpenSection(bytes, *stringSection);
    if (!opened) return opened.GetStatus();
    Decoder stringsDecoder = opened.Value();
    Base::Result<std::uint32_t> stringCount = stringsDecoder.ReadU32();
    if (!stringCount) return stringCount.GetStatus();
    if (stringCount.Value() != stringSection->count) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 string table count is invalid");
    }
    Base::Vector<Base::String> strings;
    strings.Reserve(stringCount.Value());
    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < stringCount.Value(); ++index) {
        Base::Result<Base::String> string = stringsDecoder.ReadString(
            totalStringBytes, limits.maxStringBytes);
        if (!string) return string.GetStatus();
        strings.PushBack(std::move(string).Value());
    }
    if (!stringsDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 string table has trailing data");
    }

    opened = OpenSection(bytes, *typeSection);
    if (!opened) return opened.GetStatus();
    Decoder typesDecoder = opened.Value();
    Base::Result<std::uint32_t> typeCount = typesDecoder.ReadU32();
    if (!typeCount) return typeCount.GetStatus();
    if (typeCount.Value() != typeSection->count ||
        typeCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 type table count exceeds limits");
    }
    Base::Vector<std::uint64_t> types;
    Base::Vector<CompiledTypeBinding> typeBindings;
    types.Reserve(typeCount.Value());
    typeBindings.Reserve(typeCount.Value());
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        Base::Result<std::uint64_t> id = typesDecoder.ReadU64();
        if (!id) return id.GetStatus();
        if (id.Value() == Meta::InvalidTypeId) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 type table contains an invalid type id");
        }
        Base::Result<CompiledTypeBinding> binding =
            ResolveCompiledTypeBinding(domain, id.Value());
        if (!binding) return binding.GetStatus();
        types.PushBack(id.Value());
        typeBindings.PushBack(binding.Value());
    }
    if (!typesDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 type table has trailing data");
    }

    opened = OpenSection(bytes, *memberSection);
    if (!opened) return opened.GetStatus();
    Decoder membersDecoder = opened.Value();
    Base::Result<std::uint32_t> memberCount = membersDecoder.ReadU32();
    if (!memberCount) return memberCount.GetStatus();
    if (memberCount.Value() != memberSection->count ||
        memberCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 member table count exceeds limits");
    }
    Base::Vector<CompiledMemberBinding> members;
    members.Reserve(memberCount.Value());
    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        Base::Result<std::uint64_t> id =
            membersDecoder.ReadU64();
        if (!id) return id.GetStatus();
        if (id.Value() == Meta::InvalidMemberId) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 member table contains an invalid member id");
        }
        Base::Result<CompiledMemberBinding> binding =
            ResolveCompiledMemberBinding(
                domain, id.Value());
        if (!binding) return binding.GetStatus();
        members.PushBack(
            binding.Value());
    }
    if (!membersDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 member table has trailing data");
    }

    opened = OpenSection(bytes, *valueSection);
    if (!opened) return opened.GetStatus();
    Decoder valuesDecoder = opened.Value();
    Base::Result<std::uint32_t> valueCount = valuesDecoder.ReadU32();
    if (!valueCount) return valueCount.GetStatus();
    if (valueCount.Value() != valueSection->count ||
        valueCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 value table count exceeds limits");
    }
    Base::Vector<AxbValueRecord> values;
    values.Reserve(valueCount.Value());
    for (std::uint32_t index = 0U;
         index < valueCount.Value(); ++index) {
        Base::Result<std::uint8_t> kind =
            valuesDecoder.ReadU8();
        if (!kind) return kind.GetStatus();
        Base::Result<std::uint8_t> payloadCount =
            valuesDecoder.ReadU8();
        if (!payloadCount) return payloadCount.GetStatus();
        Base::Result<std::uint16_t> reservedBits =
            valuesDecoder.ReadU16();
        if (!reservedBits) return reservedBits.GetStatus();
        Base::Result<std::uint32_t> typeIndex =
            valuesDecoder.ReadU32();
        if (!typeIndex) return typeIndex.GetStatus();

        if (kind.Value() >
                static_cast<std::uint8_t>(
                    AxbValueKind::Custom) ||
            payloadCount.Value() == 0U ||
            payloadCount.Value() > AxbMaxValuePayloads ||
            reservedBits.Value() != 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 value record header is invalid");
        }

        AxbValueRecord record;
        record.kind =
            static_cast<AxbValueKind>(kind.Value());
        record.payloadCount = payloadCount.Value();
        record.typeIndex = typeIndex.Value();
        for (std::uint32_t payloadIndex = 0U;
             payloadIndex < record.payloadCount;
             ++payloadIndex) {
            Base::Result<std::uint64_t> payload =
                valuesDecoder.ReadU64();
            if (!payload) return payload.GetStatus();
            record.payload[payloadIndex] =
                payload.Value();
        }

        const bool textValue =
            record.kind == AxbValueKind::Text;
        if ((textValue &&
             record.typeIndex != InvalidTableIndex) ||
            (!textValue &&
             (record.typeIndex == InvalidTableIndex ||
              record.typeIndex >= types.Size()))) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 value record type is invalid");
        }
        if (record.kind != AxbValueKind::Custom &&
            record.payloadCount != 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 scalar value payload count is invalid");
        }
        if ((record.kind == AxbValueKind::Text ||
             record.kind == AxbValueKind::String) &&
            (record.payload[0] > UINT32_MAX ||
             record.payload[0] >= strings.Size())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 value record string is invalid");
        }
        if (record.kind == AxbValueKind::Boolean &&
            record.payload[0] > 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Boolean payload is invalid");
        }
        values.PushBack(record);
    }
    if (!valuesDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 value table has trailing data");
    }

    opened = OpenSection(bytes, *dependencySection);
    if (!opened) return opened.GetStatus();
    Decoder dependenciesDecoder = opened.Value();
    Base::Result<std::uint32_t> originIndex = dependenciesDecoder.ReadU32();
    if (!originIndex) return originIndex.GetStatus();
    Base::Result<std::uint32_t> dependencyCount =
        dependenciesDecoder.ReadU32();
    if (!dependencyCount) return dependencyCount.GetStatus();
    if (dependencyCount.Value() != dependencySection->count ||
        dependencyCount.Value() > limits.maxDependencies ||
        originIndex.Value() >= strings.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 dependency table exceeds limits");
    }
    if (!strings[originIndex.Value()].Empty()) {
        Base::Result<Base::ResourceUri> parsed = Base::ResourceUri::Parse(
            strings[originIndex.Value()].View());
        if (!parsed) return parsed.GetStatus();
        document.originUri_ = std::move(parsed).Value();
    }
    document.dependencies_.Reserve(dependencyCount.Value());
    for (std::uint32_t index = 0U; index < dependencyCount.Value(); ++index) {
        Base::Result<std::uint32_t> stringIndex =
            dependenciesDecoder.ReadU32();
        if (!stringIndex) return stringIndex.GetStatus();
        if (stringIndex.Value() >= strings.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 dependency references an invalid string");
        }
        Base::Result<Base::ResourceUri> parsed = Base::ResourceUri::Parse(
            strings[stringIndex.Value()].View());
        if (!parsed) return parsed.GetStatus();
        Base::Result<void> added = document.AddDependency(parsed.Value());
        if (!added) return added.GetStatus();
    }
    if (!dependenciesDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 dependency table has trailing data");
    }

    Base::Vector<::Aero::Diagnostics::SourceSpan> sourceMap;
    if (sourceMapSection != nullptr) {
        opened = OpenSection(bytes, *sourceMapSection);
        if (!opened) return opened.GetStatus();
        Decoder sourceDecoder = opened.Value();
        Base::Result<std::uint32_t> sourceCount =
            sourceDecoder.ReadVarU32();
        if (!sourceCount) return sourceCount.GetStatus();
        if (sourceCount.Value() != sourceMapSection->count ||
            sourceCount.Value() > limits.maxNodes) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 source map count exceeds limits");
        }
        sourceMap.Reserve(sourceCount.Value());
        for (std::uint32_t index = 0U; index < sourceCount.Value(); ++index) {
            Base::Result<::Aero::Diagnostics::SourcePosition> begin =
                ReadPosition(sourceDecoder);
            if (!begin) return begin.GetStatus();
            Base::Result<::Aero::Diagnostics::SourcePosition> end =
                ReadPosition(sourceDecoder);
            if (!end) return end.GetStatus();
            ::Aero::Diagnostics::SourceSpan span{begin.Value(), end.Value()};
            if (!::Aero::Diagnostics::IsValidSourceSpan(span)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 source map span is invalid");
            }
            sourceMap.PushBack(span);
        }
        if (!sourceDecoder.AtEnd()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 source map has trailing data");
        }
    }

    opened = OpenSection(bytes, *instructionSection);
    if (!opened) return opened.GetStatus();
    Decoder instructionsDecoder = opened.Value();
    Base::Result<std::uint32_t> instructionCount =
        instructionsDecoder.ReadVarU32();
    if (!instructionCount) return instructionCount.GetStatus();
    if (instructionCount.Value() == 0U ||
        instructionCount.Value() != instructionSection->count ||
        instructionCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 instruction count exceeds limits");
    }
    if (sourceMapSection != nullptr &&
        sourceMap.Size() != instructionCount.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 source map is not aligned with instructions");
    }

    document.nodes_.Reserve(instructionCount.Value());
    for (std::uint32_t index = 0U;
         index < instructionCount.Value(); ++index) {
        Base::Result<std::uint8_t> kind =
            instructionsDecoder.ReadU8();
        if (!kind) return kind.GetStatus();
        Base::Result<std::uint8_t> flags =
            instructionsDecoder.ReadU8();
        if (!flags) return flags.GetStatus();
        if (kind.Value() ==
                static_cast<std::uint8_t>(
                    NodeKind::None) ||
            kind.Value() >
                static_cast<std::uint8_t>(
                    NodeKind::EndOfDocument)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 instruction kind is invalid");
        }

        const NodeKind nodeKind =
            static_cast<NodeKind>(kind.Value());
        if (!IsInstructionShapeValid(
                nodeKind, flags.Value())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 instruction shape is invalid");
        }

        std::uint32_t typeIndex =
            InvalidTableIndex;
        std::uint32_t memberIndex =
            InvalidTableIndex;
        std::uint32_t valueIndex =
            InvalidTableIndex;
        std::uint32_t qualifiedName[3]{
            InvalidTableIndex,
            InvalidTableIndex,
            InvalidTableIndex};
        std::uint32_t namespacePair[2]{
            InvalidTableIndex,
            InvalidTableIndex};

        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasType)) {
            Base::Result<std::uint32_t> read =
                instructionsDecoder.ReadVarU32();
            if (!read) return read.GetStatus();
            typeIndex = read.Value();
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasMember)) {
            Base::Result<std::uint32_t> read =
                instructionsDecoder.ReadVarU32();
            if (!read) return read.GetStatus();
            memberIndex = read.Value();
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasValue)) {
            Base::Result<std::uint32_t> read =
                instructionsDecoder.ReadVarU32();
            if (!read) return read.GetStatus();
            valueIndex = read.Value();
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasQualifiedName)) {
            for (std::uint32_t& stringIndex :
                 qualifiedName) {
                Base::Result<std::uint32_t> read =
                    instructionsDecoder.ReadVarU32();
                if (!read) return read.GetStatus();
                stringIndex = read.Value();
            }
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasNamespace)) {
            for (std::uint32_t& stringIndex :
                 namespacePair) {
                Base::Result<std::uint32_t> read =
                    instructionsDecoder.ReadVarU32();
                if (!read) return read.GetStatus();
                stringIndex = read.Value();
            }
        }

        if ((typeIndex != InvalidTableIndex &&
             typeIndex >= types.Size()) ||
            (memberIndex != InvalidTableIndex &&
             memberIndex >= members.Size()) ||
            (valueIndex != InvalidTableIndex &&
             valueIndex >= values.Size())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 instruction operand is outside its table");
        }
        for (std::uint32_t stringIndex :
             qualifiedName) {
            if (stringIndex != InvalidTableIndex &&
                stringIndex >= strings.Size()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 qualified name index is invalid");
            }
        }
        for (std::uint32_t stringIndex :
             namespacePair) {
            if (stringIndex != InvalidTableIndex &&
                stringIndex >= strings.Size()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 namespace index is invalid");
            }
        }

        Node node;
        node.kind_ = nodeKind;
        node.fromAttribute_ =
            HasInstructionFlag(
                flags.Value(),
                AxbInstructionFromAttribute);
        if (typeIndex != InvalidTableIndex) {
            node.BindCompiledType(typeBindings[typeIndex]);
        }
        if (memberIndex != InvalidTableIndex) {
            node.BindCompiledMember(
                members[memberIndex]);
        }

        Base::Result<void> assigned;
        if (qualifiedName[0] != InvalidTableIndex) {
            assigned = AssignInternedString(
                node.name_.prefix_,
                strings.AsSpan(),
                qualifiedName[0]);
            if (!assigned) return assigned.GetStatus();
            assigned = AssignInternedString(
                node.name_.localName_,
                strings.AsSpan(),
                qualifiedName[1]);
            if (!assigned) return assigned.GetStatus();
            assigned = AssignInternedString(
                node.name_.namespaceUri_,
                strings.AsSpan(),
                qualifiedName[2]);
            if (!assigned) return assigned.GetStatus();
        } else {
            assigned = ReconstructInstructionName(
                node.kind_,
                node.fromAttribute_,
                node.name_.localName_,
                node.name_.namespaceUri_,
                domain,
                typeIndex != InvalidTableIndex
                    ? types[typeIndex]
                    : Meta::InvalidTypeId,
                memberIndex != InvalidTableIndex
                    ? &members[memberIndex]
                    : nullptr);
            if (!assigned) return assigned.GetStatus();
        }
        if (namespacePair[0] != InvalidTableIndex) {
            assigned = AssignInternedString(
                node.namespacePrefix_,
                strings.AsSpan(),
                namespacePair[0]);
            if (!assigned) return assigned.GetStatus();
            assigned = AssignInternedString(
                node.namespaceUri_,
                strings.AsSpan(),
                namespacePair[1]);
            if (!assigned) return assigned.GetStatus();
        }
        if (valueIndex != InvalidTableIndex) {
            const AxbValueRecord& stored =
                values[valueIndex];
            if (stored.kind == AxbValueKind::Text) {
                assigned = node.value_.Assign(
                    strings[static_cast<std::uint32_t>(
                        stored.payload[0])].View());
                if (!assigned) {
                    return assigned.GetStatus();
                }
            } else {
                Base::Result<Meta::Value> decoded =
                    DecodeCompiledValue(
                        stored,
                        types.AsSpan(),
                        strings.AsSpan());
                if (!decoded) {
                    return decoded.GetStatus();
                }
                assigned = FormatDecodedCompiledValueText(
                    node.value_,
                    stored,
                    domain,
                    types.AsSpan(),
                    strings.AsSpan());
                if (!assigned) {
                    return assigned.GetStatus();
                }
                node.BindCompiledValue(
                    std::move(decoded).Value());
            }
        }
        if (!sourceMap.Empty()) {
            node.source_ = sourceMap[index];
        }
        document.nodes_.PushBack(
            std::move(node));
    }
    if (!instructionsDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 instruction stream has trailing data");
    }
    if (!document.originUri_.Empty()) {
        bool originListed = false;
        for (const Base::ResourceUri& dependency :
             document.dependencies_) {
            originListed =
                originListed ||
                dependency == document.originUri_;
        }
        if (!originListed) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML origin is absent from dependencies");
        }
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 instruction stream is incomplete");
    }
    return document;
}

} // namespace Aero::Markup

