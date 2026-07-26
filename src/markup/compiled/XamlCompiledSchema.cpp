#include <Aero/Markup/Compiled/XamlCompiledDocument.hpp>

#include <Aero/Markup/Parsing/XamlNodeReader.hpp>
#include <Aero/Markup/Resources/XamlNamesResources.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>

#include <utility>

namespace Aero::Markup {

Base::Result<XamlCompiledDocument>
XamlCompiledDocument::Compile(
    XamlNodeReader& reader,
    const XamlSchemaContext& schema) noexcept {
    return Compile(reader, schema, {});
}

Base::Result<XamlCompiledDocument>
XamlCompiledDocument::Compile(
    XamlNodeReader& reader,
    const XamlSchemaContext& schema,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<XamlCompiledDocument> compiled =
        Compile(reader, schema.Domain(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<XamlCompiledCacheIdentity> identity =
        BuildXamlCompiledCacheIdentity(schema.Domain());
    if (!identity) return identity.GetStatus();
    compiled.Value().identity_ = identity.Value();
    Base::Result<void> valid =
        compiled.Value().ValidateSchema(schema);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<void> XamlCompiledDocument::ValidateSchema(
    const XamlSchemaContext& schema) const noexcept {
    if (!schema.IsFrozen() || !IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML schema validation is not ready");
    }
    enum class FrameKind : std::uint8_t {
        Object = 0U,
        ValueObject,
        Member,
        PropertyElement,
        NullObject
    };
    struct Frame final {
        FrameKind kind = FrameKind::Object;
        Core::TypeId type = Core::InvalidTypeId;
    };
    Base::Vector<Frame> frames;
    bool rootSeen = false;
    for (const XamlNode& node : nodes_) {
        switch (node.Kind()) {
        case XamlNodeKind::NamespaceDeclaration:
        case XamlNodeKind::Value:
            break;
        case XamlNodeKind::StartObject: {
            const bool nullObject =
                node.Name().NamespaceUri() ==
                    XamlLanguageNamespaceUri() &&
                node.Name().LocalName() ==
                    Base::StringView("Null");
            bool propertyElement = false;
            for (std::uint32_t index = 0U;
                 index < node.Name().LocalName().SizeBytes();
                 ++index) {
                propertyElement = propertyElement ||
                    node.Name().LocalName()[index] == '.';
            }
            if (propertyElement &&
                !frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                Base::Result<XamlResolvedMember> member =
                    schema.ResolveMember(
                        frames.Back().type,
                        node.Name(),
                        XamlMemberSyntax::PropertyElement);
                if (!member) return member.GetStatus();
                Base::Result<void> appended =
                    frames.TryPushBack({
                        FrameKind::PropertyElement,
                        Core::InvalidTypeId});
                if (!appended) return appended.GetStatus();
                break;
            }
            if (nullObject) {
                Base::Result<void> appended =
                    frames.TryPushBack({
                        FrameKind::NullObject,
                        Core::InvalidTypeId});
                if (!appended) return appended.GetStatus();
                break;
            }
            Base::Result<const Core::MetadataTypeDescriptor*> type =
                schema.ResolveType(
                    node.Name().NamespaceUri(),
                    node.Name().LocalName());
            if (!type) return type.GetStatus();
            if (!frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                Base::Result<XamlResolvedMember> content =
                    schema.ResolveContentMember(
                        frames.Back().type);
                if (!content) return content.GetStatus();
            } else if (
                !frames.Empty() &&
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
            Base::Result<void> appended =
                frames.TryPushBack({
                    Core::HasTypeFlag(
                        type.Value()->Flags(),
                        Core::TypeFlags::ValueType)
                        ? FrameKind::ValueObject
                        : FrameKind::Object,
                    type.Value()->Id()});
            if (!appended) return appended.GetStatus();
            break;
        }
        case XamlNodeKind::EndObject:
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
        case XamlNodeKind::StartMember:
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
                 frames.Back().kind != FrameKind::ValueObject)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member has no object owner");
            }
            if (frames.Back().kind == FrameKind::ValueObject &&
                node.Name().NamespaceUri() !=
                    XamlLanguageNamespaceUri()) {
                if (!node.IsFromAttribute() ||
                    node.Name().LocalName() !=
                        Base::StringView("Value")) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Compiled XAML value-type member was not found");
                }
            } else if (node.Name().NamespaceUri() !=
                       XamlLanguageNamespaceUri()) {
                Base::Result<XamlResolvedMember> member =
                    schema.ResolveMember(
                        frames.Back().type,
                        node.Name(),
                        XamlMemberSyntax::Attribute);
                if (!member) return member.GetStatus();
            } else if (
                (frames.Back().kind == FrameKind::ValueObject &&
                 node.Name().LocalName() != Base::StringView("Key")) ||
                (frames.Back().kind == FrameKind::Object &&
                 node.Name().LocalName() != Base::StringView("Name") &&
                 node.Name().LocalName() != Base::StringView("Key"))) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Compiled XAML directive is not supported");
            }
            {
                Base::Result<void> appended =
                    frames.TryPushBack({
                        FrameKind::Member,
                        Core::InvalidTypeId});
                if (!appended) return appended.GetStatus();
            }
            break;
        case XamlNodeKind::EndMember:
            if (frames.Empty() ||
                frames.Back().kind != FrameKind::Member) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member frame is unbalanced");
            }
            frames.PopBack();
            break;
        case XamlNodeKind::EndOfDocument:
            if (!frames.Empty() || !rootSeen) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML document is incomplete");
            }
            return {};
        case XamlNodeKind::None:
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML contains an empty node");
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Compiled XAML has no end-of-document node");
}


} // namespace Aero::Markup
