#include <Aero/Markup/XamlStyle.hpp>

#include <Aero/Base/String.hpp>

#include <utility>

namespace Aero::Markup {
namespace {

constexpr std::uint32_t InvalidIndex = UINT32_MAX;

Base::Status InvalidStyleXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<XamlValue> CloneValue(const XamlValue& value) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML setter value is empty");
    }
    return value;
}

Base::Result<Core::PropertyValue> ToPropertyValue(
    const XamlValue& value,
    Core::TypeId expectedType) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML setter value cannot be represented as a dependency-property value");
    }
    if (value.IsNullObject() && value.Type() != expectedType) {
        return Core::PropertyValue::NullObject(expectedType);
    }
    return value;
}

Base::Result<void> ResolveQualifiedType(
    const XamlServiceProvider& services,
    Base::StringView name,
    Core::TypeId& output) noexcept {
    if (services.schema == nullptr || name.Empty()) {
        return InvalidStyleXaml("Style TargetType is invalid");
    }

    std::uint32_t colon = name.SizeBytes();
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == ':') {
            if (colon != name.SizeBytes()) {
                return InvalidStyleXaml("Style TargetType contains multiple prefixes");
            }
            colon = index;
        }
    }

    Base::StringView prefix;
    Base::StringView localName = name;
    if (colon != name.SizeBytes()) {
        if (colon == 0U || colon + 1U >= name.SizeBytes()) {
            return InvalidStyleXaml("Style TargetType prefix is malformed");
        }
        prefix = name.Substr(0U, colon);
        localName = name.Substr(colon + 1U, name.SizeBytes() - colon - 1U);
    }

    Base::Result<Base::StringView> ns = services.namespaces.Lookup(prefix);
    if (!ns) {
        return ns.GetStatus();
    }
    const Core::TypeInfo* type = services.schema->Types().FindType(
        ns.Value(), localName);
    if (type == nullptr || HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style TargetType was not found or is not an object type");
    }
    output = type->Id();
    return {};
}

} // namespace

class XamlStyleExtension::SetterObject final : public Base::Object {
public:
    explicit SetterObject(Core::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

    Base::MetaTypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }

    Base::Result<void> SetProperty(
        Base::StringView property) noexcept {
        if (property.Empty()) {
            return InvalidStyleXaml("Setter Property is empty");
        }
        propertySet_ = true;
        return property_.TryAssign(property);
    }

    Base::Result<void> SetValue(
        const XamlValue& value) noexcept {
        Base::Result<XamlValue> copied = CloneValue(value);
        if (!copied) {
            return copied.GetStatus();
        }
        value_ = std::move(copied).Value();
        valueSet_ = true;
        return {};
    }

    Base::StringView Property() const noexcept {
        return property_.View();
    }
    const XamlValue& Value() const noexcept { return value_; }
    bool IsConfigured() const noexcept {
        return propertySet_ && valueSet_;
    }

private:
    Core::TypeId runtimeType_ = Core::InvalidTypeId;
    Base::String property_;
    XamlValue value_;
    bool propertySet_ = false;
    bool valueSet_ = false;
};

class XamlStyleExtension::StyleObject final : public Base::Object {
public:
    explicit StyleObject(Core::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType),
          plan_(Core::InvalidTypeId),
          setters_() {}

    Base::MetaTypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }

    Core::Style& Plan() noexcept { return plan_; }
    const Core::Style& Plan() const noexcept { return plan_; }

    Base::Result<void> SetBasedOn(
        const Base::Ref<Base::Object>& value,
        const StyleObject& style) noexcept {
        basedOn_ = value;
        return plan_.TrySetBasedOn(&style.Plan());
    }

    Base::Result<void> AddSetter(
        const Base::Ref<Base::Object>& setter) noexcept {
        return setters_.TryPushBack(setter);
    }

    Base::Span<const Base::Ref<Base::Object>> Setters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }

private:
    Core::TypeId runtimeType_ = Core::InvalidTypeId;
    Core::Style plan_;
    Base::Ref<Base::Object> basedOn_;
    Base::Vector<Base::Ref<Base::Object>> setters_;
};

XamlStyleExtension::XamlStyleExtension(
    const XamlStyleExtensionOptions& options) noexcept
    : options_(options), applications_() {}

XamlStyleExtension::~XamlStyleExtension() noexcept {
    for (Application& application : applications_) {
        if (application.object != nullptr && options_.styles != nullptr) {
            const Base::Result<bool> detached = options_.styles->DetachObject(
                *application.object);
            (void)detached;
        }
    }
}

Base::Result<void> XamlStyleExtension::Register(
    XamlSchemaContext& schema,
    XamlActivationProviderRegistry& activation,
    Core::TypeId styleType,
    Core::TypeId setterType,
    Core::DependencyPropertyHandle styleProperty) noexcept {
    if (schema.IsFrozen() || activation.IsFrozen() ||
        options_.styles == nullptr || options_.properties == nullptr ||
        options_.asDependencyObject == nullptr || !styleProperty.IsValid() ||
        !options_.properties->IsFrozen() || !options_.properties->Types().IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML Style extension registries are not ready");
    }
    if (&activation.Schema() != &schema || schema_ != nullptr ||
        styleType == Core::InvalidTypeId || setterType == Core::InvalidTypeId) {
        return InvalidStyleXaml("XAML Style extension registration is invalid");
    }

    const Core::TypeInfo* styleInfo = schema.Types().FindType(styleType);
    const Core::TypeInfo* setterInfo = schema.Types().FindType(setterType);
    if (styleInfo == nullptr || setterInfo == nullptr ||
        HasTypeFlag(styleInfo->Flags(), Core::TypeFlags::ValueType) ||
        HasTypeFlag(setterInfo->Flags(), Core::TypeFlags::ValueType)) {
        return InvalidStyleXaml("XAML Style and Setter must be registered object types");
    }

    const Core::PropertyInfo* targetType = schema.Types().FindProperty(
        styleType, Base::StringView("TargetType"), false);
    const Core::PropertyInfo* basedOn = schema.Types().FindProperty(
        styleType, Base::StringView("BasedOn"), false);
    const Core::PropertyInfo* setters = schema.Types().FindProperty(
        styleType, Base::StringView("Setters"), false);
    const Core::PropertyInfo* property = schema.Types().FindProperty(
        setterType, Base::StringView("Property"), false);
    const Core::PropertyInfo* value = schema.Types().FindProperty(
        setterType, Base::StringView("Value"), false);
    if (targetType == nullptr || basedOn == nullptr || setters == nullptr ||
        property == nullptr || value == nullptr ||
        basedOn->ValueType() != styleType || setters->ValueType() != setterType ||
        schema.Types().FindContentMember(styleType) != setters->Id()) {
        return InvalidStyleXaml("XAML Style metadata members are invalid");
    }
    const Core::DependencyProperty* styleDependency = options_.properties->Find(
        styleProperty);
    if (styleDependency == nullptr || styleDependency->ValueType() != styleType) {
        return InvalidStyleXaml("XAML Style property does not accept the registered Style type");
    }

    schema_ = &schema;
    styleType_ = styleType;
    setterType_ = setterType;
    styleProperty_ = styleProperty;
    targetTypeMember_ = targetType->Id();
    basedOnMember_ = basedOn->Id();
    settersMember_ = setters->Id();
    setterPropertyMember_ = property->Id();
    setterValueMember_ = value->Id();

    const XamlMemberAdapterRegistration members[] = {
        {targetTypeMember_, XamlMemberWriteMode::SetOnce, nullptr, this, &SetTargetType, true},
        {basedOnMember_, XamlMemberWriteMode::SetOnce, &SetBasedOn, this, nullptr},
        {settersMember_, XamlMemberWriteMode::Collection, &AddSetter, this, nullptr},
        {setterPropertyMember_, XamlMemberWriteMode::SetOnce, &SetSetterProperty, this, nullptr},
        {setterValueMember_, XamlMemberWriteMode::SetOnce, &SetSetterValue, this, nullptr, true},
        {styleProperty.value, XamlMemberWriteMode::SetOnce, nullptr, this, &SetStyleMember}
    };
    for (const XamlMemberAdapterRegistration& member : members) {
        Base::Result<void> registered = schema.TryRegisterMemberAdapter(member);
        if (!registered) {
            return registered.GetStatus();
        }
    }
    Base::Result<void> styleAdapter = schema.TryRegisterTypeAdapter({
        styleType_, nullptr, &EndStyleInit, nullptr, this});
    if (!styleAdapter) {
        return styleAdapter.GetStatus();
    }
    Base::Result<void> setterAdapter = schema.TryRegisterTypeAdapter({
        setterType_, nullptr, nullptr, nullptr, this});
    if (!setterAdapter) {
        return setterAdapter.GetStatus();
    }
    Base::Result<void> styleActivation = activation.TryRegister({
        styleType_, &ActivateStyle, this});
    if (!styleActivation) {
        return styleActivation.GetStatus();
    }
    return activation.TryRegister({setterType_, &ActivateSetter, this});
}

Base::Result<bool> XamlStyleExtension::DetachObject(
    Core::DependencyObject& object) noexcept {
    const std::uint32_t index = FindApplication(object);
    if (index == InvalidIndex) {
        return false;
    }
    Base::Ref<Base::Object> style = applications_[index].style;
    Base::Result<bool> detached = options_.styles->DetachObject(object);
    if (!detached) {
        return detached.GetStatus();
    }
    Base::Result<void> cleared = object.ClearValue(styleProperty_);
    if (!cleared) {
        const StyleObject* restored = static_cast<const StyleObject*>(style.Get());
        if (restored != nullptr) {
            const Base::Result<void> reapplied = options_.styles->Apply(
                object, restored->Plan());
            (void)reapplied;
        }
        return cleared.GetStatus();
    }
    RemoveApplication(index);
    return detached.Value();
}

Base::Result<void> XamlStyleExtension::FinalizeStyle(
    StyleObject& style) noexcept {
    if (schema_ == nullptr || options_.properties == nullptr ||
        style.Plan().TargetType() == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style TargetType must be assigned before initialization completes");
    }
    for (const Base::Ref<Base::Object>& entry : style.Setters()) {
        const SetterObject* setter = static_cast<const SetterObject*>(entry.Get());
        if (setter == nullptr || !setter->IsConfigured()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Setter requires Property and Value");
        }
        const Core::DependencyProperty* property = options_.properties->Find(
            style.Plan().TargetType(), setter->Property());
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Style Setter property was not found on TargetType");
        }

        const XamlValue* candidate = &setter->Value();
        Base::Result<XamlValue> converted = Base::Status::Failure(
            Base::ErrorCode::InvalidState, "Style setter conversion was not attempted");
        if (candidate->Kind() == XamlValueKind::String) {
            converted = schema_->ConvertText(property->ValueType(), candidate->AsString());
            if (!converted) {
                return converted.GetStatus();
            }
            candidate = &converted.Value();
        }
        Base::Result<Core::PropertyValue> value = ToPropertyValue(
            *candidate, property->ValueType());
        if (!value) {
            return value.GetStatus();
        }
        Base::Result<void> added = style.Plan().TryAddSetter(
            property->Handle(), value.Value());
        if (!added) {
            return added.GetStatus();
        }
    }
    return style.Plan().Seal(*options_.properties);
}

Base::Result<void> XamlStyleExtension::ApplyStyle(
    Core::DependencyObject& object,
    const Base::Ref<Base::Object>& value) noexcept {
    const std::uint32_t existing = FindApplication(object);
    if (!value) {
        if (existing == InvalidIndex) {
            return object.ClearValue(styleProperty_);
        }
        Base::Ref<Base::Object> old = applications_[existing].style;
        Base::Result<void> cleared = options_.styles->Clear(
            object,
            static_cast<StyleObject*>(applications_[existing].style.Get())->Plan());
        if (!cleared) {
            return cleared.GetStatus();
        }
        cleared = object.ClearValue(styleProperty_);
        if (!cleared) {
            const StyleObject* restored = static_cast<const StyleObject*>(old.Get());
            if (restored != nullptr) {
                const Base::Result<void> reapplied = options_.styles->Apply(
                    object, restored->Plan());
                (void)reapplied;
            }
            return cleared.GetStatus();
        }
        RemoveApplication(existing);
        return {};
    }
    StyleObject* style = static_cast<StyleObject*>(value.Get());
    if (style == nullptr || !style->Plan().IsSealed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML Style must be sealed before it can be applied");
    }

    if (existing == InvalidIndex) {
        Base::Result<void> reserved = applications_.TryPushBack({&object, value});
        if (!reserved) {
            return reserved.GetStatus();
        }
        Base::Result<void> applied = options_.styles->Apply(object, style->Plan());
        if (!applied) {
            applications_.PopBack();
            return applied.GetStatus();
        }
        applied = object.SetValue(
            styleProperty_, Core::PropertyValue::FromObject(styleType_, value));
        if (!applied) {
            const Base::Result<void> cleared = options_.styles->Clear(
                object, style->Plan());
            (void)cleared;
            applications_.PopBack();
            return applied.GetStatus();
        }
        return {};
    }

    Base::Ref<Base::Object> old = applications_[existing].style;
    Base::Result<void> applied = options_.styles->Apply(object, style->Plan());
    if (!applied) {
        return applied.GetStatus();
    }
    applied = object.SetValue(
        styleProperty_, Core::PropertyValue::FromObject(styleType_, value));
    if (!applied) {
        const StyleObject* restored = static_cast<const StyleObject*>(old.Get());
        if (restored != nullptr) {
            const Base::Result<void> reapplied = options_.styles->Apply(
                object, restored->Plan());
            (void)reapplied;
        }
        return applied.GetStatus();
    }
    applications_[existing].style = value;
    return {};
}

std::uint32_t XamlStyleExtension::FindApplication(
    const Core::DependencyObject& object) const noexcept {
    for (std::uint32_t index = 0U; index < applications_.Size(); ++index) {
        if (applications_[index].object == &object) {
            return index;
        }
    }
    return InvalidIndex;
}

void XamlStyleExtension::RemoveApplication(std::uint32_t index) noexcept {
    if (index + 1U != applications_.Size()) {
        applications_[index] = std::move(applications_[applications_.Size() - 1U]);
    }
    applications_.PopBack();
}

Base::Result<Base::Ref<Base::Object>> XamlStyleExtension::ActivateStyle(
    Core::TypeId requestedType,
    const XamlActivationContext&,
    void* context) noexcept {
    XamlStyleExtension* extension = static_cast<XamlStyleExtension*>(context);
    if (extension == nullptr || requestedType != extension->styleType_) {
        return InvalidStyleXaml("XAML Style activation type is invalid");
    }
    Base::Result<Base::Ref<StyleObject>> created = Base::MakeRef<StyleObject>(requestedType);
    if (!created) {
        return created.GetStatus();
    }
    return Base::Ref<Base::Object>(std::move(created).Value());
}

Base::Result<Base::Ref<Base::Object>> XamlStyleExtension::ActivateSetter(
    Core::TypeId requestedType,
    const XamlActivationContext&,
    void* context) noexcept {
    XamlStyleExtension* extension = static_cast<XamlStyleExtension*>(context);
    if (extension == nullptr || requestedType != extension->setterType_) {
        return InvalidStyleXaml("XAML Setter activation type is invalid");
    }
    Base::Result<Base::Ref<SetterObject>> created = Base::MakeRef<SetterObject>(requestedType);
    if (!created) {
        return created.GetStatus();
    }
    return Base::Ref<Base::Object>(std::move(created).Value());
}

Base::Result<void> XamlStyleExtension::SetTargetType(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    XamlStyleExtension* extension = static_cast<XamlStyleExtension*>(context);
    if (extension == nullptr) {
        return InvalidStyleXaml("Style TargetType requires an extension context");
    }
    if (value.Kind() == XamlValueKind::String) {
        Core::TypeId type = Core::InvalidTypeId;
        Base::Result<void> resolved = ResolveQualifiedType(
            services, value.AsString(), type);
        if (!resolved) {
            return resolved.GetStatus();
        }
        return static_cast<StyleObject&>(object).Plan().TrySetTargetType(type);
    }
    if (value.Kind() == XamlValueKind::UnsignedInteger &&
        extension->options_.typeReferenceType != Core::InvalidTypeId &&
        value.Type() == extension->options_.typeReferenceType) {
        const Core::TypeId type = value.AsUnsignedInteger();
        const Core::TypeInfo* info = services.schema != nullptr
            ? services.schema->Types().FindType(type) : nullptr;
        if (info == nullptr || HasTypeFlag(info->Flags(), Core::TypeFlags::ValueType)) {
            return InvalidStyleXaml("Style TargetType token is invalid");
        }
        return static_cast<StyleObject&>(object).Plan().TrySetTargetType(type);
    }
    return InvalidStyleXaml("Style TargetType expects a type name or x:Type token");
}

Base::Result<void> XamlStyleExtension::SetBasedOn(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    XamlStyleExtension* extension = static_cast<XamlStyleExtension*>(context);
    if (extension == nullptr || value.Kind() != XamlValueKind::Object ||
        !value.AsObject() || value.Type() != extension->styleType_) {
        return InvalidStyleXaml("Style BasedOn expects a Style object");
    }
    return static_cast<StyleObject&>(object).SetBasedOn(
        value.AsObject(),
        static_cast<const StyleObject&>(*value.AsObject()));
}

Base::Result<void> XamlStyleExtension::AddSetter(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    XamlStyleExtension* extension = static_cast<XamlStyleExtension*>(context);
    if (extension == nullptr || value.Kind() != XamlValueKind::Object ||
        !value.AsObject() || value.Type() != extension->setterType_) {
        return InvalidStyleXaml("Style Setters expects a Setter object");
    }
    return static_cast<StyleObject&>(object).AddSetter(value.AsObject());
}

Base::Result<void> XamlStyleExtension::SetSetterProperty(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    if (context == nullptr || value.Kind() != XamlValueKind::String) {
        return InvalidStyleXaml("Setter Property expects a string");
    }
    return static_cast<SetterObject&>(object).SetProperty(value.AsString());
}

Base::Result<void> XamlStyleExtension::SetSetterValue(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    if (context == nullptr) {
        return InvalidStyleXaml("Setter Value requires an extension context");
    }
    return static_cast<SetterObject&>(object).SetValue(value);
}

Base::Result<void> XamlStyleExtension::EndStyleInit(
    Base::Object& object,
    void* context) noexcept {
    XamlStyleExtension* extension = static_cast<XamlStyleExtension*>(context);
    if (extension == nullptr) {
        return InvalidStyleXaml("Style initialization requires an extension context");
    }
    return extension->FinalizeStyle(static_cast<StyleObject&>(object));
}

Base::Result<void> XamlStyleExtension::SetStyleMember(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider&,
    void* context) noexcept {
    XamlStyleExtension* extension = static_cast<XamlStyleExtension*>(context);
    if (extension == nullptr || extension->options_.asDependencyObject == nullptr ||
        value.Kind() != XamlValueKind::Object) {
        return InvalidStyleXaml("Style property expects a Style object or x:Null");
    }
    Core::DependencyObject* target = extension->options_.asDependencyObject(
        object, extension->options_.castContext);
    if (target == nullptr || &target->PropertyRegistry() != extension->options_.properties) {
        return InvalidStyleXaml("Style property target is not a registered DependencyObject");
    }
    if (!value.IsNullObject() && value.Type() != extension->styleType_) {
        return InvalidStyleXaml("Style property received an incompatible object type");
    }
    return extension->ApplyStyle(*target, value.AsObject());
}

} // namespace Aero::Markup
