#include <Aero/Markup/XamlBinding.hpp>

#include <Aero/Base/StringView.hpp>

namespace Aero::Markup {
namespace {

constexpr Base::StringView ElementNameKey("ElementName");
constexpr Base::StringView PathKey("Path");
constexpr Base::StringView ModeKey("Mode");
constexpr Base::StringView OneTimeMode("OneTime");
constexpr Base::StringView OneWayMode("OneWay");
constexpr Base::StringView TwoWayMode("TwoWay");
constexpr Base::StringView OneWayToSourceMode("OneWayToSource");

Base::StringView TrimAscii(Base::StringView value) noexcept {
    std::uint32_t first = 0U;
    std::uint32_t last = value.SizeBytes();
    while (first < last &&
           (value[first] == ' ' || value[first] == '\t' ||
            value[first] == '\r' || value[first] == '\n')) {
        ++first;
    }
    while (last > first &&
           (value[last - 1U] == ' ' || value[last - 1U] == '\t' ||
            value[last - 1U] == '\r' || value[last - 1U] == '\n')) {
        --last;
    }
    return value.Substr(first, last - first);
}

Base::Result<XamlValue> ToXamlValue(
    const Core::PropertyValue& value) noexcept {
    switch (value.Kind()) {
    case Core::PropertyValueKind::Boolean:
        return XamlValue::FromBoolean(value.Type(), value.AsBoolean());
    case Core::PropertyValueKind::SignedInteger:
        return XamlValue::FromSignedInteger(value.Type(), value.AsSignedInteger());
    case Core::PropertyValueKind::UnsignedInteger:
        return XamlValue::FromUnsignedInteger(value.Type(), value.AsUnsignedInteger());
    case Core::PropertyValueKind::Double:
        return XamlValue::FromDouble(value.Type(), value.AsDouble());
    case Core::PropertyValueKind::Object:
        if (value.AsObject()) {
            return XamlValue::FromObject(value.Type(), value.AsObject());
        }
        return XamlValue::NullObject(value.Type());
    case Core::PropertyValueKind::Unset:
        break;
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Binding target has no representable initial value");
}

Base::Result<void> ParseArguments(
    Base::StringView arguments,
    Base::StringView& elementName,
    Base::StringView& path,
    Core::BindingMode& mode) noexcept {
    elementName = {};
    path = {};
    mode = Core::BindingMode::OneWay;

    std::uint32_t begin = 0U;
    while (begin < arguments.SizeBytes()) {
        std::uint32_t end = begin;
        while (end < arguments.SizeBytes() && arguments[end] != ',') {
            ++end;
        }
        const Base::StringView item = TrimAscii(arguments.Substr(begin, end - begin));
        const std::uint32_t equals = [&item]() noexcept {
            for (std::uint32_t index = 0U; index < item.SizeBytes(); ++index) {
                if (item[index] == '=') {
                    return index;
                }
            }
            return item.SizeBytes();
        }();
        if (item.Empty() || equals == item.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding arguments must use key=value syntax");
        }
        const Base::StringView key = TrimAscii(item.Substr(0U, equals));
        const Base::StringView value = TrimAscii(item.Substr(
            equals + 1U,
            item.SizeBytes() - equals - 1U));
        if (value.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding argument value is empty");
        }
        if (key == ElementNameKey) {
            if (!elementName.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding ElementName is specified more than once");
            }
            elementName = value;
        } else if (key == PathKey) {
            if (!path.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding Path is specified more than once");
            }
            path = value;
        } else if (key == ModeKey) {
            if (value == OneTimeMode) {
                mode = Core::BindingMode::OneTime;
            } else if (value == OneWayMode) {
                mode = Core::BindingMode::OneWay;
            } else if (value == TwoWayMode) {
                mode = Core::BindingMode::TwoWay;
            } else if (value == OneWayToSourceMode) {
                mode = Core::BindingMode::OneWayToSource;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding mode is not supported");
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Binding argument is not supported");
        }
        begin = end + 1U;
    }
    if (path.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binding requires Path");
    }
    return {};
}

} // namespace

XamlBindingExtension::XamlBindingExtension(
    const XamlBindingExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlBindingExtension::Register(
    XamlSchemaContext& schema,
    Core::TypeId bindingExtensionType) noexcept {
    if (options_.bindings == nullptr || options_.asDependencyObject == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding markup extension options are incomplete");
    }
    return schema.TryRegisterMarkupExtension({
        bindingExtensionType,
        &XamlBindingExtension::ProvideValue,
        this});
}

Base::Result<XamlValue> XamlBindingExtension::ProvideValue(
    Base::StringView arguments,
    const XamlServiceProvider& services,
    void* context) noexcept {
    XamlBindingExtension* extension =
        static_cast<XamlBindingExtension*>(context);
    if (extension == nullptr || extension->options_.bindings == nullptr ||
        extension->options_.asDependencyObject == nullptr ||
        services.schema == nullptr || services.targetObject == nullptr ||
        services.nameScope == nullptr ||
        services.targetMember == Core::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding markup extension has no target service context");
    }

    Base::StringView elementName;
    Base::StringView path;
    Core::BindingMode mode = Core::BindingMode::OneWay;
    Base::Result<void> parsed = ParseArguments(arguments, elementName, path, mode);
    if (!parsed) {
        return parsed.GetStatus();
    }

    Core::DependencyObject* target = extension->options_.asDependencyObject(
        *services.targetObject, extension->options_.castContext);
    if (target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding target must be a DependencyObject instance");
    }

    Core::DependencyObject* source = nullptr;
    if (!elementName.Empty()) {
        Base::Object* sourceObject = services.nameScope->Find(elementName);
        if (sourceObject == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding ElementName was not found in the active NameScope");
        }
        source = extension->options_.asDependencyObject(
            *sourceObject, extension->options_.castContext);
    } else {
        if (!extension->options_.dataContextProperty.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Binding without ElementName requires a DataContext property");
        }
        Base::Result<Core::PropertyValue> dataContext = target->GetValue(
            extension->options_.dataContextProperty);
        if (!dataContext) {
            return dataContext.GetStatus();
        }
        if (dataContext.Value().Kind() != Core::PropertyValueKind::Object ||
            !dataContext.Value().AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding target has no DataContext object");
        }
        source = extension->options_.asDependencyObject(
            *dataContext.Value().AsObject(), extension->options_.castContext);
    }
    if (source == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding source must be a DependencyObject instance");
    }

    const Core::DependencyProperty* sourceProperty =
        source->PropertyRegistry().Find(source->RuntimeType(), path);
    const Core::DependencyPropertyHandle targetHandle{services.targetMember};
    const Core::DependencyProperty* targetProperty =
        target->PropertyRegistry().Find(targetHandle);
    if (sourceProperty == nullptr || targetProperty == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding source path or target dependency property was not found");
    }

    Base::Result<Core::BindingHandle> attached =
        extension->options_.bindings->Attach({
            source,
            sourceProperty->Handle(),
            target,
            targetHandle,
            mode});
    if (!attached) {
        return attached.GetStatus();
    }

    Base::Result<Core::PropertyValue> current = target->GetValue(targetHandle);
    if (!current) {
        return current.GetStatus();
    }
    return ToXamlValue(current.Value());
}

} // namespace Aero::Markup
