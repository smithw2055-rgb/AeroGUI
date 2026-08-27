#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
// Consolidated implementation. Keep sections ordered by dependency.

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


// ===== Metadata =====



// Markup-specific metadata declarations.

#include <Aero/Meta.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/VisualStateManager.hpp>




namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;


class DynamicResourceExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        DynamicResourceExtensionToken,
        Base::Object,
        "urn:aero",
        "DynamicResource")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class StaticExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        StaticExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Static")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TypeExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TypeExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Type")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TemplateBindingExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TemplateBindingExtensionToken,
        Base::Object,
        "urn:aero",
        "TemplateBinding")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

// AeroGUI's application samples expose Loc both as a markup extension and as
// an attached Source property.  The token deliberately lives in the normal
// schema so the legacy AeroGUIExtensions namespace resolves to the same type
// as other compatibility extensions.
class LocExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        LocExtensionToken,
        Base::Object,
        "urn:aero",
        "Loc")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Meta::AttachedPropertyRef<
        LocExtensionToken, Base::ResourceUri>
        SourceProperty{"Source"};
};

void AddGroupState(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualState::StaticTypeId()) {
        return;
    }
    (void)static_cast<VisualStateGroup&>(object).AddState(
        Base::Ref<VisualState>::FromBorrowed(
            *static_cast<VisualState*>(value.Get())));
}

void ClearGroupStates(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(object).ClearStates();
}

void AddGroupTransition(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualTransition::StaticTypeId()) {
        return;
    }
    (void)static_cast<VisualStateGroup&>(object).AddTransition(
        Base::Ref<VisualTransition>::FromBorrowed(
            *static_cast<VisualTransition*>(value.Get())));
}

void ClearGroupTransitions(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(
        object).ClearTransitions();
}

[[maybe_unused]] void AddElementVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != VisualStateGroup::StaticTypeId()) {
        return;
    }
    auto& target = static_cast<::Aero::DependencyObject&>(object);
    Base::Ref<VisualStateGroupCollection> valueStore = target.GetValueOr(
        VisualStateManager::VisualStateGroupsProperty,
        Base::Ref<VisualStateGroupCollection>{});
    if (!valueStore) {
        Base::Result<Base::Ref<VisualStateGroupCollection>> created =
            Base::MakeRef<VisualStateGroupCollection>();
        if (!created) return;
        valueStore = std::move(created).Value();
        target.SetValue(
            VisualStateManager::VisualStateGroupsProperty,
            valueStore);
    }
    (void)valueStore->Add(
        Base::Ref<VisualStateGroup>::FromBorrowed(
            *static_cast<VisualStateGroup*>(value.Get())));
}

[[maybe_unused]] void ClearElementVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    static_cast<::Aero::DependencyObject&>(object).SetValue(
        VisualStateManager::VisualStateGroupsProperty,
        Base::Ref<VisualStateGroupCollection>{});
}

void AddStateContent(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    auto& state =
        static_cast<VisualState&>(object);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        (void)state.AddSetter(value);
        return;
    }
    if (value->RuntimeType() ==
        Media::Animation::Storyboard::StaticTypeId()) {
        state.SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(value.Get())));
        return;
    }
    return;
}

void ClearStateContent(
    Base::Object& object,
    void*) noexcept {
    auto& state =
        static_cast<VisualState&>(object);
    state.ClearSetters();
    state.SetStoryboard({});
}

void SetTransitionStoryboard(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            Media::Animation::Storyboard::StaticTypeId()) {
        return;
    }
    static_cast<VisualTransition&>(
        object).SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(
                    value.Get())));
}

void ClearTransitionStoryboard(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualTransition&>(object).SetStoryboard({});
}

[[maybe_unused]] void AddVisualStateGroupToCollection(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != VisualStateGroup::StaticTypeId()) return;
    auto& collection = static_cast<VisualStateGroupCollection&>(owner);
    (void)collection.Add(
        Base::Ref<VisualStateGroup>::FromBorrowed(
            *static_cast<VisualStateGroup*>(value.Get())));
}

[[maybe_unused]] void ClearVisualStateGroupCollection(
    Base::Object& owner,
    void*) noexcept {
    static_cast<VisualStateGroupCollection&>(owner).Clear();
}

} // namespace

Base::Result<void> PopulateMarkupMetadata(
    Meta::Registration& context) noexcept {
    Base::Result<void> status =
        Meta::Register<MarkupExtension>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Abstract).Result();
    if (!status) return status.GetStatus();
    status =
        Meta::Register<DynamicResourceExtensionToken>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<TypeExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<TemplateBindingExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    auto loc = Meta::Register<LocExtensionToken>(
        context,
        TypeFlags::MarkupExtension | TypeFlags::Abstract);
    loc.Property(
        LocExtensionToken::SourceProperty,
        FrameworkPropertyMetadata(Base::ResourceUri{}).Inherits().Changed(
            &LocExtension::OnSourceChanged));
    status = loc.Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticResourceObject>(context)
        .Property(
            StaticResourceObject::ResourceKeyProperty,
            FrameworkPropertyMetadata(Base::String{}))
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<VisualStateGroupCollection>(
        context, TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();

    auto visualStateManager =
        Meta::Register<VisualStateManager>(
            context, TypeFlags::Abstract);
    visualStateManager
        .Property(
            VisualStateManager::VisualStateGroupsProperty,
            FrameworkPropertyMetadata(
                Base::Ref<VisualStateGroupCollection>{})
                .Structural());
    status = visualStateManager.Result();
    if (!status) return status.GetStatus();

    auto stateGroup =
        Meta::Register<VisualStateGroup>(context);
    stateGroup
        .Property(
            "Name",
            &VisualStateGroup::GetName,
            &VisualStateGroup::SetName)
        .Content<VisualState>(
            "States",
            ContentKind::Collection,
            &AddGroupState,
            &ClearGroupStates)
        .Collection<VisualTransition>(
            "Transitions",
            &AddGroupTransition,
            &ClearGroupTransitions)
        .Factory();
    status = stateGroup.Result();
    if (!status) return status.GetStatus();

    auto state = Meta::Register<VisualState>(context);
    state
        .Property(
            "Name",
            &VisualState::GetName,
            &VisualState::SetName)
        .Property<
            Base::Ref<Media::Animation::Storyboard>,
            &VisualState::GetStoryboard,
            &VisualState::SetStoryboard>(
            "Storyboard",
            PropertyFlags::Structural)
        .Content<Base::Object>(
            "Content",
            ContentKind::Collection,
            &AddStateContent,
            &ClearStateContent)
        .Factory();
    status = state.Result();
    if (!status) return status.GetStatus();

    auto transition =
        Meta::Register<VisualTransition>(context);
    transition
        .Property(
            "From",
            &VisualTransition::GetFrom,
            &VisualTransition::SetFrom)
        .Property(
            "To",
            &VisualTransition::GetTo,
            &VisualTransition::SetTo)
        .Property(
            "GeneratedDuration",
            &VisualTransition::GetGeneratedDuration,
            &VisualTransition::SetGeneratedDuration)
        .Property<
            Base::Ref<Media::Animation::EasingFunctionBase>,
            &VisualTransition::GetGeneratedEasingFunction,
            &VisualTransition::SetGeneratedEasingFunction>(
            "GeneratedEasingFunction",
            PropertyFlags::Structural)
        .Content<Media::Animation::Storyboard>(
            "Storyboard",
            ContentKind::Single,
            &SetTransitionStoryboard,
            &ClearTransitionStoryboard)
        .Factory();
    status = transition.Result();
    return status;
}

} // namespace Aero::Markup

// ===== FacetStore =====



#include <cstdint>

namespace Aero::Markup {
namespace {

bool HasTypeFlag(
    Meta::TypeFlags value,
    Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status FrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML facet store is frozen");
}

Base::Result<void> ValidateObjectType(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) noexcept {
    const Meta::TypeInfo* descriptor = descriptors.FindType(type);
    if (descriptor == nullptr ||
        HasTypeFlag(descriptor->Flags(), Meta::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML type capability requires a registered object type");
    }
    return {};
}

Base::Status InvalidFacet(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status DuplicateFacet(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::AlreadyExists, message);
}

template<class T, class ExactLookup>
const T* FindInherited(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors,
    ExactLookup&& lookup) noexcept {
    Meta::TypeId current = type;
    std::uint32_t depth = 0U;
    while (current != Meta::InvalidTypeId &&
           depth <= descriptors.TypeCount()) {
        const T* facet = lookup(current);
        if (facet != nullptr) return facet;
        const Meta::TypeInfo* descriptor = descriptors.FindType(current);
        if (descriptor == nullptr) return nullptr;
        current = descriptor->BaseType();
        ++depth;
    }
    return nullptr;
}

template<class T, class ExactLookup>
const T* FindByPolicy(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors,
    ExactLookup&& lookup) noexcept {
    if constexpr (
        T::InheritancePolicy ==
        XamlFacetInheritancePolicy::ExactOnly) {
        return lookup(type);
    } else {
        return FindInherited<T>(
            type, descriptors,
            std::forward<ExactLookup>(lookup));
    }
}

} // namespace

std::uint16_t XamlFacets::FacetCountBefore(
    FacetMask mask,
    FacetKind kind) noexcept {
    return ::Aero::CompactFacetIndex::CountBefore(mask, kind);
}

XamlFacets::DraftType* XamlFacets::FindDraft(
    Meta::TypeId type) noexcept {
    for (DraftType& draft : drafts_) {
        if (draft.type == type) return &draft;
    }
    return nullptr;
}

const XamlFacets::DraftType* XamlFacets::FindDraft(
    Meta::TypeId type) const noexcept {
    for (const DraftType& draft : drafts_) {
        if (draft.type == type) return &draft;
    }
    return nullptr;
}

Base::Result<XamlFacets::DraftType*>
XamlFacets::EnsureType(Meta::TypeId type) noexcept {
    DraftType* existing = FindDraft(type);
    if (existing != nullptr) return existing;
    Base::Result<DraftType*> added = drafts_.EmplaceBack();
    if (!added) return added.GetStatus();
    added.Value()->type = type;
    return added.Value();
}

const XamlFacets::XamlTypePlan* XamlFacets::FindPlan(
    Meta::TypeId type) const noexcept {
    if (!frozen_) return nullptr;
    const std::uint32_t* position = index_.Find(type);
    return position != nullptr && *position < plans_.Size()
        ? &plans_[*position]
        : nullptr;
}

std::uint32_t XamlFacets::FindFacetIndex(
    Meta::TypeId type,
    FacetKind kind) const noexcept {
    const XamlTypePlan* plan = FindPlan(type);
    const FacetMask bit = FacetBit(kind);
    if (plan == nullptr || (plan->facetMask & bit) == 0U) {
        return InvalidFacetIndex;
    }
    const std::uint32_t reference = plan->firstFacetRef +
        FacetCountBefore(plan->facetMask, kind);
    return reference < facetRefs_.Size()
        ? facetRefs_[reference]
        : InvalidFacetIndex;
}

Base::Result<void> XamlFacets::Add(
    const XamlTypeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    if (facet.abiVersion != XamlFacetAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML type facet ABI version is incompatible");
    }
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);
    if (!valid) return valid.GetStatus();

    const bool addLifecycle =
        facet.beginInit != nullptr || facet.endInit != nullptr ||
        facet.abortInit != nullptr ||
        facet.endInitWithServices != nullptr;
    const bool addNameScope =
        facet.createsNameScope || facet.registerName != nullptr;
    const bool addResourceScope =
        facet.createsResourceScope || facet.addResource != nullptr ||
        facet.resolveResourceScope != nullptr;
    const bool addDeferredContent = facet.defersVisualContent;
    const bool addImplicitResourceKey =
        facet.resolveImplicitResourceKey != nullptr;
    const bool addPropertyTarget = facet.resolvePropertyTarget != nullptr;

    if (!addLifecycle && !addNameScope && !addResourceScope &&
        !addDeferredContent && !addImplicitResourceKey &&
        !addPropertyTarget) {
        return InvalidFacet(
            "XAML aggregate facet contains no capabilities");
    }

    const DraftType* existing = FindDraft(facet.type);
    const auto occupied = [existing](FacetKind kind) noexcept {
        return existing != nullptr &&
            existing->facets[static_cast<std::uint8_t>(kind)] !=
                InvalidFacetIndex;
    };
    if ((addLifecycle && occupied(FacetKind::Lifecycle)) ||
        (addNameScope && occupied(FacetKind::NameScope)) ||
        (addResourceScope && occupied(FacetKind::ResourceScope)) ||
        (addDeferredContent && occupied(FacetKind::DeferredContent)) ||
        (addImplicitResourceKey &&
            occupied(FacetKind::ImplicitResourceKey)) ||
        (addPropertyTarget && occupied(FacetKind::PropertyTarget))) {
        return DuplicateFacet(
            "XAML aggregate facet overlaps an existing capability");
    }

    Base::Result<void> reserved = drafts_.Reserve(
        drafts_.Size() + (existing == nullptr ? 1U : 0U));
    if (!reserved) return reserved.GetStatus();
    if (addLifecycle) {
        reserved = lifecycles_.Reserve(lifecycles_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addNameScope) {
        reserved = nameScopes_.Reserve(nameScopes_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addResourceScope) {
        reserved = resourceScopes_.Reserve(resourceScopes_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addDeferredContent) {
        reserved = deferredContents_.Reserve(deferredContents_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addImplicitResourceKey) {
        reserved = implicitResourceKeys_.Reserve(
            implicitResourceKeys_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }
    if (addPropertyTarget) {
        reserved = propertyTargets_.Reserve(propertyTargets_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    Base::Result<DraftType*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    DraftType& draft = *ensured.Value();
    if (addLifecycle) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::Lifecycle)] =
            lifecycles_.Size();
        lifecycles_.PushBack({
            facet.type, facet.beginInit, facet.endInit, facet.abortInit,
            facet.endInitWithServices, facet.context});
    }
    if (addNameScope) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::NameScope)] =
            nameScopes_.Size();
        nameScopes_.PushBack({
            facet.type, facet.createsNameScope,
            facet.registerName, facet.context});
    }
    if (addResourceScope) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::ResourceScope)] =
            resourceScopes_.Size();
        resourceScopes_.PushBack({
            facet.type, facet.createsResourceScope, facet.addResource,
            facet.resolveResourceScope, facet.context});
    }
    if (addDeferredContent) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::DeferredContent)] =
            deferredContents_.Size();
        deferredContents_.PushBack({facet.type, true});
    }
    if (addImplicitResourceKey) {
        draft.facets[
            static_cast<std::uint8_t>(FacetKind::ImplicitResourceKey)] =
                implicitResourceKeys_.Size();
        implicitResourceKeys_.PushBack({
            facet.type, facet.resolveImplicitResourceKey, facet.context});
    }
    if (addPropertyTarget) {
        draft.facets[static_cast<std::uint8_t>(FacetKind::PropertyTarget)] =
            propertyTargets_.Size();
        propertyTargets_.PushBack({
            facet.type, facet.resolvePropertyTarget, facet.context});
    }
    return {};
}

#define AERO_ADD_XAML_FACET(                                              \
    FacetType, Column, KindValue, InvalidExpression, InvalidMessage,       \
    DuplicateMessage)                                                     \
    if (frozen_) return FrozenStatus();                                   \
    if (facet.abiVersion != XamlFacetAbiVersion) {                        \
        return Base::Status::Failure(                                     \
            Base::ErrorCode::Unsupported,                                 \
            "XAML capability facet ABI version is incompatible");       \
    }                                                                     \
    Base::Result<void> valid = ValidateObjectType(facet.type, descriptors);\
    if (!valid) return valid.GetStatus();                                  \
    if (InvalidExpression) return InvalidFacet(InvalidMessage);           \
    DraftType* existing = FindDraft(facet.type);                          \
    if (existing != nullptr &&                                            \
        existing->facets[static_cast<std::uint8_t>(KindValue)] !=         \
            InvalidFacetIndex) {                                          \
        return DuplicateFacet(DuplicateMessage);                          \
    }                                                                     \
    Base::Result<void> reserved = drafts_.Reserve(                     \
        drafts_.Size() + (existing == nullptr ? 1U : 0U));                \
    if (!reserved) return reserved.GetStatus();                           \
    reserved = Column.Reserve(Column.Size() + 1U);                     \
    if (!reserved) return reserved.GetStatus();                           \
    Base::Result<DraftType*> ensured = EnsureType(facet.type);            \
    if (!ensured) return ensured.GetStatus();                             \
    ensured.Value()->facets[static_cast<std::uint8_t>(KindValue)] =       \
        Column.Size();                                                     \
    return Column.PushBack(facet)

Base::Result<void> XamlFacets::Add(
    const XamlLifecycleFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlLifecycleFacet, lifecycles_, FacetKind::Lifecycle,
        facet.beginInit == nullptr && facet.endInit == nullptr &&
            facet.abortInit == nullptr &&
            facet.endInitWithServices == nullptr,
        "XAML lifecycle facet is invalid",
        "XAML lifecycle facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlNameScopeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlNameScopeFacet, nameScopes_, FacetKind::NameScope,
        !facet.createsNameScope && facet.registerName == nullptr,
        "XAML name-scope facet is invalid",
        "XAML name-scope facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlResourceScopeFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlResourceScopeFacet, resourceScopes_, FacetKind::ResourceScope,
        !facet.createsResourceScope && facet.addResource == nullptr &&
            facet.resolveResourceScope == nullptr,
        "XAML resource-scope facet is invalid",
        "XAML resource-scope facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlDeferredContentFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlDeferredContentFacet, deferredContents_,
        FacetKind::DeferredContent,
        !facet.defersVisualContent,
        "XAML deferred-content facet is invalid",
        "XAML deferred-content facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlImplicitResourceKeyFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlImplicitResourceKeyFacet, implicitResourceKeys_,
        FacetKind::ImplicitResourceKey,
        facet.resolve == nullptr,
        "XAML implicit-resource-key facet is invalid",
        "XAML implicit-resource-key facet is already registered");
}

Base::Result<void> XamlFacets::Add(
    const XamlPropertyTargetFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    AERO_ADD_XAML_FACET(
        XamlPropertyTargetFacet, propertyTargets_, FacetKind::PropertyTarget,
        facet.resolve == nullptr,
        "XAML property-target facet is invalid",
        "XAML property-target facet is already registered");
}

#undef AERO_ADD_XAML_FACET

Base::Result<void> XamlFacets::Add(
    const XamlMarkupExtensionFacet& facet,
    const Meta::TypeRegistry& descriptors) noexcept {
    if (frozen_) return FrozenStatus();
    const Meta::TypeInfo* type = descriptors.FindType(facet.type);
    if (facet.abiVersion != XamlFacetAbiVersion ||
        type == nullptr || facet.provideValue == nullptr ||
        !HasTypeFlag(type->Flags(), Meta::TypeFlags::MarkupExtension)) {
        return InvalidFacet(
            "XAML markup-extension facet requires a flagged type and provider");
    }
    DraftType* existing = FindDraft(facet.type);
    if (existing != nullptr &&
        existing->facets[static_cast<std::uint8_t>(
            FacetKind::MarkupExtension)] != InvalidFacetIndex) {
        return DuplicateFacet(
            "XAML markup-extension facet is already registered");
    }
    Base::Result<void> reserved = drafts_.Reserve(
        drafts_.Size() + (existing == nullptr ? 1U : 0U));
    if (!reserved) return reserved.GetStatus();
    reserved = markupExtensions_.Reserve(markupExtensions_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<DraftType*> ensured = EnsureType(facet.type);
    if (!ensured) return ensured.GetStatus();
    ensured.Value()->facets[static_cast<std::uint8_t>(
        FacetKind::MarkupExtension)] = markupExtensions_.Size();
    return markupExtensions_.PushBack(facet);
}

Base::Result<void> XamlFacets::BuildLifecyclePlans(
    const Meta::TypeRegistry& descriptors) noexcept {
    Base::Vector<Meta::TypeId> ancestry;
    Base::Result<void> reserved = ancestry.Reserve(descriptors.TypeCount());
    if (!reserved) return reserved.GetStatus();

    for (XamlTypePlan& plan : plans_) {
        plan.firstLifecycleRef = lifecycleRefs_.Size();
        ancestry.Clear();
        Meta::TypeId current = plan.type;
        std::uint32_t depth = 0U;
        while (current != Meta::InvalidTypeId &&
               depth <= descriptors.TypeCount()) {
            Base::Result<void> added = ancestry.PushBack(current);
            if (!added) return added.GetStatus();
            const Meta::TypeInfo* descriptor = descriptors.FindType(current);
            if (descriptor == nullptr) break;
            current = descriptor->BaseType();
            ++depth;
        }
        if (current != Meta::InvalidTypeId &&
            depth > descriptors.TypeCount()) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Lifecycle facet inheritance chain contains a cycle");
        }

        std::uint32_t count = 0U;
        for (std::uint32_t index = ancestry.Size(); index > 0U; --index) {
            const DraftType* draft = FindDraft(ancestry[index - 1U]);
            if (draft == nullptr) continue;
            const std::uint32_t facet = draft->facets[
                static_cast<std::uint8_t>(FacetKind::Lifecycle)];
            if (facet == InvalidFacetIndex) continue;
            if (count == UINT16_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "XAML lifecycle plan exceeds the compact record limit");
            }
            Base::Result<void> added = lifecycleRefs_.PushBack(facet);
            if (!added) return added.GetStatus();
            ++count;
        }
        plan.lifecycleCount = static_cast<std::uint16_t>(count);
    }
    return {};
}

Base::Result<void> XamlFacets::Freeze(
    const Meta::TypeRegistry& descriptors) noexcept {
    if (frozen_) return {};

    plans_.Clear();
    facetRefs_.Clear();
    lifecycleRefs_.Clear();
    index_.Clear();

    Base::Result<void> reserved = plans_.Reserve(descriptors.TypeCount());
    if (!reserved) return reserved.GetStatus();
    reserved = index_.Reserve(descriptors.TypeCount());
    if (!reserved) return reserved.GetStatus();
    reserved = facetRefs_.Reserve(
        lifecycles_.Size() + nameScopes_.Size() + resourceScopes_.Size() +
        deferredContents_.Size() + implicitResourceKeys_.Size() +
        propertyTargets_.Size() + markupExtensions_.Size());
    if (!reserved) return reserved.GetStatus();

    for (const Meta::TypeInfo& descriptor : descriptors.Types()) {
        XamlTypePlan plan;
        plan.type = descriptor.Id();
        plan.firstFacetRef = facetRefs_.Size();
        const DraftType* draft = FindDraft(plan.type);
        if (draft != nullptr) {
            for (std::uint8_t kind = 0U;
                 kind < static_cast<std::uint8_t>(FacetKind::Count);
                 ++kind) {
                const std::uint32_t facet = draft->facets[kind];
                if (facet == InvalidFacetIndex) continue;
                Base::Result<void> added = facetRefs_.PushBack(facet);
                if (!added) return added.GetStatus();
                plan.facetMask |= static_cast<FacetMask>(1U << kind);
                ++plan.facetCount;
            }
        }
        const std::uint32_t position = plans_.Size();
        Base::Result<void> added = plans_.PushBack(plan);
        if (!added) return added.GetStatus();
        Base::Result<FacetIndex::InsertResult> inserted =
            index_.Insert(plan.type, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML facet index contains a duplicate type");
        }
    }

    Base::Result<void> lifecycle = BuildLifecyclePlans(descriptors);
    if (!lifecycle) return lifecycle.GetStatus();
    drafts_.Clear();
    frozen_ = true;
    return {};
}

Base::Span<const std::uint32_t> XamlFacets::LifecyclePlan(
    Meta::TypeId type) const noexcept {
    const XamlTypePlan* plan = FindPlan(type);
    if (plan == nullptr || plan->lifecycleCount == 0U ||
        plan->firstLifecycleRef + plan->lifecycleCount >
            lifecycleRefs_.Size()) {
        return {};
    }
    return {
        lifecycleRefs_.Data() + plan->firstLifecycleRef,
        plan->lifecycleCount};
}

const XamlLifecycleFacet* XamlFacets::LifecycleAt(
    std::uint32_t index) const noexcept {
    return index < lifecycles_.Size() ? &lifecycles_[index] : nullptr;
}

const XamlLifecycleFacet* XamlFacets::FindLifecycle(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlLifecycleFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindLifecycleExact(current);
        });
}

const XamlNameScopeFacet* XamlFacets::FindNameScope(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlNameScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindNameScopeExact(current);
        });
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScope(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlResourceScopeFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindResourceScopeExact(current);
        });
}

const XamlDeferredContentFacet* XamlFacets::FindDeferredContent(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlDeferredContentFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindDeferredContentExact(current);
        });
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKey(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlImplicitResourceKeyFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindImplicitResourceKeyExact(current);
        });
}

const XamlPropertyTargetFacet* XamlFacets::FindPropertyTarget(
    Meta::TypeId type,
    const Meta::TypeRegistry& descriptors) const noexcept {
    return FindByPolicy<XamlPropertyTargetFacet>(
        type, descriptors,
        [this](Meta::TypeId current) noexcept {
            return FindPropertyTargetExact(current);
        });
}

const XamlMarkupExtensionFacet* XamlFacets::FindMarkupExtension(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::MarkupExtension);
    return index < markupExtensions_.Size()
        ? &markupExtensions_[index]
        : nullptr;
}

const XamlLifecycleFacet* XamlFacets::FindLifecycleExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(type, FacetKind::Lifecycle);
    return index < lifecycles_.Size() ? &lifecycles_[index] : nullptr;
}

const XamlNameScopeFacet* XamlFacets::FindNameScopeExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(type, FacetKind::NameScope);
    return index < nameScopes_.Size() ? &nameScopes_[index] : nullptr;
}

const XamlResourceScopeFacet* XamlFacets::FindResourceScopeExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::ResourceScope);
    return index < resourceScopes_.Size()
        ? &resourceScopes_[index]
        : nullptr;
}

const XamlDeferredContentFacet*
XamlFacets::FindDeferredContentExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::DeferredContent);
    return index < deferredContents_.Size()
        ? &deferredContents_[index]
        : nullptr;
}

const XamlImplicitResourceKeyFacet*
XamlFacets::FindImplicitResourceKeyExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::ImplicitResourceKey);
    return index < implicitResourceKeys_.Size()
        ? &implicitResourceKeys_[index]
        : nullptr;
}

const XamlPropertyTargetFacet*
XamlFacets::FindPropertyTargetExact(
    Meta::TypeId type) const noexcept {
    const std::uint32_t index = FindFacetIndex(
        type, FacetKind::PropertyTarget);
    return index < propertyTargets_.Size()
        ? &propertyTargets_[index]
        : nullptr;
}

} // namespace Aero::Markup


// ===== SchemaManifest =====


// Immutable compiled-schema manifest implementation.

#include <Aero/Base/Assert.hpp>

#include <new>

namespace Aero::Markup {
namespace {

constexpr std::uint32_t ManifestMagic = UINT32_C(0x48435341); // ASCH
constexpr std::uint32_t ManifestEncodingVersion = 1U;

enum class ManifestMemberKind : std::uint8_t {
    Property = 0U,
    Event
};

Base::Status InvalidManifest(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed, message);
}

Base::Status ManifestNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML schema manifest is not initialized");
}

Base::Status TypeNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML schema manifest type was not found");
}

Base::Status MemberNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML schema manifest member was not found");
}

bool HasPropertyFlag(
    Meta::PropertyFlags value,
    Meta::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasEventFlag(
    Meta::EventFlags value,
    Meta::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr Base::StringView WpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
constexpr Base::StringView BehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
constexpr Base::StringView BlendInteractivityNamespace(
    "http://schemas.microsoft.com/expression/2010/interactivity");
constexpr Base::StringView SystemNamespacePrefix(
    "clr-namespace:System");

bool MatchesClrNamespacePrefix(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

bool IsExtensionsClrNamespace(Base::StringView value) noexcept {
    return MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:AeroGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Aero.GUI.Extensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:NoesisGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Noesis.GUI.Extensions"));
}

Base::StringView CanonicalXamlNamespace(
    Base::StringView value) noexcept {
    return value == WpfPresentationNamespace ||
            value == BehaviorsNamespace ||
            value == BlendInteractivityNamespace ||
            IsExtensionsClrNamespace(value)
        ? Meta::AeroNamespaceUri()
        : value;
}

bool IsSystemNamespace(
    Base::StringView value) noexcept {
    return value.SizeBytes() >=
            SystemNamespacePrefix.SizeBytes() &&
        value.Substr(0U, SystemNamespacePrefix.SizeBytes()) ==
            SystemNamespacePrefix;
}

Base::StringView CanonicalXamlTypeName(
    Base::StringView value) noexcept {
    if (value == Base::StringView("Geometry")) {
        return Base::StringView("StreamGeometry");
    }
    if (value == Base::StringView("VisualStateTransition")) {
        return Base::StringView("VisualTransition");
    }
    if (value == Base::StringView("MonochromeBrush")) {
        return Base::StringView("MonochromeShader");
    }
    if (value == Base::StringView("ConicGradientBrush")) {
        return Base::StringView("ConicGradientShader");
    }
    if (value == Base::StringView("WavesBrush")) {
        return Base::StringView("WavesShader");
    }
    return value;
}

bool IsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    return IsExtensionsClrNamespace(xamlNamespace) &&
        (ownerName == Base::StringView("Text") ||
         ownerName == Base::StringView("Path") ||
         ownerName == Base::StringView("Brush") ||
          ownerName == Base::StringView("Element") ||
          ownerName == Base::StringView("RichText"));
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
    Base::Result<void> result = AppendU32(output, value.SizeBytes());
    if (!result) return result.GetStatus();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        result = AppendU8(
            output,
            static_cast<std::uint8_t>(value[index]));
        if (!result) return result.GetStatus();
    }
    return {};
}

class Decoder {
public:
    explicit Decoder(Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    Base::Result<std::uint8_t> ReadU8() noexcept {
        if (offset_ >= bytes_.Size()) return Truncated();
        return bytes_[offset_++];
    }

    Base::Result<std::uint32_t> ReadU32() noexcept {
        if (bytes_.Size() - offset_ < 4U) return Truncated();
        std::uint32_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadU64() noexcept {
        if (bytes_.Size() - offset_ < 8U) return Truncated();
        std::uint64_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<Base::String> ReadString(
        Base::IAllocator& allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        Base::Result<std::uint32_t> length = ReadU32();
        if (!length) return length.GetStatus();
        if (length.Value() > bytes_.Size() - offset_ ||
            length.Value() > maxStringBytes ||
            totalStringBytes > maxStringBytes - length.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML schema manifest string bounds are invalid");
        }
        Base::String value(&allocator);
        Base::Result<void> assigned = value.Assign(
            Base::StringView(
                reinterpret_cast<const char*>(bytes_.Data() + offset_),
                length.Value()));
        if (!assigned) return assigned.GetStatus();
        offset_ += length.Value();
        totalStringBytes += length.Value();
        return value;
    }

    bool AtEnd() const noexcept { return offset_ == bytes_.Size(); }

private:
    static Base::Status Truncated() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest payload is truncated");
    }

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
};

} // namespace

struct SchemaManifestState {
    struct TypeRecord {
        explicit TypeRecord(Base::IAllocator& allocator) noexcept
            : xamlNamespace(&allocator), name(&allocator) {}

        Meta::TypeId id = Meta::InvalidTypeId;
        Meta::TypeId baseType = Meta::InvalidTypeId;
        Meta::MetadataTypeKind kind = Meta::MetadataTypeKind::Object;
        Meta::TypeFlags flags = Meta::TypeFlags::None;
        Meta::MemberId contentMember = Meta::InvalidMemberId;
        Base::String xamlNamespace;
        Base::String name;
    };

    struct MemberRecord {
        explicit MemberRecord(Base::IAllocator& allocator) noexcept
            : name(&allocator) {}

        Meta::MemberId id = Meta::InvalidMemberId;
        ManifestMemberKind kind = ManifestMemberKind::Property;
        Meta::TypeId ownerType = Meta::InvalidTypeId;
        Meta::TypeId valueType = Meta::InvalidTypeId;
        std::uint32_t flags = 0U;
        Base::String name;
    };

    explicit SchemaManifestState(Base::IAllocator& allocator) noexcept
        : types(&allocator),
          members(&allocator),
          typeIndex(&allocator),
          memberIndex(&allocator) {}

    CompiledCacheIdentity identity;
    Base::Vector<TypeRecord> types;
    Base::Vector<MemberRecord> members;
    Base::HashMap<Meta::TypeId, std::uint32_t> typeIndex;
    Base::HashMap<Meta::MemberId, std::uint32_t> memberIndex;
    bool valid = false;

    Base::Result<void> RebuildIndexes() noexcept {
        typeIndex.Clear();
        memberIndex.Clear();
        for (std::uint32_t index = 0U; index < types.Size(); ++index) {
            Base::Result<typename Base::HashMap<Meta::TypeId, std::uint32_t>::InsertResult>
                inserted = typeIndex.Insert(types[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate TypeId values");
            }
        }
        for (std::uint32_t index = 0U; index < members.Size(); ++index) {
            Base::Result<typename Base::HashMap<Meta::MemberId, std::uint32_t>::InsertResult>
                inserted = memberIndex.Insert(members[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate MemberId values");
            }
        }
        return {};
    }

    const TypeRecord* FindType(Meta::TypeId id) const noexcept {
        const std::uint32_t* index = typeIndex.Find(id);
        return index != nullptr && *index < types.Size()
            ? &types[*index] : nullptr;
    }

    const TypeRecord* FindType(
        Base::StringView xamlNamespace,
        Base::StringView name) const noexcept {
        for (const TypeRecord& type : types) {
            if (type.xamlNamespace.View() == xamlNamespace &&
                type.name.View() == name) {
                return &type;
            }
        }
        return nullptr;
    }

    const MemberRecord* FindMember(Meta::MemberId id) const noexcept {
        const std::uint32_t* index = memberIndex.Find(id);
        return index != nullptr && *index < members.Size()
            ? &members[*index] : nullptr;
    }

    const MemberRecord* FindMember(
        Meta::TypeId ownerType,
        Base::StringView name,
        ManifestMemberKind kind,
        bool includeBaseTypes) const noexcept {
        Meta::TypeId current = ownerType;
        for (std::uint32_t depth = 0U;
             current != Meta::InvalidTypeId && depth <= types.Size();
             ++depth) {
            for (const MemberRecord& member : members) {
                if (member.ownerType == current &&
                    member.kind == kind &&
                    member.name.View() == name) {
                    return &member;
                }
            }
            if (!includeBaseTypes) break;
            const TypeRecord* type = FindType(current);
            if (type == nullptr) break;
            current = type->baseType;
        }
        return nullptr;
    }

    bool IsDerivedFrom(
        Meta::TypeId type,
        Meta::TypeId expectedBase) const noexcept {
        if (type == Meta::InvalidTypeId ||
            expectedBase == Meta::InvalidTypeId) {
            return false;
        }
        Meta::TypeId current = type;
        for (std::uint32_t depth = 0U;
             current != Meta::InvalidTypeId && depth <= types.Size();
             ++depth) {
            if (current == expectedBase) return true;
            const TypeRecord* descriptor = FindType(current);
            if (descriptor == nullptr) return false;
            current = descriptor->baseType;
        }
        return false;
    }

    Base::Result<ResolvedMember> ResolvePropertyOrEvent(
        Meta::TypeId targetType,
        Meta::TypeId ownerType,
        Base::StringView memberName,
        MemberSyntax syntax,
        bool ownerWasExplicit) const noexcept {
        const MemberRecord* property = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Property,
            true);
        if (property != nullptr) {
            const Meta::PropertyFlags flags =
                static_cast<Meta::PropertyFlags>(property->flags);
            const bool attached = HasPropertyFlag(
                flags, Meta::PropertyFlags::Attached);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && !attached &&
                !IsDerivedFrom(targetType, property->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Explicit XAML attribute owner requires an attached property");
            }
            if (ownerWasExplicit &&
                syntax == MemberSyntax::PropertyElement && !attached &&
                !IsDerivedFrom(targetType, property->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "XAML property element owner is incompatible with the target type");
            }
            ResolvedMember resolved;
            resolved.id = property->id;
            resolved.kind = Meta::MemberKind::Property;
            resolved.ownerType = property->ownerType;
            resolved.valueType = property->valueType;
            resolved.propertyFlags = flags;
            resolved.attached = attached;
            return resolved;
        }

        const MemberRecord* event = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Event,
            true);
        if (event != nullptr) {
            const Meta::EventFlags flags =
                static_cast<Meta::EventFlags>(event->flags);
            const bool attached = HasEventFlag(
                flags, Meta::EventFlags::Attached);
            const bool routed = HasEventFlag(
                flags, Meta::EventFlags::Routed);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute &&
                !attached && !routed) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Explicit XAML attribute owner requires an attached event");
            }
            if (ownerWasExplicit &&
                syntax == MemberSyntax::PropertyElement && !attached &&
                !IsDerivedFrom(targetType, event->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "XAML event element owner is incompatible with the target type");
            }
            ResolvedMember resolved;
            resolved.id = event->id;
            resolved.kind = Meta::MemberKind::Event;
            resolved.ownerType = event->ownerType;
            resolved.valueType = event->valueType;
            resolved.eventFlags = flags;
            resolved.attached = attached ||
                (ownerWasExplicit &&
                 syntax == MemberSyntax::Attribute && routed);
            return resolved;
        }
        return MemberNotFound();
    }
};

static_assert(
    sizeof(SchemaManifestState) <= 2048,
    "SchemaManifest inline state storage is too small");
static_assert(
    alignof(SchemaManifestState) <= alignof(std::max_align_t),
    "SchemaManifest inline state alignment is insufficient");

namespace {

template<class T>
Base::Result<T*> AllocateObject(
    Base::IAllocator& allocator) noexcept {
    void* memory = allocator.Allocate({
        sizeof(T), alignof(T), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML schema manifest allocation failed");
    }
    return new (memory) T(allocator);
}

void DestroyManifestState(
    Base::IAllocator& allocator,
    SchemaManifestState*& state) noexcept {
    if (state == nullptr) return;
    state->~SchemaManifestState();
    allocator.Deallocate(
        state,
        sizeof(SchemaManifestState),
        alignof(SchemaManifestState),
        Base::MemoryTag::Markup);
    state = nullptr;
}

Base::Result<void> AppendIdentity(
    Base::Vector<std::uint8_t>& output,
    const CompiledCacheIdentity& identity) noexcept {
    Base::Result<void> result = AppendU32(
        output, identity.cacheFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.typeIdAlgorithmVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataSchemaFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataProgramFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.schemaVersion);
    if (!result) return result.GetStatus();
    return AppendU64(output, identity.metadataSchemaHash);
}

Base::Result<CompiledCacheIdentity> ReadIdentity(
    Decoder& decoder) noexcept {
    CompiledCacheIdentity identity;
    Base::Result<std::uint32_t> value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.cacheFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.typeIdAlgorithmVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataSchemaFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataProgramFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.schemaVersion = value.Value();
    Base::Result<std::uint64_t> hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    identity.metadataSchemaHash = hash.Value();
    return identity;
}

} // namespace

SchemaManifest::SchemaManifest(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator : &Base::GetDefaultAllocator()) {}

SchemaManifest::SchemaManifest(
    Base::IAllocator& allocator,
    SchemaManifestState* state) noexcept
    : allocator_(&allocator) {
    if (state == nullptr) return;
    state_ = new (stateStorage_) SchemaManifestState(std::move(*state));
    DestroyManifestState(allocator, state);
}

SchemaManifest::~SchemaManifest() noexcept {
    if (state_ == nullptr) return;
    state_->~SchemaManifestState();
    state_ = nullptr;
}

SchemaManifest::SchemaManifest(
    SchemaManifest&& other) noexcept
    : allocator_(other.allocator_) {
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            SchemaManifestState(std::move(*other.state_));
        other.state_->~SchemaManifestState();
        other.state_ = nullptr;
    }
    other.allocator_ = &Base::GetDefaultAllocator();
}

SchemaManifest& SchemaManifest::operator=(
    SchemaManifest&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        state_->~SchemaManifestState();
        state_ = nullptr;
    }
    allocator_ = other.allocator_;
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            SchemaManifestState(std::move(*other.state_));
        other.state_->~SchemaManifestState();
        other.state_ = nullptr;
    }
    other.allocator_ = &Base::GetDefaultAllocator();
    return *this;
}

Base::Result<SchemaManifest> SchemaManifest::Capture(
    const Schema& schema,
    Base::IAllocator* allocator) noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML schema manifest capture requires a frozen schema");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Result<SchemaManifestState*> created = AllocateObject<SchemaManifestState>(selected);
    if (!created) return created.GetStatus();
    SchemaManifestState* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) {
        DestroyManifestState(selected, impl);
        return identity.GetStatus();
    }
    impl->identity = identity.Value();

    const Meta::TypeRegistry& descriptors = schema.Types();
    Base::Result<void> reserved = impl->types.Reserve(descriptors.TypeCount());
    if (!reserved) {
        DestroyManifestState(selected, impl);
        return reserved.GetStatus();
    }
    reserved = impl->members.Reserve(
        descriptors.PropertyCount() + descriptors.EventCount());
    if (!reserved) {
        DestroyManifestState(selected, impl);
        return reserved.GetStatus();
    }

    for (const Meta::TypeInfo& type : descriptors.Types()) {
        SchemaManifestState::TypeRecord record(selected);
        record.id = type.Id();
        record.baseType = type.BaseType();
        record.kind = type.Kind();
        record.flags = type.Flags();
        Base::Result<void> assigned = record.xamlNamespace.Assign(
            type.XamlNamespace());
        if (assigned) assigned = record.name.Assign(type.Name());
        if (!assigned) {
            DestroyManifestState(selected, impl);
            return assigned.GetStatus();
        }
        Base::Result<ResolvedMember> content =
            schema.ResolveContentMember(type.Id());
        if (content) {
            record.contentMember = content.Value().id;
        } else if (content.GetStatus().code != Base::ErrorCode::NotFound) {
            DestroyManifestState(selected, impl);
            return content.GetStatus();
        }
        Base::Result<void> appended = impl->types.PushBack(
            std::move(record));
        if (!appended) {
            DestroyManifestState(selected, impl);
            return appended.GetStatus();
        }

        for (const Meta::PropertyInfo& property : type.Properties()) {
            SchemaManifestState::MemberRecord member(selected);
            member.id = property.Id();
            member.kind = ManifestMemberKind::Property;
            member.ownerType = property.OwnerType();
            member.valueType = property.ValueType();
            member.flags = static_cast<std::uint32_t>(property.Flags());
            assigned = member.name.Assign(property.Name());
            if (!assigned) {
                DestroyManifestState(selected, impl);
                return assigned.GetStatus();
            }
            appended = impl->members.PushBack(std::move(member));
            if (!appended) {
                DestroyManifestState(selected, impl);
                return appended.GetStatus();
            }
        }

        for (const Meta::EventInfo& event : type.Events()) {
            SchemaManifestState::MemberRecord member(selected);
            member.id = event.Id();
            member.kind = ManifestMemberKind::Event;
            member.ownerType = event.OwnerType();
            member.valueType = event.EventArgsType();
            member.flags = static_cast<std::uint32_t>(event.Flags());
            assigned = member.name.Assign(event.Name());
            if (!assigned) {
                DestroyManifestState(selected, impl);
                return assigned.GetStatus();
            }
            appended = impl->members.PushBack(std::move(member));
            if (!appended) {
                DestroyManifestState(selected, impl);
                return appended.GetStatus();
            }
        }
    }

    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyManifestState(selected, impl);
        return indexed.GetStatus();
    }
    impl->valid = true;
    return SchemaManifest(selected, impl);
}

Base::Result<SchemaManifest> SchemaManifest::Deserialize(
    Base::Span<const std::uint8_t> bytes,
    const SchemaManifestLimits& limits,
    Base::IAllocator* allocator) noexcept {
    if (limits.maxTypes == 0U ||
        limits.maxMembers == 0U ||
        limits.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML schema manifest limits must be positive");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Decoder decoder(bytes);
    Base::Result<std::uint32_t> magic = decoder.ReadU32();
    if (!magic) return magic.GetStatus();
    Base::Result<std::uint32_t> encoding = decoder.ReadU32();
    if (!encoding) return encoding.GetStatus();
    Base::Result<std::uint32_t> format = decoder.ReadU32();
    if (!format) return format.GetStatus();
    if (magic.Value() != ManifestMagic ||
        encoding.Value() != ManifestEncodingVersion ||
        format.Value() != XamlSchemaManifestFormatVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest format is not supported");
    }

    Base::Result<SchemaManifestState*> created = AllocateObject<SchemaManifestState>(selected);
    if (!created) return created.GetStatus();
    SchemaManifestState* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity = ReadIdentity(decoder);
    if (!identity) {
        DestroyManifestState(selected, impl);
        return identity.GetStatus();
    }
    CompiledCacheIdentity current;
    current.metadataSchemaHash = identity.Value().metadataSchemaHash;
    if (CompareCompiledCacheIdentity(identity.Value(), current) !=
        CompiledCacheCompatibility::Compatible) {
        DestroyManifestState(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest ABI is incompatible with this tool");
    }
    impl->identity = identity.Value();

    Base::Result<std::uint32_t> typeCount = decoder.ReadU32();
    if (!typeCount) {
        DestroyManifestState(selected, impl);
        return typeCount.GetStatus();
    }
    Base::Result<std::uint32_t> memberCount = decoder.ReadU32();
    if (!memberCount) {
        DestroyManifestState(selected, impl);
        return memberCount.GetStatus();
    }
    if (typeCount.Value() > limits.maxTypes ||
        memberCount.Value() > limits.maxMembers) {
        DestroyManifestState(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest descriptor count exceeds limits");
    }
    Base::Result<void> reserved = impl->types.Reserve(typeCount.Value());
    if (reserved) reserved = impl->members.Reserve(memberCount.Value());
    if (!reserved) {
        DestroyManifestState(selected, impl);
        return reserved.GetStatus();
    }

    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        SchemaManifestState::TypeRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyManifestState(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint64_t> baseType = decoder.ReadU64();
        if (!baseType) {
            DestroyManifestState(selected, impl);
            return baseType.GetStatus();
        }
        Base::Result<std::uint32_t> kind = decoder.ReadU32();
        if (!kind) {
            DestroyManifestState(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyManifestState(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<std::uint64_t> content = decoder.ReadU64();
        if (!content) {
            DestroyManifestState(selected, impl);
            return content.GetStatus();
        }
        Base::Result<Base::String> xamlNamespace = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!xamlNamespace) {
            DestroyManifestState(selected, impl);
            return xamlNamespace.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyManifestState(selected, impl);
            return name.GetStatus();
        }
        if (id.Value() == Meta::InvalidTypeId || name.Value().Empty() ||
            Meta::MakeTypeId(
                xamlNamespace.Value().View(),
                name.Value().View()) != id.Value() ||
            kind.Value() > static_cast<std::uint32_t>(Meta::MetadataTypeKind::Primitive)) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest type descriptor is invalid");
        }
        record.id = id.Value();
        record.baseType = baseType.Value();
        record.kind = static_cast<Meta::MetadataTypeKind>(kind.Value());
        record.flags = static_cast<Meta::TypeFlags>(flags.Value());
        record.contentMember = content.Value();
        record.xamlNamespace = std::move(xamlNamespace).Value();
        record.name = std::move(name).Value();
        Base::Result<void> appended = impl->types.PushBack(std::move(record));
        if (!appended) {
            DestroyManifestState(selected, impl);
            return appended.GetStatus();
        }
    }

    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        SchemaManifestState::MemberRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyManifestState(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint8_t> kind = decoder.ReadU8();
        if (!kind) {
            DestroyManifestState(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> reservedField = decoder.ReadU32();
        if (!reservedField) {
            DestroyManifestState(selected, impl);
            return reservedField.GetStatus();
        }
        Base::Result<std::uint64_t> owner = decoder.ReadU64();
        if (!owner) {
            DestroyManifestState(selected, impl);
            return owner.GetStatus();
        }
        Base::Result<std::uint64_t> valueType = decoder.ReadU64();
        if (!valueType) {
            DestroyManifestState(selected, impl);
            return valueType.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyManifestState(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyManifestState(selected, impl);
            return name.GetStatus();
        }
        const bool validKind =
            kind.Value() <= static_cast<std::uint8_t>(ManifestMemberKind::Event);
        const Meta::MemberKind metadataKind =
            kind.Value() == static_cast<std::uint8_t>(ManifestMemberKind::Event)
            ? Meta::MemberKind::Event
            : Meta::MemberKind::Property;
        if (id.Value() == Meta::InvalidMemberId ||
            owner.Value() == Meta::InvalidTypeId ||
            valueType.Value() == Meta::InvalidTypeId ||
            name.Value().Empty() ||
            reservedField.Value() != 0U ||
            !validKind ||
            Meta::MakeMemberId(
                owner.Value(), metadataKind, name.Value().View()) != id.Value()) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest member descriptor is invalid");
        }
        record.id = id.Value();
        record.kind = static_cast<ManifestMemberKind>(kind.Value());
        record.ownerType = owner.Value();
        record.valueType = valueType.Value();
        record.flags = flags.Value();
        record.name = std::move(name).Value();
        Base::Result<void> appended = impl->members.PushBack(std::move(record));
        if (!appended) {
            DestroyManifestState(selected, impl);
            return appended.GetStatus();
        }
    }

    if (!decoder.AtEnd()) {
        DestroyManifestState(selected, impl);
        return InvalidManifest("XAML schema manifest has trailing bytes");
    }
    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyManifestState(selected, impl);
        return indexed.GetStatus();
    }
    for (const SchemaManifestState::TypeRecord& type : impl->types) {
        if (type.baseType != Meta::InvalidTypeId &&
            impl->FindType(type.baseType) == nullptr) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest base type is missing");
        }
        Meta::TypeId current = type.id;
        std::uint32_t depth = 0U;
        while (current != Meta::InvalidTypeId && depth <= impl->types.Size()) {
            const SchemaManifestState::TypeRecord* currentType = impl->FindType(current);
            if (currentType == nullptr) break;
            current = currentType->baseType;
            ++depth;
        }
        if (current != Meta::InvalidTypeId) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest type hierarchy contains a cycle");
        }
        if (type.contentMember != Meta::InvalidMemberId) {
            const SchemaManifestState::MemberRecord* content = impl->FindMember(type.contentMember);
            if (content == nullptr ||
                content->kind != ManifestMemberKind::Property ||
                !impl->IsDerivedFrom(type.id, content->ownerType)) {
                DestroyManifestState(selected, impl);
                return InvalidManifest("XAML schema manifest content member is missing or incompatible");
            }
        }
    }
    for (const SchemaManifestState::MemberRecord& member : impl->members) {
        if (impl->FindType(member.ownerType) == nullptr ||
            impl->FindType(member.valueType) == nullptr) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest member type is missing");
        }
    }
    impl->valid = true;
    return SchemaManifest(selected, impl);
}

Base::Result<Base::Vector<std::uint8_t>>
SchemaManifest::Serialize() const noexcept {
    if (!IsValid()) return ManifestNotReady();
    Base::Vector<std::uint8_t> output(allocator_);
    Base::Result<void> result = AppendU32(output, ManifestMagic);
    if (result) result = AppendU32(output, ManifestEncodingVersion);
    if (result) result = AppendU32(output, XamlSchemaManifestFormatVersion);
    if (result) result = AppendIdentity(output, state_->identity);
    if (result) result = AppendU32(output, state_->types.Size());
    if (result) result = AppendU32(output, state_->members.Size());
    if (!result) return result.GetStatus();

    for (const SchemaManifestState::TypeRecord& type : state_->types) {
        result = AppendU64(output, type.id);
        if (result) result = AppendU64(output, type.baseType);
        if (result) result = AppendU32(
            output, static_cast<std::uint32_t>(type.kind));
        if (result) result = AppendU32(
            output, static_cast<std::uint32_t>(type.flags));
        if (result) result = AppendU64(output, type.contentMember);
        if (result) result = AppendString(output, type.xamlNamespace.View());
        if (result) result = AppendString(output, type.name.View());
        if (!result) return result.GetStatus();
    }
    for (const SchemaManifestState::MemberRecord& member : state_->members) {
        result = AppendU64(output, member.id);
        if (result) result = AppendU8(
            output, static_cast<std::uint8_t>(member.kind));
        if (result) result = AppendU32(output, 0U);
        if (result) result = AppendU64(output, member.ownerType);
        if (result) result = AppendU64(output, member.valueType);
        if (result) result = AppendU32(output, member.flags);
        if (result) result = AppendString(output, member.name.View());
        if (!result) return result.GetStatus();
    }
    return output;
}

bool SchemaManifest::IsValid() const noexcept {
    return state_ != nullptr && state_->valid;
}

std::uint32_t SchemaManifest::TypeCount() const noexcept {
    return IsValid() ? state_->types.Size() : 0U;
}

std::uint32_t SchemaManifest::MemberCount() const noexcept {
    return IsValid() ? state_->members.Size() : 0U;
}

const CompiledCacheIdentity& SchemaManifest::Identity() const noexcept {
    AERO_ASSERT(IsValid());
    return state_->identity;
}

Base::Result<SchemaTypeInfo> SchemaManifest::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifestState::TypeRecord* type = state_->FindType(
        IsSystemNamespace(xamlNamespace)
            ? Meta::AeroNamespaceUri()
            : CanonicalXamlNamespace(xamlNamespace),
        CanonicalXamlTypeName(localName));
    if (type == nullptr) return TypeNotFound();
    return SchemaTypeInfo{type->id, type->kind, type->flags};
}

Base::Result<ResolvedMember> SchemaManifest::ResolveMember(
    Meta::TypeId targetType,
    const QualifiedName& name,
    MemberSyntax syntax) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifestState::TypeRecord* target = state_->FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) return MemberNotFound();

    const Base::StringView localName = name.LocalName();
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML schema manifest member contains multiple owner separators");
        }
        dot = index;
    }

    if (dot == localName.SizeBytes()) {
        if (!name.NamespaceUri().Empty() &&
            CanonicalXamlNamespace(name.NamespaceUri()) !=
                target->xamlNamespace.View()) {
            return MemberNotFound();
        }
        return state_->ResolvePropertyOrEvent(
            targetType, targetType, localName, syntax, false);
    }
    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML schema manifest member owner syntax is invalid");
    }
    const Base::StringView ownerName = localName.Substr(0U, dot);
    const Base::StringView memberName = localName.Substr(
        dot + 1U, localName.SizeBytes() - dot - 1U);
    const Base::StringView ownerNamespace = name.NamespaceUri().Empty()
        ? target->xamlNamespace.View() : name.NamespaceUri();
    if (IsAeroExtensionsFacade(
            ownerNamespace, ownerName)) {
        // The reference Gallery uses the legacy Element.BlendingMode
        // extension name. Element is also a real Aero extension owner
        // (PPAAOut), so normalize this one compatibility alias before
        // looking up the owner rather than letting that type shadow the
        // inherited UIElement BlendMode property.
        if (ownerName == Base::StringView("Element") &&
            memberName == Base::StringView("BlendingMode")) {
            return state_->ResolvePropertyOrEvent(
                targetType,
                targetType,
                Base::StringView("BlendMode"),
                syntax,
                false);
        }
        // The legacy AeroGUIExtensions facade predates real attached
        // properties. Prefer a registered Aero owner (for example
        // aero:Path.TrimEnd) and retain the facade only for extension-only
        // members such as aero:Text.*.
        const SchemaManifestState::TypeRecord* aeroOwner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(ownerName));
        if (aeroOwner != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, aeroOwner->id, memberName, syntax, true);
        }
        return state_->ResolvePropertyOrEvent(
            targetType,
            targetType,
            memberName,
            syntax,
            false);
    }
    const SchemaManifestState::TypeRecord* owner = state_->FindType(
        CanonicalXamlNamespace(ownerNamespace),
        CanonicalXamlTypeName(ownerName));
    if (owner == nullptr && name.NamespaceUri().Empty()) {
        owner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(ownerName));
    }
    if (owner == nullptr) return MemberNotFound();
    // WPF exposes ContextMenu through FrameworkElement property-element
    // syntax (for example Border.ContextMenu) while storage is supplied by
    // the attached ContextMenuService property.
    if (memberName == Base::StringView("ContextMenu")) {
        const SchemaManifestState::TypeRecord* service = state_->FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, service->id, memberName, syntax, true);
        }
    }
    return state_->ResolvePropertyOrEvent(
        targetType, owner->id, memberName, syntax, true);
}

Base::Result<ResolvedMember> SchemaManifest::ResolveContentMember(
    Meta::TypeId targetType) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifestState::TypeRecord* type = state_->FindType(targetType);
    if (type == nullptr) return TypeNotFound();
    if (type->contentMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML schema manifest type has no content member");
    }
    const SchemaManifestState::MemberRecord* member = state_->FindMember(type->contentMember);
    if (member == nullptr || member->kind != ManifestMemberKind::Property) {
        return InvalidManifest("XAML schema manifest content member is invalid");
    }
    const Meta::PropertyFlags flags =
        static_cast<Meta::PropertyFlags>(member->flags);
    ResolvedMember resolved;
    resolved.id = member->id;
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = member->ownerType;
    resolved.valueType = member->valueType;
    resolved.propertyFlags = flags;
    resolved.attached = HasPropertyFlag(flags, Meta::PropertyFlags::Attached);
    return resolved;
}

} // namespace Aero::Markup


// ===== Schema =====



#include <Aero/FrameworkElement.hpp>

// Query surface is public; execution operations are reached by source-side
// friends and SchemaPrivate.

namespace Aero::Markup {

namespace {

Base::Status RuntimeSchemaNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML runtime schema requires a frozen descriptor runtime");
}

Base::Status RuntimeTypeNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML runtime type was not found");
}

Base::Status RuntimeMemberNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML runtime member was not found");
}

bool SchemaHasPropertyFlag(
    Meta::PropertyFlags value,
    Meta::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool SchemaHasEventFlag(
    Meta::EventFlags value,
    Meta::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr Base::StringView SchemaWpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
constexpr Base::StringView SchemaBehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
constexpr Base::StringView SchemaBlendInteractivityNamespace(
    "http://schemas.microsoft.com/expression/2010/interactivity");
constexpr Base::StringView SchemaSystemNamespacePrefix(
    "clr-namespace:System");

bool SchemaMatchesClrNamespacePrefix(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

bool SchemaIsExtensionsClrNamespace(Base::StringView value) noexcept {
    return SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:AeroGUIExtensions")) ||
        SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Aero.GUI.Extensions")) ||
        SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:NoesisGUIExtensions")) ||
        SchemaMatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Noesis.GUI.Extensions"));
}

Base::StringView SchemaCanonicalXamlNamespace(
    Base::StringView value) noexcept {
    return value == SchemaWpfPresentationNamespace ||
            value == SchemaBehaviorsNamespace ||
            value == SchemaBlendInteractivityNamespace ||
            SchemaIsExtensionsClrNamespace(value)
        ? Meta::AeroNamespaceUri()
        : value;
}

bool SchemaIsSystemNamespace(
    Base::StringView value) noexcept {
    return value.SizeBytes() >=
            SchemaSystemNamespacePrefix.SizeBytes() &&
        value.Substr(0U, SchemaSystemNamespacePrefix.SizeBytes()) ==
            SchemaSystemNamespacePrefix;
}

Base::StringView SchemaCanonicalXamlTypeName(
    Base::StringView value) noexcept {
    if (value == Base::StringView("Geometry")) {
        return Base::StringView("StreamGeometry");
    }
    if (value == Base::StringView("VisualStateTransition")) {
        return Base::StringView("VisualTransition");
    }
    if (value == Base::StringView("MonochromeBrush")) {
        return Base::StringView("MonochromeShader");
    }
    if (value == Base::StringView("ConicGradientBrush")) {
        return Base::StringView("ConicGradientShader");
    }
    if (value == Base::StringView("WavesBrush")) {
        return Base::StringView("WavesShader");
    }
    return value;
}

bool SchemaIsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    return SchemaIsExtensionsClrNamespace(xamlNamespace) &&
        (ownerName == Base::StringView("Text") ||
         ownerName == Base::StringView("Path") ||
         ownerName == Base::StringView("Brush") ||
          ownerName == Base::StringView("Element") ||
          ownerName == Base::StringView("RichText"));
}

} // namespace

Schema::Schema(
    ::Aero::Meta::Registry& metadata,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      domain_(&metadata) {
    AERO_ASSERT(metadata.IsSealed());
    state_ = new (stateStorage_) SchemaState();
}

Schema::~Schema() noexcept {
    if (state_ == nullptr) return;
    state_->~SchemaState();
    state_ = nullptr;
}

Base::Result<const Meta::TypeInfo*> Schema::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::TypeInfo* descriptor =
        domain_->Types().FindType(
            SchemaIsSystemNamespace(xamlNamespace)
                ? Meta::AeroNamespaceUri()
                : SchemaCanonicalXamlNamespace(xamlNamespace),
            SchemaCanonicalXamlTypeName(localName));
    if (descriptor == nullptr) return RuntimeTypeNotFound();
    return descriptor;
}

Base::Result<ResolvedMember> Schema::ResolveMember(
    Meta::TypeId targetType,
    const QualifiedName& name,
    MemberSyntax syntax) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::TypeInfo* target =
        domain_->Types().FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) {
        return RuntimeMemberNotFound();
    }

    const Base::StringView localName = name.LocalName();
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML runtime member contains multiple owner separators");
        }
        dot = index;
    }

    if (dot == localName.SizeBytes()) {
        if (!name.NamespaceUri().Empty() &&
            SchemaCanonicalXamlNamespace(name.NamespaceUri()) !=
                target->XamlNamespace()) {
            return RuntimeMemberNotFound();
        }
        Base::Result<ResolvedMember> resolved =
            ResolvePropertyOrEvent(
            targetType, targetType, localName, syntax, false);
        if (resolved || localName != Base::StringView("ToolTip")) {
            return resolved;
        }

        // WPF exposes ToolTip as a FrameworkElement property even though the
        // storage and display policy live in ToolTipService. Retain that XAML
        // surface while keeping the existing shared service implementation.
        const Meta::TypeInfo* service =
            domain_->Types().FindType(
                Meta::AeroNamespaceUri(), "ToolTipService");
        if (service == nullptr) return resolved.GetStatus();
        return ResolvePropertyOrEvent(
            targetType, service->Id(), localName, syntax, false);
    }

    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML runtime member owner syntax is invalid");
    }

    const Base::StringView ownerName = localName.Substr(0U, dot);
    const Base::StringView memberName = localName.Substr(
        dot + 1U, localName.SizeBytes() - dot - 1U);
    const Base::StringView ownerNamespace = name.NamespaceUri().Empty()
        ? target->XamlNamespace() : name.NamespaceUri();
    if (SchemaIsAeroExtensionsFacade(
            ownerNamespace, ownerName)) {
        // Keep the runtime schema in lockstep with the compiled manifest:
        // Element is a real extension owner, but the legacy Gallery alias
        // Element.BlendingMode targets UIElement.BlendMode.
        if (ownerName == Base::StringView("Element") &&
            memberName == Base::StringView("BlendingMode")) {
            return ResolvePropertyOrEvent(
                targetType,
                targetType,
                Base::StringView("BlendMode"),
                syntax,
                false);
        }
        const Meta::TypeInfo* aeroOwner = domain_->Types().FindType(
            Meta::AeroNamespaceUri(),
            SchemaCanonicalXamlTypeName(ownerName));
        if (aeroOwner != nullptr) {
            return ResolvePropertyOrEvent(
                targetType, aeroOwner->Id(), memberName, syntax, true);
        }
        return ResolvePropertyOrEvent(
            targetType,
            targetType,
            memberName,
            syntax,
            false);
    }
    const Meta::TypeInfo* owner =
        domain_->Types().FindType(
            SchemaCanonicalXamlNamespace(ownerNamespace),
            SchemaCanonicalXamlTypeName(ownerName));
    if (owner == nullptr && name.NamespaceUri().Empty()) {
        owner = domain_->Types().FindType(
            Meta::AeroNamespaceUri(),
            SchemaCanonicalXamlTypeName(ownerName));
    }
    if (owner == nullptr) return RuntimeMemberNotFound();
    if (memberName == Base::StringView("ContextMenu")) {
        const Meta::TypeInfo* service = domain_->Types().FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return ResolvePropertyOrEvent(
                targetType, service->Id(), memberName, syntax, true);
        }
    }
    return ResolvePropertyOrEvent(
        targetType, owner->Id(), memberName, syntax, true);
}

Base::Result<ResolvedMember> Schema::ResolveMember(
    Meta::TypeId targetType,
    Meta::MemberId memberId) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }
    if (targetType == Meta::InvalidTypeId ||
        memberId == Meta::InvalidMemberId) {
        return RuntimeMemberNotFound();
    }

    const Meta::TypeRegistry& descriptors = domain_->Types();
    if (descriptors.FindType(targetType) == nullptr) {
        return RuntimeMemberNotFound();
    }
    if (const Meta::PropertyInfo* property =
            descriptors.FindProperty(memberId)) {
        const bool attached = SchemaHasPropertyFlag(
            property->Flags(), Meta::PropertyFlags::Attached);
        if (!attached && !descriptors.IsDerivedFrom(
                targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AXB2 member owner is incompatible with the target type");
        }
        ResolvedMember resolved;
        resolved.id = property->Id();
        resolved.kind = Meta::MemberKind::Property;
        resolved.ownerType = property->OwnerType();
        resolved.valueType = property->ValueType();
        resolved.propertyFlags = property->Flags();
        resolved.attached = attached;
        return resolved;
    }
    if (const Meta::EventInfo* event =
            descriptors.FindEvent(memberId)) {
        const bool attached = SchemaHasEventFlag(
            event->Flags(), Meta::EventFlags::Attached);
        if (!attached && !descriptors.IsDerivedFrom(
                targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AXB2 event owner is incompatible with the target type");
        }
        ResolvedMember resolved;
        resolved.id = event->Id();
        resolved.kind = Meta::MemberKind::Event;
        resolved.ownerType = event->OwnerType();
        resolved.valueType = event->EventArgsType();
        resolved.eventFlags = event->Flags();
        resolved.attached = attached;
        return resolved;
    }
    return RuntimeMemberNotFound();
}

Base::Result<ResolvedMember>
Schema::ResolvePropertyOrEvent(
    Meta::TypeId targetType,
    Meta::TypeId ownerType,
    Base::StringView memberName,
    MemberSyntax syntax,
    bool ownerWasExplicit) const noexcept {
    const Meta::TypeRegistry& descriptors = domain_->Types();
    const Meta::PropertyInfo* property =
        descriptors.FindProperty(ownerType, memberName, true);
    if (property != nullptr &&
        syntax == MemberSyntax::Attribute &&
        SchemaHasPropertyFlag(
            property->Flags(),
            Meta::PropertyFlags::Collection)) {
        Base::String textAlias;
        Base::Result<void> aliasStatus =
            textAlias.Assign(memberName);
        if (aliasStatus) {
            aliasStatus = textAlias.Append("Text");
        }
        if (!aliasStatus) return aliasStatus.GetStatus();

        const Meta::PropertyInfo* alias =
            descriptors.FindProperty(
                ownerType, textAlias.View(), true);
        if (alias != nullptr &&
            !SchemaHasPropertyFlag(
                alias->Flags(),
                Meta::PropertyFlags::Collection)) {
            property = alias;
        }
    }
    if (property != nullptr) {
        const bool attached = SchemaHasPropertyFlag(
            property->Flags(), Meta::PropertyFlags::Attached);
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute &&
            !attached &&
            !descriptors.IsDerivedFrom(targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached property");
        }
        if (ownerWasExplicit && syntax == MemberSyntax::PropertyElement &&
            !attached &&
            !descriptors.IsDerivedFrom(targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML property element owner is incompatible with the target type");
        }

        ResolvedMember resolved;
        resolved.id = property->Id();
        resolved.kind = Meta::MemberKind::Property;
        resolved.ownerType = property->OwnerType();
        resolved.valueType = property->ValueType();
        resolved.propertyFlags = property->Flags();
        resolved.attached = attached;
        return resolved;
    }

    const Meta::EventInfo* event =
        descriptors.FindEvent(ownerType, memberName, true);
    if (event != nullptr) {
        const bool attached = SchemaHasEventFlag(
            event->Flags(), Meta::EventFlags::Attached);
        const bool routed = SchemaHasEventFlag(
            event->Flags(), Meta::EventFlags::Routed);
        if (ownerWasExplicit && syntax == MemberSyntax::Attribute &&
            !attached && !routed) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Explicit XAML attribute owner requires an attached event");
        }
        if (ownerWasExplicit && syntax == MemberSyntax::PropertyElement &&
            !attached &&
            !descriptors.IsDerivedFrom(targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event element owner is incompatible with the target type");
        }

        ResolvedMember resolved;
        resolved.id = event->Id();
        resolved.kind = Meta::MemberKind::Event;
        resolved.ownerType = event->OwnerType();
        resolved.valueType = event->EventArgsType();
        resolved.eventFlags = event->Flags();
        resolved.attached = attached ||
            (ownerWasExplicit &&
             syntax == MemberSyntax::Attribute && routed);
        return resolved;
    }

    return RuntimeMemberNotFound();
}

Base::Result<ResolvedMember> Schema::ResolveContentMember(
    Meta::TypeId targetType) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }

    const Meta::MemberId contentMember =
        domain_->FindContentMember(targetType);
    if (contentMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML runtime type has no content facet");
    }
    const Meta::PropertyInfo* property =
        domain_->Types().FindProperty(contentMember);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Content facet references a missing property descriptor");
    }

    ResolvedMember resolved;
    resolved.id = property->Id();
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = property->OwnerType();
    resolved.valueType = property->ValueType();
    resolved.propertyFlags = property->Flags();
    resolved.attached = SchemaHasPropertyFlag(
        property->Flags(), Meta::PropertyFlags::Attached);
    return resolved;
}

Base::Result<Base::Ref<Base::Object>> Schema::CreateObject(
    Meta::TypeId type) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }
    return domain_->CreateObject(type);
}

Base::Result<::Aero::DependencyObject*>
Schema::ResolvePropertyTarget(
    Base::Object& object) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return RuntimeSchemaNotReady();
    }
    ::Aero::DependencyObject* target = nullptr;
    if (domain_->Types().IsAssignableFrom(
            Meta::TypeOf<::Aero::DependencyObject>(),
            object.RuntimeType())) {
        target = static_cast<::Aero::DependencyObject*>(&object);
    } else {
        const XamlPropertyTargetFacet* facet =
            state_->facets.FindPropertyTarget(
                object.RuntimeType(), domain_->Types());
        if (facet != nullptr && facet->resolve != nullptr) {
            target = facet->resolve(object, facet->context);
        }
    }
    if (target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML target does not support dependency properties");
    }
    if (&target->PropertyRegistry() !=
        &static_cast<const ::Aero::Meta::Registry&>(
            *domain_).DependencyProperties()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML target belongs to a different metadata domain");
    }
    return target;
}

Base::Result<void> Schema::SetMember(
    Base::Object& object,
    Meta::TypeId objectType,
    const ResolvedMember& member,
    const Meta::Value& value) const noexcept {
    if (!frozen_ || domain_ == nullptr ||
        !domain_->IsReady() || !member.IsValid()) {
        return RuntimeSchemaNotReady();
    }
    if (member.kind != Meta::MemberKind::Property) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML runtime event assignment requires an event adapter");
    }
    if (!member.attached &&
        !domain_->Types().IsDerivedFrom(objectType, member.ownerType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML runtime member owner is incompatible with the target object");
    }

    const bool runtimeWritable =
        domain_->CanWriteProperty(member.id);
    Base::Result<Meta::ContentInfo> content =
        !runtimeWritable
        ? domain_->GetContentInfo(member.id)
        : Base::Result<Meta::ContentInfo>(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Runtime content metadata was not requested"));
    const bool runtimeContentWritable = content &&
        content.Value().writable;
    if (!runtimeWritable && !runtimeContentWritable) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML runtime member has no writable facet or adapter");
    }

    Meta::Value convertedValue = value;
    const bool metadataAcceptsAnyValue =
        (static_cast<std::uint32_t>(
             member.propertyFlags) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::AnyValue)) != 0U;
    // Meta::Value is the metadata representation of WPF's object-valued
    // member and must retain the concrete type supplied by markup (enums,
    // scalars, objects, or null), even when an older descriptor omitted the
    // redundant AnyValue flag.
    const bool acceptsAnyValue = metadataAcceptsAnyValue ||
        member.valueType == Meta::TypeOf<Meta::Value>();
    if (!acceptsAnyValue) {
        if (member.id == VisualStateManager::VisualStateGroupsProperty.Handle().value &&
            convertedValue.Type() == VisualStateGroup::StaticTypeId() &&
            convertedValue.AsObject()) {
            auto& target = static_cast<::Aero::DependencyObject&>(object);
            Base::Ref<VisualStateGroupCollection> valueStore = target.GetValueOr(
                VisualStateManager::VisualStateGroupsProperty,
                Base::Ref<VisualStateGroupCollection>{});
            if (!valueStore) {
                Base::Result<Base::Ref<VisualStateGroupCollection>> created =
                    Base::MakeRef<VisualStateGroupCollection>();
                if (!created) return created.GetStatus();
                valueStore = std::move(created).Value();
                target.SetValue(
                    VisualStateManager::VisualStateGroupsProperty,
                    valueStore);
            }
            (void)valueStore->Add(
                Base::Ref<VisualStateGroup>::FromBorrowed(
                    *static_cast<VisualStateGroup*>(convertedValue.AsObject().Get())));
            return {};
        }

        bool compatible = convertedValue.Type() == member.valueType;
        if (convertedValue.Kind() == Meta::ValueKind::Object &&
            convertedValue.AsObject()) {
            compatible = domain_->Types().IsDerivedFrom(
                convertedValue.Type(), member.valueType);
        }
        if (!compatible) {
            const Meta::TypeInfo* owner =
                domain_->Types().FindType(member.ownerType);
            const Meta::TypeInfo* expected =
                domain_->Types().FindType(member.valueType);
            const Meta::TypeInfo* actual =
                domain_->Types().FindType(convertedValue.Type());
            thread_local char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "XAML member on '%.*s' expects '%.*s' but received '%.*s'",
                owner != nullptr
                    ? static_cast<int>(owner->Name().SizeBytes()) : 9,
                owner != nullptr ? owner->Name().Data() : "<unknown>",
                expected != nullptr
                    ? static_cast<int>(expected->Name().SizeBytes()) : 9,
                expected != nullptr ? expected->Name().Data() : "<unknown>",
                actual != nullptr
                    ? static_cast<int>(actual->Name().SizeBytes()) : 9,
                actual != nullptr ? actual->Name().Data() : "<unknown>");
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                message);
        }
    }

    if (runtimeWritable) {
        return domain_->SetProperty(
            object, member.id, convertedValue);
    }
    if (runtimeContentWritable) {
        if (convertedValue.Kind() != Meta::ValueKind::Object ||
            convertedValue.IsNullObject() || !convertedValue.AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML content member requires a non-null object value");
        }
        return domain_->WriteContent(
            object, member.id, convertedValue.AsObject());
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "XAML runtime member is not writable");
}

MemberWritePolicy Schema::ResolveMemberWritePolicy(
    const ResolvedMember& member) const noexcept {
    if (domain_ == nullptr || !domain_->IsReady()) return {};
    // WPF attached VisualStateManager.VisualStateGroups is a collection of
    // VisualStateGroup children. The property type is VisualStateGroupCollection,
    // but markup writes one VisualStateGroup at a time.
    if (member.id ==
        VisualStateManager::VisualStateGroupsProperty.Handle().value) {
        return {MemberWriteMode::Collection, false, true};
    }
    if (domain_->CanWriteProperty(member.id)) {
        Base::Result<Meta::ContentInfo> content =
            domain_->GetContentInfo(member.id);
        const bool acceptsAnyValue =
            (static_cast<std::uint32_t>(
                 member.propertyFlags) &
             static_cast<std::uint32_t>(
                 Meta::PropertyFlags::AnyValue)) != 0U ||
            member.valueType == Meta::TypeOf<Meta::Value>();
        return {
            content && content.Value().writable &&
                    content.Value().kind ==
                        Meta::ContentKind::Collection
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce,
            acceptsAnyValue,
            true};
    }
    Base::Result<Meta::ContentInfo> content =
        domain_->GetContentInfo(member.id);
    if (content && content.Value().writable) {
        return {
            content.Value().kind == Meta::ContentKind::Collection
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce,
            false,
            true};
    }
    return {};
}

} // namespace Aero::Markup

// XAML object construction and lifecycle operations.


#include <Aero/Value.hpp>


namespace Aero::Markup {

namespace {

constexpr const char* MessageSchemaNotFrozen =
    "XAML schema context must be frozen before use";
constexpr const char* MessageSchemaAlreadyFrozen =
    "XAML schema context is frozen";
constexpr const char* MessageInvalidMarkupExtension =
    "XAML markup-extension registration requires a flagged type and provider";
constexpr const char* MessageMissingMarkupExtension =
    "XAML markup-extension type has no registered value provider";

bool SchemaHasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<Meta::TypeReference> ResolveTypeReference(
    Base::StringView name,
    const ExtensionServices& services) noexcept {
    if (services.schema == nullptr || name.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type reference requires a qualified type name");
    }
    std::uint32_t colon = name.SizeBytes();
    for (std::uint32_t index = 0U;
         index < name.SizeBytes();
         ++index) {
        if (name[index] != ':') continue;
        if (colon != name.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Type reference contains multiple namespace prefixes");
        }
        colon = index;
    }
    Base::StringView prefix;
    Base::StringView localName = name;
    if (colon != name.SizeBytes()) {
        if (colon == 0U ||
            colon + 1U >= name.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Type reference namespace prefix is malformed");
        }
        prefix = name.Substr(0U, colon);
        localName = name.Substr(
            colon + 1U,
            name.SizeBytes() - colon - 1U);
    }
    Base::Result<Base::StringView> uri =
        services.namespaces.Lookup(prefix);
    if (!uri) return uri.GetStatus();
    Base::Result<const Meta::TypeInfo*> resolved =
        services.schema->ResolveType(
            uri.Value(), localName);
    if (!resolved) return resolved.GetStatus();
    const Meta::TypeInfo* type = resolved.Value();
    if (type == nullptr ||
        SchemaHasTypeFlag(
            type->Flags(),
            Meta::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Type reference target was not found or is not an object type");
    }
    return Meta::TypeReference{type->Id()};
}

} // namespace

Base::Result<void> Schema::Freeze() noexcept {
    if (frozen_) return {};
    if (domain_ == nullptr || domain_ == nullptr ||
        !domain_->IsSealed() || !domain_->IsReady()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Meta::Registry must be complete before XAML schema freeze");
    }
    Base::Result<void> facetsFrozen =
        state_->facets.Freeze(domain_->Types());
    if (!facetsFrozen) return facetsFrozen.GetStatus();
    frozen_ = true;
    return {};
}

Base::Result<Meta::Value> Schema::ConvertText(
    Meta::TypeId type,
    Base::StringView text,
    const ExtensionServices* services) const noexcept {
    if (!frozen_ || domain_ == nullptr || !domain_->IsReady()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    if (type == Meta::TypeOf<Meta::TypeReference>()) {
        if (services == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Type-reference conversion requires markup services");
        }
        Base::Result<Meta::TypeReference> reference =
            ResolveTypeReference(text, *services);
        return reference
            ? Meta::ValueCodec<Meta::TypeReference>::Encode(
                  reference.Value())
            : Base::Result<Meta::Value>(
                  reference.GetStatus());
    }
    // Members flagged AnyValue still need a concrete runtime value. Preserve
    // literal XAML text as a String so style and template finalizers can
    // convert it after resolving the actual target dependency property.
    if (type == Meta::TypeOf<Meta::Value>()) {
        return Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(), text);
    }
    const bool fontFamilyValue =
        type == Meta::TypeOf<Media::FontFamily>();
    const bool fontFamilySource =
        type == Meta::TypeOf<Base::String>() &&
        services != nullptr &&
        services->targetObjectType ==
            Media::FontFamily::StaticTypeId() &&
        services->targetMember == Meta::MakeMemberId(
            Media::FontFamily::StaticTypeId(),
            Meta::MemberKind::Property,
            "Source");
    if ((fontFamilyValue || fontFamilySource) &&
        services != nullptr &&
        services->baseUri != nullptr &&
        !services->baseUri->Empty()) {
        std::uint32_t familySeparator = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < text.SizeBytes(); ++index) {
            if (text[index] == '#') {
                familySeparator = index;
                break;
            }
        }
        if (familySeparator != UINT32_MAX && familySeparator != 0U) {
            Base::Result<Base::ResourceUri> uri =
                Base::ResourceUri::Resolve(
                    *services->baseUri,
                    text);
            if (!uri) return uri.GetStatus();
            const Base::StringView resolved =
                uri.Value().Scheme() == Base::StringView("file")
                ? uri.Value().Path()
                : uri.Value().Canonical();
            return domain_->TryConvertText(type, resolved);
        }
    }
    if (type == Meta::TypeOf<Base::ResourceUri>() &&
        services != nullptr &&
        services->baseUri != nullptr &&
        !services->baseUri->Empty()) {
        // A leading-slash WPF component URI names an assembly resource; it
        // must not inherit the file scheme of the containing XAML document.
        bool componentUri = false;
        if (!text.Empty() && text[0] == '/') {
            for (std::uint32_t index = 1U;
                 index + 11U <= text.SizeBytes(); ++index) {
                if (text.Substr(index, 11U) ==
                        Base::StringView(";component/")) {
                    componentUri = true;
                    break;
                }
            }
        }
        Base::Result<Base::ResourceUri> uri = componentUri
            ? Base::ResourceUri::Parse(text)
            : Base::ResourceUri::Resolve(
                  *services->baseUri,
                  text);
        if (!uri) return uri.GetStatus();
        return domain_->TryCreateValue(
            type,
            &uri.Value());
    }
    Base::Result<Meta::Value> converted =
        domain_->TryConvertText(type, text);
    if (!converted || services == nullptr ||
        services->baseUri == nullptr || services->baseUri->Empty() ||
        converted.Value().Kind() != Meta::ValueKind::Object ||
        converted.Value().IsNullObject()) {
        return converted;
    }
    const Base::Ref<Base::Object> object =
        converted.Value().AsObject();
    if (!object || object->RuntimeType() !=
            Media::BitmapImage::StaticTypeId()) {
        return converted;
    }
    auto& bitmap = static_cast<Media::BitmapImage&>(*object);
    const Base::ResourceUri authored = bitmap.GetUriSource();
    if (authored.Empty() || authored.IsAbsolute()) {
        return converted;
    }
    Base::Result<Base::ResourceUri> resolved =
        Base::ResourceUri::Resolve(*services->baseUri, authored.Canonical());
    if (!resolved) return resolved.GetStatus();
    bitmap.SetUriSource(std::move(resolved).Value());
    return converted;
}

Base::Result<ProvidedValue> Schema::ProvideMarkupExtensionValue(
    Meta::TypeId type,
    Base::StringView arguments,
    const ExtensionServices& services) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Meta::TypeInfo* info =
        domain_->Types().FindType(type);
    const XamlMarkupExtensionFacet* registration =
        state_->facets.FindMarkupExtension(type);
    const bool isMarkupExtension =
        info != nullptr &&
        (SchemaHasTypeFlag(info->Flags(), Meta::TypeFlags::MarkupExtension) ||
         domain_->Types().IsDerivedFrom(
             type, MarkupExtension::StaticTypeId()));
    if (!isMarkupExtension) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingMarkupExtension);
    }
    if (registration != nullptr && registration->provideValue != nullptr) {
        return registration->provideValue(
            arguments,
            services,
            registration->context);
    }
    Base::Result<Base::Ref<Base::Object>> created = CreateObject(type);
    if (!created) return created.GetStatus();
    MarkupExtension* extension =
        ::Aero::TryCast<MarkupExtension>(created.Value().Get());
    if (extension == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingMarkupExtension);
    }
    (void)arguments;
    (void)services;
    Base::Result<Meta::Value> provided = extension->ProvideValue();
    if (!provided) return provided.GetStatus();
    return ProvidedValue::FromValue(std::move(provided).Value());
}

Base::Result<void> Schema::BeginInit(
    Meta::TypeId type,
    Base::Object& object) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        state_->facets.LifecyclePlan(type);
    for (std::uint32_t reference : lifecycle) {
        const XamlLifecycleFacet* facet =
            state_->facets.LifecycleAt(reference);
        if (facet == nullptr || facet->beginInit == nullptr) continue;
        Base::Result<void> initialized =
            facet->beginInit(object, facet->context);
        if (!initialized) return initialized.GetStatus();
    }
    return {};
}

Base::Result<void> Schema::EndInit(
    Meta::TypeId type,
    Base::Object& object,
    const ExtensionServices& services) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        state_->facets.LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            state_->facets.LifecycleAt(lifecycle[index - 1U]);
        if (facet == nullptr) continue;
        Base::Result<void> initialized;
        if (facet->endInitWithServices != nullptr) {
            initialized = facet->endInitWithServices(
                object, services, facet->context);
        } else if (facet->endInit != nullptr) {
            initialized = facet->endInit(
                object, facet->context);
        }
        if (!initialized) return initialized.GetStatus();
    }
    return {};
}

void Schema::AbortInit(
    Meta::TypeId type,
    Base::Object& object) const noexcept {
    const Base::Span<const std::uint32_t> lifecycle =
        state_->facets.LifecyclePlan(type);
    for (std::uint32_t index = lifecycle.Size();
         index > 0U;
         --index) {
        const XamlLifecycleFacet* facet =
            state_->facets.LifecycleAt(lifecycle[index - 1U]);
        if (facet != nullptr && facet->abortInit != nullptr) {
            facet->abortInit(object, facet->context);
        }
    }
}

bool Schema::CreatesNameScope(Meta::TypeId type) const noexcept {
    const XamlNameScopeFacet* facet = state_->facets.FindNameScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsNameScope;
}

bool Schema::CreatesResourceScope(
    Meta::TypeId type) const noexcept {
    const XamlResourceScopeFacet* facet = state_->facets.FindResourceScope(
        type, domain_->Types());
    return facet != nullptr && facet->createsResourceScope;
}

bool Schema::DefersVisualContent(
    Meta::TypeId type) const noexcept {
    const XamlDeferredContentFacet* facet =
        state_->facets.FindDeferredContent(
        type, domain_->Types());
    return facet != nullptr && facet->defersVisualContent;
}

Base::Result<void> Schema::RegisterName(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object) const noexcept {
    const XamlNameScopeFacet* facet = state_->facets.FindNameScope(
        scopeType, domain_->Types());
    if (facet == nullptr || facet->registerName == nullptr) return {};
    return facet->registerName(
        scopeOwner, name, object, facet->context);
}

Base::Result<void> Schema::AddResource(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner,
    const Aero::ResourceKey& key,
    const Meta::Value& value) const noexcept {
    const XamlResourceScopeFacet* facet = state_->facets.FindResourceScope(
        scopeType, domain_->Types());
    if (facet == nullptr) return {};
    if (facet->addResource != nullptr) {
        return facet->addResource(
            scopeOwner, key, value, facet->context);
    }
    Aero::ResourceDictionary* resources =
        facet->resolveResourceScope != nullptr
        ? facet->resolveResourceScope(
              scopeOwner,
              facet->context)
        : nullptr;
    return resources != nullptr
        ? resources->Add(key, value)
        : Base::Result<void>();
}

Aero::ResourceDictionary* Schema::ResolveResourceScope(
    Meta::TypeId scopeType,
    Base::Object& scopeOwner) const noexcept {
    const XamlResourceScopeFacet* facet = state_->facets.FindResourceScope(
        scopeType, domain_->Types());
    return facet != nullptr && facet->resolveResourceScope != nullptr
        ? facet->resolveResourceScope(scopeOwner, facet->context)
        : nullptr;
}

Base::Result<Aero::ResourceKey>
Schema::ResolveImplicitResourceKey(
    Meta::TypeId type,
    const Base::Object& object) const noexcept {
    const XamlImplicitResourceKeyFacet* facet =
        state_->facets.FindImplicitResourceKey(
            type, domain_->Types());
    if (facet == nullptr || facet->resolve == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML type has no implicit resource-key facet");
    }
    return facet->resolve(object, facet->context);
}

} // namespace Aero::Markup
