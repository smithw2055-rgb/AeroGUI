#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
// Consolidated implementation. Keep sections ordered by dependency.

#include <atomic>

// ===== CompiledCache =====


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


// Canonical compiled-document implementation.

#include <cmath>
#include <cstring>
#include <utility>

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

template<class T>
Base::Result<Meta::Value> MakeStoredCustomValue(
    Meta::TypeId type,
    const T& value) noexcept {
    Base::Result<Base::Ref<Meta::ValueTypeSemantics>> semantics =
        Base::MakeRef<Meta::ValueTypeSemantics>(
            Meta::MakeValueTypeRegistration<T>());
    if (!semantics) return semantics.GetStatus();
    return Meta::Value::TryFromCustom(
        type, &value, semantics.Value());
}


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

Base::Result<void> AppendU16(
    Base::Vector<std::uint8_t>& output,
    std::uint16_t value) noexcept {
    Base::Result<void> appended = output.PushBack(
        static_cast<std::uint8_t>(value));
    if (!appended) return appended.GetStatus();
    return output.PushBack(static_cast<std::uint8_t>(value >> 8U));
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

Base::Result<void> AppendVarU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    do {
        std::uint8_t byte =
            static_cast<std::uint8_t>(value & 0x7FU);
        value >>= 7U;
        if (value != 0U) {
            byte = static_cast<std::uint8_t>(
                byte | 0x80U);
        }
        Base::Result<void> appended =
            output.PushBack(byte);
        if (!appended) return appended.GetStatus();
    } while (value != 0U);
    return {};
}

Base::Result<void> AppendVarU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    return AppendVarU64(output, value);
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

    Base::Result<std::uint16_t> ReadU16() noexcept {
        if (bytes_.Size() - offset_ < 2U) return Truncated();
        const std::uint16_t value = static_cast<std::uint16_t>(
            bytes_[offset_]) |
            static_cast<std::uint16_t>(bytes_[offset_ + 1U] << 8U);
        offset_ += 2U;
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

    Base::Result<std::uint64_t> ReadVarU64() noexcept {
        std::uint64_t value = 0U;
        for (std::uint32_t byteIndex = 0U;
             byteIndex < 10U; ++byteIndex) {
            Base::Result<std::uint8_t> byte = ReadU8();
            if (!byte) return byte.GetStatus();
            const std::uint8_t payload =
                static_cast<std::uint8_t>(
                    byte.Value() & 0x7FU);
            if (byteIndex == 9U && payload > 1U) {
                return InvalidVarUInt();
            }
            value |= static_cast<std::uint64_t>(
                payload) << (byteIndex * 7U);
            if ((byte.Value() & 0x80U) == 0U) {
                return value;
            }
        }
        return InvalidVarUInt();
    }

    Base::Result<std::uint32_t> ReadVarU32() noexcept {
        Base::Result<std::uint64_t> value =
            ReadVarU64();
        if (!value) return value.GetStatus();
        if (value.Value() > UINT32_MAX) {
            return InvalidVarUInt();
        }
        return static_cast<std::uint32_t>(
            value.Value());
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

    std::uint32_t Offset() const noexcept { return offset_; }

private:
    static Base::Status Truncated() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Compiled XAML payload is truncated");
    }

    static Base::Status InvalidVarUInt() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 variable-length integer is invalid");
    }

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
};

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

Base::Result<std::uint32_t> InternString(
    Base::Vector<Base::String>& strings,
    Base::StringView value) noexcept {
    for (std::uint32_t index = 0U; index < strings.Size(); ++index) {
        if (strings[index].View() == value) return index;
    }
    Base::String stored;
    Base::Result<void> assigned = stored.Assign(value);
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> appended = strings.PushBack(std::move(stored));
    if (!appended) return appended.GetStatus();
    return strings.Size() - 1U;
}

Base::Result<std::uint32_t> InternId(
    Base::Vector<std::uint64_t>& ids,
    std::uint64_t value) noexcept {
    if (value == 0U) return InvalidTableIndex;
    for (std::uint32_t index = 0U; index < ids.Size(); ++index) {
        if (ids[index] == value) return index;
    }
    Base::Result<void> appended = ids.PushBack(value);
    if (!appended) return appended.GetStatus();
    return ids.Size() - 1U;
}

Base::Result<std::uint32_t> FindInternedString(
    const Base::Vector<Base::String>& strings,
    Base::StringView value) noexcept {
    for (std::uint32_t index = 0U;
         index < strings.Size(); ++index) {
        if (strings[index].View() == value) {
            return index;
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "AXB2 instruction string was not interned");
}

Base::Result<std::uint32_t> FindInternedId(
    const Base::Vector<std::uint64_t>& ids,
    std::uint64_t value) noexcept {
    if (value == 0U) return InvalidTableIndex;
    for (std::uint32_t index = 0U;
         index < ids.Size(); ++index) {
        if (ids[index] == value) return index;
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "AXB2 instruction id was not interned");
}

Base::Result<AxbValueRecord> MakeAxbValueRecord(
    const Node& node,
    Base::Vector<Base::String>& strings,
    Base::Vector<std::uint64_t>& types) noexcept {
    const auto makeText = [&]() noexcept
        -> Base::Result<AxbValueRecord> {
        Base::Result<std::uint32_t> stringIndex =
            InternString(strings, node.Value());
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
        InternId(types, value.Type());
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
            InternString(strings, value.AsString());
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
            record.payloadCount = 4U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().left);
            record.payload[1] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().top);
            record.payload[2] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().right);
            record.payload[3] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().bottom);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::CornerRadius>()) {
            Base::Result<Base::CornerRadius> decoded =
                Meta::ValueCodec<Base::CornerRadius>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 4U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().topLeft);
            record.payload[1] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().topRight);
            record.payload[2] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().bottomRight);
            record.payload[3] =
                CopyValueBits<std::uint64_t>(
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
            record.payloadCount = 2U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().x);
            record.payload[1] =
                CopyValueBits<std::uint64_t>(
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
                    decoded.Value().value);
            record.payload[1] =
                static_cast<std::uint64_t>(
                    decoded.Value().unit);
            return record;
        }
        return makeText();
    }
    case Meta::ValueKind::Unset:
    case Meta::ValueKind::Object:
        return makeText();
    }
    return makeText();
}

Base::Result<std::uint32_t> InternAxbValue(
    Base::Vector<AxbValueRecord>& values,
    const AxbValueRecord& value) noexcept {
    for (std::uint32_t index = 0U;
         index < values.Size(); ++index) {
        const AxbValueRecord& current = values[index];
        if (current.kind != value.kind ||
            current.payloadCount != value.payloadCount ||
            current.typeIndex != value.typeIndex) {
            continue;
        }
        bool equal = true;
        for (std::uint32_t payloadIndex = 0U;
             payloadIndex < value.payloadCount;
             ++payloadIndex) {
            equal = equal &&
                current.payload[payloadIndex] ==
                    value.payload[payloadIndex];
        }
        if (equal) return index;
    }
    Base::Result<void> appended =
        values.PushBack(value);
    if (!appended) return appended.GetStatus();
    return values.Size() - 1U;
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
            return MakeStoredCustomValue(
                type,
                record.payload[1] != 0U
                    ? ::Aero::Length::Auto()
                    : ::Aero::Length::Pixels(value));
        }
        if (type == Meta::TypeOf<Base::Thickness>() &&
            requirePayloads(4U)) {
            const Base::Thickness value{
                CopyValueBits<double>(record.payload[0]),
                CopyValueBits<double>(record.payload[1]),
                CopyValueBits<double>(record.payload[2]),
                CopyValueBits<double>(record.payload[3])};
            if (!::Aero::IsFinite(value)) break;
            return MakeStoredCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::CornerRadius>() &&
            requirePayloads(4U)) {
            const Base::CornerRadius value{
                CopyValueBits<double>(record.payload[0]),
                CopyValueBits<double>(record.payload[1]),
                CopyValueBits<double>(record.payload[2]),
                CopyValueBits<double>(record.payload[3])};
            if (!std::isfinite(value.topLeft) ||
                !std::isfinite(value.topRight) ||
                !std::isfinite(value.bottomRight) ||
                !std::isfinite(value.bottomLeft)) {
                break;
            }
            return MakeStoredCustomValue(type, value);
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
            return MakeStoredCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::Point>() &&
            requirePayloads(2U)) {
            const Base::Point value{
                CopyValueBits<double>(record.payload[0]),
                CopyValueBits<double>(record.payload[1])};
            if (!::Aero::IsFinite(value)) break;
            return MakeStoredCustomValue(type, value);
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
            return MakeStoredCustomValue(type, value);
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
            return MakeStoredCustomValue(
                type,
                ::Aero::GridLength{
                    value,
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
            contentWritable &&
                    content.Value().kind ==
                        Meta::ContentKind::Collection
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

    Base::Result<std::uint32_t> originString = InternString(
        strings, originUri_.Canonical());
    if (!originString) return originString.GetStatus();
    for (const Base::ResourceUri& dependency : dependencies_) {
        Base::Result<std::uint32_t> index = InternString(
            strings, dependency.Canonical());
        if (!index) return index.GetStatus();
    }
    Base::Result<void> valueIndexCapacity =
        nodeValueIndices.Reserve(nodes_.Size());
    if (!valueIndexCapacity) {
        return valueIndexCapacity.GetStatus();
    }

    for (const Node& node : nodes_) {
        if (NeedsInstructionQualifiedName(node)) {
            const Base::StringView qualifiedName[] = {
                node.name_.prefix_.View(),
                node.name_.localName_.View(),
                node.name_.namespaceUri_.View()};
            for (Base::StringView value : qualifiedName) {
                Base::Result<std::uint32_t> index =
                    InternString(strings, value);
                if (!index) return index.GetStatus();
            }
        }
        if (node.kind_ == NodeKind::NamespaceDeclaration) {
            Base::Result<std::uint32_t> prefix =
                InternString(
                    strings,
                    node.namespacePrefix_.View());
            if (!prefix) return prefix.GetStatus();
            Base::Result<std::uint32_t> uri =
                InternString(
                    strings,
                    node.namespaceUri_.View());
            if (!uri) return uri.GetStatus();
        }

        std::uint32_t valueIndex = InvalidTableIndex;
        if (node.kind_ == NodeKind::Value) {
            Base::Result<AxbValueRecord> record =
                MakeAxbValueRecord(node, strings, types);
            if (!record) return record.GetStatus();
            Base::Result<std::uint32_t> interned =
                InternAxbValue(values, record.Value());
            if (!interned) return interned.GetStatus();
            valueIndex = interned.Value();
        }
        Base::Result<void> tracked =
            nodeValueIndices.PushBack(valueIndex);
        if (!tracked) return tracked.GetStatus();

        Base::Result<std::uint32_t> typeIndex = InternId(
            types, node.compiledTypeId_);
        if (!typeIndex) return typeIndex.GetStatus();
        Base::Result<std::uint32_t> memberIndex = InternId(
            members, node.compiledMemberId_);
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
        return sections.PushBack(std::move(section));
    };

    Base::Vector<std::uint8_t> dependencyBytes;
    Base::Result<void> result = AppendU32(
        dependencyBytes, originString.Value());
    if (!result) return result.GetStatus();
    result = AppendU32(dependencyBytes, dependencies_.Size());
    if (!result) return result.GetStatus();
    for (const Base::ResourceUri& dependency : dependencies_) {
        Base::Result<std::uint32_t> index = InternString(
            strings, dependency.Canonical());
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
            FindInternedId(
                types, node.compiledTypeId_);
        if (!typeIndex) return typeIndex.GetStatus();
        Base::Result<std::uint32_t> memberIndex =
            FindInternedId(
                members, node.compiledMemberId_);
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
                    FindInternedString(
                        strings, value);
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
                FindInternedString(
                    strings,
                    node.namespacePrefix_.View());
            if (!prefix) return prefix.GetStatus();
            Base::Result<std::uint32_t> uri =
                FindInternedString(
                    strings,
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
            result = output.PushBack(byte);
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
            "AXB2 encoding is not supported");
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
    Base::Result<void> reserved = directory.Reserve(sectionCount.Value());
    if (!reserved) return reserved.GetStatus();
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
        reserved = directory.PushBack({
            static_cast<AxbSectionKind>(kind.Value()),
            offset.Value(), size.Value(), count.Value()});
        if (!reserved) return reserved.GetStatus();
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
    reserved = strings.Reserve(stringCount.Value());
    if (!reserved) return reserved.GetStatus();
    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < stringCount.Value(); ++index) {
        Base::Result<Base::String> string = stringsDecoder.ReadString(
            totalStringBytes, limits.maxStringBytes);
        if (!string) return string.GetStatus();
        reserved = strings.PushBack(std::move(string).Value());
        if (!reserved) return reserved.GetStatus();
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
    reserved = types.Reserve(typeCount.Value());
    if (!reserved) return reserved.GetStatus();
    reserved = typeBindings.Reserve(typeCount.Value());
    if (!reserved) return reserved.GetStatus();
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
        reserved = types.PushBack(id.Value());
        if (!reserved) return reserved.GetStatus();
        reserved = typeBindings.PushBack(binding.Value());
        if (!reserved) return reserved.GetStatus();
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
    reserved = members.Reserve(memberCount.Value());
    if (!reserved) return reserved.GetStatus();
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
        reserved = members.PushBack(
            binding.Value());
        if (!reserved) return reserved.GetStatus();
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
    reserved = values.Reserve(valueCount.Value());
    if (!reserved) return reserved.GetStatus();
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
        reserved = values.PushBack(record);
        if (!reserved) return reserved.GetStatus();
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
    reserved = document.dependencies_.Reserve(dependencyCount.Value());
    if (!reserved) return reserved.GetStatus();
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
        reserved = sourceMap.Reserve(sourceCount.Value());
        if (!reserved) return reserved.GetStatus();
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
            reserved = sourceMap.PushBack(span);
            if (!reserved) return reserved.GetStatus();
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

    reserved = document.nodes_.Reserve(instructionCount.Value());
    if (!reserved) return reserved.GetStatus();
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
                node.BindCompiledValue(
                    std::move(decoded).Value());
            }
        }
        if (!sourceMap.Empty()) {
            node.source_ = sourceMap[index];
        }
        reserved = document.nodes_.PushBack(
            std::move(node));
        if (!reserved) return reserved.GetStatus();
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


// ===== DocumentCache =====



#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>

#include <new>


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

struct DependencyGraphState {
    struct Node {
        explicit Node(Base::IAllocator& allocator) noexcept
            : dependencies(&allocator), dependents(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<Base::String> dependencies;
        Base::Vector<Base::String> dependents;
    };

    explicit DependencyGraphState(Base::IAllocator& allocator) noexcept
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

static_assert(
    sizeof(DependencyGraphState) <= 2048,
    "DependencyGraph inline state storage is too small");
static_assert(
    alignof(DependencyGraphState) <= alignof(std::max_align_t),
    "DependencyGraph inline state alignment is insufficient");

DependencyGraph::DependencyGraph(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) DependencyGraphState(*allocator_);
}

DependencyGraph::~DependencyGraph() noexcept {
    if (state_ == nullptr) return;
    state_->~DependencyGraphState();
    state_ = nullptr;
}

DependencyGraph::DependencyGraph(
    DependencyGraph&& other) noexcept
    : allocator_(other.allocator_) {
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DependencyGraphState(std::move(*other.state_));
        other.state_->~DependencyGraphState();
        other.state_ = nullptr;
    }
    other.allocator_ = nullptr;
}

DependencyGraph& DependencyGraph::operator=(
    DependencyGraph&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        state_->~DependencyGraphState();
        state_ = nullptr;
    }
    allocator_ = other.allocator_;
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DependencyGraphState(std::move(*other.state_));
        other.state_->~DependencyGraphState();
        other.state_ = nullptr;
    }
    other.allocator_ = nullptr;
    return *this;
}

Base::Result<void> DependencyGraph::Update(
    const Base::ResourceUri& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph is unavailable");
    }
    Base::Result<Base::String> documentKey =
        MakeKey(document, *allocator_);
    if (!documentKey) return documentKey.GetStatus();
    Base::Result<DependencyGraphState::Node*> documentNode =
        state_->EnsureNode(document);
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
        Base::Result<DependencyGraphState::Node*> dependencyNode =
            state_->EnsureNode(dependencyUri);
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
        DependencyGraphState::Node* dependency = state_->nodes.Find(dependencyKey);
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

    DependencyGraphState::Node* node = state_->nodes.Find(documentKey.Value());
    if (node == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph lost the document node");
    }
    for (const Base::String& oldDependency : node->dependencies) {
        DependencyGraphState::Node* dependency = state_->nodes.Find(oldDependency);
        if (dependency == nullptr) continue;
        RemoveKey(
            dependency->dependents,
            documentKey.Value().View());
        if (!ContainsKey(newDependencies, oldDependency.View()) &&
            dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            state_->nodes.Erase(oldDependency);
        }
    }
    node->dependencies = std::move(newDependencies);
    for (std::uint32_t index = 0U;
         index < node->dependencies.Size();
         ++index) {
        DependencyGraphState::Node* dependency =
            state_->nodes.Find(node->dependencies[index]);
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
    ++state_->generation;
    return {};
}

bool DependencyGraph::Remove(
    const Base::ResourceUri& document) noexcept {
    if (state_ == nullptr || document.Empty()) return false;
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return false;
    DependencyGraphState::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return false;

    Base::Vector<Base::String> previousDependencies(allocator_);
    if (!previousDependencies.Append(
            node->dependencies.AsSpan())) {
        return false;
    }
    node->dependencies.Clear();
    for (const Base::String& dependencyKey : previousDependencies) {
        DependencyGraphState::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        RemoveKey(dependency->dependents, key.Value().View());
        if (dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            state_->nodes.Erase(dependencyKey);
        }
    }
    if (node->dependents.Empty()) {
        state_->nodes.Erase(key.Value());
    }
    ++state_->generation;
    return true;
}

void DependencyGraph::Clear() noexcept {
    if (state_ == nullptr) return;
    state_->nodes.Clear();
    ++state_->generation;
}

Base::Result<void> DependencyGraph::CopyDependencies(
    const Base::ResourceUri& document,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (state_ == nullptr || document.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return key.GetStatus();
    const DependencyGraphState::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.Reserve(node->dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependencyKey : node->dependencies) {
        const DependencyGraphState::Node* dependency = state_->nodes.Find(dependencyKey);
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
    if (state_ == nullptr || dependency.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(dependency, *allocator_);
    if (!key) return key.GetStatus();
    const DependencyGraphState::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.Reserve(node->dependents.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependentKey : node->dependents) {
        const DependencyGraphState::Node* dependent = state_->nodes.Find(dependentKey);
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
    if (state_ == nullptr || changed.Empty()) return {};

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

        const DependencyGraphState::Node* node = state_->nodes.Find(key);
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
    return state_ != nullptr ? state_->nodes.Size() : 0U;
}

std::uint64_t DependencyGraph::Generation() const noexcept {
    return state_ != nullptr ? state_->generation : 0U;
}

struct DocumentCacheState {
    struct Entry {
        explicit Entry(Base::IAllocator& allocator) noexcept
            : compiledBytes(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> compiledBytes;
        std::uint64_t sourceRevision = 0U;
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t lastAccess = 0U;
    };

    DocumentCacheState(
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

static_assert(
    sizeof(DocumentCacheState) <= 8192,
    "DocumentCache inline state storage is too small");
static_assert(
    alignof(DocumentCacheState) <= alignof(std::max_align_t),
    "DocumentCache inline state alignment is insufficient");

DocumentCache::DocumentCache(
    Base::IAllocator* allocator,
    const DocumentCacheLimits& limits) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_)
        DocumentCacheState(*allocator_, limits);
}

DocumentCache::~DocumentCache() noexcept {
    if (state_ == nullptr) return;
    state_->~DocumentCacheState();
    state_ = nullptr;
}

DocumentCache::DocumentCache(
    DocumentCache&& other) noexcept
    : allocator_(other.allocator_) {
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DocumentCacheState(std::move(*other.state_));
        other.state_->~DocumentCacheState();
        other.state_ = nullptr;
    }
    other.allocator_ = nullptr;
}

DocumentCache& DocumentCache::operator=(
    DocumentCache&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        state_->~DocumentCacheState();
        state_ = nullptr;
    }
    allocator_ = other.allocator_;
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DocumentCacheState(std::move(*other.state_));
        other.state_->~DocumentCacheState();
        other.state_ = nullptr;
    }
    other.allocator_ = nullptr;
    return *this;
}

Base::Result<DocumentCacheLookup> DocumentCache::Lookup(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const ::Aero::Meta::Registry& domain,
    const CompiledDocumentLimits& limits) noexcept {
    DocumentCacheLookup result;
    if (state_ == nullptr || uri.Empty()) return result;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    DocumentCacheState::Entry* entry = state_->entries.Find(key.Value());
    if (entry == nullptr) {
        ++state_->misses;
        return result;
    }
    if (entry->sourceRevision != sourceRevision ||
        entry->sourceIdentity != sourceIdentity) {
        ++state_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }

    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
            entry->compiledBytes.AsSpan(), domain, limits);
    if (!document) {
        ++state_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }
    entry->lastAccess = ++state_->accessSequence;
    ++state_->hits;
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
    if (state_ == nullptr || uri.Empty() || !document.IsValid()) {
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
    DocumentCacheState::Entry* existing = state_->entries.Find(key.Value());
    if (existing != nullptr) {
        state_->compiledBytes -= existing->compiledBytes.Size();
        existing->uri = uri;
        existing->compiledBytes = std::move(serialized).Value();
        existing->sourceRevision = sourceRevision;
        existing->sourceIdentity = sourceIdentity;
        existing->lastAccess = ++state_->accessSequence;
        state_->compiledBytes += existing->compiledBytes.Size();
    } else {
        DocumentCacheState::Entry entry(*allocator_);
        entry.uri = uri;
        entry.compiledBytes = std::move(serialized).Value();
        entry.sourceRevision = sourceRevision;
        entry.sourceIdentity = sourceIdentity;
        entry.lastAccess = ++state_->accessSequence;
        state_->compiledBytes += entry.compiledBytes.Size();
        Base::Result<typename Base::HashMap<Base::String, DocumentCacheState::Entry>::InsertResult>
            inserted = state_->entries.Insert(
                std::move(key).Value(), std::move(entry));
        if (!inserted) {
            state_->compiledBytes -= serializedSize;
            return inserted.GetStatus();
        }
    }
    Base::Result<void> graph =
        state_->graph.Update(uri, dependencies);
    if (!graph) {
        state_->EraseEntry(uri, false);
        return graph.GetStatus();
    }
    ++state_->stores;
    ++state_->generation;
    state_->EvictToLimits();
    return {};
}

Base::Result<std::uint32_t> DocumentCache::Invalidate(
    const Base::ResourceUri& uri,
    bool includeDependents) noexcept {
    if (state_ == nullptr || uri.Empty()) return 0U;
    Base::Vector<Base::ResourceUri> affected(allocator_);
    if (includeDependents) {
        Base::Result<void> collected =
            state_->graph.CollectAffected(uri, affected);
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
        if (state_->EraseEntry(affectedUri, false)) {
            ++count;
        } else {
            static_cast<void>(state_->graph.Remove(affectedUri));
        }
    }
    return count;
}

void DocumentCache::Clear() noexcept {
    if (state_ == nullptr) return;
    state_->entries.Clear();
    state_->graph.Clear();
    state_->compiledBytes = 0U;
    ++state_->generation;
}

bool DocumentCache::Contains(
    const Base::ResourceUri& uri) const noexcept {
    if (state_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    return key && state_->entries.Contains(key.Value());
}

bool DocumentCache::GetSourceRevision(
    const Base::ResourceUri& uri,
    std::uint64_t sourceIdentity,
    std::uint64_t& revision) const noexcept {
    revision = 0U;
    if (state_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return false;
    const DocumentCacheState::Entry* entry = state_->entries.Find(key.Value());
    if (entry == nullptr || entry->sourceIdentity != sourceIdentity) {
        return false;
    }
    revision = entry->sourceRevision;
    return true;
}

Base::Result<void> DocumentCache::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    return state_ != nullptr
        ? state_->graph.CollectAffected(changed, output)
        : Base::Result<void>{};
}

const DependencyGraph& DocumentCache::Dependencies() const noexcept {
    static const DependencyGraph empty;
    return state_ != nullptr ? state_->graph : empty;
}

DocumentCacheStatistics DocumentCache::Statistics() const noexcept {
    DocumentCacheStatistics result;
    if (state_ == nullptr) return result;
    result.entryCount = state_->entries.Size();
    result.compiledBytes = state_->compiledBytes;
    result.hitCount = state_->hits;
    result.missCount = state_->misses;
    result.storeCount = state_->stores;
    result.invalidationCount = state_->invalidations;
    result.evictionCount = state_->evictions;
    result.generation = state_->generation;
    return result;
}

const DocumentCacheLimits& DocumentCache::Limits() const noexcept {
    static const DocumentCacheLimits empty{};
    return state_ != nullptr ? state_->limits : empty;
}

} // namespace Aero::Markup


// ===== LoaderResult =====



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
    Aero::Media::Visual& node) noexcept {
    for (Aero::Media::Visual* existing : nodes) {
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








#include <Aero/Base/Hash.hpp>

#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>


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

struct LoaderState {
    LoaderState(
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

static_assert(
    sizeof(LoaderState) <= 512,
    "Loader inline state storage is too small");
static_assert(
    alignof(LoaderState) <= alignof(std::max_align_t),
    "Loader inline state alignment is insufficient");

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

Base::Result<::Aero::Markup::StreamResourceInfo> CreateMemoryResource(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    ::Aero::Markup::StreamResourceInfo result;
    result.uri = uri;
    result.stream = std::move(stream).Value();
    result.revision = revision;
    return result;
}

Base::Result<Base::ResourceUri> ResolveRequestedUri(
    Base::StringView uri,
    const Base::ResourceUri& baseUri) noexcept {
    // WPF component URIs beginning with '/' are assembly resources, not
    // filesystem-rooted paths. Resolving them against a file-backed App.xaml
    // would otherwise incorrectly manufacture a file: URI.
    if (uri.SizeBytes() > 0U && uri[0] == '/') {
        for (std::uint32_t index = 1U;
             index + 10U <= uri.SizeBytes(); ++index) {
            if (uri.Substr(index, 10U) ==
                Base::StringView(";component")) {
                return Base::ResourceUri::Parse(uri);
            }
        }
    }
    if (!baseUri.Empty()) {
        return Base::ResourceUri::Resolve(
            baseUri, uri);
    }
    return Base::ResourceUri::Parse(uri);
}

} // namespace

Base::Result<void> XamlProviderRegistry::Set(
    Base::Ref<XamlProvider> provider,
    Base::StringView scheme,
    Base::StringView assembly,
    Base::Ref<XamlProvider>* replaced) noexcept {
    if (!provider) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Markup XAML provider is required");
    }
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
    registration.provider = std::move(provider);
    for (const XamlProviderRegistration& existing : registrations_) {
        if (existing.provider.Get() == registration.provider.Get()) {
            registration.identity = existing.identity;
            break;
        }
    }
    static std::atomic<std::uint64_t> nextIdentity{1U};
    if (registration.identity == 0U) {
        registration.identity =
            nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        if (registration.identity == 0U) {
            registration.identity =
                nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        }
    }

    for (XamlProviderRegistration& existing : registrations_) {
        if (existing.scheme.View() == registration.scheme.View() &&
            existing.assembly.View() ==
                registration.assembly.View()) {
            if (replaced != nullptr) {
                *replaced = std::move(existing.provider);
            }
            existing = std::move(registration);
            return {};
        }
    }
    return registrations_.PushBack(
        std::move(registration));
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveRoute(
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (!RegistrationMatches(
                registration, uri, requireScheme, requireAssembly)) {
            continue;
        }
        XamlProviderResolution result;
        result.provider = registration.provider.Get();
        result.cacheIdentity = Base::MixHash64(
            registration.identity ^
            Base::DefaultHash<Base::StringView>{}(
                registration.scheme.View()) ^
            Base::DefaultHash<Base::StringView>{}(
                registration.assembly.View(), UINT64_C(0xA3E0)));
        return result;
    }
    if (parent_ != nullptr) {
        Base::Result<XamlProviderResolution> inherited =
            parent_->ResolveRoute(uri, requireScheme, requireAssembly);
        if (inherited) return inherited;
        if (inherited.GetStatus().code != Base::ErrorCode::NotFound) {
            return inherited.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches this route shape");
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
        Base::Result<XamlProviderResolution> resolved =
            ResolveRoute(uri, route.scheme, route.assembly);
        if (resolved) return resolved;
        if (resolved.GetStatus().code != Base::ErrorCode::NotFound) {
            return resolved.GetStatus();
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

bool XamlProviderRegistry::Contains(
    const XamlProvider& provider) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (registration.provider.Get() == &provider) return true;
    }
    return false;
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

Base::Result<::Aero::Markup::StreamResourceInfo>
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

Base::Result<::Aero::Markup::StreamResourceInfo>
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
    ::Aero::Markup::StreamResourceInfo source;
    source.uri = uri;
    source.stream = std::move(stream).Value();
    Base::Result<std::uint64_t> revision = Revision(uri);
    source.revision = revision
        ? revision.Value()
        : 0U;
    return source;
}

struct LoaderState::Operation {
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

LoaderState::LoaderState(
    Schema& schema,
    XamlProviderRegistry& providers,
    Diagnostics::IDiagnosticSink* diagnostics,
    const LoadState* runtime) noexcept
    : schema_(&schema),
      providers_(&providers),
      diagnostics_(diagnostics),
      runtime_(runtime) {}

Base::Result<LoaderResult> LoaderState::Load(
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

Base::Result<LoaderResult> LoaderState::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.LoadCore(uri, options, {});
}

Base::Result<LoaderResult> LoaderState::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.ParseCore(text, baseUri, options, {}, true);
}

Base::Result<LoaderResult> LoaderState::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.ParseStreamCore(stream, baseUri, options, {}, true);
}

Base::Result<LoaderResult> LoaderState::LoadComponent(
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

Base::Result<LoaderResult> LoaderState::LoadComponent(
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

Base::Result<LoaderResult> LoaderState::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.LoadCompiled(bytes, originUri, options);
}

Base::Result<LoaderResult>
LoaderState::Operation::LoadCompiled(
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
LoaderState::Operation::LoadCompiledDocument(
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
    // Source-backed compiled documents need the same two-phase resource
    // resolution as streamed documents. Merged ResourceDictionary sources
    // are committed by the load finalizer before queued StaticResource
    // references are written.
    context.deferUnresolvedStaticResources = true;
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

Base::Result<LoaderResult> LoaderState::Operation::LoadCore(
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

    Base::Result<::Aero::Markup::StreamResourceInfo> source =
        provider.Value().provider->Open(uri);
    if (!source) {
        loadStack_.PopBack();
        return Failure(
            source.GetStatus(),
            LoaderDiagnosticCodes::SourceLoadFailed,
            Base::StringView("XAML source could not be loaded"));
    }
    ::Aero::Markup::StreamResourceInfo sourceInfo =
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

Base::Result<LoaderResult> LoaderState::Operation::ParseCore(
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

Base::Result<LoaderResult> LoaderState::Operation::ParseStreamCore(
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

Base::Result<void> LoaderState::Operation::FinalizeLoad(
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

Base::Result<void> LoaderState::Operation::FinalizeResult(
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
LoaderState::Operation::ResolveResourceDependencies(
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

    for (Aero::Media::Visual* visual : result.visualContent.nodes) {
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
LoaderState::Operation::ResolveDictionaryDependencies(
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
LoaderState::Operation::CommitResourceDependencies(
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

Base::Result<void> LoaderState::Operation::AppendDependencies(
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

Base::Result<void> LoaderState::Operation::AppendDependency(
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

Base::Result<void> LoaderState::Operation::ValidateOptions(
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

Base::Result<void> LoaderState::Operation::CheckPolicy(
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

bool LoaderState::Operation::IsLoading(
    const Base::ResourceUri& uri) const noexcept {
    for (const Base::ResourceUri& active : loadStack_) {
        if (active == uri) {
            return true;
        }
    }
    return false;
}

Base::Status LoaderState::Operation::Failure(
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
        ::Aero::Markup::AdoptXamlDocument(
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
    state_ = new (stateStorage_) LoaderState(
        schema, providers, diagnostics, runtime);
}

Loader::~Loader() noexcept {
    if (state_ == nullptr) return;
    state_->~LoaderState();
    state_ = nullptr;
}

Base::Result<XamlDocument> Loader::Load(
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Load(uri, options), *allocator_);
}

Base::Result<XamlDocument> Loader::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Load(uri, options), *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Parse(text, baseUri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Parse(stream, baseUri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->LoadComponent(existingRoot, uri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->LoadComponent(existingRoot, uri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->LoadCompiled(bytes, originUri, options),
        *allocator_);
}

} // namespace Aero::Markup


// ===== Resources =====




#include <Aero/Base/ResourceUri.hpp>

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
            static_cast<::Aero::Media::Visual&>(scopeOwner)
                .AsFrameworkElement());
    if (element == nullptr) {
        return InvalidResource(
            "XAML resource scope is not a FrameworkElement");
    }
    return element->GetResources().Add(key, value);
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
    ::Aero::Media::Visual& visual =
        static_cast<::Aero::Media::Visual&>(scopeOwner);
    FrameworkElement* element =
        visual.AsFrameworkElement();
    return element != nullptr
        ? &element->GetResources()
        : nullptr;
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
        SchemaPrivate::AddResourceScope(schema, {
            ResourceDictionary::StaticTypeId(),
            true,
            &AddResource,
            &ResolveDictionaryScope,
            this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = SchemaPrivate::AddResourceScope(schema, {
        FrameworkElement::StaticTypeId(),
        true,
        &AddFrameworkResource,
        &ResolveFrameworkScope,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup


// ===== XamlDocument =====

#include <Aero/Markup/XamlReader.hpp>

#include <Aero/Base/Result.hpp>




namespace Aero::Markup {

struct XamlDocumentState {
    explicit XamlDocumentState(LoaderResult&& value) noexcept
        : result(std::move(value)) {}

    static Base::Result<XamlDocument> Adopt(
        LoaderResult&& result,
        Base::IAllocator& allocator) noexcept;
    static LoaderResult Take(
        XamlDocument& document) noexcept;
    static const EffectLifetime* RuntimeLifetime(
        const XamlDocument& document) noexcept;

    LoaderResult result;
};

XamlDocument::~XamlDocument() noexcept {
    Reset();
}

XamlDocument::XamlDocument(XamlDocument&& other) noexcept
    : allocator_(other.allocator_), state_(other.state_) {
    other.allocator_ = nullptr;
    other.state_ = nullptr;
}

XamlDocument& XamlDocument::operator=(XamlDocument&& other) noexcept {
    if (this == &other) return *this;
    Reset();
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.allocator_ = nullptr;
    other.state_ = nullptr;
    return *this;
}

} // namespace Aero::Markup

namespace Aero::Markup {

Base::Result<::Aero::Markup::XamlDocument> AdoptXamlDocument(
    ::Aero::Markup::LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    return ::Aero::Markup::XamlDocumentState::Adopt(
        std::move(result), allocator);
}

} // namespace Aero::Markup

namespace Aero::Markup {

Base::Result<XamlDocument> XamlDocumentState::Adopt(
    LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    if (!result.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document requires a loaded root object");
    }
    void* memory = allocator.Allocate({
        sizeof(XamlDocumentState),
        alignof(XamlDocumentState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "UI document allocation failed");
    }
    XamlDocument document;
    document.allocator_ = &allocator;
    document.state_ =
        new (memory) XamlDocumentState(std::move(result));
    return document;
}

} // namespace Aero::Markup

namespace Aero::Markup {

bool XamlDocument::IsValid() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr && state->result.root;
}

const Base::Ref<Base::Object>& XamlDocument::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.root : empty;
}

Base::Object* XamlDocument::RootObject(
    Meta::TypeId expectedType) noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr || !state->result.root) return nullptr;
    Base::Object* root = state->result.root.Get();
    if (expectedType == Meta::InvalidTypeId) return root;
    const Meta::Registry* metadata = state->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        root->RuntimeType(), expectedType)
        ? root
        : nullptr;
}

Base::Object* XamlDocument::FindName(
    Base::StringView name,
    Meta::TypeId expectedType) noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr || name.Empty()) return nullptr;
    Base::Object* object = state->result.names.Find(name);
    if (object == nullptr || expectedType == Meta::InvalidTypeId) {
        return object;
    }
    const Meta::Registry* metadata = state->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        object->RuntimeType(), expectedType)
        ? object
        : nullptr;
}

std::uint32_t XamlDocument::NamedObjectCount() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.names.Size() : 0U;
}

Aero::ResourceDictionary* XamlDocument::Resources() noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    return state != nullptr ? &state->result.resources : nullptr;
}

const Aero::ResourceDictionary* XamlDocument::Resources() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? &state->result.resources : nullptr;
}

const Base::ResourceUri& XamlDocument::CanonicalUri() const noexcept {
    static const Base::ResourceUri empty;
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.canonicalUri : empty;
}

Base::Span<const Base::ResourceUri> XamlDocument::Dependencies() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr
        ? Base::Span<const Base::ResourceUri>{
              state->result.dependencies.Data(),
              state->result.dependencies.Size()}
        : Base::Span<const Base::ResourceUri>{};
}

} // namespace Aero::Markup

namespace Aero::Markup {

const EffectLifetime*
XamlDocumentState::RuntimeLifetime(
    const XamlDocument& document) noexcept {
    const auto* state =
        static_cast<const XamlDocumentState*>(document.state_);
    return state != nullptr
        ? state->result.runtimeLifetime.Get()
        : nullptr;
}

LoaderResult XamlDocumentState::Take(
    XamlDocument& document) noexcept {
    auto* state = static_cast<XamlDocumentState*>(document.state_);
    if (state == nullptr) {
        LoaderResult empty;
        return empty;
    }
    LoaderResult result = std::move(state->result);
    document.Reset();
    return result;
}

} // namespace Aero::Markup

namespace Aero::Markup {

const ::Aero::Markup::EffectLifetime*
XamlDocumentRuntimeLifetime(
    const ::Aero::Markup::XamlDocument& document) noexcept {
    return ::Aero::Markup::XamlDocumentState::RuntimeLifetime(document);
}

::Aero::Markup::LoaderResult TakeXamlDocument(
    ::Aero::Markup::XamlDocument& document) noexcept {
    return ::Aero::Markup::XamlDocumentState::Take(document);
}

} // namespace Aero::Markup

namespace Aero::Markup {

void XamlDocument::Reset() noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr) return;
    state->result.Clear();
    state->~XamlDocumentState();
    allocator_->Deallocate(
        state,
        sizeof(XamlDocumentState),
        alignof(XamlDocumentState),
        Base::MemoryTag::Markup);
    state_ = nullptr;
    allocator_ = nullptr;
}

} // namespace Aero::Markup
