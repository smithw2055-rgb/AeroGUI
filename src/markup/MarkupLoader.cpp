#include "markup/MarkupWriterInternal.hpp"
#if !defined(AERO_MARKUP_XAML_READER_ONLY)
// Consolidated implementation. Keep sections ordered by dependency.

// ===== CompiledCache =====

#include "markup/MarkupInternal.hpp"

namespace Aero::Markup {

Base::Result<CompiledCacheIdentity> BuildCompiledCacheIdentity(
    const ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<Base::HashCode> hash = domain.ComputeSchemaHash();
    if (!hash) return hash.GetStatus();

    CompiledCacheIdentity identity;
    identity.metadataSchemaHash = hash.Value();
    return identity;
}

CompiledCacheCompatibility CompareCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const CompiledCacheIdentity& current) noexcept {
    if (cached.cacheFormatVersion != current.cacheFormatVersion) {
        return CompiledCacheCompatibility::CacheFormatMismatch;
    }
    if (cached.typeIdAlgorithmVersion != current.typeIdAlgorithmVersion) {
        return CompiledCacheCompatibility::TypeIdAlgorithmMismatch;
    }
    if (cached.metadataSchemaFormatVersion !=
        current.metadataSchemaFormatVersion) {
        return CompiledCacheCompatibility::MetadataSchemaFormatMismatch;
    }
    if (cached.metadataProgramFormatVersion !=
        current.metadataProgramFormatVersion) {
        return CompiledCacheCompatibility::MetadataProgramFormatMismatch;
    }
    if (cached.schemaVersion != current.schemaVersion) {
        return CompiledCacheCompatibility::SchemaVersionMismatch;
    }
    if (cached.metadataSchemaHash != current.metadataSchemaHash) {
        return CompiledCacheCompatibility::MetadataSchemaMismatch;
    }
    return CompiledCacheCompatibility::Compatible;
}

Base::Result<void> ValidateCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const ::Aero::Meta::Registry& currentDomain) noexcept {
    Base::Result<CompiledCacheIdentity> current =
        BuildCompiledCacheIdentity(currentDomain);
    if (!current) return current.GetStatus();

    switch (CompareCompiledCacheIdentity(cached, current.Value())) {
    case CompiledCacheCompatibility::Compatible:
        return {};
    case CompiledCacheCompatibility::CacheFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML cache format version is incompatible");
    case CompiledCacheCompatibility::TypeIdAlgorithmMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML TypeId algorithm version is incompatible");
    case CompiledCacheCompatibility::MetadataSchemaFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata descriptor format is incompatible");
    case CompiledCacheCompatibility::MetadataProgramFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata facet format is incompatible");
    case CompiledCacheCompatibility::SchemaVersionMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML schema ABI version is incompatible");
    case CompiledCacheCompatibility::MetadataSchemaMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML metadata schema hash does not match the runtime");
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError,
        "Compiled XAML compatibility result is invalid");
}

} // namespace Aero::Markup


// ===== CompiledDocument =====

#include "markup/MarkupInternal.hpp"

// Canonical compiled-document implementation.

#include <utility>

namespace Aero::Markup {
namespace {

constexpr std::uint32_t CompiledDocumentMagic =
    UINT32_C(0x52495841);


Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    return output.PushBack(value);
}

Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        Base::Result<void> appended = output.PushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        Base::Result<void> appended = output.PushBack(
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
        Base::Result<void> appended = output.PushBack(
            static_cast<std::uint8_t>(value[index]));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

class Decoder {
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
        Base::Result<void> assigned = value.Assign(
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
    ::Aero::Diagnostics::SourcePosition position) noexcept {
    Base::Result<void> result =
        AppendU32(output, position.line);
    if (!result) return result.GetStatus();
    result = AppendU32(output, position.column);
    if (!result) return result.GetStatus();
    return AppendU64(output, position.byteOffset);
}

Base::Result<::Aero::Diagnostics::SourcePosition> ReadPosition(
    Decoder& decoder) noexcept {
    Base::Result<std::uint32_t> line = decoder.ReadU32();
    if (!line) return line.GetStatus();
    Base::Result<std::uint32_t> column = decoder.ReadU32();
    if (!column) return column.GetStatus();
    Base::Result<std::uint64_t> offset = decoder.ReadU64();
    if (!offset) return offset.GetStatus();
    return ::Aero::Diagnostics::SourcePosition{
        line.Value(), column.Value(), offset.Value()};
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
        Base::Result<void> dependency =
            document.dependencies_.PushBack(originUri);
        if (!dependency) return dependency.GetStatus();
    }
    Base::Result<void> reserved =
        document.nodes_.Reserve(nodes.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Node& node : nodes) {
        Base::Result<Node> cloned = Node::Clone(node);
        if (!cloned) return cloned.GetStatus();
        Base::Result<void> appended = document.nodes_.PushBack(
            std::move(cloned).Value());
        if (!appended) return appended.GetStatus();
    }
    return std::move(document);
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    Base::Span<const Node> nodes,
    const Schema& schema,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledDocument> compiled =
        Compile(nodes, schema.Domain(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<void> valid = compiled.Value().ValidateSchema(schema);
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
        Base::Result<void> dependency =
            document.dependencies_.PushBack(originUri);
        if (!dependency) return dependency.GetStatus();
    }
    Node node;
    while (true) {
        Base::Result<NodeKind> read = reader.Read(node);
        if (!read) return read.GetStatus();
        Base::Result<Node> cloned = Node::Clone(node);
        if (!cloned) return cloned.GetStatus();
        Base::Result<void> appended = document.nodes_.PushBack(
            std::move(cloned).Value());
        if (!appended) return appended.GetStatus();
        if (read.Value() == NodeKind::EndOfDocument) break;
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
    return dependencies_.PushBack(dependency);
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
    result = AppendU32(output, identity_.metadataProgramFormatVersion);
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
    document.identity_.metadataProgramFormatVersion = value.Value();
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
        document.dependencies_.Reserve(
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
            document.AddDependency(
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
    reserved = document.nodes_.Reserve(count.Value());
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
        Base::Result<::Aero::Diagnostics::SourcePosition> begin =
            ReadPosition(decoder);
        if (!begin) return begin.GetStatus();
        Base::Result<::Aero::Diagnostics::SourcePosition> end =
            ReadPosition(decoder);
        if (!end) return end.GetStatus();
        node.source_ = {begin.Value(), end.Value()};
        if (!::Aero::Diagnostics::IsValidSourceSpan(node.source_)) {
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
            document.nodes_.PushBack(std::move(node));
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


// ===== DocumentCache =====



#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include "gui/MetadataInternal.hpp"

#include <new>
#include <utility>


namespace Aero::Markup {
namespace {

Base::Result<Base::String> MakeKey(
    const Base::ResourceUri& uri,
    Base::IAllocator& allocator) noexcept {
    if (uri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML cache URI cannot be empty");
    }
    Base::String key(&allocator);
    Base::Result<void> assigned = key.Assign(uri.Canonical());
    if (!assigned) return assigned.GetStatus();
    return key;
}

bool ContainsKey(
    const Base::Vector<Base::String>& values,
    Base::StringView key) noexcept {
    for (const Base::String& value : values) {
        if (value.View() == key) return true;
    }
    return false;
}

void RemoveKey(
    Base::Vector<Base::String>& values,
    Base::StringView key) noexcept {
    for (std::uint32_t index = 0U; index < values.Size(); ++index) {
        if (values[index].View() != key) continue;
        if (index + 1U != values.Size()) {
            values[index] = std::move(values.Back());
        }
        values.PopBack();
        return;
    }
}

} // namespace

struct DependencyGraph::Impl {
    struct Node {
        explicit Node(Base::IAllocator& allocator) noexcept
            : dependencies(&allocator), dependents(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<Base::String> dependencies;
        Base::Vector<Base::String> dependents;
    };

    explicit Impl(Base::IAllocator& allocator) noexcept
        : allocator(&allocator), nodes(&allocator) {}

    Base::Result<Node*> EnsureNode(
        const Base::ResourceUri& uri) noexcept {
        Base::Result<Base::String> key =
            MakeKey(uri, *allocator);
        if (!key) return key.GetStatus();
        Node* current = nodes.Find(key.Value());
        if (current != nullptr) return current;
        Node node(*allocator);
        node.uri = uri;
        Base::Result<typename Base::HashMap<Base::String, Node>::InsertResult>
            inserted = nodes.Insert(
                std::move(key).Value(), std::move(node));
        if (!inserted) return inserted.GetStatus();
        return &inserted.Value().entry->Value();
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Node> nodes;
    std::uint64_t generation = 0U;
};

DependencyGraph::DependencyGraph(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_);
}

DependencyGraph::~DependencyGraph() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
}

DependencyGraph::DependencyGraph(
    DependencyGraph&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
}

DependencyGraph& DependencyGraph::operator=(
    DependencyGraph&& other) noexcept {
    if (this == &other) return *this;
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
    return *this;
}

Base::Result<void> DependencyGraph::Update(
    const Base::ResourceUri& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph is unavailable");
    }
    Base::Result<Base::String> documentKey =
        MakeKey(document, *allocator_);
    if (!documentKey) return documentKey.GetStatus();
    Base::Result<Impl::Node*> documentNode =
        impl_->EnsureNode(document);
    if (!documentNode) return documentNode.GetStatus();

    Base::Vector<Base::String> newDependencies(allocator_);
    Base::Result<void> reserved =
        newDependencies.Reserve(dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::ResourceUri& dependencyUri : dependencies) {
        if (dependencyUri.Empty() || dependencyUri == document) continue;
        Base::Result<Base::String> dependencyKey =
            MakeKey(dependencyUri, *allocator_);
        if (!dependencyKey) return dependencyKey.GetStatus();
        if (ContainsKey(newDependencies, dependencyKey.Value().View())) {
            continue;
        }
        Base::Result<Impl::Node*> dependencyNode =
            impl_->EnsureNode(dependencyUri);
        if (!dependencyNode) return dependencyNode.GetStatus();
        Base::Result<void> appended = newDependencies.PushBack(
            std::move(dependencyKey).Value());
        if (!appended) return appended.GetStatus();
    }

    // Prepare reverse-edge key ownership and vector capacity before mutating
    // any edge. No hash-map insertions occur after this point, so node
    // references remain stable even when EnsureNode() previously rehashed.
    Base::Vector<Base::String> reverseKeys(allocator_);
    Base::Result<void> reverseReserved =
        reverseKeys.Reserve(newDependencies.Size());
    if (!reverseReserved) return reverseReserved.GetStatus();
    for (const Base::String& dependencyKey : newDependencies) {
        Impl::Node* dependency = impl_->nodes.Find(dependencyKey);
        if (dependency == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML dependency graph lost a prepared node");
        }
        Base::Result<void> reverseCapacity =
            dependency->dependents.Reserve(
                dependency->dependents.Size() + 1U);
        if (!reverseCapacity) return reverseCapacity.GetStatus();
        Base::String reverseKey(allocator_);
        Base::Result<void> copied = reverseKey.Assign(
            documentKey.Value().View());
        if (!copied) return copied.GetStatus();
        Base::Result<void> stored = reverseKeys.PushBack(
            std::move(reverseKey));
        if (!stored) return stored.GetStatus();
    }

    Impl::Node* node = impl_->nodes.Find(documentKey.Value());
    if (node == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph lost the document node");
    }
    for (const Base::String& oldDependency : node->dependencies) {
        Impl::Node* dependency = impl_->nodes.Find(oldDependency);
        if (dependency == nullptr) continue;
        RemoveKey(
            dependency->dependents,
            documentKey.Value().View());
        if (!ContainsKey(newDependencies, oldDependency.View()) &&
            dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            impl_->nodes.Erase(oldDependency);
        }
    }
    node->dependencies = std::move(newDependencies);
    for (std::uint32_t index = 0U;
         index < node->dependencies.Size();
         ++index) {
        Impl::Node* dependency =
            impl_->nodes.Find(node->dependencies[index]);
        if (dependency == nullptr) continue;
        if (ContainsKey(
                dependency->dependents,
                documentKey.Value().View())) {
            continue;
        }
        Base::Result<void> reverse =
            dependency->dependents.PushBack(
                std::move(reverseKeys[index]));
        if (!reverse) return reverse.GetStatus();
    }
    ++impl_->generation;
    return {};
}

bool DependencyGraph::Remove(
    const Base::ResourceUri& document) noexcept {
    if (impl_ == nullptr || document.Empty()) return false;
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return false;
    Impl::Node* node = impl_->nodes.Find(key.Value());
    if (node == nullptr) return false;

    Base::Vector<Base::String> previousDependencies(allocator_);
    if (!previousDependencies.Append(
            node->dependencies.AsSpan())) {
        return false;
    }
    node->dependencies.Clear();
    for (const Base::String& dependencyKey : previousDependencies) {
        Impl::Node* dependency = impl_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        RemoveKey(dependency->dependents, key.Value().View());
        if (dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            impl_->nodes.Erase(dependencyKey);
        }
    }
    if (node->dependents.Empty()) {
        impl_->nodes.Erase(key.Value());
    }
    ++impl_->generation;
    return true;
}

void DependencyGraph::Clear() noexcept {
    if (impl_ == nullptr) return;
    impl_->nodes.Clear();
    ++impl_->generation;
}

Base::Result<void> DependencyGraph::CopyDependencies(
    const Base::ResourceUri& document,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (impl_ == nullptr || document.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return key.GetStatus();
    const Impl::Node* node = impl_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.Reserve(node->dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependencyKey : node->dependencies) {
        const Impl::Node* dependency = impl_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        Base::Result<void> pushed = output.PushBack(dependency->uri);
        if (!pushed) return pushed.GetStatus();
    }
    return {};
}

Base::Result<void> DependencyGraph::CopyDependents(
    const Base::ResourceUri& dependency,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (impl_ == nullptr || dependency.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(dependency, *allocator_);
    if (!key) return key.GetStatus();
    const Impl::Node* node = impl_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.Reserve(node->dependents.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependentKey : node->dependents) {
        const Impl::Node* dependent = impl_->nodes.Find(dependentKey);
        if (dependent == nullptr) continue;
        Base::Result<void> pushed = output.PushBack(dependent->uri);
        if (!pushed) return pushed.GetStatus();
    }
    return {};
}

Base::Result<void> DependencyGraph::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (impl_ == nullptr || changed.Empty()) return {};

    Base::Vector<Base::String> queue(allocator_);
    Base::HashSet<Base::String> visited(allocator_);
    Base::Result<Base::String> changedKey =
        MakeKey(changed, *allocator_);
    if (!changedKey) return changedKey.GetStatus();
    Base::Result<void> queued =
        queue.PushBack(changedKey.Value());
    if (!queued) return queued.GetStatus();

    std::uint32_t cursor = 0U;
    while (cursor < queue.Size()) {
        Base::String key = queue[cursor++];
        Base::Result<typename Base::HashSet<Base::String>::InsertResult>
            inserted = visited.Insert(key);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) continue;

        const Impl::Node* node = impl_->nodes.Find(key);
        Base::ResourceUri uri = node != nullptr
            ? node->uri
            : changed;
        Base::Result<void> appended = output.PushBack(uri);
        if (!appended) return appended.GetStatus();
        if (node == nullptr) continue;
        for (const Base::String& dependent : node->dependents) {
            if (visited.Contains(dependent)) continue;
            Base::Result<void> next = queue.PushBack(dependent);
            if (!next) return next.GetStatus();
        }
    }
    return {};
}

std::uint32_t DependencyGraph::NodeCount() const noexcept {
    return impl_ != nullptr ? impl_->nodes.Size() : 0U;
}

std::uint64_t DependencyGraph::Generation() const noexcept {
    return impl_ != nullptr ? impl_->generation : 0U;
}

struct DocumentCache::Impl {
    struct Entry {
        explicit Entry(Base::IAllocator& allocator) noexcept
            : compiledBytes(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> compiledBytes;
        std::uint64_t sourceRevision = 0U;
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t lastAccess = 0U;
    };

    Impl(
        Base::IAllocator& allocator,
        const DocumentCacheLimits& valueLimits) noexcept
        : allocator(&allocator),
          entries(&allocator),
          graph(&allocator),
          limits(valueLimits) {}

    bool EraseEntry(
        const Base::ResourceUri& uri,
        bool eviction) noexcept {
        Base::Result<Base::String> key = MakeKey(uri, *allocator);
        if (!key) return false;
        Entry* entry = entries.Find(key.Value());
        if (entry == nullptr) return false;
        compiledBytes -= entry->compiledBytes.Size();
        entries.Erase(key.Value());
        graph.Remove(uri);
        if (eviction) ++evictions;
        else ++invalidations;
        ++generation;
        return true;
    }

    void EvictToLimits() noexcept {
        while ((limits.maxEntries != 0U &&
                entries.Size() > limits.maxEntries) ||
               (limits.maxCompiledBytes != 0U &&
                compiledBytes > limits.maxCompiledBytes)) {
            const typename Base::HashMap<Base::String, Entry>::Entry*
                oldest = nullptr;
            for (const auto& current : entries) {
                if (oldest == nullptr ||
                    current.Value().lastAccess <
                        oldest->Value().lastAccess) {
                    oldest = &current;
                }
            }
            if (oldest == nullptr) break;
            Base::ResourceUri uri = oldest->Value().uri;
            if (!EraseEntry(uri, true)) break;
        }
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Entry> entries;
    DependencyGraph graph;
    DocumentCacheLimits limits;
    std::uint64_t compiledBytes = 0U;
    std::uint64_t accessSequence = 0U;
    std::uint64_t hits = 0U;
    std::uint64_t misses = 0U;
    std::uint64_t stores = 0U;
    std::uint64_t invalidations = 0U;
    std::uint64_t evictions = 0U;
    std::uint64_t generation = 0U;
};

DocumentCache::DocumentCache(
    Base::IAllocator* allocator,
    const DocumentCacheLimits& limits) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_, limits);
}

DocumentCache::~DocumentCache() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
}

DocumentCache::DocumentCache(
    DocumentCache&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
}

DocumentCache& DocumentCache::operator=(
    DocumentCache&& other) noexcept {
    if (this == &other) return *this;
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
    return *this;
}

Base::Result<DocumentCacheLookup> DocumentCache::Lookup(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const ::Aero::Meta::Registry& domain,
    const CompiledDocumentLimits& limits) noexcept {
    DocumentCacheLookup result;
    if (impl_ == nullptr || uri.Empty()) return result;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    Impl::Entry* entry = impl_->entries.Find(key.Value());
    if (entry == nullptr) {
        ++impl_->misses;
        return result;
    }
    if (entry->sourceRevision != sourceRevision ||
        entry->sourceIdentity != sourceIdentity) {
        ++impl_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }

    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
            entry->compiledBytes.AsSpan(), domain, limits);
    if (!document) {
        ++impl_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }
    entry->lastAccess = ++impl_->accessSequence;
    ++impl_->hits;
    result.hit = true;
    result.sourceRevision = entry->sourceRevision;
    result.document = std::move(document).Value();
    return result;
}

Base::Result<void> DocumentCache::Store(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const CompiledDocument& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (impl_ == nullptr || uri.Empty() || !document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML document cache entry is invalid");
    }
    Base::Result<Base::Vector<std::uint8_t>> serialized =
        document.Serialize();
    if (!serialized) return serialized.GetStatus();
    const std::uint64_t serializedSize =
        serialized.Value().Size();

    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    Impl::Entry* existing = impl_->entries.Find(key.Value());
    if (existing != nullptr) {
        impl_->compiledBytes -= existing->compiledBytes.Size();
        existing->uri = uri;
        existing->compiledBytes = std::move(serialized).Value();
        existing->sourceRevision = sourceRevision;
        existing->sourceIdentity = sourceIdentity;
        existing->lastAccess = ++impl_->accessSequence;
        impl_->compiledBytes += existing->compiledBytes.Size();
    } else {
        Impl::Entry entry(*allocator_);
        entry.uri = uri;
        entry.compiledBytes = std::move(serialized).Value();
        entry.sourceRevision = sourceRevision;
        entry.sourceIdentity = sourceIdentity;
        entry.lastAccess = ++impl_->accessSequence;
        impl_->compiledBytes += entry.compiledBytes.Size();
        Base::Result<typename Base::HashMap<Base::String, Impl::Entry>::InsertResult>
            inserted = impl_->entries.Insert(
                std::move(key).Value(), std::move(entry));
        if (!inserted) {
            impl_->compiledBytes -= serializedSize;
            return inserted.GetStatus();
        }
    }
    Base::Result<void> graph =
        impl_->graph.Update(uri, dependencies);
    if (!graph) {
        impl_->EraseEntry(uri, false);
        return graph.GetStatus();
    }
    ++impl_->stores;
    ++impl_->generation;
    impl_->EvictToLimits();
    return {};
}

Base::Result<std::uint32_t> DocumentCache::Invalidate(
    const Base::ResourceUri& uri,
    bool includeDependents) noexcept {
    if (impl_ == nullptr || uri.Empty()) return 0U;
    Base::Vector<Base::ResourceUri> affected(allocator_);
    if (includeDependents) {
        Base::Result<void> collected =
            impl_->graph.CollectAffected(uri, affected);
        if (!collected) return collected.GetStatus();
    } else {
        Base::Result<void> pushed = affected.PushBack(uri);
        if (!pushed) return pushed.GetStatus();
    }
    std::uint32_t count = 0U;
    for (std::uint32_t index = affected.Size();
         index > 0U;
         --index) {
        const Base::ResourceUri& affectedUri = affected[index - 1U];
        if (impl_->EraseEntry(affectedUri, false)) {
            ++count;
        } else {
            static_cast<void>(impl_->graph.Remove(affectedUri));
        }
    }
    return count;
}

void DocumentCache::Clear() noexcept {
    if (impl_ == nullptr) return;
    impl_->entries.Clear();
    impl_->graph.Clear();
    impl_->compiledBytes = 0U;
    ++impl_->generation;
}

bool DocumentCache::Contains(
    const Base::ResourceUri& uri) const noexcept {
    if (impl_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    return key && impl_->entries.Contains(key.Value());
}

bool DocumentCache::GetSourceRevision(
    const Base::ResourceUri& uri,
    std::uint64_t sourceIdentity,
    std::uint64_t& revision) const noexcept {
    revision = 0U;
    if (impl_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return false;
    const Impl::Entry* entry = impl_->entries.Find(key.Value());
    if (entry == nullptr || entry->sourceIdentity != sourceIdentity) {
        return false;
    }
    revision = entry->sourceRevision;
    return true;
}

Base::Result<void> DocumentCache::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    return impl_ != nullptr
        ? impl_->graph.CollectAffected(changed, output)
        : Base::Result<void>{};
}

const DependencyGraph& DocumentCache::Dependencies() const noexcept {
    static const DependencyGraph empty;
    return impl_ != nullptr ? impl_->graph : empty;
}

DocumentCacheStatistics DocumentCache::Statistics() const noexcept {
    DocumentCacheStatistics result;
    if (impl_ == nullptr) return result;
    result.entryCount = impl_->entries.Size();
    result.compiledBytes = impl_->compiledBytes;
    result.hitCount = impl_->hits;
    result.missCount = impl_->misses;
    result.storeCount = impl_->stores;
    result.invalidationCount = impl_->invalidations;
    result.evictionCount = impl_->evictions;
    result.generation = impl_->generation;
    return result;
}

const DocumentCacheLimits& DocumentCache::Limits() const noexcept {
    static const DocumentCacheLimits empty{};
    return impl_ != nullptr ? impl_->limits : empty;
}

} // namespace Aero::Markup


// ===== LoaderResult =====


#include "gui/MetadataInternal.hpp"

namespace Aero::Markup {

Base::Result<void> VisualContentPlan::Reserve(
    std::uint32_t contentEdgeCount,
    std::uint32_t mountEdgeCount,
    std::uint32_t nodeCount) noexcept {
    Base::Result<void> reserved = contentEdges.Reserve(contentEdgeCount);
    if (!reserved) return reserved.GetStatus();
    reserved = mountEdges.Reserve(mountEdgeCount);
    if (!reserved) return reserved.GetStatus();
    return nodes.Reserve(nodeCount);
}

Base::Result<void> VisualContentPlan::AddNode(
    Aero::Visual& node) noexcept {
    for (Aero::Visual* existing : nodes) {
        if (existing == &node) return {};
    }
    return nodes.PushBack(&node);
}

void VisualContentPlan::ReleaseContent() noexcept {
    for (std::uint32_t index = 0U; index < contentEdges.Size(); ++index) {
        VisualContentEdge& edge = contentEdges[index];
        bool firstForParent = true;
        for (std::uint32_t prior = 0U; prior < index; ++prior) {
            if (contentEdges[prior].parentOwner.Get() ==
                    edge.parentOwner.Get() &&
                (!edge.property ||
                 contentEdges[prior].member ==
                     edge.member)) {
                firstForParent = false;
                break;
            }
        }
        if (firstForParent && edge.metadata != nullptr && edge.parentOwner) {
            if (edge.property) {
                const Meta::PropertyInfo* property =
                    edge.metadata->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.metadata->SetProperty(
                        *edge.parentOwner.Get(),
                        edge.member,
                        Meta::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.metadata->ClearContent(
                    *edge.parentOwner.Get(),
                    edge.member);
            }
        }
    }
}

void VisualContentPlan::Clear() noexcept {
    contentEdges.Clear();
    mountEdges.Clear();
    nodes.Clear();
}

} // namespace Aero::Markup


// ===== Loader =====








#include "gui/PropertyInternal.hpp"
#include <Aero/Base/Hash.hpp>

#include "markup/MarkupInternal.hpp"
#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <new>
#include <utility>


namespace Aero::Markup {
namespace LoaderDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidUri =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 301U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode XamlProviderNotFound =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 302U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceLoadFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 303U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceRejected =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 304U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode RecursiveLoad =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 305U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode LoadComponentTypeMismatch =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 306U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode ResourceDependencyFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 307U);
} // namespace LoaderDiagnosticCodes

struct Loader::Impl {
    Impl(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        const LoadState* runtime = nullptr) noexcept;

    Base::Result<LoaderResult> Load(
        Base::StringView uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> Load(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> Parse(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> Parse(
        Base::Stream& stream,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> LoadComponent(
        Base::Object& existingRoot,
        Base::StringView uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> LoadComponent(
        Base::Object& existingRoot,
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options = {}) noexcept;

private:
    struct Operation;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* runtime_ = nullptr;
};

using Aero::ResourceDictionary;

namespace {

class MemoryStream : public Base::Stream {
public:
    explicit MemoryStream(
        Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    bool CanRead() const noexcept override { return true; }

    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        const std::uint32_t available = bytes_.Size() - position_;
        const std::uint32_t count =
            std::min(available, destination.Size());
        if (count != 0U) {
            std::memcpy(
                destination.Data(),
                bytes_.Data() + position_,
                count);
            position_ += count;
        }
        return count;
    }

    bool CanSeek() const noexcept override { return true; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        return static_cast<std::uint64_t>(position_);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return static_cast<std::uint64_t>(bytes_.Size());
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        const std::int64_t base = origin == Base::SeekOrigin::Begin
            ? 0
            : origin == Base::SeekOrigin::Current
                ? static_cast<std::int64_t>(position_)
                : static_cast<std::int64_t>(bytes_.Size());
        const std::int64_t next = base + offset;
        if (next < 0 || static_cast<std::uint64_t>(next) > bytes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Memory stream seek is outside its bounds");
        }
        position_ = static_cast<std::uint32_t>(next);
        return static_cast<std::uint64_t>(position_);
    }

private:
    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t position_ = 0U;
};

class FileStream : public Base::Stream {
public:
    FileStream(std::FILE* file, std::uint64_t length) noexcept
        : file_(file), length_(length) {}
    ~FileStream() noexcept override {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    bool CanRead() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        if (destination.Empty()) return std::uint32_t{0U};
        const std::size_t count = std::fread(
            destination.Data(), 1U, destination.Size(), file_);
        if (count == 0U && std::ferror(file_) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream read failed");
        }
        return static_cast<std::uint32_t>(count);
    }
    bool CanSeek() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        const long position = std::ftell(file_);
        if (position < 0L) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream position could not be read");
        }
        return static_cast<std::uint64_t>(position);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return length_;
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        if (file_ == nullptr ||
            offset < static_cast<std::int64_t>(std::numeric_limits<long>::min()) ||
            offset > static_cast<std::int64_t>(std::numeric_limits<long>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "File stream seek is outside its bounds");
        }
        const int whence = origin == Base::SeekOrigin::Begin
            ? SEEK_SET
            : origin == Base::SeekOrigin::Current
                ? SEEK_CUR
                : SEEK_END;
        if (std::fseek(file_, static_cast<long>(offset), whence) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream seek failed");
        }
        return Position();
    }

private:
    std::FILE* file_ = nullptr;
    std::uint64_t length_ = 0U;
};

class HashingStream : public Base::Stream {
public:
    explicit HashingStream(Base::Stream& source) noexcept
        : source_(&source) {}

    bool CanRead() const noexcept override {
        return source_ != nullptr && source_->CanRead();
    }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (source_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Hashing stream is not initialized");
        }
        Base::Result<std::uint32_t> result =
            source_->Read(destination);
        if (!result || result.Value() == 0U) return result;
        constexpr Base::HashCode Prime = UINT64_C(1099511628211);
        for (std::uint32_t index = 0U;
             index < result.Value(); ++index) {
            hash_ ^= static_cast<Base::HashCode>(
                destination[index]);
            hash_ *= Prime;
        }
        size_ += result.Value();
        return result;
    }
    bool CanSeek() const noexcept override { return false; }
    Base::HashCode Hash() const noexcept {
        return Base::MixHash64(hash_ ^ size_);
    }

private:
    Base::Stream* source_ = nullptr;
    Base::HashCode hash_ =
        UINT64_C(14695981039346656037) ^
        Base::MixHash64(0U);
    std::uint64_t size_ = 0U;
};

char ToLowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

Base::Result<void> AssignLowerAscii(
    Base::String& output,
    Base::StringView value) noexcept {
    Base::String replacement(&output.Allocator());
    Base::Result<void> reserve =
        replacement.Reserve(value.SizeBytes());
    if (!reserve) {
        return reserve.GetStatus();
    }
    for (char character : value) {
        const char lower = ToLowerAscii(character);
        Base::Result<void> append =
            replacement.AppendUnchecked(
                Base::StringView(&lower, 1U));
        if (!append) {
            return append.GetStatus();
        }
    }
    output = std::move(replacement);
    return {};
}

bool RegistrationMatches(
    const XamlProviderRegistration& registration,
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) noexcept {
    const bool schemeMatches = requireScheme
        ? !registration.scheme.Empty() &&
            registration.scheme.View() == uri.Scheme()
        : registration.scheme.Empty();
    const bool assemblyMatches = requireAssembly
        ? !registration.assembly.Empty() &&
            registration.assembly.View() == uri.Assembly()
        : registration.assembly.Empty();
    return schemeMatches && assemblyMatches;
}

Base::Result<Integration::StreamResourceInfo> CreateMemoryResource(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    Integration::StreamResourceInfo result;
    result.uri = uri;
    result.stream = std::move(stream).Value();
    result.revision = revision;
    return result;
}

Base::Result<Base::ResourceUri> ResolveRequestedUri(
    Base::StringView uri,
    const Base::ResourceUri& baseUri) noexcept {
    if (!baseUri.Empty()) {
        return Base::ResourceUri::Resolve(
            baseUri, uri);
    }
    return Base::ResourceUri::Parse(uri);
}

} // namespace

Base::Result<void> XamlProviderRegistry::Register(
    XamlProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    XamlProviderRegistration registration;
    Base::Result<void> schemeResult =
        AssignLowerAscii(registration.scheme, scheme);
    if (!schemeResult) {
        return schemeResult.GetStatus();
    }
    Base::Result<void> assemblyResult =
        registration.assembly.Assign(assembly);
    if (!assemblyResult) {
        return assemblyResult.GetStatus();
    }
    registration.provider = &provider;

    for (const XamlProviderRegistration& existing :
         registrations_) {
        if (existing.scheme.View() == registration.scheme.View() &&
            existing.assembly.View() ==
                registration.assembly.View()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "A XAML source provider is already registered for this route");
        }
    }
    return registrations_.PushBack(
        std::move(registration));
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveDetailed(
    const Base::ResourceUri& uri) const noexcept {
    const struct Route {
        bool scheme;
        bool assembly;
    } routes[] = {
        {true, true},
        {true, false},
        {false, true},
        {false, false}};

    for (const Route route : routes) {
        if ((route.scheme && uri.Scheme().Empty()) ||
            (route.assembly && uri.Assembly().Empty())) {
            continue;
        }
        for (const XamlProviderRegistration& registration :
             registrations_) {
            if (RegistrationMatches(
                    registration,
                    uri,
                    route.scheme,
                    route.assembly)) {
                XamlProviderResolution result;
                result.provider = registration.provider;
                result.cacheIdentity = Base::MixHash64(
                    registration.provider->CacheIdentity() ^
                    Base::DefaultHash<Base::StringView>{}(
                        registration.scheme.View()) ^
                    Base::DefaultHash<Base::StringView>{}(
                        registration.assembly.View(), UINT64_C(0xA3E0)));
                return result;
            }
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches the resource URI");
}

Base::Result<XamlProvider*>
XamlProviderRegistry::Resolve(
    const Base::ResourceUri& uri) const noexcept {
    Base::Result<XamlProviderResolution> resolved =
        ResolveDetailed(uri);
    return resolved
        ? Base::Result<XamlProvider*>(resolved.Value().provider)
        : Base::Result<XamlProvider*>(resolved.GetStatus());
}

Base::Result<void> EmbeddedXamlProvider::Add(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Embedded XAML source provider is frozen");
    }
    if (uri.Empty() || bytes.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Embedded XAML source registration is invalid");
    }
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Embedded XAML source URI is already registered");
        }
    }

    Entry entry;
    entry.uri = uri;
    Base::Result<void> copied =
        entry.bytes.Append(bytes);
    if (!copied) {
        return copied.GetStatus();
    }
    entry.revision = revision;
    Base::Result<void> stored = entries_.PushBack(std::move(entry));
    if (!stored) return stored.GetStatus();
    cacheIdentity_ = Base::HashBytes(
        uri.Canonical().Data(),
        uri.Canonical().SizeBytes(),
        cacheIdentity_ ^ revision);
    cacheIdentity_ = Base::HashBytes(
        bytes.Data(), bytes.Size(), cacheIdentity_);
    return {};
}

Base::Result<void> EmbeddedXamlProvider::AddText(
    const Base::ResourceUri& uri,
    Base::StringView text,
    std::uint64_t revision) noexcept {
    return Add(
        uri,
        Base::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(text.Data()),
            text.SizeBytes()),
        revision);
}

Base::Result<void> EmbeddedXamlProvider::Freeze() noexcept {
    frozen_ = true;
    return {};
}

Base::Result<Integration::StreamResourceInfo>
EmbeddedXamlProvider::Open(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return CreateMemoryResource(
                entry.uri,
                entry.bytes.AsSpan(),
                entry.revision);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> EmbeddedXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) return entry.revision;
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> FileXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() || maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }
    Base::String path;
    Base::Result<void> assigned = path.Assign(uri.Path());
    if (!assigned) return assigned.GetStatus();
    std::error_code error;
    const std::filesystem::path filePath(path.CStr());
    const std::uintmax_t size =
        std::filesystem::file_size(filePath, error);
    if (error || size > maxFileBytes_ || size > UINT32_MAX) {
        return Base::Status::Failure(
            error ? Base::ErrorCode::NotFound : Base::ErrorCode::OutOfRange,
            "XAML source file revision could not be read");
    }
    const auto writeTime =
        std::filesystem::last_write_time(filePath, error);
    if (error) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file timestamp could not be read");
    }
    const std::uint64_t ticks = static_cast<std::uint64_t>(
        writeTime.time_since_epoch().count());
    return Base::MixHash64(
        static_cast<std::uint64_t>(size) ^ Base::MixHash64(ticks));
}

Base::Result<Integration::StreamResourceInfo>
FileXamlProvider::Open(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() ||
        maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }

    Base::String path;
    Base::Result<void> assigned = path.Assign(uri.Path());
    if (!assigned) {
        return assigned.GetStatus();
    }
    std::FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path.CStr(), "rb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(path.CStr(), "rb");
#endif
    if (file == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file could not be opened");
    }
    if (std::fseek(file, 0L, SEEK_END) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file size could not be read");
    }
    const long length = std::ftell(file);
    if (length < 0 ||
        static_cast<std::uint64_t>(length) > maxFileBytes_ ||
        static_cast<std::uint64_t>(length) > UINT32_MAX) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML source file exceeds configured limits");
    }
    if (std::fseek(file, 0L, SEEK_SET) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file could not be rewound");
    }

    Base::Result<Base::Ref<FileStream>> stream =
        Base::MakeRef<FileStream>(
            file, static_cast<std::uint64_t>(length));
    if (!stream) {
        std::fclose(file);
        return stream.GetStatus();
    }
    Integration::StreamResourceInfo source;
    source.uri = uri;
    source.stream = std::move(stream).Value();
    Base::Result<std::uint64_t> revision = Revision(uri);
    source.revision = revision
        ? revision.Value()
        : 0U;
    return source;
}

struct Loader::Impl::Operation {
    struct FinalizeState {
        Operation* operation = nullptr;
        const XamlReaderSettings* options = nullptr;
        const Base::ResourceUri* origin = nullptr;
        const CompiledDocument* compiled = nullptr;
    };

    struct PendingResourceMerge {
        ResourceDictionary target;
        ResourceDictionary source;
    };

    Operation(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics,
        const LoadState* runtime) noexcept
        : schema_(&schema),
          providers_(&providers),
          diagnostics_(diagnostics),
          runtime_(runtime) {}

    const LoadState& Runtime() const noexcept {
        static const LoadState empty;
        return runtime_ != nullptr ? *runtime_ : empty;
    }

    Base::Result<LoaderResult> LoadCore(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<LoaderResult> ParseCore(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false) noexcept;
    Base::Result<LoaderResult> ParseStreamCore(
        Base::Stream& stream,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false,
        Base::Vector<Node>* recordingNodes = nullptr) noexcept;
    Base::Result<LoaderResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options) noexcept;
    Base::Result<LoaderResult> LoadCompiledDocument(
        CompiledDocument& document,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<void> ResolveResourceDependencies(
        LoaderResult& result,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> ResolveDictionaryDependencies(
        ResourceDictionary& dictionary,
        LoaderResult& owner,
        const XamlReaderSettings& options,
        std::uint32_t& resourceCount,
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> CommitResourceDependencies(
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> AppendDependencies(
        LoaderResult& destination,
        const LoaderResult& source,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> AppendDependency(
        LoaderResult& destination,
        const Base::ResourceUri& dependency,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> FinalizeResult(
        LoaderResult& result,
        const XamlReaderSettings& options,
        const Base::ResourceUri& origin,
        const CompiledDocument* compiled) noexcept;
    static Base::Result<void> FinalizeLoad(
        LoaderResult& result,
        void* context) noexcept;
    Base::Result<void> ValidateOptions(
        const XamlReaderSettings& options) const noexcept;
    Base::Result<void> CheckPolicy(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options) noexcept;
    bool IsLoading(const Base::ResourceUri& uri) const noexcept;
    Base::Status Failure(
        Base::Status status,
        ::Aero::Diagnostics::DiagnosticCode code,
        Base::StringView message) noexcept;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* runtime_ = nullptr;
    Base::Vector<Base::ResourceUri> loadStack_;
};

Loader::Impl::Impl(
    Schema& schema,
    XamlProviderRegistry& providers,
    Diagnostics::IDiagnosticSink* diagnostics,
    const LoadState* runtime) noexcept
    : schema_(&schema),
      providers_(&providers),
      diagnostics_(diagnostics),
      runtime_(runtime) {}

Base::Result<LoaderResult> Loader::Impl::Load(
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return operation.Failure(
            resolved.GetStatus(),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI is invalid"));
    }
    return operation.LoadCore(
        resolved.Value(), options, {});
}

Base::Result<LoaderResult> Loader::Impl::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.LoadCore(uri, options, {});
}

Base::Result<LoaderResult> Loader::Impl::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.ParseCore(text, baseUri, options, {}, true);
}

Base::Result<LoaderResult> Loader::Impl::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.ParseStreamCore(stream, baseUri, options, {}, true);
}

Base::Result<LoaderResult> Loader::Impl::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return operation.Failure(
            resolved.GetStatus(),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML component URI is invalid"));
    }
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return operation.Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent requires a managed root object"),
            LoaderDiagnosticCodes::LoadComponentTypeMismatch,
            Base::StringView(
                "XAML component root cannot be retained"));
    }
    return operation.LoadCore(
        resolved.Value(), options, retained);
}

Base::Result<LoaderResult> Loader::Impl::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return operation.Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent requires a managed root object"),
            LoaderDiagnosticCodes::LoadComponentTypeMismatch,
            Base::StringView(
                "XAML component root cannot be retained"));
    }
    return operation.LoadCore(uri, options, retained);
}

Base::Result<LoaderResult> Loader::Impl::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.LoadCompiled(bytes, originUri, options);
}

Base::Result<LoaderResult>
Loader::Impl::Operation::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (bytes.Size() > options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Compiled XAML source exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Compiled XAML source exceeds configured limits"));
    }
    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
            bytes,
            schema_->Domain(),
            options.limits.compiled);
    if (!document) {
        const Base::Status status = document.GetStatus();
        if (!originUri.Empty() &&
            (status.code == Base::ErrorCode::Unsupported ||
             status.code == Base::ErrorCode::ValidationFailed)) {
            // A compatible source is authoritative when the cache identity no
            // longer matches this runtime. Hosts may persist a replacement
            // cache after this successful source load.
            return LoadCore(originUri, options, {});
        }
        return status;
    }

    return LoadCompiledDocument(
        document.Value(), originUri, options, {});
}

Base::Result<LoaderResult>
Loader::Impl::Operation::LoadCompiledDocument(
    CompiledDocument& document,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& runtime = Runtime();
    LoadState context;
    context.resources = runtime.resources;
    context.effectiveValues = runtime.effectiveValues;
    context.bindings = runtime.bindings;
    context.fallbackResources = runtime.fallbackResources;
    context.baseUri = &originUri;
    context.templatedParent = runtime.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = runtime.effectLifetime;
    context.effectCommitMode = runtime.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    FinalizeState finalize{
        this, &options, &originUri, &document};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    ObjectWriter writer(*schema_, diagnostics_);
    Base::Result<LoaderResult> loaded =
        runtime.dispatcher != nullptr &&
        runtime.dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<LoaderResult> {
              Meta::ObjectFactoryScope services(
                  *runtime.dispatcher,
                  *runtime.dependencyProperties,
                  schema_->Metadata());
              ObjectBuilder state(writer);
              return state.Load(document, context);
          }()
        : [&]() noexcept {
              ObjectBuilder state(writer);
              return state.Load(document, context);
          }();
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<LoaderResult> Loader::Impl::Operation::LoadCore(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& runtime = Runtime();
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    Base::Result<void> policy = CheckPolicy(uri, options);
    if (!policy) {
        return policy.GetStatus();
    }
    if (IsLoading(uri)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive XAML source load was detected"),
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive XAML source load was detected"));
    }
    if (loadStack_.Size() >=
        options.limits.maxDependencyDepth) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency depth exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency depth exceeds configured limits"));
    }

    Base::Result<XamlProviderResolution> provider =
        providers_->ResolveDetailed(uri);
    if (!provider) {
        return Failure(
            provider.GetStatus(),
            LoaderDiagnosticCodes::XamlProviderNotFound,
            Base::StringView(
                "No XAML source provider matches the resource URI"));
    }
    Base::Result<void> pushed =
        loadStack_.PushBack(uri);
    if (!pushed) {
        return pushed.GetStatus();
    }

    if (runtime.documentCache != nullptr) {
        Base::Result<std::uint64_t> probedRevision =
            provider.Value().provider->Revision(uri);
        if (probedRevision && probedRevision.Value() != 0U) {
            Base::Result<DocumentCacheLookup> cached =
                runtime.documentCache->Lookup(
                    uri,
                    probedRevision.Value(),
                    provider.Value().cacheIdentity,
                    schema_->Domain(),
                    options.limits.compiled);
            if (cached && cached.Value().hit) {
                Base::Result<LoaderResult> loaded =
                    LoadCompiledDocument(
                        cached.Value().document,
                        uri,
                        options,
                        existingRoot);
                if (loaded) {
                    static_cast<void>(runtime.documentCache->Store(
                        uri,
                        probedRevision.Value(),
                        provider.Value().cacheIdentity,
                        cached.Value().document,
                        {loaded.Value().dependencies.Data(),
                         loaded.Value().dependencies.Size()}));
                }
                loadStack_.PopBack();
                return loaded;
            }
        }
    }

    Base::Result<Integration::StreamResourceInfo> source =
        provider.Value().provider->Open(uri);
    if (!source) {
        loadStack_.PopBack();
        return Failure(
            source.GetStatus(),
            LoaderDiagnosticCodes::SourceLoadFailed,
            Base::StringView("XAML source could not be loaded"));
    }
    Integration::StreamResourceInfo sourceInfo =
        std::move(source).Value();
    if (!sourceInfo.stream ||
        !sourceInfo.stream->CanRead()) {
        loadStack_.PopBack();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML source stream is invalid"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source stream could not be read"));
    }

    const Base::ResourceUri& origin =
        sourceInfo.uri.Empty()
        ? uri
        : sourceInfo.uri;
    HashingStream hashing(*sourceInfo.stream);
    Base::Vector<Node> recordedNodes;
    Base::Result<LoaderResult> loaded = ParseStreamCore(
        hashing,
        origin,
        options,
        existingRoot,
        true,
        runtime.documentCache != nullptr
            ? &recordedNodes
            : nullptr);
    if (sourceInfo.revision == 0U) {
        sourceInfo.revision = hashing.Hash();
    }
    if (loaded && runtime.documentCache != nullptr &&
        !recordedNodes.Empty()) {
        Base::Result<CompiledDocument> compiled =
            CompiledDocument::Compile(
                {recordedNodes.Data(), recordedNodes.Size()},
                *schema_,
                origin);
        if (compiled) {
            for (const Base::ResourceUri& dependency :
                 loaded.Value().dependencies) {
                Base::Result<void> added =
                    compiled.Value().AddDependency(dependency);
                if (!added) {
                    compiled = added.GetStatus();
                    break;
                }
            }
        }
        if (compiled) {
            static_cast<void>(runtime.documentCache->Store(
                uri,
                sourceInfo.revision,
                provider.Value().cacheIdentity,
                compiled.Value(),
                {loaded.Value().dependencies.Data(),
                 loaded.Value().dependencies.Size()}));
        }
    }
    loadStack_.PopBack();
    return loaded;
}

Base::Result<LoaderResult> Loader::Impl::Operation::ParseCore(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    bool deferUnresolvedStaticResources) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (text.SizeBytes() >
        options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML source exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source exceeds configured limits"));
    }

    const Base::Span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(text.Data()),
        text.SizeBytes()};
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    return ParseStreamCore(
        *stream.Value(),
        baseUri,
        options,
        existingRoot,
        deferUnresolvedStaticResources);
}

Base::Result<LoaderResult> Loader::Impl::Operation::ParseStreamCore(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    bool deferUnresolvedStaticResources,
    Base::Vector<Node>* recordingNodes) noexcept {
    const LoadState& runtime = Runtime();
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }

#if AERO_WITH_EXPAT
    ExpatXmlTokenizer tokenizer(options.limits.xml);
#else
    Utf8XmlTokenizer tokenizer(options.limits.xml);
#endif
    Base::Result<void> reset =
        tokenizer.Reset(stream, diagnostics_);
    if (!reset) return reset.GetStatus();

    NodeReader reader(tokenizer, diagnostics_);
    LoadState context;
    context.resources = runtime.resources;
    context.effectiveValues = runtime.effectiveValues;
    context.bindings = runtime.bindings;
    context.fallbackResources = runtime.fallbackResources;
    context.baseUri = &baseUri;
    context.templatedParent = runtime.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = runtime.effectLifetime;
    context.effectCommitMode = runtime.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    context.deferUnresolvedStaticResources =
        deferUnresolvedStaticResources;
    context.recordingNodes = recordingNodes;
    FinalizeState finalize{
        this, &options, &baseUri, nullptr};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    ObjectWriter writer(*schema_, diagnostics_);
    Base::Result<LoaderResult> loaded =
        runtime.dispatcher != nullptr &&
        runtime.dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<LoaderResult> {
              Meta::ObjectFactoryScope services(
                  *runtime.dispatcher,
                  *runtime.dependencyProperties,
                  schema_->Metadata());
              ObjectBuilder state(writer);
              return state.Load(reader, context);
          }()
        : [&]() noexcept -> Base::Result<LoaderResult> {
              ObjectBuilder state(writer);
              return state.Load(reader, context);
          }();
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<void> Loader::Impl::Operation::FinalizeLoad(
    LoaderResult& result,
    void* context) noexcept {
    auto* finalize =
        static_cast<FinalizeState*>(context);
    if (finalize == nullptr ||
        finalize->operation == nullptr ||
        finalize->options == nullptr ||
        finalize->origin == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML load finalization context is invalid");
    }
    return finalize->operation->FinalizeResult(
        result,
        *finalize->options,
        *finalize->origin,
        finalize->compiled);
}

Base::Result<void> Loader::Impl::Operation::FinalizeResult(
    LoaderResult& result,
    const XamlReaderSettings& options,
    const Base::ResourceUri& origin,
    const CompiledDocument* compiled) noexcept {
    const Base::ResourceUri& effectiveOrigin =
        compiled != nullptr && origin.Empty()
        ? compiled->OriginUri()
        : origin;
    result.canonicalUri = effectiveOrigin;
    if (compiled != nullptr) {
        for (const Base::ResourceUri& dependency :
             compiled->Dependencies()) {
            Base::Result<void> appended =
                AppendDependency(
                    result, dependency, options);
            if (!appended) return appended.GetStatus();
        }
    }
    Base::Result<void> originDependency =
        AppendDependency(
            result, effectiveOrigin, options);
    if (!originDependency) {
        return originDependency.GetStatus();
    }
    return ResolveResourceDependencies(
        result, options);
}

Base::Result<void>
Loader::Impl::Operation::ResolveResourceDependencies(
    LoaderResult& result,
    const XamlReaderSettings& options) noexcept {
    std::uint32_t resourceCount = 0U;
    Base::Vector<PendingResourceMerge> pending;

    auto resolveDictionary = [&](ResourceDictionary& dictionary)
        noexcept -> Base::Result<void> {
        if (dictionary.Size() == 0U &&
            dictionary.MergedDictionaryCount() == 0U &&
            dictionary.GetSource().Empty()) {
            return {};
        }
        return ResolveDictionaryDependencies(
            dictionary,
            result,
            options,
            resourceCount,
            pending);
    };

    Base::Result<void> resolved = resolveDictionary(result.resources);
    if (!resolved) return resolved.GetStatus();

    ResourceDictionary* rootResources = nullptr;
    if (result.root) {
        rootResources = schema_->ResolveResourceScope(
            result.root->RuntimeType(), *result.root);
        if (rootResources != nullptr) {
            resolved = resolveDictionary(*rootResources);
            if (!resolved) return resolved.GetStatus();
        }
    }

    for (Aero::Visual* visual : result.visualContent.nodes) {
        if (visual == nullptr ||
            (result.root && visual == result.root.Get())) {
            continue;
        }
        ResourceDictionary* resources = schema_->ResolveResourceScope(
            visual->RuntimeType(), *visual);
        if (resources == nullptr || resources == rootResources) continue;
        resolved = resolveDictionary(*resources);
        if (!resolved) return resolved.GetStatus();
    }
    return CommitResourceDependencies(pending);
}

Base::Result<void>
Loader::Impl::Operation::ResolveDictionaryDependencies(
    ResourceDictionary& dictionary,
    LoaderResult& owner,
    const XamlReaderSettings& options,
    std::uint32_t& resourceCount,
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    if (resourceCount >
            options.limits.maxResources ||
        dictionary.Size() >
            options.limits.maxResources - resourceCount) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML resource count exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML resource count exceeds configured limits"));
    }
    resourceCount += dictionary.Size();

    for (std::uint32_t index = 0U;
         index < dictionary.Size();
         ++index) {
        Base::Result<Aero::ResourceEntrySnapshot>
            entry = dictionary.EntryAt(index);
        if (!entry) return entry.GetStatus();
        const Meta::Value& value = entry.Value().value;
        if (value.Kind() != Meta::ValueKind::Object ||
            value.IsNullObject() ||
            !value.AsObject()) {
            continue;
        }
        Base::Object& object = *value.AsObject();
        ResourceDictionary* nested = schema_->ResolveResourceScope(
            object.RuntimeType(), object);
        if (nested == nullptr ||
            (nested->Size() == 0U &&
             nested->MergedDictionaryCount() == 0U &&
             nested->GetSource().Empty())) {
            continue;
        }
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                *nested,
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const std::uint32_t mergedCount =
        dictionary.MergedDictionaryCount();
    for (std::uint32_t index = 0U;
         index < mergedCount;
         ++index) {
        Base::Result<ResourceDictionary> merged =
            dictionary.MergedDictionaryAt(index);
        if (!merged) return merged.GetStatus();
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                merged.Value(),
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const Base::ResourceUri source =
        dictionary.GetSource();
    if (source.Empty()) return {};
    if ((!owner.canonicalUri.Empty() &&
         source == owner.canonicalUri) ||
        IsLoading(source)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive ResourceDictionary Source was detected"),
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive ResourceDictionary Source was detected"));
    }

    // Merged dictionaries are evaluated in declaration order. Supply already
    // discovered siblings as ambient resources while loading the next source,
    // so WPF-style DynamicResource values in styles can resolve against an
    // earlier palette or brush dictionary.
    ResourceDictionary ambientResources;
    Base::Result<void> ambientMerged =
        ambientResources.AddMerged(dictionary);
    for (PendingResourceMerge& discovered : pending) {
        if (ambientMerged) {
            ambientMerged = ambientResources.AddMerged(
                discovered.source);
        }
    }
    if (!ambientMerged) return ambientMerged.GetStatus();
    const LoadState& runtime = Runtime();
    LoadState resourceContext = runtime;
    resourceContext.resources = &ambientResources;
    resourceContext.fallbackResources = &ambientResources;
    const LoadState* previousRuntime = runtime_;
    runtime_ = &resourceContext;
    Base::Result<LoaderResult> loaded =
        LoadCore(source, options, {});
    runtime_ = previousRuntime;
    if (!loaded) {
        return Failure(
            loaded.GetStatus(),
            LoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source could not be loaded"));
    }
    if (!loaded.Value().root ||
        loaded.Value().root->RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        loaded.Value().Clear();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "ResourceDictionary Source root has an incompatible type"),
            LoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source root must be ResourceDictionary"));
    }
    auto& sourceDictionary =
        static_cast<ResourceDictionary&>(
            *loaded.Value().root);
    Base::Result<void> dependencies =
        AppendDependencies(owner, loaded.Value(), options);
    if (!dependencies) {
        loaded.Value().Clear();
        return dependencies.GetStatus();
    }
    PendingResourceMerge merge;
    Base::Result<ResourceDictionary> target =
        dictionary.Share();
    if (!target) {
        loaded.Value().Clear();
        return target.GetStatus();
    }
    merge.target = std::move(target).Value();
    merge.source = std::move(sourceDictionary);
    Base::Result<void> staged =
        pending.PushBack(std::move(merge));
    loaded.Value().Clear();
    return staged;
}

Base::Result<void>
Loader::Impl::Operation::CommitResourceDependencies(
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    std::uint32_t committed = 0U;
    Base::Status failure = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML resource merge target is invalid");
    for (; committed < pending.Size(); ++committed) {
        PendingResourceMerge& merge = pending[committed];
        Base::Result<void> added =
        merge.target.AddMerged(merge.source);
        if (!added) {
            failure = added.GetStatus();
            break;
        }
    }
    if (committed == pending.Size()) return {};

    for (std::uint32_t index = committed;
         index > 0U;
         --index) {
        PendingResourceMerge& merge =
            pending[index - 1U];
        Base::Result<bool> removed =
            merge.target.RemoveMerged(
                merge.source);
        if (!removed || !removed.Value()) {
            return removed
                ? Base::Result<void>(
                      Base::Status::Failure(
                          Base::ErrorCode::InvalidState,
                          "XAML resource dependency rollback failed"))
                : Base::Result<void>(
                      removed.GetStatus());
        }
    }
    return failure;
}

Base::Result<void> Loader::Impl::Operation::AppendDependencies(
    LoaderResult& destination,
    const LoaderResult& source,
    const XamlReaderSettings& options) noexcept {
    for (const Base::ResourceUri& dependency :
         source.dependencies) {
        Base::Result<void> appended =
            AppendDependency(
                destination, dependency, options);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> Loader::Impl::Operation::AppendDependency(
    LoaderResult& destination,
    const Base::ResourceUri& dependency,
    const XamlReaderSettings& options) noexcept {
    if (dependency.Empty()) return {};
    for (const Base::ResourceUri& existing :
         destination.dependencies) {
        if (existing == dependency) return {};
    }
    if (destination.dependencies.Size() >=
        options.limits.compiled.maxDependencies) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency count exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency count exceeds configured limits"));
    }
    return destination.dependencies.PushBack(
        dependency);
}

Base::Result<void> Loader::Impl::Operation::ValidateOptions(
    const XamlReaderSettings& options) const noexcept {
    const LoadState& runtime = Runtime();
    if (schema_ == nullptr || providers_ == nullptr ||
        !schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML loader requires a frozen schema and provider registry");
    }
    if (options.limits.maxSourceBytes == 0U ||
        options.limits.maxObjects == 0U ||
        options.limits.maxResources == 0U ||
        options.limits.maxDependencyDepth == 0U ||
        options.limits.xml.maxInputBytes == 0U ||
        options.limits.xml.maxDepth == 0U ||
        options.limits.compiled.maxNodes == 0U ||
        options.limits.compiled.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML load limits must be positive");
    }
    if ((runtime.dispatcher == nullptr) !=
        (runtime.dependencyProperties == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML object factory require dispatcher and property metadata");
    }
    return {};
}

Base::Result<void> Loader::Impl::Operation::CheckPolicy(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (uri.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML resource URI cannot be empty"),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI cannot be empty"));
    }
    if (uri.IsNetwork() &&
        !options.policy.allowNetwork) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Network XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Network XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("file") &&
        !options.policy.allowFile) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "File XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "File XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("pack") &&
        !options.policy.allowPackApplication) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Pack application XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Pack application XAML sources are disabled by policy"));
    }
    return {};
}

bool Loader::Impl::Operation::IsLoading(
    const Base::ResourceUri& uri) const noexcept {
    for (const Base::ResourceUri& active : loadStack_) {
        if (active == uri) {
            return true;
        }
    }
    return false;
}

Base::Status Loader::Impl::Operation::Failure(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode code,
    Base::StringView message) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> diagnostic =
            ::Aero::Diagnostics::Diagnostic::Create(
                code,
                ::Aero::Diagnostics::DiagnosticSeverity::Error,
                message);
        if (diagnostic) {
            diagnostics_->Report(
                std::move(diagnostic).Value());
        }
    }
    return status;
}

} // namespace Aero::Markup

namespace Aero::Markup {

namespace {

Base::Result<XamlDocument> AdoptResult(
    Base::Result<LoaderResult>&& loaded,
    Base::IAllocator& allocator) noexcept {
    if (!loaded) return loaded.GetStatus();
    Base::Result<XamlDocument> document =
        Aero::Internal::XamlDocumentPrivate::Adopt(
        std::move(loaded).Value(), allocator);
    return document;
}

} // namespace

Loader::Loader(
    Schema& schema,
    XamlProviderRegistry& providers,
    Diagnostics::IDiagnosticSink* diagnostics,
    Base::IAllocator* allocator,
    const LoadState* runtime) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory != nullptr) {
        impl_ = new (memory) Impl(
            schema, providers, diagnostics, runtime);
    }
}

Loader::~Loader() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<XamlDocument> Loader::Load(
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        impl_->Load(uri, options), *allocator_);
}

Base::Result<XamlDocument> Loader::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        impl_->Load(uri, options), *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        impl_->Parse(text, baseUri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        impl_->Parse(stream, baseUri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        impl_->LoadComponent(existingRoot, uri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        impl_->LoadComponent(existingRoot, uri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        impl_->LoadCompiled(bytes, originUri, options),
        *allocator_);
}

} // namespace Aero::Markup


// ===== Resources =====




#include <Aero/Application.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include "markup/MarkupInternal.hpp"
#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>

namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;


Base::Status InvalidResource(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Base::Result<void> AddResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    if (scopeOwner.RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "XAML resource scope is not a ResourceDictionary");
    }
    return static_cast<ResourceDictionary&>(scopeOwner)
        .Add(key, value);
}

Base::Result<void> AddFrameworkResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    auto* element =
        static_cast<FrameworkElement*>(
            static_cast<Visual&>(scopeOwner)
                .AsFrameworkElement());
    if (element == nullptr) {
        return InvalidResource(
            "XAML resource scope is not a FrameworkElement");
    }
    return element->GetResources().Add(key, value);
}

Base::Result<void> AddApplicationResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    // This callback is selected through the inherited Application XAML facet,
    // so derived application types are valid scope owners.
    Base::Ref<ResourceDictionary> resources =
        static_cast<Aero::Application&>(scopeOwner).GetResources();
    return resources
        ? resources->Add(key, value)
        : Base::Result<void>(InvalidResource(
              "Application resource dictionary is unavailable"));
}

ResourceDictionary* ResolveDictionaryScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    return scopeOwner.RuntimeType() ==
            ResourceDictionary::StaticTypeId()
        ? &static_cast<ResourceDictionary&>(
              scopeOwner)
        : nullptr;
}

ResourceDictionary* ResolveFrameworkScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    Visual& visual =
        static_cast<Visual&>(scopeOwner);
    FrameworkElement* element =
        visual.AsFrameworkElement();
    return element != nullptr
        ? &element->GetResources()
        : nullptr;
}

ResourceDictionary* ResolveApplicationScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    Base::Ref<ResourceDictionary> resources =
        static_cast<Aero::Application&>(scopeOwner).GetResources();
    return resources.Get();
}

} // namespace

Base::Result<void> ResourceExtension::Register(
    Schema& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML resource extension registration is invalid");
    }
    const PropertyInfo* source =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Source"),
            false);
    const PropertyInfo* merged =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("MergedDictionaries"),
            false);
    const PropertyInfo* entries =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Entries"),
            false);
    if (source == nullptr || merged == nullptr ||
        entries == nullptr ||
        source->ValueType() !=
            TypeOf<Base::ResourceUri>() ||
        merged->ValueType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "ResourceDictionary XAML metadata is incomplete");
    }

    schema_ = &schema;
    Base::Result<void> status =
        Detail::SchemaPrivate::AddResourceScope(schema, {
            ResourceDictionary::StaticTypeId(),
            true,
            &AddResource,
            &ResolveDictionaryScope,
            this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = Detail::SchemaPrivate::AddResourceScope(schema, {
        FrameworkElement::StaticTypeId(),
        true,
        &AddFrameworkResource,
        &ResolveFrameworkScope,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = Detail::SchemaPrivate::AddResourceScope(schema, {
        Aero::Application::StaticTypeId(),
        true,
        &AddApplicationResource,
        &ResolveApplicationScope,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup


// ===== XamlDocument =====

#include <Aero/Markup.hpp>

#include <Aero/Base/Result.hpp>
#include <Aero/Resources.hpp>



#include <new>
#include <utility>

namespace Aero::Markup {

struct XamlDocument::Impl {
    explicit Impl(LoaderResult&& value) noexcept
        : result(std::move(value)) {}

    LoaderResult result;
};

XamlDocument::~XamlDocument() noexcept {
    Reset();
}

XamlDocument::XamlDocument(XamlDocument&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
}

XamlDocument& XamlDocument::operator=(XamlDocument&& other) noexcept {
    if (this == &other) return *this;
    Reset();
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
    return *this;
}

} // namespace Aero::Markup

namespace Aero::Internal {

Base::Result<Markup::XamlDocument> XamlDocumentPrivate::Adopt(
    Markup::LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    if (!result.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document requires a loaded root object");
    }
    void* memory = allocator.Allocate({
        sizeof(Markup::XamlDocument::Impl),
        alignof(Markup::XamlDocument::Impl),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "UI document allocation failed");
    }
    Markup::XamlDocument document;
    document.allocator_ = &allocator;
    document.impl_ =
        new (memory) Markup::XamlDocument::Impl(std::move(result));
    return document;
}

} // namespace Aero::Internal

namespace Aero::Markup {

bool XamlDocument::IsValid() const noexcept {
    return impl_ != nullptr && impl_->result.root;
}

const Base::Ref<Base::Object>& XamlDocument::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    return impl_ != nullptr ? impl_->result.root : empty;
}

Base::Object* XamlDocument::RootObject(
    Meta::TypeId expectedType) noexcept {
    if (impl_ == nullptr || !impl_->result.root) return nullptr;
    Base::Object* root = impl_->result.root.Get();
    if (expectedType == Meta::InvalidTypeId) return root;
    const Meta::Registry* metadata = impl_->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        root->RuntimeType(), expectedType)
        ? root
        : nullptr;
}

Base::Object* XamlDocument::FindName(
    Base::StringView name,
    Meta::TypeId expectedType) noexcept {
    if (impl_ == nullptr || name.Empty()) return nullptr;
    Base::Object* object = impl_->result.names.Find(name);
    if (object == nullptr || expectedType == Meta::InvalidTypeId) {
        return object;
    }
    const Meta::Registry* metadata = impl_->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        object->RuntimeType(), expectedType)
        ? object
        : nullptr;
}

std::uint32_t XamlDocument::NamedObjectCount() const noexcept {
    return impl_ != nullptr ? impl_->result.names.Size() : 0U;
}

Aero::ResourceDictionary* XamlDocument::Resources() noexcept {
    return impl_ != nullptr ? &impl_->result.resources : nullptr;
}

const Aero::ResourceDictionary* XamlDocument::Resources() const noexcept {
    return impl_ != nullptr ? &impl_->result.resources : nullptr;
}

const Base::ResourceUri& XamlDocument::CanonicalUri() const noexcept {
    static const Base::ResourceUri empty;
    return impl_ != nullptr ? impl_->result.canonicalUri : empty;
}

Base::Span<const Base::ResourceUri> XamlDocument::Dependencies() const noexcept {
    return impl_ != nullptr
        ? Base::Span<const Base::ResourceUri>{
              impl_->result.dependencies.Data(),
              impl_->result.dependencies.Size()}
        : Base::Span<const Base::ResourceUri>{};
}

} // namespace Aero::Markup

namespace Aero::Internal {

const Markup::EffectLifetime*
XamlDocumentPrivate::RuntimeLifetime(
    const Markup::XamlDocument& document) noexcept {
    return document.impl_ != nullptr
        ? document.impl_->result.runtimeLifetime.Get()
        : nullptr;
}

Markup::LoaderResult XamlDocumentPrivate::Take(
    Markup::XamlDocument& document) noexcept {
    if (document.impl_ == nullptr) {
        Markup::LoaderResult empty;
        return empty;
    }
    Markup::LoaderResult result =
        std::move(document.impl_->result);
    document.Reset();
    return result;
}

} // namespace Aero::Internal

namespace Aero::Markup {

void XamlDocument::Reset() noexcept {
    if (impl_ == nullptr) return;
    impl_->result.Clear();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(XamlDocument::Impl),
        alignof(XamlDocument::Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
    allocator_ = nullptr;
}

} // namespace Aero::Markup

#endif
#if defined(AERO_MARKUP_XAML_READER_ONLY)
// ===== XamlReader =====
#include <Aero/Markup.hpp>

#include <Aero/View.hpp>

#include <utility>

namespace Aero::Markup {

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return view_->LoadDocument(uri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::Stream& source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return view_->ParseStreamDocument(
        source, baseUri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::LoadComponentCore(
    Base::StringView uri,
    Meta::TypeId expectedRoot,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<XamlDocument> loaded = Load(
        uri, settings, diagnostics);
    if (!loaded) return loaded.GetStatus();
    const Base::Ref<Base::Object>& root = loaded.Value().Root();
    if (!root || expectedRoot == Meta::InvalidTypeId ||
        !view_->IsInstanceOf(*root, expectedRoot)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return view_->ParseDocument(source, baseUri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    return view_->LoadCompiledDocument(bytes, originUri);
}

Base::Result<void> XamlReader::RegisterXamlProvider(
    Integration::XamlProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    return view_->AddXamlProvider(provider, scheme, assembly);
}

Base::Result<void> XamlReader::Mount(
    Controls::ContentControl& host,
    XamlDocument&& document) noexcept {
    return view_->MountContent(host, std::move(document));
}

Base::Result<void> XamlReader::Unmount(
    Controls::ContentControl& host) noexcept {
    return view_->UnmountContent(host);
}

Base::Result<void> XamlReader::LoadResources(
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return view_->LoadResources(layer, uri, mode, diagnostics);
}

Base::Result<void> XamlReader::LoadCompiledResources(
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    return view_->LoadCompiledResources(
        layer, bytes, originUri, mode);
}

void XamlReader::SetResources(
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    view_->SetResourceDictionary(layer, dictionary, mode);
}

Base::Result<void> XamlReader::LoadTheme(
    BuiltInTheme theme) noexcept {
    return view_->LoadBuiltInTheme(theme);
}

} // namespace Aero::Markup

#endif
