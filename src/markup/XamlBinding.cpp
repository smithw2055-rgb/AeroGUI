#include <Aero/Markup/XamlBinding.hpp>

#include <Aero/Base/StringView.hpp>

namespace Aero::Markup {
namespace {

constexpr Base::StringView ElementNameKey("ElementName");
constexpr Base::StringView PathKey("Path");
constexpr Base::StringView ModeKey("Mode");
constexpr Base::StringView UpdateSourceTriggerKey("UpdateSourceTrigger");
constexpr Base::StringView OneTimeMode("OneTime");
constexpr Base::StringView OneWayMode("OneWay");
constexpr Base::StringView TwoWayMode("TwoWay");
constexpr Base::StringView OneWayToSourceMode("OneWayToSource");
constexpr Base::StringView PropertyChangedTrigger("PropertyChanged");
constexpr Base::StringView ExplicitTrigger("Explicit");

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
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding target has no representable initial value");
    }
    return value;
}

Base::Result<void> ParseArguments(
    Base::StringView arguments,
    Base::StringView& elementName,
    Base::StringView& path,
    Presentation::BindingMode& mode,
    Core::UpdateSourceTrigger& updateSourceTrigger) noexcept {
    elementName = {};
    path = {};
    mode = Presentation::BindingMode::OneWay;
    updateSourceTrigger = Core::UpdateSourceTrigger::PropertyChanged;

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
                mode = Presentation::BindingMode::OneTime;
            } else if (value == OneWayMode) {
                mode = Presentation::BindingMode::OneWay;
            } else if (value == TwoWayMode) {
                mode = Presentation::BindingMode::TwoWay;
            } else if (value == OneWayToSourceMode) {
                mode = Presentation::BindingMode::OneWayToSource;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding mode is not supported");
            }
        } else if (key == UpdateSourceTriggerKey) {
            if (value == PropertyChangedTrigger) {
                updateSourceTrigger = Core::UpdateSourceTrigger::PropertyChanged;
            } else if (value == ExplicitTrigger) {
                updateSourceTrigger = Core::UpdateSourceTrigger::Explicit;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding update trigger is not supported");
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
    Presentation::BindingMode mode = Presentation::BindingMode::OneWay;
    Core::UpdateSourceTrigger updateSourceTrigger =
        Core::UpdateSourceTrigger::PropertyChanged;
    Base::Result<void> parsed = ParseArguments(
        arguments, elementName, path, mode, updateSourceTrigger);
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

    Base::Object* source = nullptr;
    if (!elementName.Empty()) {
        source = services.nameScope->Find(elementName);
        if (source == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding ElementName was not found in the active NameScope");
        }
    } else {
        if (!extension->options_.dataContextProperty.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Binding without ElementName requires a DataContext property");
        }
    }

    const Core::DependencyPropertyHandle targetHandle{services.targetMember};
    const Core::DependencyProperty* targetProperty =
        target->PropertyRegistry().Find(targetHandle);
    if (targetProperty == nullptr ||
        services.schema->Runtime() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target property or metadata runtime was not found");
    }

    Presentation::MetadataBindingDescriptor descriptor;
    descriptor.metadata = services.schema->Runtime();
    descriptor.source = source;
    descriptor.target = target;
    descriptor.targetProperty = targetHandle;
    descriptor.dataContextProperty =
        extension->options_.dataContextProperty;
    descriptor.path = path;
    descriptor.mode = mode;
    descriptor.updateSourceTrigger = updateSourceTrigger;
    Base::Result<Presentation::BindingHandle> attached =
        extension->options_.bindings->Attach(descriptor);
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
