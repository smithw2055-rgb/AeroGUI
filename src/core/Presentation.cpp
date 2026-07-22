#include <Aero/Core/Presentation.hpp>

#include <Aero/Core/Controls.hpp>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace Aero::Core {
namespace {

constexpr Base::StringView PresentationNamespace("urn:aero");
constexpr double DefaultMaximum = 1.0e12;

bool EqualsAsciiInsensitive(Base::StringView value, const char* literal) noexcept {
    std::uint32_t size = 0U;
    while (literal[size] != '\0') ++size;
    if (value.SizeBytes() != size) return false;
    for (std::uint32_t index = 0U; index < size; ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            std::tolower(static_cast<unsigned char>(literal[index]))) return false;
    }
    return true;
}

Base::StringView Trim(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1U]))) --end;
    return value.Substr(begin, end - begin);
}

Base::Result<double> ParseDouble(
    Base::StringView text, Base::IAllocator& allocator) noexcept {
    Base::String buffer(&allocator);
    Base::Result<void> assigned = buffer.TryAssign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    const double value = std::strtod(buffer.CStr(), &end);
    if (end == buffer.CStr() || *end != '\0' || !std::isfinite(value)) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Text is not a finite number");
    }
    return value;
}

Base::Result<Value> ConvertBoolean(TypeId type, Base::StringView text,
    Base::IAllocator&, void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "true")) return Value::FromBoolean(type, true);
    if (EqualsAsciiInsensitive(value, "false")) return Value::FromBoolean(type, false);
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Boolean text must be true or false");
}

Base::Result<Value> ConvertUnsigned(TypeId type, Base::StringView text,
    Base::IAllocator& allocator, void*) noexcept {
    Base::String buffer(&allocator);
    Base::Result<void> assigned = buffer.TryAssign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(buffer.CStr(), &end, 10);
    if (end == buffer.CStr() || *end != '\0' || errno == ERANGE ||
        value > static_cast<unsigned long long>(UINT32_MAX) ||
        (!buffer.Empty() && buffer.View()[0] == '-')) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Text is not an unsigned integer");
    }
    return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(value));
}

Base::Result<Value> ConvertDoubleValue(TypeId type, Base::StringView text,
    Base::IAllocator& allocator, void*) noexcept {
    Base::Result<double> value = ParseDouble(text, allocator);
    return value ? Base::Result<Value>(Value::FromDouble(type, value.Value()))
                 : Base::Result<Value>(value.GetStatus());
}

Base::Result<Value> ConvertString(TypeId type, Base::StringView text,
    Base::IAllocator& allocator, void*) noexcept {
    return Value::TryFromString(type, text, &allocator);
}

Base::Result<Value> ConvertLength(TypeId type, Base::StringView text,
    Base::IAllocator& allocator, void* context) noexcept {
    auto* types = static_cast<TypeRegistry*>(context);
    const Base::StringView value = Trim(text);
    Length length = Length::Auto();
    if (!EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed = ParseDouble(value, allocator);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Length must be Auto or a nonnegative number");
        }
        length = Length::Pixels(parsed.Value());
    }
    return types->TryCreateValue(type, &length, &allocator);
}

Base::Result<Thickness> ParseThickness(
    Base::StringView input, Base::IAllocator& allocator) noexcept {
    Base::String text(&allocator);
    Base::Result<void> assigned = text.TryAssign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[4]{};
    std::uint32_t count = 0U;
    bool valid = true;
    while (*cursor != '\0') {
        while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (*cursor == '\0') break;
        if (count == 4U) {
            valid = false;
            break;
        }
        char* end = nullptr;
        values[count] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[count])) {
            valid = false;
            break;
        }
        ++count;
        cursor = end;
        const char* whitespace = cursor;
        while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (*cursor == '\0') break;
        if (*cursor == ',') {
            ++cursor;
            while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
            if (*cursor == '\0') {
                valid = false;
                break;
            }
        } else if (cursor == whitespace) {
            valid = false;
            break;
        }
    }
    Thickness result;
    if (!valid) return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness contains invalid text");
    if (count == 1U) result = {values[0], values[0], values[0], values[0]};
    else if (count == 2U) result = {values[0], values[1], values[0], values[1]};
    else if (count == 4U) result = {values[0], values[1], values[2], values[3]};
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness accepts one, two, or four numbers");
    return result;
}

Base::Result<Value> ConvertThickness(TypeId type, Base::StringView text,
    Base::IAllocator& allocator, void* context) noexcept {
    Base::Result<Thickness> parsed = ParseThickness(text, allocator);
    if (!parsed) return parsed.GetStatus();
    return static_cast<TypeRegistry*>(context)->TryCreateValue(
        type, &parsed.Value(), &allocator);
}

int Hex(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Base::Result<Value> ConvertColor(TypeId type, Base::StringView text,
    Base::IAllocator& allocator, void* context) noexcept {
    const Base::StringView value = Trim(text);
    if ((value.SizeBytes() != 7U && value.SizeBytes() != 9U) || value[0] != '#') {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Color requires #RRGGBB or #AARRGGBB");
    }
    std::uint8_t bytes[4]{255U, 0U, 0U, 0U};
    const std::uint32_t count = value.SizeBytes() == 9U ? 4U : 3U;
    for (std::uint32_t index = 0U; index < count; ++index) {
        const int high = Hex(value[1U + index * 2U]);
        const int low = Hex(value[2U + index * 2U]);
        if (high < 0 || low < 0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Color contains a non-hex digit");
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    Color color = count == 3U
        ? Color{bytes[0] / 255.0F, bytes[1] / 255.0F, bytes[2] / 255.0F, 1.0F}
        : Color{bytes[1] / 255.0F, bytes[2] / 255.0F,
            bytes[3] / 255.0F, bytes[0] / 255.0F};
    return static_cast<TypeRegistry*>(context)->TryCreateValue(
        type, &color, &allocator);
}

bool EqualLength(const void* left, const void* right, void*) noexcept {
    const Length& a = *static_cast<const Length*>(left);
    const Length& b = *static_cast<const Length*>(right);
    return a.isAuto == b.isAuto && (a.isAuto || a.value == b.value);
}
bool EqualThickness(const void* left, const void* right, void*) noexcept {
    const Thickness& a = *static_cast<const Thickness*>(left);
    const Thickness& b = *static_cast<const Thickness*>(right);
    return a.left == b.left && a.top == b.top &&
        a.right == b.right && a.bottom == b.bottom;
}
bool EqualColor(const void* left, const void* right, void*) noexcept {
    const Color& a = *static_cast<const Color*>(left);
    const Color& b = *static_cast<const Color*>(right);
    return a.red == b.red && a.green == b.green &&
        a.blue == b.blue && a.alpha == b.alpha;
}

Base::Result<Value> ConvertHorizontal(TypeId type, Base::StringView text,
    Base::IAllocator&, void*) noexcept {
    const Base::StringView value = Trim(text);
    HorizontalAlignment result;
    if (EqualsAsciiInsensitive(value, "stretch")) result = HorizontalAlignment::Stretch;
    else if (EqualsAsciiInsensitive(value, "left")) result = HorizontalAlignment::Left;
    else if (EqualsAsciiInsensitive(value, "center")) result = HorizontalAlignment::Center;
    else if (EqualsAsciiInsensitive(value, "right")) result = HorizontalAlignment::Right;
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "HorizontalAlignment is invalid");
    return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(result));
}
Base::Result<Value> ConvertVertical(TypeId type, Base::StringView text,
    Base::IAllocator&, void*) noexcept {
    const Base::StringView value = Trim(text);
    VerticalAlignment result;
    if (EqualsAsciiInsensitive(value, "stretch")) result = VerticalAlignment::Stretch;
    else if (EqualsAsciiInsensitive(value, "top")) result = VerticalAlignment::Top;
    else if (EqualsAsciiInsensitive(value, "center")) result = VerticalAlignment::Center;
    else if (EqualsAsciiInsensitive(value, "bottom")) result = VerticalAlignment::Bottom;
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "VerticalAlignment is invalid");
    return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(result));
}
Base::Result<Value> ConvertOrientation(TypeId type, Base::StringView text,
    Base::IAllocator&, void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "horizontal"))
        return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(Orientation::Horizontal));
    if (EqualsAsciiInsensitive(value, "vertical"))
        return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(Orientation::Vertical));
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Orientation is invalid");
}

bool ValidateLength(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Length& length = *static_cast<const Length*>(value.AsCustom());
    return length.isAuto || (std::isfinite(length.value) && length.value >= 0.0);
}
bool ValidateNonnegativeDouble(const Value& value) noexcept {
    return value.Kind() == ValueKind::Double &&
        std::isfinite(value.AsDouble()) && value.AsDouble() >= 0.0;
}
bool ValidateFiniteDouble(const Value& value) noexcept {
    return value.Kind() == ValueKind::Double && std::isfinite(value.AsDouble());
}
bool ValidateThicknessValue(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Thickness& t = *static_cast<const Thickness*>(value.AsCustom());
    return IsFinite(t) && t.left >= 0.0 && t.top >= 0.0 &&
        t.right >= 0.0 && t.bottom >= 0.0;
}
bool ValidateColorValue(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Color& color = *static_cast<const Color*>(value.AsCustom());
    return std::isfinite(color.red) && std::isfinite(color.green) &&
        std::isfinite(color.blue) && std::isfinite(color.alpha) &&
        color.red >= 0.0F && color.red <= 1.0F &&
        color.green >= 0.0F && color.green <= 1.0F &&
        color.blue >= 0.0F && color.blue <= 1.0F &&
        color.alpha >= 0.0F && color.alpha <= 1.0F;
}
bool ValidateHorizontalValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(HorizontalAlignment::Right);
}
bool ValidateVerticalValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(VerticalAlignment::Bottom);
}
bool ValidateOrientationValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(Orientation::Vertical);
}
bool ValidateUInt32(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= std::numeric_limits<std::uint32_t>::max();
}

Base::Result<Value> CheckMinimum(DependencyObject& object,
    const Value& value, DependencyPropertyHandle maximum) noexcept {
    Base::Result<Value> other = object.GetValue(maximum);
    if (!other || value.AsDouble() > other.Value().AsDouble()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Minimum layout size exceeds maximum layout size");
    }
    return value;
}
Base::Result<Value> CheckMaximum(DependencyObject& object,
    const Value& value, DependencyPropertyHandle minimum) noexcept {
    Base::Result<Value> other = object.GetValue(minimum);
    if (!other || value.AsDouble() < other.Value().AsDouble()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Maximum layout size is below minimum layout size");
    }
    return value;
}
Base::Result<Value> CoerceMinWidth(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMinimum(o, v, LayoutElement::MaxWidthProperty()); }
Base::Result<Value> CoerceMaxWidth(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMaximum(o, v, LayoutElement::MinWidthProperty()); }
Base::Result<Value> CoerceMinHeight(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMinimum(o, v, LayoutElement::MaxHeightProperty()); }
Base::Result<Value> CoerceMaxHeight(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMaximum(o, v, LayoutElement::MinHeightProperty()); }

Base::Result<TypeId> RegisterType(TypeRegistry& types, Base::StringView name,
    TypeId base, TypeFlags flags = TypeFlags::None) noexcept {
    return types.TryRegisterType({PresentationNamespace, name, base, flags, nullptr});
}

Base::Result<DependencyPropertyHandle> RegisterProperty(
    DependencyPropertyRegistry& properties, Base::StringView name,
    TypeId owner, TypeId valueType, DependencyPropertyFlags propertyFlags,
    Value defaultValue, PropertyMetadataFlags metadataFlags,
    ValidateValueCallback validate = nullptr,
    CoerceValueCallback coerce = nullptr) noexcept {
    DependencyPropertyRegistration registration;
    registration.name = name;
    registration.ownerType = owner;
    registration.valueType = valueType;
    registration.flags = propertyFlags;
    registration.metadata.defaultValue = std::move(defaultValue);
    registration.metadata.flags = metadataFlags;
    registration.metadata.validate = validate;
    registration.metadata.coerce = coerce;
    Base::Result<DependencyPropertyRegistrationResult> result =
        properties.TryRegister(registration);
    return result ? Base::Result<DependencyPropertyHandle>(result.Value().property)
                  : Base::Result<DependencyPropertyHandle>(result.GetStatus());
}

} // namespace

Base::StringView AeroPresentationNamespaceUri() noexcept {
    return PresentationNamespace;
}

Base::Result<CorePresentationMetadata> TryRegisterCorePresentationMetadata(
    TypeRegistry& types,
    DependencyPropertyRegistry& properties) noexcept {
    CorePresentationMetadata m;
#define AERO_REGISTER_TYPE(field, name, base, flags) \
    do { auto r = RegisterType(types, Base::StringView(name), base, flags); \
         if (!r) return r.GetStatus(); m.field = r.Value(); } while (false)
    AERO_REGISTER_TYPE(objectType, "Object", InvalidTypeId, TypeFlags::None);
    AERO_REGISTER_TYPE(dependencyObjectType, "DependencyObject", m.objectType, TypeFlags::Abstract);
    AERO_REGISTER_TYPE(treeNodeType, "TreeNode", m.dependencyObjectType, TypeFlags::Abstract);
    AERO_REGISTER_TYPE(layoutElementType, "LayoutElement", m.treeNodeType, TypeFlags::Abstract);
    AERO_REGISTER_TYPE(renderElementType, "RenderElement", m.layoutElementType, TypeFlags::Abstract);
    AERO_REGISTER_TYPE(stackPanelType, "StackPanel", m.renderElementType, TypeFlags::None);
    AERO_REGISTER_TYPE(canvasType, "Canvas", m.renderElementType, TypeFlags::None);
    AERO_REGISTER_TYPE(gridType, "Grid", m.renderElementType, TypeFlags::None);
    AERO_REGISTER_TYPE(borderType, "Border", m.renderElementType, TypeFlags::None);
    AERO_REGISTER_TYPE(textBlockType, "TextBlock", m.renderElementType, TypeFlags::None);
    AERO_REGISTER_TYPE(contentPresenterType, "ContentPresenter", m.renderElementType, TypeFlags::None);
    AERO_REGISTER_TYPE(booleanType, "Boolean", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(unsignedIntegerType, "UInt32", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(doubleType, "Double", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(stringType, "String", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(lengthType, "Length", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(thicknessType, "Thickness", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(colorType, "Color", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(horizontalAlignmentType, "HorizontalAlignment", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(verticalAlignmentType, "VerticalAlignment", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(orientationType, "Orientation", InvalidTypeId, TypeFlags::ValueType);
#undef AERO_REGISTER_TYPE

    Base::Result<void> status = types.TryRegisterValueSemantics(m.lengthType,
        {sizeof(Length), alignof(Length), nullptr, nullptr, &EqualLength, nullptr, true});
    if (!status) return status.GetStatus();
    status = types.TryRegisterValueSemantics(m.thicknessType,
        {sizeof(Thickness), alignof(Thickness), nullptr, nullptr, &EqualThickness, nullptr, true});
    if (!status) return status.GetStatus();
    status = types.TryRegisterValueSemantics(m.colorType,
        {sizeof(Color), alignof(Color), nullptr, nullptr, &EqualColor, nullptr, true});
    if (!status) return status.GetStatus();

    const TextValueConverterRegistration converters[] = {
        {m.booleanType, &ConvertBoolean, nullptr},
        {m.unsignedIntegerType, &ConvertUnsigned, nullptr},
        {m.doubleType, &ConvertDoubleValue, nullptr},
        {m.stringType, &ConvertString, nullptr},
        {m.lengthType, &ConvertLength, &types},
        {m.thicknessType, &ConvertThickness, &types},
        {m.colorType, &ConvertColor, &types},
        {m.horizontalAlignmentType, &ConvertHorizontal, nullptr},
        {m.verticalAlignmentType, &ConvertVertical, nullptr},
        {m.orientationType, &ConvertOrientation, nullptr}
    };
    for (const TextValueConverterRegistration& converter : converters) {
        status = types.TryRegisterTextConverter(converter);
        if (!status) return status.GetStatus();
    }

    const Length autoLengthValue = Length::Auto();
    Base::Result<Value> autoLength = types.TryCreateValue(
        m.lengthType, &autoLengthValue);
    if (!autoLength) return autoLength.GetStatus();
    const Thickness zeroThickness{};
    Base::Result<Value> thickness = types.TryCreateValue(m.thicknessType, &zeroThickness);
    if (!thickness) return thickness.GetStatus();
    const Color transparent{};
    Base::Result<Value> transparentValue = types.TryCreateValue(m.colorType, &transparent);
    if (!transparentValue) return transparentValue.GetStatus();
    const Color black{0.0F, 0.0F, 0.0F, 1.0F};
    Base::Result<Value> blackValue = types.TryCreateValue(m.colorType, &black);
    if (!blackValue) return blackValue.GetStatus();
    Base::Result<Value> emptyString = Value::TryFromString(m.stringType, Base::StringView(), &types.Allocator());
    if (!emptyString) return emptyString.GetStatus();

#define AERO_REGISTER_DP(expr) do { auto r = (expr); if (!r) return r.GetStatus(); } while (false)
    const auto measure = PropertyMetadataFlags::AffectsMeasure;
    const auto arrange = PropertyMetadataFlags::AffectsArrange;
    const auto render = PropertyMetadataFlags::AffectsRender;
    AERO_REGISTER_DP(RegisterProperty(properties, "Width", m.layoutElementType, m.lengthType,
        DependencyPropertyFlags::None, autoLength.Value(), measure, &ValidateLength));
    AERO_REGISTER_DP(RegisterProperty(properties, "Height", m.layoutElementType, m.lengthType,
        DependencyPropertyFlags::None, autoLength.Value(), measure, &ValidateLength));
    AERO_REGISTER_DP(RegisterProperty(properties, "MinWidth", m.layoutElementType, m.doubleType,
        DependencyPropertyFlags::None, Value::FromDouble(m.doubleType, 0.0), measure,
        &ValidateNonnegativeDouble, &CoerceMinWidth));
    AERO_REGISTER_DP(RegisterProperty(properties, "MaxWidth", m.layoutElementType, m.doubleType,
        DependencyPropertyFlags::None, Value::FromDouble(m.doubleType, DefaultMaximum), measure,
        &ValidateNonnegativeDouble, &CoerceMaxWidth));
    AERO_REGISTER_DP(RegisterProperty(properties, "MinHeight", m.layoutElementType, m.doubleType,
        DependencyPropertyFlags::None, Value::FromDouble(m.doubleType, 0.0), measure,
        &ValidateNonnegativeDouble, &CoerceMinHeight));
    AERO_REGISTER_DP(RegisterProperty(properties, "MaxHeight", m.layoutElementType, m.doubleType,
        DependencyPropertyFlags::None, Value::FromDouble(m.doubleType, DefaultMaximum), measure,
        &ValidateNonnegativeDouble, &CoerceMaxHeight));
    AERO_REGISTER_DP(RegisterProperty(properties, "Margin", m.layoutElementType, m.thicknessType,
        DependencyPropertyFlags::None, thickness.Value(), measure, &ValidateThicknessValue));
    AERO_REGISTER_DP(RegisterProperty(properties, "HorizontalAlignment", m.layoutElementType,
        m.horizontalAlignmentType, DependencyPropertyFlags::None,
        Value::FromUnsignedInteger(m.horizontalAlignmentType, 0U), arrange, &ValidateHorizontalValue));
    AERO_REGISTER_DP(RegisterProperty(properties, "VerticalAlignment", m.layoutElementType,
        m.verticalAlignmentType, DependencyPropertyFlags::None,
        Value::FromUnsignedInteger(m.verticalAlignmentType, 0U), arrange, &ValidateVerticalValue));
    AERO_REGISTER_DP(RegisterProperty(properties, "ClipToBounds", m.layoutElementType, m.booleanType,
        DependencyPropertyFlags::None, Value::FromBoolean(m.booleanType, false), arrange));
    AERO_REGISTER_DP(RegisterProperty(properties, "IsHitTestVisible", m.layoutElementType, m.booleanType,
        DependencyPropertyFlags::None, Value::FromBoolean(m.booleanType, true), PropertyMetadataFlags::None));
    AERO_REGISTER_DP(RegisterProperty(properties, "UseLayoutRounding", m.layoutElementType, m.booleanType,
        DependencyPropertyFlags::None, Value::FromBoolean(m.booleanType, false), measure));
    AERO_REGISTER_DP(RegisterProperty(properties, "Orientation", m.stackPanelType, m.orientationType,
        DependencyPropertyFlags::None, Value::FromUnsignedInteger(m.orientationType, 1U), measure,
        &ValidateOrientationValue));
    AERO_REGISTER_DP(RegisterProperty(properties, "Left", m.canvasType, m.doubleType,
        DependencyPropertyFlags::Attached, Value::FromDouble(m.doubleType, 0.0),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateFiniteDouble));
    AERO_REGISTER_DP(RegisterProperty(properties, "Top", m.canvasType, m.doubleType,
        DependencyPropertyFlags::Attached, Value::FromDouble(m.doubleType, 0.0),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateFiniteDouble));
    AERO_REGISTER_DP(RegisterProperty(properties, "Row", m.gridType, m.unsignedIntegerType,
        DependencyPropertyFlags::Attached, Value::FromUnsignedInteger(m.unsignedIntegerType, 0U),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32));
    AERO_REGISTER_DP(RegisterProperty(properties, "Column", m.gridType, m.unsignedIntegerType,
        DependencyPropertyFlags::Attached, Value::FromUnsignedInteger(m.unsignedIntegerType, 0U),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32));
    AERO_REGISTER_DP(RegisterProperty(properties, "Background", m.borderType, m.colorType,
        DependencyPropertyFlags::None, transparentValue.Value(), render, &ValidateColorValue));
    AERO_REGISTER_DP(RegisterProperty(properties, "BorderBrush", m.borderType, m.colorType,
        DependencyPropertyFlags::None, transparentValue.Value(), render, &ValidateColorValue));
    AERO_REGISTER_DP(RegisterProperty(properties, "BorderThickness", m.borderType, m.doubleType,
        DependencyPropertyFlags::None, Value::FromDouble(m.doubleType, 0.0), render,
        &ValidateNonnegativeDouble));
    AERO_REGISTER_DP(RegisterProperty(properties, "Padding", m.borderType, m.thicknessType,
        DependencyPropertyFlags::None, thickness.Value(), measure, &ValidateThicknessValue));
    AERO_REGISTER_DP(RegisterProperty(properties, "Text", m.textBlockType, m.stringType,
        DependencyPropertyFlags::None, emptyString.Value(), measure));
    AERO_REGISTER_DP(RegisterProperty(properties, "Foreground", m.textBlockType, m.colorType,
        DependencyPropertyFlags::None, blackValue.Value(), render, &ValidateColorValue));
#undef AERO_REGISTER_DP

    const struct {
        TypeId owner;
        Base::StringView name;
    } structuralMembers[] = {
        {m.stackPanelType, "Children"},
        {m.canvasType, "Children"},
        {m.gridType, "Children"},
        {m.borderType, "Content"},
        {m.contentPresenterType, "Content"}
    };
    for (const auto& member : structuralMembers) {
        Base::Result<MemberId> registered = types.TryRegisterProperty(
            member.owner, {member.name, m.layoutElementType,
                PropertyFlags::None});
        if (!registered) return registered.GetStatus();
    }
    return m;
}

} // namespace Aero::Core
