#include <Aero/Markup/CompiledDocument.hpp>

// Canonical compiled-document implementation.

#include <utility>

namespace Aero::Markup {
namespace {

constexpr std::uint32_t CompiledDocumentMagic =
    UINT32_C(0x52495841);


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

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    NodeReader& reader,
    const Core::MetadataDomain& domain) noexcept {
    return Compile(reader, domain, {});
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    NodeReader& reader,
    const Core::MetadataDomain& domain,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(domain);
    if (!identity) return identity.GetStatus();
    return CompileWithIdentity(reader, identity.Value(), originUri);
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
        Base::Result<void> dependency =
            document.dependencies_.TryPushBack(originUri);
        if (!dependency) return dependency.GetStatus();
    }
    Node node;
    while (true) {
        Base::Result<NodeKind> read = reader.Read(node);
        if (!read) return read.GetStatus();
        Base::Result<Node> cloned = Node::TryClone(node);
        if (!cloned) return cloned.GetStatus();
        Base::Result<void> appended = document.nodes_.TryPushBack(
            std::move(cloned).Value());
        if (!appended) return appended.GetStatus();
        if (read.Value() == NodeKind::EndOfDocument) break;
    }
    return document;
}

Base::Result<void> CompiledDocument::TryAddDependency(
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
    return dependencies_.TryPushBack(dependency);
}

Base::Result<Base::Vector<std::uint8_t>>
CompiledDocument::Serialize() const noexcept {
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
        output, XamlCompiledDocumentEncodingVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.cacheFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.typeIdAlgorithmVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.metadataSchemaFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.metadataRuntimeFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.schemaVersion);
    if (!result) return result.GetStatus();
    result = AppendU64(output, identity_.metadataSchemaHash);
    if (!result) return result.GetStatus();
    result = AppendString(
        output, originUri_.Canonical());
    if (!result) return result.GetStatus();
    result = AppendU32(output, dependencies_.Size());
    if (!result) return result.GetStatus();
    for (const Base::ResourceUri& dependency :
         dependencies_) {
        result = AppendString(
            output, dependency.Canonical());
        if (!result) return result.GetStatus();
    }
    result = AppendU32(output, nodes_.Size());
    if (!result) return result.GetStatus();

    for (const Node& node : nodes_) {
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

Base::Result<CompiledDocument>
CompiledDocument::Deserialize(
    Base::Span<const std::uint8_t> bytes,
    const Core::MetadataDomain& domain,
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
            "Compiled XAML encoding is not supported");
    }

    CompiledDocument document;
    Base::Result<std::uint32_t> value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.cacheFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.typeIdAlgorithmVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.metadataSchemaFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.metadataRuntimeFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.schemaVersion = value.Value();
    Base::Result<std::uint64_t> hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    document.identity_.metadataSchemaHash = hash.Value();
    Base::Result<void> compatible =
        ValidateCompiledCacheIdentity(
            document.identity_, domain);
    if (!compatible) return compatible.GetStatus();

    std::uint32_t totalStringBytes = 0U;
    Base::Result<Base::String> origin =
        decoder.ReadString(
            totalStringBytes,
            limits.maxStringBytes);
    if (!origin) return origin.GetStatus();
    if (!origin.Value().Empty()) {
        Base::Result<Base::ResourceUri> parsed =
            Base::ResourceUri::Parse(origin.Value().View());
        if (!parsed) return parsed.GetStatus();
        document.originUri_ =
            std::move(parsed).Value();
    }
    Base::Result<std::uint32_t> dependencyCount =
        decoder.ReadU32();
    if (!dependencyCount) {
        return dependencyCount.GetStatus();
    }
    if (dependencyCount.Value() >
        limits.maxDependencies) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Compiled XAML dependency count exceeds limits");
    }
    Base::Result<void> reserved =
        document.dependencies_.TryReserve(
            dependencyCount.Value());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
         index < dependencyCount.Value(); ++index) {
        Base::Result<Base::String> text =
            decoder.ReadString(
                totalStringBytes,
                limits.maxStringBytes);
        if (!text) return text.GetStatus();
        Base::Result<Base::ResourceUri> parsed =
            Base::ResourceUri::Parse(text.Value().View());
        if (!parsed) return parsed.GetStatus();
        Base::Result<void> added =
            document.TryAddDependency(
                parsed.Value());
        if (!added) return added.GetStatus();
    }
    Base::Result<std::uint32_t> count = decoder.ReadU32();
    if (!count) return count.GetStatus();
    if (count.Value() == 0U ||
        count.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Compiled XAML node count exceeds limits");
    }
    reserved = document.nodes_.TryReserve(count.Value());
    if (!reserved) return reserved.GetStatus();
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
                    NodeKind::EndOfDocument) ||
            kind.Value() ==
                static_cast<std::uint8_t>(
                    NodeKind::None) ||
            attribute.Value() > 1U ||
            reservedValue.Value() != 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML node header is invalid");
        }
        Node node;
        node.kind_ =
            static_cast<NodeKind>(kind.Value());
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
    if (!decoder.AtEnd() || !document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML payload has trailing or incomplete data");
    }
    return document;
}

} // namespace Aero::Markup
