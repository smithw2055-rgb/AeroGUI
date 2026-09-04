#include "gui/data/BindingInternal.hpp"

#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/controls/State.hpp"
#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Visual.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/SolidColorBrush.hpp>
#include <Aero/Media/StreamGeometry.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace Aero::Data {

using namespace Aero::Meta;
using namespace Aero::Threading;


Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status BindingTypeMismatch(
    const TypeRegistry& types,
    Base::StringView path,
    TypeId sourceType,
    const ::Aero::DependencyObject& target,
    const DependencyProperty* targetProperty) noexcept {
    thread_local char message[512];
    const TypeInfo* source =
        types.FindType(sourceType);
    const TypeInfo* targetType =
        types.FindType(target.RuntimeType());
    const TypeInfo* targetValue =
        targetProperty != nullptr
        ? types.FindType(
              targetProperty->ValueType())
        : nullptr;
    const Base::StringView sourceName =
        source != nullptr
        ? source->Name()
        : Base::StringView("<unknown>");
    const Base::StringView targetName =
        targetType != nullptr
        ? targetType->Name()
        : Base::StringView("<unknown>");
    const Base::StringView propertyName =
        targetProperty != nullptr
        ? targetProperty->Name()
        : Base::StringView("<unknown>");
    const Base::StringView targetValueName =
        targetValue != nullptr
        ? targetValue->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "Binding path '%.*s' result '%.*s' does not match target '%.*s.%.*s' type '%.*s'",
        static_cast<int>(path.SizeBytes()),
        path.Data(),
        static_cast<int>(
            sourceName.SizeBytes()),
        sourceName.Data(),
        static_cast<int>(
            targetName.SizeBytes()),
        targetName.Data(),
        static_cast<int>(
            propertyName.SizeBytes()),
        propertyName.Data(),
        static_cast<int>(
            targetValueName.SizeBytes()),
        targetValueName.Data());
    return InvalidArgument(message);
}

Base::Status BindingPathFailure(
    const TypeRegistry& types,
    Base::StringView path,
    const BindingPathCompileError& error,
    Base::Status status) noexcept {
    thread_local char message[512];
    const TypeInfo* input = types.FindType(error.inputType);
    const Base::StringView inputName = input != nullptr
        ? input->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "Binding path '%.*s' failed at segment %u '%.*s' on type '%.*s': %s",
        static_cast<int>(path.SizeBytes()),
        path.Data(),
        error.segmentIndex,
        static_cast<int>(error.segment.SizeBytes()),
        error.segment.CStr(),
        static_cast<int>(inputName.SizeBytes()),
        inputName.Data(),
        status.message != nullptr ? status.message : "operation failed");
    return Base::Status::Failure(status.code, message);
}

bool IsNumericType(TypeId type) noexcept {
    return type == TypeOf<std::int8_t>() ||
        type == TypeOf<std::int16_t>() ||
        type == TypeOf<std::int32_t>() ||
        type == TypeOf<std::int64_t>() ||
        type == TypeOf<std::uint8_t>() ||
        type == TypeOf<std::uint16_t>() ||
        type == TypeOf<std::uint32_t>() ||
        type == TypeOf<std::uint64_t>() ||
        type == TypeOf<double>();
}

Base::Result<PropertyValue> ConvertNumericValue(
    const PropertyValue& value,
    TypeId targetType) noexcept {
    long double number = 0.0L;
    switch (value.Kind()) {
    case ValueKind::SignedInteger:
        number = static_cast<long double>(
            value.AsSignedInteger());
        break;
    case ValueKind::UnsignedInteger:
        number = static_cast<long double>(
            value.AsUnsignedInteger());
        break;
    case ValueKind::Double:
        if (!std::isfinite(value.AsDouble())) {
            return InvalidArgument(
                "Binding numeric value is not finite");
        }
        number = static_cast<long double>(
            value.AsDouble());
        break;
    default:
        return InvalidArgument(
            "Binding numeric value has an invalid representation");
    }

    if (targetType == TypeOf<double>()) {
        const double result =
            static_cast<double>(number);
        if (!std::isfinite(result)) {
            return InvalidArgument(
                "Binding numeric value exceeds the Double range");
        }
        return PropertyValue::FromDouble(
            targetType, result);
    }

    const bool signedTarget =
        targetType == TypeOf<std::int8_t>() ||
        targetType == TypeOf<std::int16_t>() ||
        targetType == TypeOf<std::int32_t>() ||
        targetType == TypeOf<std::int64_t>();
    if (signedTarget) {
        long double minimum = static_cast<long double>(
            std::numeric_limits<std::int64_t>::min());
        long double maximum = static_cast<long double>(
            std::numeric_limits<std::int64_t>::max());
        if (targetType == TypeOf<std::int8_t>()) {
            minimum = std::numeric_limits<std::int8_t>::min();
            maximum = std::numeric_limits<std::int8_t>::max();
        } else if (
            targetType == TypeOf<std::int16_t>()) {
            minimum = std::numeric_limits<std::int16_t>::min();
            maximum = std::numeric_limits<std::int16_t>::max();
        } else if (
            targetType == TypeOf<std::int32_t>()) {
            minimum = std::numeric_limits<std::int32_t>::min();
            maximum = std::numeric_limits<std::int32_t>::max();
        }
        if (number < minimum || number > maximum) {
            return InvalidArgument(
                "Binding numeric value exceeds the signed target range");
        }
        return PropertyValue::FromSignedInteger(
            targetType,
            static_cast<std::int64_t>(number));
    }

    long double maximum = static_cast<long double>(
        std::numeric_limits<std::uint64_t>::max());
    if (targetType == TypeOf<std::uint8_t>()) {
        maximum = std::numeric_limits<std::uint8_t>::max();
    } else if (
        targetType == TypeOf<std::uint16_t>()) {
        maximum = std::numeric_limits<std::uint16_t>::max();
    } else if (
        targetType == TypeOf<std::uint32_t>()) {
        maximum = std::numeric_limits<std::uint32_t>::max();
    }
    if (number < 0.0L || number > maximum) {
        return InvalidArgument(
            "Binding numeric value exceeds the unsigned target range");
    }
    return PropertyValue::FromUnsignedInteger(
        targetType,
        static_cast<std::uint64_t>(number));
}

bool HasDefaultTargetConversion(
    TypeId sourceType,
    TypeId targetType) noexcept {
    return sourceType == TypeOf<Meta::Value>() ||
        (sourceType == TypeOf<bool>() &&
         targetType == TypeOf<::Aero::Nullable<bool>>()) ||
        (sourceType == TypeOf<::Aero::Nullable<bool>>() &&
         targetType == TypeOf<bool>()) ||
        (sourceType != targetType &&
         IsNumericType(sourceType) &&
         IsNumericType(targetType)) ||
        (IsNumericType(sourceType) &&
         targetType == TypeOf<Aero::Length>()) ||
        (IsNumericType(sourceType) &&
         targetType == TypeOf<Base::Thickness>()) ||
        (sourceType == TypeOf<Aero::Length>() &&
         IsNumericType(targetType)) ||
        (sourceType == TypeOf<Base::Color>() &&
         targetType == Media::Brush::StaticTypeId()) ||
        (targetType == TypeOf<Base::String>() &&
         sourceType != InvalidTypeId) ||
        (sourceType == TypeOf<Base::String>() &&
         targetType != InvalidTypeId);
}

bool TargetAcceptsPathResult(
    const TypeRegistry& types,
    const DependencyProperty* targetProperty,
    TypeId resultType,
    bool hasDynamicResult) noexcept {
    if (targetProperty == nullptr) {
        return false;
    }
    if (targetProperty->AcceptsAnyValue() ||
        hasDynamicResult ||
        resultType == TypeOf<Base::Object>() ||
        targetProperty->ValueType() == TypeOf<Base::Object>()) {
        return true;
    }
    return types.IsAssignableFrom(
               targetProperty->ValueType(), resultType) ||
        HasDefaultTargetConversion(
            resultType, targetProperty->ValueType());
}

::Aero::DependencyObject* BindingParent(
    ::Aero::DependencyObject& node) noexcept {
    ::Aero::DependencyObject* parent =
        ::Aero::LogicalTreeHelper::GetParent(node);
    if (parent == nullptr) {
        if (::Aero::Media::Visual* visual =
                ::Aero::TryCast<::Aero::Media::Visual>(&node)) {
            parent = ::Aero::Media::VisualTreeHelper::GetParent(*visual);
        }
    }
    return parent;
}

bool BindingOwnerSeesDataContextChange(
    ::Aero::DependencyObject* owner,
    ::Aero::DependencyObject& changed) noexcept {
    ::Aero::DependencyObject* node = owner;
    for (std::uint32_t depth = 0U; depth < 64U && node != nullptr; ++depth) {
        if (node == &changed) {
            return true;
        }
        node = BindingParent(*node);
    }
    return false;
}

Base::Result<PropertyValue> ReadDataContextValue(
    ::Aero::DependencyObject& node,
    Meta::DependencyPropertyHandle handle) noexcept {
    if (::Aero::FrameworkElement* element =
            ::Aero::TryCast<::Aero::FrameworkElement>(&node)) {
        return element->GetDataContextResult();
    }
    if (node.PropertyRegistry().Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DataContext property is not registered on this object");
    }
    return node.GetValue(handle);
}

Base::Result<PropertyValue> ConvertNullableBooleanValue(
    const PropertyValue& value,
    TypeId targetType) noexcept {
    if (targetType == TypeOf<::Aero::Nullable<bool>>() &&
        value.Type() == TypeOf<bool>() &&
        value.Kind() == ValueKind::Boolean) {
        return ValueCodec<::Aero::Nullable<bool>>::Encode(
            ::Aero::Nullable<bool>{value.AsBoolean()});
    }
    if (targetType == TypeOf<bool>() &&
        value.Type() == TypeOf<::Aero::Nullable<bool>>()) {
        Base::Result<::Aero::Nullable<bool>> decoded =
            ValueCodec<::Aero::Nullable<bool>>::Decode(value);
        if (!decoded) return decoded.GetStatus();
        if (!decoded.Value().GetHasValue()) {
            return InvalidArgument(
                "Indeterminate Nullable Boolean cannot be converted to Boolean");
        }
        return PropertyValue::FromBoolean(
            targetType, decoded.Value().GetValue());
    }
    return InvalidArgument(
        "Nullable Boolean conversion is incompatible");
}

Base::Result<PropertyValue> ConvertThicknessValue(
    const PropertyValue& value) noexcept {
    Base::Result<PropertyValue> numeric =
        ConvertNumericValue(value, TypeOf<double>());
    if (!numeric) return numeric.GetStatus();
    const double uniform = numeric.Value().AsDouble();
    return Meta::ValueCodec<Base::Thickness>::Encode(
        Base::Thickness{uniform, uniform, uniform, uniform});
}

Base::Result<PropertyValue> ConvertLengthValue(
    const PropertyValue& value,
    TypeId targetType) noexcept {
    if (targetType == TypeOf<Aero::Length>() &&
        IsNumericType(value.Type())) {
        Base::Result<PropertyValue> numeric =
            ConvertNumericValue(value, TypeOf<double>());
        if (!numeric) return numeric.GetStatus();
        return Meta::ValueCodec<Aero::Length>::Encode(
            Aero::Length::Pixels(numeric.Value().AsDouble()));
    }
    if (value.Type() == TypeOf<Aero::Length>() &&
        IsNumericType(targetType)) {
        Base::Result<Aero::Length> length =
            Meta::ValueCodec<Aero::Length>::Decode(value);
        if (!length) return length.GetStatus();
        if (length.Value().isAuto) {
            return InvalidArgument(
                "Auto Length cannot be converted to a numeric binding target");
        }
        return ConvertNumericValue(
            PropertyValue::FromDouble(
                TypeOf<double>(), length.Value().value),
            targetType);
    }
    return InvalidArgument(
        "Binding Length conversion is not supported");
}

Base::Result<PropertyValue> ConvertColorToBrush(
    const PropertyValue& value) noexcept {
    Base::Result<Base::Color> color =
        Meta::ValueCodec<Base::Color>::Decode(value);
    if (!color) return color.GetStatus();
    Base::Result<Base::Ref<Media::Brush>> brush =
        Media::MakeSolidColorBrush(color.Value());
    if (!brush) return brush.GetStatus();
    return PropertyValue::FromObject(
        Media::Brush::StaticTypeId(),
        Base::Ref<Base::Object>(std::move(brush).Value()));
}

// A TwoWay object binding may intentionally expose a concrete source object
// through an Object-typed target property (for example a view-model Language
// through Selector.SelectedItem). The forward assignment is type-safe; the
// reverse assignment is checked against the runtime object's real type.
bool CanRoundTripObjectValue(
    const TypeRegistry& types,
    TypeId sourceType,
    TypeId targetType) noexcept {
    return sourceType != InvalidTypeId &&
        targetType != InvalidTypeId &&
        sourceType != targetType &&
        types.IsAssignableFrom(targetType, sourceType);
}

bool TryParseFixedPrecision(
    Base::StringView specifier,
    std::uint32_t& precision) noexcept {
    if (specifier.Empty() ||
        (specifier[0] != 'F' && specifier[0] != 'f')) {
        return false;
    }
    if (specifier.SizeBytes() == 1U) {
        precision = 2U;
        return true;
    }
    std::uint32_t parsed = 0U;
    for (std::uint32_t index = 1U;
         index < specifier.SizeBytes();
         ++index) {
        const char digit = specifier[index];
        if (digit < '0' || digit > '9') return false;
        parsed = parsed * 10U +
            static_cast<std::uint32_t>(digit - '0');
        if (parsed > 15U) return false;
    }
    precision = parsed;
    return true;
}

bool IsZeroPaddingFormat(
    Base::StringView specifier) noexcept {
    if (specifier.Empty()) return false;
    for (std::uint32_t index = 0U;
         index < specifier.SizeBytes();
         ++index) {
        if (specifier[index] != '0') return false;
    }
    return true;
}

Base::Result<Base::String> FormatBindingString(
    const PropertyValue& value,
    Base::StringView format,
    const Registry* metadata) noexcept {
    // Markup extensions write \{ and \} so the XAML parser does not treat
    // the placeholder as nested markup. WPF stores StringFormat without those
    // slashes ("Orbit: {0:F2} AU"). Unescape before looking up {0:...}.
    Base::String unescapedFormat;
    if (!format.Empty()) {
        Base::Result<void> reserved =
            unescapedFormat.Reserve(format.SizeBytes());
        if (!reserved) return reserved.GetStatus();
        for (std::uint32_t index = 0U;
             index < format.SizeBytes();
             ++index) {
            if (format[index] == '\\' &&
                index + 1U < format.SizeBytes()) {
                ++index;
            }
            const char character = format[index];
            Base::Result<void> appended = unescapedFormat.Append(
                Base::StringView(&character, 1U));
            if (!appended) return appended.GetStatus();
        }
    }
    Base::StringView activeFormat = unescapedFormat.View();
    if (activeFormat.SizeBytes() >= 2U &&
        activeFormat[0] == '{' &&
        activeFormat[1] == '}') {
        activeFormat = activeFormat.Substr(
            2U, activeFormat.SizeBytes() - 2U);
    }

    char raw[128]{};
    if (value.Type() == TypeOf<::Aero::Nullable<bool>>()) {
        Base::Result<::Aero::Nullable<bool>> decoded =
            ValueCodec<::Aero::Nullable<bool>>::Decode(value);
        if (!decoded) return decoded.GetStatus();
        if (decoded.Value().GetHasValue()) {
            std::snprintf(
                raw, sizeof(raw), "%s",
                decoded.Value().GetValue() ? "True" : "False");
        }
    }
    bool numeric = false;
    double numericValue = 0.0;
    switch (value.Type() == TypeOf<::Aero::Nullable<bool>>()
        ? ValueKind::Unset
        : value.Kind()) {
    case ValueKind::Unset:
        break;
    case ValueKind::String: {
        Base::String result;
        Base::Result<void> assigned =
            result.Assign(value.AsString());
        return assigned
            ? Base::Result<Base::String>(
                  std::move(result))
            : Base::Result<Base::String>(
                  assigned.GetStatus());
    }
    case ValueKind::Boolean:
        std::snprintf(
            raw, sizeof(raw),
            "%s",
            value.AsBoolean() ? "True" : "False");
        break;
    case ValueKind::SignedInteger: {
        const std::uint64_t rawValue =
            static_cast<std::uint64_t>(value.AsSignedInteger());
        const EnumValueInfo* enumVal = metadata != nullptr &&
            value.Type() != TypeOf<std::int32_t>() &&
            value.Type() != TypeOf<std::int64_t>() &&
            value.Type() != TypeOf<std::int16_t>() &&
            value.Type() != TypeOf<std::int8_t>()
            ? metadata->Types().FindEnumValue(value.Type(), rawValue)
            : nullptr;
        if (enumVal != nullptr) {
            std::snprintf(
                raw, sizeof(raw),
                "%.*s",
                static_cast<int>(enumVal->Name().SizeBytes()),
                enumVal->Name().Data());
        } else {
            numeric = true;
            numericValue = static_cast<double>(
                value.AsSignedInteger());
            std::snprintf(
                raw, sizeof(raw),
                "%lld",
                static_cast<long long>(
                    value.AsSignedInteger()));
        }
        break;
    }
    case ValueKind::UnsignedInteger: {
        const std::uint64_t rawValue = value.AsUnsignedInteger();
        const EnumValueInfo* enumVal = metadata != nullptr &&
            value.Type() != TypeOf<std::uint32_t>() &&
            value.Type() != TypeOf<std::uint64_t>() &&
            value.Type() != TypeOf<std::uint16_t>() &&
            value.Type() != TypeOf<std::uint8_t>()
            ? metadata->Types().FindEnumValue(value.Type(), rawValue)
            : nullptr;
        if (enumVal != nullptr) {
            std::snprintf(
                raw, sizeof(raw),
                "%.*s",
                static_cast<int>(enumVal->Name().SizeBytes()),
                enumVal->Name().Data());
        } else {
            numeric = true;
            numericValue = static_cast<double>(
                value.AsUnsignedInteger());
            std::snprintf(
                raw, sizeof(raw),
                "%llu",
                static_cast<unsigned long long>(
                    value.AsUnsignedInteger()));
        }
        break;
    }
    case ValueKind::Double:
        numeric = true;
        numericValue = value.AsDouble();
        std::snprintf(
            raw, sizeof(raw),
            "%.15g",
            numericValue);
        break;
    case ValueKind::Object:
        if (!value.IsNullObject() &&
            value.AsObject() &&
            value.AsObject()->RuntimeType() ==
                Media::StreamGeometry::StaticTypeId()) {
            Base::String result;
            Base::Result<void> assigned =
                result.Assign(
                    static_cast<Media::StreamGeometry&>(
                        *value.AsObject()).GetData());
            return assigned
                ? Base::Result<Base::String>(
                      std::move(result))
                : Base::Result<Base::String>(
                      assigned.GetStatus());
        }
        if (value.IsNullObject()) raw[0] = '\0';
        else {
            return InvalidArgument(
                "Binding object has no default text conversion");
        }
        break;
    default:
        return InvalidArgument(
            "Binding value has no default text conversion");
    }

    Base::StringView prefix;
    Base::StringView suffix;
    Base::StringView specifier;
    for (std::uint32_t index = 0U;
         index + 1U < activeFormat.SizeBytes();
         ++index) {
        if (activeFormat[index] != '{' ||
            activeFormat[index + 1U] != '0') {
            continue;
        }
        std::uint32_t close = index + 2U;
        while (close < activeFormat.SizeBytes() &&
               activeFormat[close] != '}') {
            ++close;
        }
        if (close >= activeFormat.SizeBytes()) {
            return InvalidArgument(
                "Binding StringFormat placeholder is incomplete");
        }
        prefix = activeFormat.Substr(0U, index);
        suffix = activeFormat.Substr(
            close + 1U,
            activeFormat.SizeBytes() - close - 1U);
        if (index + 2U < close &&
            activeFormat[index + 2U] == ':') {
            specifier = activeFormat.Substr(
                index + 3U,
                close - index - 3U);
        }
        break;
    }
    if (specifier.Empty() && !activeFormat.Empty() &&
        activeFormat[0] != '{') {
        specifier = activeFormat;
    }

    char formatted[160]{};
    std::uint32_t fixedPrecision = 0U;
    const bool fixedPoint =
        numeric &&
        TryParseFixedPrecision(
            specifier, fixedPrecision);
    const bool zeroPadding =
        numeric &&
        IsZeroPaddingFormat(specifier);
    const bool thousandsScale =
        numeric &&
        specifier == Base::StringView("#,.##");
    if (fixedPoint) {
        std::snprintf(
            formatted,
            sizeof(formatted),
            "%.*f",
            static_cast<int>(fixedPrecision),
            numericValue);
    } else if (zeroPadding) {
        std::snprintf(
            formatted,
            sizeof(formatted),
            "%0*lld",
            static_cast<int>(specifier.SizeBytes()),
            static_cast<long long>(numericValue));
    } else if (thousandsScale) {
        char decimal[96]{};
        std::snprintf(
            decimal,
            sizeof(decimal),
            "%.2f",
            numericValue / 1000.0);
        std::uint32_t decimalLength =
            static_cast<std::uint32_t>(
                std::strlen(decimal));
        while (decimalLength > 0U &&
               decimal[decimalLength - 1U] == '0') {
            decimal[--decimalLength] = '\0';
        }
        if (decimalLength > 0U &&
            decimal[decimalLength - 1U] == '.') {
            decimal[--decimalLength] = '\0';
        }
        const char* digits = decimal;
        bool negative = *digits == '-';
        if (negative) ++digits;
        const char* point = std::strchr(digits, '.');
        const std::size_t integerLengthBytes =
            point != nullptr
            ? static_cast<std::size_t>(point - digits)
            : std::strlen(digits);
        const std::uint32_t integerLength =
            static_cast<std::uint32_t>(integerLengthBytes);
        std::uint32_t output = 0U;
        if (negative) formatted[output++] = '-';
        for (std::uint32_t index = 0U;
             index < integerLength;
             ++index) {
            if (index != 0U &&
                (integerLength - index) % 3U == 0U) {
                formatted[output++] = ',';
            }
            formatted[output++] = digits[index];
        }
        if (point != nullptr) {
            while (*point != '\0' &&
                   output + 1U < sizeof(formatted)) {
                formatted[output++] = *point++;
            }
        }
        formatted[output] = '\0';
    } else {
        std::snprintf(
            formatted,
            sizeof(formatted),
            "%s",
            raw);
    }

    Base::String result;
    Base::Result<void> appended =
        result.Append(prefix);
    if (appended) {
        appended = result.Append(
            Base::StringView(
                formatted,
                static_cast<std::uint32_t>(
                    std::strlen(formatted))));
    }
    if (appended) {
        appended = result.Append(suffix);
    }
    return appended
        ? Base::Result<Base::String>(
              std::move(result))
        : Base::Result<Base::String>(
              appended.GetStatus());
}

Base::Result<PropertyValue> UnboxItemsValue(
    Base::Result<PropertyValue> value) noexcept {
    if (!value) return value.GetStatus();
    const PropertyValue& current = value.Value();
    if (current.Kind() == ValueKind::Object &&
        !current.IsNullObject() && current.AsObject() &&
        current.AsObject()->RuntimeType() ==
            ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
        return static_cast<const
            ::Aero::Controls::BoxedItemValue&>(
                *current.AsObject()).Value();
    }
    return value;
}


} // namespace Aero::Data
