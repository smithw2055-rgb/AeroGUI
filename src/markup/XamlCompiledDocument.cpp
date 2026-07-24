#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Markup/XamlNamesResources.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

#include <utility>

namespace Aero::Markup {
namespace {

constexpr std::uint32_t CompiledDocumentMagic =
    UINT32_C(0x52495841);
constexpr std::uint32_t CompiledDocumentEncodingVersion = 1U;

Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    return output.TryPushBack(value);
}

Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        Base::Result<void> appended = output.TryPushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        Base::Result<void> appended = output.TryPushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendString(
    Base::Vector<std::uint8_t>& output,
    Base::StringView value) noexcept {
    Base::Result<void> length =
        AppendU32(output, value.SizeBytes());
    if (!length) return length.GetStatus();
    for (std::uint32_t index = 0U;
         index < value.SizeBytes();
         ++index) {
        Base::Result<void> appended = output.TryPushBack(
            static_cast<std::uint8_t>(value[index]));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

class Decoder final {
public:
    explicit Decoder(
        Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    Base::Result<std::uint8_t> ReadU8() noexcept {
        if (offset_ >= bytes_.Size()) return Truncated();
        return bytes_[offset_++];
    }

    Base::Result<std::uint32_t> ReadU32() noexcept {
        if (bytes_.Size() - offset_ < 4U) return Truncated();
        std::uint32_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(
                bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadU64() noexcept {
        if (bytes_.Size() - offset_ < 8U) return Truncated();
        std::uint64_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(
                bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<Base::String> ReadString(
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        Base::Result<std::uint32_t> length = ReadU32();
        if (!length) return length.GetStatus();
        if (length.Value() > bytes_.Size() - offset_ ||
            length.Value() > maxStringBytes ||
            totalStringBytes >
                maxStringBytes - length.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Compiled XAML string bounds are invalid");
        }
        Base::String value;
        Base::Result<void> assigned = value.TryAssign(
            Base::StringView(
                reinterpret_cast<const char*>(
                    bytes_.Data() + offset_),
                length.Value()));
        if (!assigned) return assigned.GetStatus();
        offset_ += length.Value();
        totalStringBytes += length.Value();
        return value;
    }

    bool AtEnd() const noexcept {
        return offset_ == bytes_.Size();
    }

private:
    static Base::Status Truncated() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Compiled XAML payload is truncated");
    }

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
};

Base::Result<void> AppendPosition(
    Base::Vector<std::uint8_t>& output,
    Core::SourcePosition position) noexcept {
    Base::Result<void> result =
        AppendU32(output, position.line);
    if (!result) return result.GetStatus();
    result = AppendU32(output, position.column);
    if (!result) return result.GetStatus();
    return AppendU64(output, position.byteOffset);
}

Base::Result<Core::SourcePosition> ReadPosition(
    Decoder& decoder) noexcept {
    Base::Result<std::uint32_t> line = decoder.ReadU32();
    if (!line) return line.GetStatus();
    Base::Result<std::uint32_t> column = decoder.ReadU32();
    if (!column) return column.GetStatus();
    Base::Result<std::uint64_t> offset = decoder.ReadU64();
    if (!offset) return offset.GetStatus();
    return Core::SourcePosition{
        line.Value(), column.Value(), offset.Value()};
}

} // namespace

Base::Result<XamlCompiledDocument>
XamlCompiledDocument::Compile(
    XamlNodeReader& reader,
    const Core::MetadataDomain& domain) noexcept {
    Base::Result<XamlCompiledCacheIdentity> identity =
        BuildXamlCompiledCacheIdentity(domain);
    if (!identity) return identity.GetStatus();

    XamlCompiledDocument document;
    document.identity_ = identity.Value();
    XamlNode node;
    while (true) {
        Base::Result<XamlNodeKind> read = reader.Read(node);
        if (!read) return read.GetStatus();
        Base::Result<XamlNode> cloned =
            XamlNode::TryClone(node);
        if (!cloned) return cloned.GetStatus();
        Base::Result<void> appended =
            document.nodes_.TryPushBack(
                std::move(cloned).Value());
        if (!appended) return appended.GetStatus();
        if (read.Value() == XamlNodeKind::EndOfDocument) {
            break;
        }
    }
    return document;
}

Base::Result<XamlCompiledDocument>
XamlCompiledDocument::Compile(
    XamlNodeReader& reader,
    const XamlSchemaContext& schema) noexcept {
    Base::Result<XamlCompiledDocument> compiled =
        Compile(reader, schema.Domain());
    if (!compiled) return compiled.GetStatus();
    Base::Result<XamlCompiledCacheIdentity> identity =
        BuildXamlCompiledCacheIdentity(
            schema.Domain(),
            schema.ModuleManifestHash());
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
                    FrameKind::Object,
                    type.Value()->Id()});
            if (!appended) return appended.GetStatus();
            break;
        }
        case XamlNodeKind::EndObject:
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
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
                frames.Back().kind != FrameKind::Object) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member has no object owner");
            }
            if (node.Name().NamespaceUri() !=
                XamlLanguageNamespaceUri()) {
                Base::Result<XamlResolvedMember> member =
                    schema.ResolveMember(
                        frames.Back().type,
                        node.Name(),
                        XamlMemberSyntax::Attribute);
                if (!member) return member.GetStatus();
            } else if (
                node.Name().LocalName() != Base::StringView("Name") &&
                node.Name().LocalName() != Base::StringView("Key")) {
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

Base::Result<Base::Vector<std::uint8_t>>
XamlCompiledDocument::Serialize() const noexcept {
    if (!IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML document is not valid");
    }
    Base::Vector<std::uint8_t> output;
    Base::Result<void> result =
        AppendU32(output, CompiledDocumentMagic);
    if (!result) return result.GetStatus();
    result = AppendU32(
        output, CompiledDocumentEncodingVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.cacheFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.typeIdAlgorithmVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.descriptorFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.facetFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU64(output, identity_.metadataSchemaHash);
    if (!result) return result.GetStatus();
    result = AppendU64(output, identity_.moduleManifestHash);
    if (!result) return result.GetStatus();
    result = AppendU32(output, nodes_.Size());
    if (!result) return result.GetStatus();

    for (const XamlNode& node : nodes_) {
        result = AppendU8(
            output, static_cast<std::uint8_t>(node.kind_));
        if (!result) return result.GetStatus();
        result = AppendU8(
            output, node.fromAttribute_ ? 1U : 0U);
        if (!result) return result.GetStatus();
        result = AppendU32(output, 0U);
        if (!result) return result.GetStatus();
        result = AppendPosition(output, node.source_.begin);
        if (!result) return result.GetStatus();
        result = AppendPosition(output, node.source_.end);
        if (!result) return result.GetStatus();
        const Base::StringView strings[] = {
            node.name_.prefix_.View(),
            node.name_.localName_.View(),
            node.name_.namespaceUri_.View(),
            node.namespacePrefix_.View(),
            node.namespaceUri_.View(),
            node.value_.View()};
        for (Base::StringView string : strings) {
            result = AppendString(output, string);
            if (!result) return result.GetStatus();
        }
    }
    return output;
}

Base::Result<XamlCompiledDocument>
XamlCompiledDocument::Deserialize(
    Base::Span<const std::uint8_t> bytes,
    const Core::MetadataDomain& domain,
    const XamlCompiledDocumentLimits& limits,
    Base::HashCode moduleManifestHash) noexcept {
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
        encoding.Value() != CompiledDocumentEncodingVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML encoding is not supported");
    }

    XamlCompiledDocument document;
    Base::Result<std::uint32_t> value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.cacheFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.typeIdAlgorithmVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.descriptorFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.facetFormatVersion = value.Value();
    Base::Result<std::uint64_t> hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    document.identity_.metadataSchemaHash = hash.Value();
    hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    document.identity_.moduleManifestHash = hash.Value();
    Base::Result<void> compatible =
        ValidateXamlCompiledCacheIdentity(
            document.identity_, domain,
            moduleManifestHash);
    if (!compatible) return compatible.GetStatus();

    Base::Result<std::uint32_t> count = decoder.ReadU32();
    if (!count) return count.GetStatus();
    if (count.Value() == 0U ||
        count.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Compiled XAML node count exceeds limits");
    }
    Base::Result<void> reserved =
        document.nodes_.TryReserve(count.Value());
    if (!reserved) return reserved.GetStatus();
    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U;
         index < count.Value();
         ++index) {
        Base::Result<std::uint8_t> kind = decoder.ReadU8();
        if (!kind) return kind.GetStatus();
        Base::Result<std::uint8_t> attribute = decoder.ReadU8();
        if (!attribute) return attribute.GetStatus();
        Base::Result<std::uint32_t> reservedValue =
            decoder.ReadU32();
        if (!reservedValue) return reservedValue.GetStatus();
        if (kind.Value() >
                static_cast<std::uint8_t>(
                    XamlNodeKind::EndOfDocument) ||
            kind.Value() ==
                static_cast<std::uint8_t>(
                    XamlNodeKind::None) ||
            attribute.Value() > 1U ||
            reservedValue.Value() != 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML node header is invalid");
        }
        XamlNode node;
        node.kind_ =
            static_cast<XamlNodeKind>(kind.Value());
        node.fromAttribute_ = attribute.Value() != 0U;
        Base::Result<Core::SourcePosition> begin =
            ReadPosition(decoder);
        if (!begin) return begin.GetStatus();
        Base::Result<Core::SourcePosition> end =
            ReadPosition(decoder);
        if (!end) return end.GetStatus();
        node.source_ = {begin.Value(), end.Value()};
        if (!Core::IsValidSourceSpan(node.source_)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML source span is invalid");
        }
        Base::String* destinations[] = {
            &node.name_.prefix_,
            &node.name_.localName_,
            &node.name_.namespaceUri_,
            &node.namespacePrefix_,
            &node.namespaceUri_,
            &node.value_};
        for (Base::String* destination : destinations) {
            Base::Result<Base::String> string =
                decoder.ReadString(
                    totalStringBytes,
                    limits.maxStringBytes);
            if (!string) return string.GetStatus();
            *destination = std::move(string).Value();
        }
        Base::Result<void> appended =
            document.nodes_.TryPushBack(std::move(node));
        if (!appended) return appended.GetStatus();
    }
    if (!decoder.AtEnd() || !document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML payload has trailing or incomplete data");
    }
    return document;
}

} // namespace Aero::Markup
