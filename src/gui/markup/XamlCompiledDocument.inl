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


