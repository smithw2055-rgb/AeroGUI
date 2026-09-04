#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"

// ===== CompiledSchema =====


// Canonical compiled-schema bridge used by Loader.

#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Media/Fonts.hpp>

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

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
        type.Value()->Id(),
        type.Value()->Kind(),
        type.Value()->Flags()};
}

Base::Result<SchemaTypeInfo> ResolveTypeInfo(
    const SchemaManifest& schema,
    Base::StringView xamlNamespace,
    Base::StringView localName) noexcept {
    return schema.ResolveType(xamlNamespace, localName);
}

bool IsCompiledWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' ||
        value == '\r' || value == '\n';
}

Base::StringView TrimCompiledText(
    Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end && IsCompiledWhitespace(value[begin])) {
        ++begin;
    }
    while (end > begin && IsCompiledWhitespace(value[end - 1U])) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

bool IsWhitespaceOnly(
    Base::StringView value) noexcept {
    return TrimCompiledText(value).Empty();
}

bool EqualsAsciiInsensitive(
    Base::StringView value,
    Base::StringView expected) noexcept {
    if (value.SizeBytes() != expected.SizeBytes()) return false;
    for (std::uint32_t index = 0U;
         index < value.SizeBytes(); ++index) {
        char left = value[index];
        char right = expected[index];
        if (left >= 'A' && left <= 'Z') {
            left = static_cast<char>(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = static_cast<char>(right - 'A' + 'a');
        }
        if (left != right) return false;
    }
    return true;
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
        return false;
    }
    return false;
}

template<class T>
Base::Result<Meta::Value> MakeManifestCustomValue(
    Meta::TypeId type,
    const T& value) noexcept {
    Base::Result<Base::Ref<Meta::ValueTypeSemantics>> semantics =
        Base::MakeRef<Meta::ValueTypeSemantics>(
            Meta::MakeValueTypeRegistration<T>());
    if (!semantics) return semantics.GetStatus();
    return Meta::Value::TryFromCustom(
        type, &value, semantics.Value());
}

Base::Result<std::uint32_t> ParseManifestNumbers(
    Base::StringView input,
    double* values,
    std::uint32_t capacity) noexcept {
    if (values == nullptr || capacity == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "AXB2 number-list destination is invalid");
    }
    Base::String text;
    Base::Result<void> assigned =
        text.Assign(TrimCompiledText(input));
    if (!assigned) return assigned.GetStatus();

    const char* cursor = text.CStr();
    std::uint32_t count = 0U;
    while (*cursor != '\0') {
        while (std::isspace(
                   static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (*cursor == '\0') break;
        if (count == capacity) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 number list has too many values");
        }

        errno = 0;
        char* end = nullptr;
        const double parsed = std::strtod(cursor, &end);
        if (errno == ERANGE || end == cursor ||
            !std::isfinite(parsed)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 number list contains an invalid value");
        }
        values[count++] = parsed;
        cursor = end;

        const char* separator = cursor;
        while (std::isspace(
                   static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (*cursor == '\0') break;
        if (*cursor == ',') {
            ++cursor;
            while (std::isspace(
                       static_cast<unsigned char>(*cursor))) {
                ++cursor;
            }
            if (*cursor == '\0') {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 number list ends with a separator");
            }
        } else if (cursor == separator) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 number list separator is invalid");
        }
    }
    return count;
}

int ManifestHexDigit(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Base::Result<Base::Color> ParseManifestColor(
    Base::StringView input) noexcept {
    const Base::StringView value = TrimCompiledText(input);
    if ((value.SizeBytes() != 7U &&
         value.SizeBytes() != 9U) ||
        value[0] != '#') {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "AXB2 manifest preconversion supports hexadecimal Color values");
    }

    std::uint8_t bytes[4]{255U, 0U, 0U, 0U};
    const std::uint32_t count =
        value.SizeBytes() == 9U ? 4U : 3U;
    for (std::uint32_t index = 0U;
         index < count; ++index) {
        const int high =
            ManifestHexDigit(value[1U + index * 2U]);
        const int low =
            ManifestHexDigit(value[2U + index * 2U]);
        if (high < 0 || low < 0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Color contains a non-hex digit");
        }
        bytes[index] = static_cast<std::uint8_t>(
            (high << 4) | low);
    }
    return count == 3U
        ? Base::Color{
              bytes[0] / 255.0F,
              bytes[1] / 255.0F,
              bytes[2] / 255.0F,
              1.0F}
        : Base::Color{
              bytes[1] / 255.0F,
              bytes[2] / 255.0F,
              bytes[3] / 255.0F,
              bytes[0] / 255.0F};
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
        if (EqualsAsciiInsensitive(text, "true") ||
            text == Base::StringView("1")) {
            return Meta::Value::FromBoolean(type, true);
        }
        if (EqualsAsciiInsensitive(text, "false") ||
            text == Base::StringView("0")) {
            return Meta::Value::FromBoolean(type, false);
        }
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 Boolean literal is invalid");
    }

    Base::String owned;
    Base::Result<void> assigned = owned.Assign(text);
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;

    if (type == Meta::TypeOf<double>()) {
        errno = 0;
        const double parsed = std::strtod(
            owned.CStr(), &end);
        if (errno == ERANGE || end == owned.CStr() ||
            *end != '\0' || !std::isfinite(parsed)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Double literal is invalid");
        }
        return Meta::Value::FromDouble(type, parsed);
    }

    const bool signedType =
        type == Meta::TypeOf<std::int8_t>() ||
        type == Meta::TypeOf<std::int16_t>() ||
        type == Meta::TypeOf<std::int32_t>() ||
        type == Meta::TypeOf<std::int64_t>();
    if (signedType) {
        errno = 0;
        const long long parsed = std::strtoll(
            owned.CStr(), &end, 10);
        if (errno == ERANGE || end == owned.CStr() ||
            *end != '\0') {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 signed integer literal is invalid");
        }
        std::int64_t minimum =
            (std::numeric_limits<std::int64_t>::min)();
        std::int64_t maximum =
            (std::numeric_limits<std::int64_t>::max)();
        if (type == Meta::TypeOf<std::int8_t>()) {
            minimum = (std::numeric_limits<std::int8_t>::min)();
            maximum = (std::numeric_limits<std::int8_t>::max)();
        } else if (type == Meta::TypeOf<std::int16_t>()) {
            minimum = (std::numeric_limits<std::int16_t>::min)();
            maximum = (std::numeric_limits<std::int16_t>::max)();
        } else if (type == Meta::TypeOf<std::int32_t>()) {
            minimum = (std::numeric_limits<std::int32_t>::min)();
            maximum = (std::numeric_limits<std::int32_t>::max)();
        }
        if (parsed < minimum || parsed > maximum) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 signed integer literal is out of range");
        }
        return Meta::Value::FromSignedInteger(
            type, static_cast<std::int64_t>(parsed));
    }

    const bool unsignedType =
        type == Meta::TypeOf<std::uint8_t>() ||
        type == Meta::TypeOf<std::uint16_t>() ||
        type == Meta::TypeOf<std::uint32_t>() ||
        type == Meta::TypeOf<std::uint64_t>();
    if (unsignedType) {
        if (!text.Empty() && text[0] == '-') {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 unsigned integer literal is negative");
        }
        errno = 0;
        const unsigned long long parsed = std::strtoull(
            owned.CStr(), &end, 10);
        if (errno == ERANGE || end == owned.CStr() ||
            *end != '\0') {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 unsigned integer literal is invalid");
        }
        std::uint64_t maximum =
            (std::numeric_limits<std::uint64_t>::max)();
        if (type == Meta::TypeOf<std::uint8_t>()) {
            maximum = (std::numeric_limits<std::uint8_t>::max)();
        } else if (type == Meta::TypeOf<std::uint16_t>()) {
            maximum = (std::numeric_limits<std::uint16_t>::max)();
        } else if (type == Meta::TypeOf<std::uint32_t>()) {
            maximum = (std::numeric_limits<std::uint32_t>::max)();
        }
        if (parsed > maximum) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 unsigned integer literal is out of range");
        }
        return Meta::Value::FromUnsignedInteger(
            type, static_cast<std::uint64_t>(parsed));
    }

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

    if (type == Meta::TypeOf<Base::Thickness>() ||
        type == Meta::TypeOf<Base::CornerRadius>()) {
        double values[4]{};
        Base::Result<std::uint32_t> count =
            ParseManifestNumbers(text, values, 4U);
        if (!count) return count.GetStatus();

        Base::Thickness thickness;
        if (count.Value() == 1U) {
            thickness = {
                values[0], values[0],
                values[0], values[0]};
        } else if (count.Value() == 2U) {
            thickness = {
                values[0], values[1],
                values[0], values[1]};
        } else if (count.Value() == 4U) {
            thickness = {
                values[0], values[1],
                values[2], values[3]};
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Thickness accepts one, two, or four values");
        }

        if (type == Meta::TypeOf<Base::Thickness>()) {
            return MakeManifestCustomValue(
                type, thickness);
        }
        return MakeManifestCustomValue(
            type,
            Base::CornerRadius{
                thickness.left,
                thickness.top,
                thickness.right,
                thickness.bottom});
    }

    if (type == Meta::TypeOf<Base::Point>()) {
        double values[2]{};
        Base::Result<std::uint32_t> count =
            ParseManifestNumbers(text, values, 2U);
        if (!count || count.Value() != 2U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Point requires two finite values");
        }
        return MakeManifestCustomValue(
            type, Base::Point{values[0], values[1]});
    }

    if (type == Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            ParseManifestColor(text);
        if (!color) return color.GetStatus();
        return MakeManifestCustomValue(
            type, color.Value());
    }

    if (type == Meta::TypeOf<Base::Transform2D>()) {
        double values[6]{};
        Base::Result<std::uint32_t> count =
            ParseManifestNumbers(text, values, 6U);
        if (!count || count.Value() != 6U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Matrix requires six finite values");
        }
        return MakeManifestCustomValue(
            type,
            Base::Transform2D{
                values[0], values[1], values[2],
                values[3], values[4], values[5]});
    }

    if (type == Meta::TypeOf<::Aero::GridLength>()) {
        if (EqualsAsciiInsensitive(text, "auto")) {
            return MakeManifestCustomValue(
                type, ::Aero::GridLength::Auto());
        }

        if (!text.Empty() &&
            text[text.SizeBytes() - 1U] == '*') {
            const Base::StringView weightText =
                TrimCompiledText(text.Substr(
                    0U, text.SizeBytes() - 1U));
            double weight = 1.0;
            if (!weightText.Empty()) {
                Base::Result<double> parsed =
                    ::Aero::Base::
                        ValueConversion::ParseDouble(
                            weightText);
                if (!parsed || parsed.Value() < 0.0) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "AXB2 GridLength star weight is invalid");
                }
                weight = parsed.Value();
            }
            return MakeManifestCustomValue(
                type, ::Aero::GridLength::Star(weight));
        }

        Base::Result<double> pixels =
            ::Aero::Base::ValueConversion::
                ParseDouble(text);
        if (!pixels || pixels.Value() < 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 GridLength pixel value is invalid");
        }
        return MakeManifestCustomValue(
            type,
            ::Aero::GridLength::Pixel(
                pixels.Value()));
    }

    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "AXB2 manifest cannot preconvert this value type");
}

Base::Result<void> BindCompiledValue(
    const Schema& schema,
    Node& node,
    Meta::TypeId valueType) noexcept {
    Base::StringView literal;
    if (valueType == Meta::InvalidTypeId ||
        !TryGetCompiledLiteral(node.Value(), literal)) {
        return {};
    }
    Base::Result<Meta::Value> converted =
        SchemaPrivate::ConvertText(
            schema, valueType, literal);
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
        SchemaPrivate::ResolveMemberWritePolicy(
            schema, member);
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
            const Frame& frame = frames.Back();
            Meta::TypeId valueType = Meta::InvalidTypeId;
            if (frame.kind == FrameKind::Member ||
                frame.kind == FrameKind::PropertyElement ||
                frame.kind == FrameKind::ValueObject) {
                valueType = frame.type;
            } else if (frame.kind == FrameKind::Object) {
                Base::Result<ResolvedMember> content =
                    schema.ResolveContentMember(frame.type);
                if (content) {
                    valueType = content.Value().valueType;
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
                Base::Result<void> appended = frames.PushBack({
                    FrameKind::PropertyElement,
                    member.Value().valueType});
                if (!appended) return appended.GetStatus();
                break;
            }
            if (nullObject) {
                Base::Result<void> appended = frames.PushBack({
                    FrameKind::NullObject,
                    Meta::InvalidTypeId});
                if (!appended) return appended.GetStatus();
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
                Base::Result<ResolvedMember> content =
                    schema.ResolveContentMember(frames.Back().type);
                if (!content) return content.GetStatus();
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
            Base::Result<void> appended = frames.PushBack({
                Meta::HasTypeFlag(
                    type.Value().flags,
                    Meta::TypeFlags::ValueType)
                    ? FrameKind::ValueObject
                    : FrameKind::Object,
                type.Value().id});
            if (!appended) return appended.GetStatus();
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
            Base::Result<void> appended = frames.PushBack({
                FrameKind::Member,
                memberValueType});
            if (!appended) return appended.GetStatus();
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
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) return identity.GetStatus();
    compiled.Value().identity_ = identity.Value();
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
    return ValidateSchemaCore(*this, schema, true);
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
    return ValidateSchemaCore(*this, manifest, true);
}

} // namespace Aero::Markup


