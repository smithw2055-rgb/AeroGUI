#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "controls/ControlInternal.hpp"
#include "controls/ItemsInternal.hpp"
#include "controls/TemplateInternal.hpp"
#include <Aero/Gui/BindingBase.hpp>
#include <Aero/Gui/ResourceDictionary.hpp>
#include <Aero/Gui/Geometry.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace Aero::Data {

using namespace Aero::Meta;
using namespace Aero::Threading;
namespace {

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
    const DependencyObject& target,
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
        (targetType == TypeOf<Base::String>() &&
         sourceType != InvalidTypeId) ||
        (sourceType == TypeOf<Base::String>() &&
         targetType != InvalidTypeId);
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
    Base::StringView format) noexcept {
    Base::StringView activeFormat = format;
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
    case ValueKind::SignedInteger:
        numeric = true;
        numericValue = static_cast<double>(
            value.AsSignedInteger());
        std::snprintf(
            raw, sizeof(raw),
            "%lld",
            static_cast<long long>(
                value.AsSignedInteger()));
        break;
    case ValueKind::UnsignedInteger:
        numeric = true;
        numericValue = static_cast<double>(
            value.AsUnsignedInteger());
        std::snprintf(
            raw, sizeof(raw),
            "%llu",
            static_cast<unsigned long long>(
                value.AsUnsignedInteger()));
        break;
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
            ::Aero::Controls::Detail::BoxedItemValue::StaticTypeId()) {
        return static_cast<const
            ::Aero::Controls::Detail::BoxedItemValue&>(
                *current.AsObject()).Value();
    }
    return value;
}

} // namespace



} // namespace Aero::Data

namespace Aero::GuiPrivate::Detail {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Data;

BindingEngine::BindingEngine(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher),
      bindings_(),
      propertyChangedHandler_(this, &BindingEngine::OnPropertyChanged) {}

BindingEngine::~BindingEngine() noexcept {
    Shutdown();
}

Base::Result<void> BindingEngine::Initialize() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess();
    }
    if (hook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> registered =
        dispatcher_->RegisterFrameHook(
            DispatcherFramePhase::DataBind,
            &BindingEngine::DataBindHook,
            this);
    if (!registered) {
        return registered.GetStatus();
    }
    hook_ = registered.Value();
    return {};
}

void BindingEngine::Shutdown() noexcept {
    if (hook_.IsValid()) {
        (void)dispatcher_->RemoveFrameHook(hook_);
        hook_ = {};
    }
    while (!bindings_.Empty()) {
        RemoveAt(bindings_.Size() - 1U);
    }
    deferredBindings_.Clear();
    flushing_ = false;
}

Base::Result<BindingHandle> BindingEngine::Attach(
    const BindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid()) {
        return InvalidState("BindingEngine must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot attach while flushing");
    }
    Base::Result<void> valid = VerifyDescriptor(descriptor);
    if (!valid) {
        return valid.GetStatus();
    }
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Binding handle sequence is exhausted");
    }

    BindingRecord record;
    record.handle.value = nextHandle_++;
    record.descriptor = descriptor;
    Base::Result<void> appended = bindings_.PushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    Base::Result<void> sourceSubscription =
        descriptor.source->AddValueChangedHandlerChecked(
            descriptor.sourceProperty, propertyChangedHandler_);
    if (!sourceSubscription) {
        RemoveAt(bindings_.Size() - 1U);
        return sourceSubscription.GetStatus();
    }
    Base::Result<void> targetSubscription =
        descriptor.target->AddValueChangedHandlerChecked(
            descriptor.targetProperty, propertyChangedHandler_);
    if (!targetSubscription) {
        (void)descriptor.source->RemoveValueChangedHandler(
            descriptor.sourceProperty, propertyChangedHandler_);
        RemoveAt(bindings_.Size() - 1U);
        return targetSubscription.GetStatus();
    }
    return bindings_.Back().handle;
}

Base::Result<BindingHandle> BindingEngine::Attach(
    const MetadataBindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid()) {
        return InvalidState("BindingEngine must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot attach while flushing");
    }
    Base::Result<void> valid = VerifyDescriptor(descriptor);
    if (!valid) return valid.GetStatus();
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Binding handle sequence is exhausted");
    }

    BindingRecord record;
    record.handle.value = nextHandle_++;
    record.sourceKind = descriptor.source != nullptr
        ? (descriptor.bindsToSource
            ? BindingSourceKind::MetadataObject
            : BindingSourceKind::MetadataPath)
        : BindingSourceKind::DataContext;
    record.metadata = descriptor.metadata;
    record.metadataSource = descriptor.source;
    record.dataContextProperty = descriptor.dataContextProperty;
    record.dataContextOwner = descriptor.dataContextOwner != nullptr
        ? descriptor.dataContextOwner
        : descriptor.target;
    // A Binding authored on FrameworkElement.DataContext reads from the
    // inherited parent DataContext. Reading the target property itself would
    // create a self-reference and hide the inherited value while the
    // expression is unresolved.
    if (record.sourceKind == BindingSourceKind::DataContext &&
        descriptor.targetProperty == descriptor.dataContextProperty &&
        record.metadata->Types().IsDerivedFrom(
            descriptor.target->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        auto& targetElement =
            *static_cast<FrameworkElement*>(descriptor.target);
        ::Aero::Media::Visual* parent = targetElement.GetLogicalParent();
        if (parent == nullptr) parent = targetElement.GetVisualParent();
        if (parent != nullptr && parent->AsFrameworkElement() != nullptr) {
            record.dataContextOwner = parent->AsFrameworkElement();
        }
    }
    record.descriptor.target = descriptor.target;
    record.descriptor.targetProperty = descriptor.targetProperty;
    record.descriptor.mode = descriptor.mode;
    record.descriptor.updateSourceTrigger =
        descriptor.updateSourceTrigger;
    record.descriptor.convert = descriptor.convert;
    record.descriptor.convertBack = descriptor.convertBack;
    record.descriptor.converterResource = descriptor.converterResource;
    record.descriptor.converterParameter = descriptor.converterParameter;
    record.descriptor.validate = descriptor.validate;
    record.descriptor.validateBack = descriptor.validateBack;
    record.descriptor.conversionContext =
        descriptor.conversionContext;
    record.descriptor.fallbackValue = descriptor.fallbackValue;
    record.descriptor.targetNullValue = descriptor.targetNullValue;
    record.descriptor.diagnostic = descriptor.diagnostic;
    record.descriptor.diagnosticContext =
        descriptor.diagnosticContext;
    record.bindsToSource = descriptor.bindsToSource;
    Base::Result<void> assigned = record.path.Assign(descriptor.path);
    if (!assigned) {
        --nextHandle_;
        return assigned.GetStatus();
    }
    assigned = record.stringFormat.Assign(
        descriptor.stringFormat);
    if (!assigned) {
        --nextHandle_;
        return assigned.GetStatus();
    }

    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        BindingPathCompileError compileError;
        Base::Result<BindingPathPlan> compiled = BindingPathPlan::Compile(
            *record.metadata,
            record.metadataSource->RuntimeType(),
            record.path.View(),
            &compileError);
        if (!compiled) {
            --nextHandle_;
            return BindingPathFailure(
                record.metadata->Types(),
                record.path.View(),
                compileError,
                compiled.GetStatus());
        }
        record.pathPlan = std::move(compiled).Value();
        const DependencyProperty* targetProperty =
            descriptor.target->PropertyRegistry().Find(
                descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (!targetProperty->AcceptsAnyValue() &&
            (descriptor.convert == nullptr &&
            !descriptor.converterResource &&
            !record.pathPlan.HasDynamicResult() &&
            record.pathPlan.ResultType() != TypeOf<Base::Object>() &&
            !record.metadata->Types().IsAssignableFrom(
                targetProperty->ValueType(),
                record.pathPlan.ResultType()) &&
            !HasDefaultTargetConversion(
                record.pathPlan.ResultType(),
                targetProperty->ValueType())))) {
            --nextHandle_;
            return BindingTypeMismatch(
                record.metadata->Types(),
                record.path.View(),
                record.pathPlan.ResultType(),
                *descriptor.target,
                targetProperty);
        }
        if ((descriptor.mode == BindingMode::TwoWay ||
             descriptor.mode == BindingMode::OneWayToSource) &&
            !record.pathPlan.CanWrite()) {
            --nextHandle_;
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Binding source path is not writable");
        }
        if ((descriptor.mode == BindingMode::TwoWay ||
             descriptor.mode == BindingMode::OneWayToSource) &&
            !record.pathPlan.HasDynamicResult() &&
            targetProperty->ValueType() != record.pathPlan.ResultType() &&
            descriptor.convertBack == nullptr &&
            !descriptor.converterResource &&
            !HasDefaultTargetConversion(
                targetProperty->ValueType(),
                record.pathPlan.ResultType()) &&
            !CanRoundTripObjectValue(
                record.metadata->Types(),
                record.pathPlan.ResultType(),
                targetProperty->ValueType())) {
            --nextHandle_;
            return InvalidArgument(
                "Binding requires ConvertBack for different source and target types");
        }
    } else if (record.sourceKind == BindingSourceKind::MetadataObject) {
        const DependencyProperty* targetProperty =
            descriptor.target->PropertyRegistry().Find(
                descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (!targetProperty->AcceptsAnyValue() &&
            !record.metadata->Types().IsAssignableFrom(
                targetProperty->ValueType(),
                record.metadataSource->RuntimeType()))) {
            --nextHandle_;
            return BindingTypeMismatch(
                record.metadata->Types(),
                Base::StringView("."),
                record.metadataSource->RuntimeType(),
                *descriptor.target,
                targetProperty);
        }
        if (descriptor.mode == BindingMode::TwoWay ||
            descriptor.mode == BindingMode::OneWayToSource) {
            --nextHandle_;
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Binding to a source object does not support writeback");
        }
    }

    Base::Result<void> appended =
        bindings_.PushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    BindingRecord& stored = bindings_.Back();
    Base::Result<void> targetSubscription =
        descriptor.target->AddValueChangedHandlerChecked(
            descriptor.targetProperty, propertyChangedHandler_);
    if (!targetSubscription) {
        RemoveAt(bindings_.Size() - 1U);
        return targetSubscription.GetStatus();
    }
    if (stored.sourceKind == BindingSourceKind::DataContext) {
        Base::Result<void> contextSubscription =
            stored.dataContextOwner->AddValueChangedHandlerChecked(
                descriptor.dataContextProperty,
                propertyChangedHandler_);
        if (!contextSubscription) {
            RemoveAt(bindings_.Size() - 1U);
            return contextSubscription.GetStatus();
        }
    } else if (stored.sourceKind == BindingSourceKind::MetadataPath) {
        Base::Result<void> sourceSubscription =
            SubscribeMetadataSource(stored);
        if (!sourceSubscription) {
            RemoveAt(bindings_.Size() - 1U);
            return sourceSubscription.GetStatus();
        }
    }
    return stored.handle;
}

Base::Result<void> BindingEngine::QueueDeferred(
    const MetadataBindingDescriptor& descriptor) noexcept {
    if (descriptor.metadata == nullptr ||
        descriptor.target == nullptr ||
        !descriptor.targetProperty.IsValid() ||
        (descriptor.path.Empty() && !descriptor.bindsToSource)) {
        return InvalidArgument(
            "Deferred Binding descriptor is invalid");
    }
    if (descriptor.source == nullptr &&
        !descriptor.dataContextProperty.IsValid()) {
        return InvalidArgument(
            "Deferred DataContext Binding requires a DataContext property");
    }
    DeferredBindingRecord record;
    record.metadata = descriptor.metadata;
    record.source = descriptor.source;
    record.target = descriptor.target;
    record.targetProperty = descriptor.targetProperty;
    record.dataContextProperty =
        descriptor.dataContextProperty;
    record.dataContextOwner = descriptor.dataContextOwner;
    record.mode = descriptor.mode;
    record.updateSourceTrigger =
        descriptor.updateSourceTrigger;
    record.bindsToSource = descriptor.bindsToSource;
    record.convert = descriptor.convert;
    record.convertBack = descriptor.convertBack;
    record.converterResource = descriptor.converterResource;
    record.converterParameter = descriptor.converterParameter;
    record.validate = descriptor.validate;
    record.validateBack = descriptor.validateBack;
    record.conversionContext =
        descriptor.conversionContext;
    record.fallbackValue = descriptor.fallbackValue;
    record.targetNullValue =
        descriptor.targetNullValue;
    record.diagnostic = descriptor.diagnostic;
    record.diagnosticContext =
        descriptor.diagnosticContext;
    Base::Result<void> assigned =
        record.path.Assign(descriptor.path);
    if (!assigned) return assigned.GetStatus();
    assigned = record.stringFormat.Assign(
        descriptor.stringFormat);
    if (!assigned) return assigned.GetStatus();
    return deferredBindings_.PushBack(
        std::move(record));
}

Base::Result<std::uint32_t>
BindingEngine::ActivateDeferred(
    DependencyObject& target) noexcept {
    std::uint32_t activated = 0U;
    for (std::uint32_t index = 0U;
         index < deferredBindings_.Size();) {
        DeferredBindingRecord& record =
            deferredBindings_[index];
        if (record.target != &target) {
            ++index;
            continue;
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = record.metadata;
        descriptor.source = record.source;
        descriptor.target = record.target;
        descriptor.targetProperty =
            record.targetProperty;
        descriptor.dataContextProperty =
            record.dataContextProperty;
        descriptor.dataContextOwner = record.dataContextOwner;
        descriptor.path = record.path.View();
        descriptor.stringFormat =
            record.stringFormat.View();
        descriptor.bindsToSource = record.bindsToSource;
        descriptor.mode = record.mode;
        descriptor.updateSourceTrigger =
            record.updateSourceTrigger;
        descriptor.convert = record.convert;
        descriptor.convertBack = record.convertBack;
        descriptor.converterResource = record.converterResource;
        descriptor.converterParameter = record.converterParameter;
        descriptor.validate = record.validate;
        descriptor.validateBack =
            record.validateBack;
        descriptor.conversionContext =
            record.conversionContext;
        descriptor.fallbackValue =
            record.fallbackValue;
        descriptor.targetNullValue =
            record.targetNullValue;
        descriptor.diagnostic = record.diagnostic;
        descriptor.diagnosticContext =
            record.diagnosticContext;
        Base::Result<BindingHandle> attached =
            Attach(descriptor);
        if (!attached) return attached.GetStatus();
        for (std::uint32_t move = index + 1U;
             move < deferredBindings_.Size();
             ++move) {
            deferredBindings_[move - 1U] =
                std::move(deferredBindings_[move]);
        }
        (void)deferredBindings_.Resize(
            deferredBindings_.Size() - 1U);
        ++activated;
    }
    return activated;
}

Base::Result<bool> BindingEngine::Detach(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot detach while flushing");
    }
    if (!handle.IsValid()) {
        return false;
    }
    for (std::uint32_t index = 0U; index < bindings_.Size(); ++index) {
        if (bindings_[index].handle.value == handle.value) {
            RemoveAt(index);
            return true;
        }
    }
    return false;
}

Base::Result<bool> BindingEngine::UpdateSource(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid() || flushing_) {
        return InvalidState("BindingEngine is not ready to update a source");
    }
    for (BindingRecord& record : bindings_) {
        if (record.handle.value != handle.value) {
            continue;
        }
        if (record.descriptor.mode != BindingMode::TwoWay &&
            record.descriptor.mode != BindingMode::OneWayToSource) {
            return false;
        }
        record.targetDirty = true;
        record.forceSourceUpdate = true;
        return true;
    }
    return false;
}

Base::Result<std::uint32_t> BindingEngine::DetachObject(
    DependencyObject& object) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot detach objects while flushing");
    }
    std::uint32_t detached = 0U;
    for (std::uint32_t index = 0U; index < bindings_.Size();) {
        const BindingRecord& record = bindings_[index];
        if (record.descriptor.source != &object &&
            record.metadataSource != &object &&
            record.descriptor.target != &object &&
            record.dataContextOwner != &object) {
            ++index;
            continue;
        }
        RemoveAt(index);
        ++detached;
    }
    for (std::uint32_t index = 0U;
         index < deferredBindings_.Size();) {
        const DeferredBindingRecord& record =
            deferredBindings_[index];
        if (record.source != &object &&
            record.target != &object &&
            record.dataContextOwner != &object) {
            ++index;
            continue;
        }
        for (std::uint32_t move = index + 1U;
             move < deferredBindings_.Size();
             ++move) {
            deferredBindings_[move - 1U] =
                std::move(deferredBindings_[move]);
        }
        (void)deferredBindings_.Resize(
            deferredBindings_.Size() - 1U);
        ++detached;
    }
    return detached;
}

Base::Result<void> BindingEngine::ApplySourceToTarget(
    BindingRecord& record,
    const PropertyValue& source,
    bool& usedFallback,
    PropertyValue& target) noexcept {
    Base::Result<PropertyValue> converted = usedFallback
        ? Base::Result<PropertyValue>(source)
        : ConvertForTarget(record, source);
    if (!converted) {
        ReportDiagnostic(
            record,
            record.conversionFailureStage,
            converted.GetStatus());
        if (record.descriptor.fallbackValue.IsUnset() || usedFallback) {
            return converted.GetStatus();
        }
        converted = record.descriptor.fallbackValue;
        usedFallback = true;
    }
    record.descriptor.target->SetValue(
        record.descriptor.targetProperty,
        converted.Value());
    target = converted.Value();
    return {};
}

Base::Result<void> BindingEngine::ApplyTargetToSource(
    BindingRecord& record,
    const PropertyValue& target,
    PropertyValue& source) noexcept {
    Base::Result<PropertyValue> converted =
        ConvertForSource(record, target);
    if (!converted) {
        ReportDiagnostic(
            record,
            record.conversionFailureStage,
            converted.GetStatus());
        return converted.GetStatus();
    }
    Base::Result<void> written =
        WriteSource(record, converted.Value());
    if (!written) {
        ReportDiagnostic(
            record,
            BindingDiagnosticStage::WriteSource,
            written.GetStatus());
        return written.GetStatus();
    }
    source = converted.Value();
    return {};
}

Base::Result<std::uint32_t> BindingEngine::Flush() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid()) {
        return InvalidState("BindingEngine is not initialized");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot flush recursively");
    }

    flushing_ = true;
    lastError_ = {};
    const std::uint32_t snapshotCount = bindings_.Size();
    std::uint32_t updated = 0U;
    for (std::uint32_t index = 0U; index < snapshotCount; ++index) {
        BindingRecord& record = bindings_[index];
        if (record.descriptor.mode == BindingMode::OneTime && record.applied) {
            continue;
        }
        const bool metadataPath =
            record.sourceKind != BindingSourceKind::DependencyProperty;
        if (record.applied && !record.sourceDirty &&
            !record.targetDirty && !metadataPath) {
            continue;
        }

        Base::Result<PropertyValue> source = ReadSource(record);
        bool usedFallback = false;
        if (!source) {
            const BindingDiagnosticStage stage =
                record.sourceKind == BindingSourceKind::DataContext &&
                source.GetStatus().code == Base::ErrorCode::NotFound
                    ? BindingDiagnosticStage::ResolveSource
                    : BindingDiagnosticStage::ReadSource;
            ReportDiagnostic(record, stage, source.GetStatus());
            if (record.descriptor.fallbackValue.IsUnset()) {
                record.applied = false;
                record.sourceDirty = true;
                continue;
            }
            source = record.descriptor.fallbackValue;
            usedFallback = true;
        }
        Base::Result<PropertyValue> target =
            record.descriptor.target->GetValue(record.descriptor.targetProperty);
        if (!target) {
            ReportDiagnostic(
                record,
                BindingDiagnosticStage::WriteTarget,
                target.GetStatus());
            continue;
        }

        const bool sourceChanged = !record.applied ||
            record.sourceDirty ||
            usedFallback ||
            (metadataPath &&
             source.Value() != record.lastSourceValue);
        const bool targetChanged = record.descriptor.updateSourceTrigger ==
                UpdateSourceTrigger::Explicit
            ? record.forceSourceUpdate
            : (!record.applied || record.targetDirty);
        Base::Result<void> applied;
        switch (record.descriptor.mode) {
        case BindingMode::OneTime:
            if (!record.applied) {
                applied = ApplySourceToTarget(
                    record,
                    source.Value(),
                    usedFallback,
                    target.Value());
                if (applied) ++updated;
            }
            break;
        case BindingMode::Default:
        case BindingMode::OneWay:
            if (sourceChanged) {
                applied = ApplySourceToTarget(
                    record,
                    source.Value(),
                    usedFallback,
                    target.Value());
                if (applied) ++updated;
            }
            break;
        case BindingMode::OneWayToSource:
            if (targetChanged) {
                applied = ApplyTargetToSource(
                    record,
                    target.Value(),
                    source.Value());
                if (applied) ++updated;
            }
            break;
        case BindingMode::TwoWay:
            if (sourceChanged) {
                applied = ApplySourceToTarget(
                    record,
                    source.Value(),
                    usedFallback,
                    target.Value());
                if (applied) ++updated;
            } else if (targetChanged) {
                applied = ApplyTargetToSource(
                    record,
                    target.Value(),
                    source.Value());
                if (applied) ++updated;
            }
            break;
        }
        if (!applied && (sourceChanged || targetChanged)) {
            record.sourceDirty = true;
            continue;
        }
        record.lastSourceValue = source.Value();
        record.lastTargetValue = target.Value();
        record.applied = true;
        record.sourceDirty = false;
        record.targetDirty = false;
        record.forceSourceUpdate = false;
    }
    flushing_ = false;
    return updated;
}

Base::Result<std::uint32_t>
BindingEngine::InspectBindings(
    const DependencyObject& object,
    Base::Vector<BindingInspection>&
        output) const noexcept {
    output.Clear();
    for (const BindingRecord& record :
        bindings_) {
        Base::Object* source =
            record.sourceKind ==
                BindingSourceKind::
                    DependencyProperty
            ? static_cast<Base::Object*>(
                record.descriptor.source)
            : record.metadataSource;
        if (source != &object &&
            record.descriptor.target !=
                &object) {
            continue;
        }
        BindingInspection inspection;
        inspection.handle = record.handle;
        inspection.source = source;
        inspection.target =
            record.descriptor.target;
        inspection.sourceProperty =
            record.descriptor.
                sourceProperty;
        inspection.targetProperty =
            record.descriptor.
                targetProperty;
        inspection.mode =
            record.descriptor.mode;
        inspection.updateSourceTrigger =
            record.descriptor.
                updateSourceTrigger;
        inspection.usesDataContext =
            record.sourceKind ==
                BindingSourceKind::
                    DataContext;
        inspection.applied =
            record.applied;
        Base::Result<void> assigned =
            inspection.path.Assign(
                record.path.View());
        if (!assigned) {
            output.Clear();
            return assigned.GetStatus();
        }
        Base::Result<void> appended =
            output.PushBack(
                std::move(inspection));
        if (!appended) {
            output.Clear();
            return appended.GetStatus();
        }
    }
    return output.Size();
}

void BindingEngine::DataBindHook(void* context) noexcept {
    BindingEngine* manager = static_cast<BindingEngine*>(context);
    if (manager != nullptr) {
        (void)manager->Flush();
    }
}

void BindingEngine::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind == BindingSourceKind::DependencyProperty &&
            record.descriptor.source == &object &&
            record.descriptor.sourceProperty == args.GetProperty()) {
            record.sourceDirty = true;
        }
        if (record.sourceKind == BindingSourceKind::DataContext &&
            record.dataContextOwner == &object &&
            record.dataContextProperty == args.GetProperty()) {
            ReleaseMetadataSource(record);
            record.metadataSource = nullptr;
            record.pathPlan = {};
            record.sourceDirty = true;
            record.applied = false;
        }
        if (record.descriptor.target == &object &&
            record.descriptor.targetProperty == args.GetProperty()) {
            record.targetDirty = true;
        }
    }
}

void BindingEngine::OnMetadataPropertyChanged(
    Base::Object& object,
    MemberId property) noexcept {
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind ==
                BindingSourceKind::DependencyProperty ||
            record.metadataSource != &object ||
            !record.pathPlan.IsValid() ||
            record.pathPlan.Segments().Empty()) {
            continue;
        }
        if (property == InvalidMemberId ||
            record.pathPlan.Segments()[0].member == property) {
            record.sourceDirty = true;
        }
    }
}

void BindingEngine::MetadataPropertyChanged(
    Base::Object& object,
    MemberId property,
    void* context) noexcept {
    BindingEngine* manager = static_cast<BindingEngine*>(context);
    if (manager != nullptr) {
        manager->OnMetadataPropertyChanged(object, property);
    }
}

Base::Result<void> BindingEngine::VerifyDescriptor(
    const BindingDescriptor& descriptor) const noexcept {
    if (descriptor.source == nullptr || descriptor.target == nullptr ||
        !descriptor.sourceProperty.IsValid() ||
        !descriptor.targetProperty.IsValid()) {
        return InvalidArgument("Binding descriptor is incomplete");
    }
    if (&descriptor.source->GetDispatcher() != dispatcher_ ||
        &descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument("Binding source and target must use this Dispatcher");
    }
    Base::Result<PropertyValue> source =
        descriptor.source->GetValue(descriptor.sourceProperty);
    if (!source) {
        return source.GetStatus();
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) {
        return target.GetStatus();
    }
    if (source.Value().Type() != target.Value().Type() &&
        descriptor.convert == nullptr &&
        !descriptor.converterResource &&
        !HasDefaultTargetConversion(
            source.Value().Type(), target.Value().Type())) {
        return InvalidArgument("Binding source and target property types differ");
    }
    if ((descriptor.mode == BindingMode::TwoWay ||
         descriptor.mode == BindingMode::OneWayToSource) &&
        source.Value().Type() != target.Value().Type() &&
        descriptor.convertBack == nullptr &&
        !descriptor.converterResource &&
        !HasDefaultTargetConversion(
            target.Value().Type(),
            source.Value().Type())) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    return {};
}

Base::Result<void> BindingEngine::VerifyDescriptor(
    const MetadataBindingDescriptor& descriptor) const noexcept {
    if (descriptor.metadata == nullptr ||
        !descriptor.metadata->IsReady() ||
        descriptor.target == nullptr ||
        !descriptor.targetProperty.IsValid() ||
        (descriptor.path.Empty() && !descriptor.bindsToSource) ||
        (descriptor.source == nullptr &&
         !descriptor.dataContextProperty.IsValid())) {
        return InvalidArgument("Metadata binding descriptor is incomplete");
    }
    if (&descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument(
            "Binding target must use this Dispatcher");
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) return target.GetStatus();
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    if (descriptor.source != nullptr &&
        (descriptor.source->RuntimeType() == InvalidTypeId ||
        descriptor.metadata->Types().FindType(
            descriptor.source->RuntimeType()) == nullptr)) {
        return InvalidArgument(
            "Binding metadata source has no registered runtime type");
    }
    return {};
}

Base::Result<void> BindingEngine::ResolveMetadataSource(
    BindingRecord& record) noexcept {
    if (record.sourceKind == BindingSourceKind::MetadataObject) {
        return record.metadataSource != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(InvalidState(
                "Binding source object is not resolved"));
    }
    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        return record.metadataSource != nullptr &&
            record.pathPlan.IsValid()
            ? Base::Result<void>()
            : Base::Result<void>(InvalidState(
                "Binding metadata source is not resolved"));
    }
    Base::Result<PropertyValue> dataContext =
        record.dataContextOwner->GetValue(
            record.dataContextProperty);
    if (!dataContext) return dataContext.GetStatus();
    if (dataContext.Value().Kind() != ValueKind::Object ||
        dataContext.Value().IsNullObject()) {
        ReleaseMetadataSource(record);
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target has no DataContext object");
    }

    Base::Object* source = dataContext.Value().AsObject().Get();
    if (source == record.metadataSource &&
        (record.bindsToSource || record.pathPlan.IsValid())) {
        return {};
    }
    ReleaseMetadataSource(record);
    record.metadataSource = nullptr;
    record.pathPlan = {};

    if (record.bindsToSource) {
        const DependencyProperty* targetProperty =
            record.descriptor.target->PropertyRegistry().Find(
                record.descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (!targetProperty->AcceptsAnyValue() &&
            (record.descriptor.convert == nullptr &&
             !record.descriptor.converterResource &&
             !record.metadata->Types().IsAssignableFrom(
                 targetProperty->ValueType(),
                 source->RuntimeType())))) {
            return BindingTypeMismatch(
                record.metadata->Types(),
                Base::StringView("."),
                source->RuntimeType(),
                *record.descriptor.target,
                targetProperty);
        }
        if (record.descriptor.mode == BindingMode::TwoWay ||
            record.descriptor.mode == BindingMode::OneWayToSource) {
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Binding to the DataContext object does not support writeback");
        }
        record.metadataSource = source;
        record.sourceDirty = true;
        record.applied = false;
        return {};
    }

    BindingPathCompileError compileError;
    Base::Result<BindingPathPlan> compiled =
        BindingPathPlan::Compile(
            *record.metadata,
            source->RuntimeType(),
            record.path.View(),
            &compileError);
    if (!compiled) {
        return BindingPathFailure(
            record.metadata->Types(),
            record.path.View(),
            compileError,
            compiled.GetStatus());
    }
    const DependencyProperty* targetProperty =
        record.descriptor.target->PropertyRegistry().Find(
            record.descriptor.targetProperty);
    if (targetProperty == nullptr ||
        (!targetProperty->AcceptsAnyValue() &&
        (record.descriptor.convert == nullptr &&
        !record.descriptor.converterResource &&
        !compiled.Value().HasDynamicResult() &&
        compiled.Value().ResultType() != TypeOf<Base::Object>() &&
        !record.metadata->Types().IsAssignableFrom(
            targetProperty->ValueType(),
            compiled.Value().ResultType()) &&
        !HasDefaultTargetConversion(
            compiled.Value().ResultType(),
            targetProperty->ValueType())))) {
        return BindingTypeMismatch(
            record.metadata->Types(),
            record.path.View(),
            compiled.Value().ResultType(),
            *record.descriptor.target,
            targetProperty);
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        !compiled.Value().CanWrite()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding source path is not writable");
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        !compiled.Value().HasDynamicResult() &&
        targetProperty->ValueType() != compiled.Value().ResultType() &&
        record.descriptor.convertBack == nullptr &&
        !record.descriptor.converterResource &&
        !HasDefaultTargetConversion(
            targetProperty->ValueType(),
            compiled.Value().ResultType()) &&
        !CanRoundTripObjectValue(
            record.metadata->Types(),
            compiled.Value().ResultType(),
            targetProperty->ValueType())) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    record.metadataSource = source;
    record.pathPlan = std::move(compiled).Value();
    Base::Result<void> subscribed =
        SubscribeMetadataSource(record);
    if (!subscribed) {
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return subscribed.GetStatus();
    }
    record.sourceDirty = true;
    record.applied = false;
    return {};
}

Base::Result<PropertyValue> BindingEngine::ReadSource(
    BindingRecord& record) noexcept {
    if (record.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        return Aero::Data::UnboxItemsValue(
            record.descriptor.source->GetValue(
                record.descriptor.sourceProperty));
    }
    if (record.sourceKind == BindingSourceKind::MetadataObject) {
        return Aero::Data::UnboxItemsValue(
            PropertyValue::FromObject(
                record.metadataSource->RuntimeType(),
                Base::Ref<Base::Object>::FromBorrowed(
                    *record.metadataSource)));
    }
    Base::Result<void> resolved = ResolveMetadataSource(record);
    if (!resolved) return resolved.GetStatus();
    if (record.bindsToSource) {
        return Aero::Data::UnboxItemsValue(
            PropertyValue::FromObject(
                record.metadataSource->RuntimeType(),
                Base::Ref<Base::Object>::FromBorrowed(
                    *record.metadataSource)));
    }
    return Aero::Data::UnboxItemsValue(
        record.pathPlan.Get(
            *record.metadata, *record.metadataSource));
}

Base::Result<void> BindingEngine::WriteSource(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    if (record.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        record.descriptor.source->SetValue(
            record.descriptor.sourceProperty, value);
        return {};
    }
    if (record.sourceKind == BindingSourceKind::MetadataObject ||
        record.bindsToSource) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding source object cannot be replaced through writeback");
    }
    Base::Result<void> resolved = ResolveMetadataSource(record);
    if (!resolved) return resolved;
    return record.pathPlan.Set(
        *record.metadata, *record.metadataSource, value);
}

Base::Result<PropertyValue> BindingEngine::ConvertForTarget(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    record.conversionFailureStage =
        BindingDiagnosticStage::Convert;
    const DependencyProperty* targetProperty =
        record.descriptor.target->PropertyRegistry().Find(
            record.descriptor.targetProperty);
    if (targetProperty == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target property was not found");
    }
    PropertyValue converted = value;
    if (converted.IsNullObject() &&
        !record.descriptor.targetNullValue.IsUnset()) {
        converted = record.descriptor.targetNullValue;
    } else if (record.descriptor.converterResource) {
        Base::Result<PropertyValue> result =
            record.descriptor.converterResource->Convert(
                converted,
                record.descriptor.converterParameter);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    } else if (record.descriptor.convert != nullptr) {
        Base::Result<PropertyValue> result =
            record.descriptor.convert(
                converted,
                targetProperty->ValueType(),
                record.descriptor.conversionContext);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    } else if (HasDefaultTargetConversion(
                   converted.Type(),
                   targetProperty->ValueType())) {
        Base::Result<PropertyValue> result =
            ((converted.Type() == TypeOf<bool>() &&
              targetProperty->ValueType() ==
                  TypeOf<::Aero::Nullable<bool>>()) ||
             (converted.Type() == TypeOf<::Aero::Nullable<bool>>() &&
              targetProperty->ValueType() == TypeOf<bool>()))
            ? ConvertNullableBooleanValue(
                  converted, targetProperty->ValueType())
            : (IsNumericType(converted.Type()) &&
             targetProperty->ValueType() == TypeOf<Base::Thickness>())
            ? ConvertThicknessValue(converted)
            : ((IsNumericType(converted.Type()) &&
              targetProperty->ValueType() == TypeOf<Aero::Length>()) ||
             (converted.Type() == TypeOf<Aero::Length>() &&
              IsNumericType(targetProperty->ValueType())))
            ? ConvertLengthValue(
                  converted, targetProperty->ValueType())
            : IsNumericType(converted.Type()) &&
                  IsNumericType(targetProperty->ValueType())
                ? ConvertNumericValue(
                      converted, targetProperty->ValueType())
                : [&]() noexcept
                  -> Base::Result<PropertyValue> {
                Base::Result<Base::String> text =
                    FormatBindingString(
                        converted,
                        record.stringFormat.View());
                if (!text) return text.GetStatus();
                return PropertyValue::TryFromString(
                    targetProperty->ValueType(),
                    text.Value().View());
            }();
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    }
    Base::Result<PropertyValue> coerced =
        NormalizeValueForProperty(
            record.metadata,
            *targetProperty,
            std::move(converted));
    if (!coerced) return coerced.GetStatus();
    converted = std::move(coerced).Value();
    if (record.descriptor.validate != nullptr) {
        record.conversionFailureStage =
            BindingDiagnosticStage::Validate;
        Base::Result<void> valid = record.descriptor.validate(
            converted, record.descriptor.conversionContext);
        if (!valid) return valid.GetStatus();
    }
    return converted;
}

Base::Result<PropertyValue> BindingEngine::ConvertForSource(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    record.conversionFailureStage =
        BindingDiagnosticStage::ConvertBack;
    TypeId sourceType = InvalidTypeId;
    if (record.sourceKind == BindingSourceKind::DependencyProperty) {
        const DependencyProperty* sourceProperty =
            record.descriptor.source->PropertyRegistry().Find(
                record.descriptor.sourceProperty);
        if (sourceProperty != nullptr) {
            sourceType = sourceProperty->ValueType();
        }
    } else if (record.sourceKind == BindingSourceKind::MetadataPath ||
               record.sourceKind == BindingSourceKind::DataContext) {
        Base::Result<void> resolved = ResolveMetadataSource(record);
        if (!resolved) return resolved.GetStatus();
        if (record.pathPlan.IsValid()) {
            sourceType = record.pathPlan.ResultType();
        }
    }
    if (sourceType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding source type was not found");
    }
    PropertyValue converted = value;
    if (record.descriptor.converterResource) {
        Base::Result<PropertyValue> result =
            record.descriptor.converterResource->ConvertBack(
                converted,
                record.descriptor.converterParameter);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    } else if (record.descriptor.convertBack != nullptr) {
        Base::Result<PropertyValue> result =
            record.descriptor.convertBack(
                converted,
                sourceType,
                record.descriptor.conversionContext);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    } else if (HasDefaultTargetConversion(
                   converted.Type(), sourceType)) {
        Base::Result<PropertyValue> result =
            ((converted.Type() == TypeOf<bool>() &&
              sourceType == TypeOf<::Aero::Nullable<bool>>()) ||
             (converted.Type() == TypeOf<::Aero::Nullable<bool>>() &&
              sourceType == TypeOf<bool>()))
            ? ConvertNullableBooleanValue(converted, sourceType)
            : ((IsNumericType(converted.Type()) &&
              sourceType == TypeOf<Aero::Length>()) ||
             (converted.Type() == TypeOf<Aero::Length>() &&
              IsNumericType(sourceType)))
            ? ConvertLengthValue(converted, sourceType)
            : IsNumericType(converted.Type()) &&
                  IsNumericType(sourceType)
                ? ConvertNumericValue(converted, sourceType)
                : [&]() noexcept
                  -> Base::Result<PropertyValue> {
                Base::Result<Base::String> text =
                    FormatBindingString(
                        converted, {});
                if (!text) return text.GetStatus();
                return PropertyValue::TryFromString(
                    sourceType,
                    text.Value().View());
            }();
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    }
    if (converted.Type() != sourceType) {
        if (converted.Kind() == ValueKind::Object &&
            !converted.IsNullObject() && converted.AsObject() &&
            record.metadata->Types().IsAssignableFrom(
                sourceType,
                converted.AsObject()->RuntimeType())) {
            converted = PropertyValue::FromObject(
                sourceType,
                Base::Ref<Base::Object>::FromBorrowed(
                    *converted.AsObject()));
        }
    }
    if (converted.Type() != sourceType) {
        return InvalidArgument(
            "Binding ConvertBack returned a value with the wrong source type");
    }
    if (record.descriptor.validateBack != nullptr) {
        record.conversionFailureStage =
            BindingDiagnosticStage::ValidateBack;
        Base::Result<void> valid = record.descriptor.validateBack(
            converted, record.descriptor.conversionContext);
        if (!valid) return valid.GetStatus();
    }
    return converted;
}

void BindingEngine::ReportDiagnostic(
    BindingRecord& record,
    BindingDiagnosticStage stage,
    Base::Status status) noexcept {
    lastError_ = status;
    if (record.descriptor.diagnostic != nullptr) {
        record.descriptor.diagnostic(
            {record.handle, stage, status},
            record.descriptor.diagnosticContext);
    }
}

Base::Result<void> BindingEngine::SubscribeMetadataSource(
    BindingRecord& record) noexcept {
    if (record.metadata == nullptr ||
        record.metadataSource == nullptr) {
        return InvalidArgument(
            "Binding metadata source subscription is incomplete");
    }
    Base::Result<std::uint64_t> subscribed =
        record.metadata->SubscribePropertyChanged(
        *record.metadataSource,
        &BindingEngine::MetadataPropertyChanged,
        this);
    if (!subscribed) return subscribed.GetStatus();
    record.notificationSubscription = subscribed.Value();
    return {};
}

void BindingEngine::ReleaseMetadataSource(
    BindingRecord& record) noexcept {
    if (record.notificationSubscription != 0U &&
        record.metadata != nullptr &&
        record.metadataSource != nullptr) {
        (void)record.metadata->UnsubscribePropertyChanged(
            *record.metadataSource,
            record.notificationSubscription);
    }
    record.notificationSubscription = 0U;
}

void BindingEngine::RemoveAt(std::uint32_t index) noexcept {
    BindingRecord& removed = bindings_[index];
    if (removed.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        (void)removed.descriptor.source->RemoveValueChangedHandler(
            removed.descriptor.sourceProperty,
            propertyChangedHandler_);
    } else {
        ReleaseMetadataSource(removed);
        if (removed.sourceKind ==
            BindingSourceKind::DataContext) {
            (void)removed.dataContextOwner->RemoveValueChangedHandler(
                removed.dataContextProperty,
                propertyChangedHandler_);
        }
    }
    (void)removed.descriptor.target->RemoveValueChangedHandler(
        removed.descriptor.targetProperty, propertyChangedHandler_);
    for (std::uint32_t current = index + 1U;
         current < bindings_.Size();
         ++current) {
        bindings_[current - 1U] = std::move(bindings_[current]);
    }
    bindings_.PopBack();
}

} // namespace Aero::GuiPrivate::Detail
