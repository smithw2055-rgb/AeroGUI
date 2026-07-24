#include <Aero/Markup/XamlDependencyProperty.hpp>

#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>

namespace Aero::Markup {
namespace {

constexpr const char* MessageBridgeFrozen =
    "XAML dependency-property bridge must be configured before schema freeze";
constexpr const char* MessageInvalidTypeRegistration =
    "XAML dependency-object type registration is invalid";
constexpr const char* MessageRegistryNotReady =
    "DependencyPropertyRegistry and metadata descriptors must be frozen before bridging";
constexpr const char* MessageTargetNotDependencyObject =
    "XAML target object is not registered as a DependencyObject";
constexpr const char* MessagePropertyNotBridged =
    "XAML member is not mapped to a dependency property";
constexpr const char* MessageUnsupportedValue =
    "XAML value kind cannot be represented by PropertyValue";
constexpr const char* MessageRuntimeMismatch =
    "XAML dependency-object runtime metadata does not match the activation result";

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<Base::Ref<Base::Object>> ActivateAeroControl(
    Core::TypeId requestedType,
    const XamlActivationContext& activation,
    void*) noexcept {
    if (!activation.IsCompatible() || activation.dispatcher == nullptr ||
        activation.dependencyProperties == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Aero presentation activation services are missing");
    }
#define AERO_ACTIVATE_CONTROL(type) \
    if (requestedType == Controls::type::StaticTypeId()) { \
        Base::Result<Base::Ref<Controls::type>> made = \
            Base::MakeRef<Controls::type>(); \
        if (!made) return made.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(made).Value()); \
    }
    AERO_ACTIVATE_CONTROL(StackPanel)
    AERO_ACTIVATE_CONTROL(Canvas)
    AERO_ACTIVATE_CONTROL(Grid)
    AERO_ACTIVATE_CONTROL(Border)
    AERO_ACTIVATE_CONTROL(TextBlock)
    AERO_ACTIVATE_CONTROL(ContentPresenter)
    AERO_ACTIVATE_CONTROL(UserControl)
#undef AERO_ACTIVATE_CONTROL
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Requested type is not a constructible Aero presentation type");
}

Presentation::Visual* AsVisual(Base::Object& object, void*) noexcept {
    return static_cast<Presentation::Visual*>(&object);
}

Base::Result<void> SetDecoratorContent(
    Base::Object& parentObject,
    const Base::Ref<Base::Object>& childObject,
    Presentation::UIElement& child,
    void*) noexcept {
    return static_cast<Controls::Decorator&>(parentObject).SetOwnedChild(
        childObject, child);
}

Base::Result<void> ClearDecoratorContent(
    Base::Object& parentObject, void*) noexcept {
    return static_cast<Controls::Decorator&>(parentObject).SetChild(nullptr);
}

Base::Result<void> SetContentControlContent(
    Base::Object& parentObject,
    const Base::Ref<Base::Object>& childObject,
    Presentation::UIElement& child,
    void*) noexcept {
    return static_cast<Controls::ContentControl&>(parentObject).SetOwnedContent(
        childObject, child);
}

Base::Result<void> ClearContentControlContent(
    Base::Object& parentObject, void*) noexcept {
    return static_cast<Controls::ContentControl&>(parentObject).SetContent(nullptr);
}

Base::Result<void> SetPresenterContent(
    Base::Object& parentObject,
    const Base::Ref<Base::Object>& childObject,
    Presentation::UIElement& child,
    void*) noexcept {
    return static_cast<Controls::ContentPresenter&>(parentObject).SetOwnedContent(
        childObject, child);
}

Base::Result<void> ClearPresenterContent(
    Base::Object& parentObject, void*) noexcept {
    return static_cast<Controls::ContentPresenter&>(parentObject).SetContent(nullptr);
}

Base::Result<void> AddPanelChild(
    Base::Object& parentObject,
    const Base::Ref<Base::Object>& childObject,
    Presentation::UIElement& child,
    void*) noexcept {
    return static_cast<Controls::Panel&>(parentObject).AddOwnedChild(
        childObject, child);
}

Base::Result<void> ClearPanelChildren(
    Base::Object& parentObject, void*) noexcept {
    return static_cast<Controls::Panel&>(parentObject).ClearOwnedChildren();
}

} // namespace

XamlDependencyPropertyBridge::XamlDependencyPropertyBridge(
    XamlSchemaContext& schema,
    Core::DependencyPropertyRegistry& properties) noexcept
    : schema_(&schema), properties_(&properties), types_() {}

Base::Result<void> XamlDependencyPropertyBridge::TryRegisterType(
    const XamlDependencyObjectTypeRegistration& registration) noexcept {
    if (schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, MessageBridgeFrozen);
    }
    const Core::MetadataTypeDescriptor* type =
        schema_->Descriptors().FindType(registration.type);
    if (type == nullptr || HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType) ||
        registration.cast == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, MessageInvalidTypeRegistration);
    }
    for (const XamlDependencyObjectTypeRegistration& current : types_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "XAML dependency-object type is already registered");
        }
    }
    return types_.TryPushBack(registration);
}

Base::Result<std::uint32_t>
XamlDependencyPropertyBridge::TryRegisterProperties() noexcept {
    if (schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, MessageBridgeFrozen);
    }
    if (!schema_->Descriptors().IsSealed() || !properties_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, MessageRegistryNotReady);
    }
    if (providerRegistered_) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "XAML dependency-property provider is already registered");
    }

    std::uint32_t registered = 0U;
    for (const Core::MetadataTypeDescriptor& type :
         schema_->Descriptors().Types()) {
        for (Core::MemberId memberId : type.DeclaredProperties()) {
            const Core::MetadataPropertyDescriptor* member =
                schema_->Descriptors().FindProperty(memberId);
            if (member == nullptr) continue;
            const Core::DependencyProperty* property = properties_->Find(
                type.Id(), member->Name());
            if (property == nullptr ||
                schema_->FindMemberAdapter(member->Id()) != nullptr) continue;
            if (registered == UINT32_MAX) {
                return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                    "XAML dependency-property adapter count overflow");
            }
            ++registered;
        }
    }

    if (types_.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "A DependencyObject base type must be registered before properties");
    }
    Core::TypeId providerType = Core::InvalidTypeId;
    for (const XamlDependencyObjectTypeRegistration& candidate : types_) {
        bool coversAll = true;
        for (const XamlDependencyObjectTypeRegistration& current : types_) {
            if (!schema_->Descriptors().IsDerivedFrom(
                    current.type, candidate.type)) {
                coversAll = false;
                break;
            }
        }
        if (coversAll) {
            providerType = candidate.type;
            break;
        }
    }
    if (providerType == Core::InvalidTypeId) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "DependencyObject bridge registrations have no common metadata base");
    }
    if (schema_->Runtime() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency-property bridge requires MetadataRuntime");
    }
    Base::Result<void> providerResult =
        Core::TryRegisterDependencyPropertyRuntimeProvider(
            *schema_->Runtime(), *properties_, providerType);
    if (!providerResult) return providerResult.GetStatus();
    registeredPropertyCount_ = registered;
    providerRegistered_ = true;
    return registered;
}

Core::DependencyObject* AsDependencyObject(
    Base::Object& object, void*) noexcept {
    return static_cast<Core::DependencyObject*>(&object);
}

bool XamlDependencyPropertyBridge::IsTypeRegistered(
    Core::TypeId type) const noexcept {
    return FindTypeRegistration(type) != nullptr;
}

const XamlDependencyObjectTypeRegistration*
XamlDependencyPropertyBridge::FindTypeRegistration(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        for (const XamlDependencyObjectTypeRegistration& registration : types_) {
            if (registration.type == current) return &registration;
        }
        const Core::MetadataTypeDescriptor* info =
            schema_->Descriptors().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Core::PropertyValue>
XamlDependencyPropertyBridge::ConvertValue(
    const XamlValue& value,
    const Core::DependencyProperty& property) const noexcept {
    if (value.Type() == property.ValueType() && !value.IsUnset()) return value;
    switch (value.Kind()) {
    case XamlValueKind::Boolean:
        return Core::PropertyValue::FromBoolean(
            property.ValueType(), value.AsBoolean());
    case XamlValueKind::SignedInteger:
        return Core::PropertyValue::FromSignedInteger(
            property.ValueType(), value.AsSignedInteger());
    case XamlValueKind::UnsignedInteger:
        return Core::PropertyValue::FromUnsignedInteger(
            property.ValueType(), value.AsUnsignedInteger());
    case XamlValueKind::Double:
        return Core::PropertyValue::FromDouble(
            property.ValueType(), value.AsDouble());
    case XamlValueKind::Object:
        return value.IsNullObject()
            ? Core::PropertyValue::NullObject(property.ValueType())
            : Core::PropertyValue::FromObject(value.Type(), value.AsObject());
    case XamlValueKind::String:
    case XamlValueKind::Custom:
    case XamlValueKind::None:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported, MessageUnsupportedValue);
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError, MessageUnsupportedValue);
}

Base::Result<std::uint32_t> TryRegisterAeroPresentationXaml(
    XamlDependencyPropertyBridge& bridge) noexcept {
    Base::Result<void> type = bridge.TryRegisterType({
        Core::TypeOf<Core::DependencyObject>(), &AsDependencyObject, nullptr});
    if (!type) return type.GetStatus();
    return bridge.TryRegisterProperties();
}

Base::Result<std::uint32_t> TryRegisterAeroPresentationXaml(
    XamlDependencyPropertyBridge& bridge,
    XamlActivationProviderRegistry& activation,
    XamlVisualTreeHost* visualTree) noexcept {
    Base::Result<std::uint32_t> bridged = TryRegisterAeroPresentationXaml(bridge);
    if (!bridged) return bridged.GetStatus();

    const Core::TypeId activatable[] = {
        Core::TypeOf<Controls::StackPanel>(), Core::TypeOf<Controls::Canvas>(),
        Core::TypeOf<Controls::Grid>(), Core::TypeOf<Controls::Border>(),
        Core::TypeOf<Controls::TextBlock>(),
        Core::TypeOf<Controls::ContentPresenter>(),
        Core::TypeOf<Controls::UserControl>()};
    for (Core::TypeId type : activatable) {
        Base::Result<void> registered = activation.TryRegister({
            type, &ActivateAeroControl, nullptr});
        if (!registered) return registered.GetStatus();
    }
    std::uint32_t count = bridged.Value() + 7U;
    if (visualTree == nullptr) return count;

    Base::Result<void> registered = visualTree->TryRegisterType({
        Core::TypeOf<Presentation::Visual>(), &AsVisual, nullptr});
    if (!registered) return registered.GetStatus();
    registered = visualTree->TryRegisterSingleContent({
        Core::TypeOf<Controls::Decorator>(),
        &SetDecoratorContent, &ClearDecoratorContent, nullptr});
    if (!registered) return registered.GetStatus();
    registered = visualTree->TryRegisterSingleContent({
        Core::TypeOf<Controls::ContentControl>(),
        &SetContentControlContent, &ClearContentControlContent, nullptr});
    if (!registered) return registered.GetStatus();
    registered = visualTree->TryRegisterSingleContent({
        Core::TypeOf<Controls::ContentPresenter>(),
        &SetPresenterContent, &ClearPresenterContent, nullptr});
    if (!registered) return registered.GetStatus();

    const Core::MemberId panelChildren = Core::MakeMemberId(
        Core::TypeOf<Controls::Panel>(), Core::MemberKind::Property,
        Base::StringView("Children"));
    registered = visualTree->TryRegisterCollectionContent({
        Core::TypeOf<Controls::Panel>(), panelChildren,
        &AddPanelChild, &ClearPanelChildren, nullptr, nullptr});
    if (!registered) return registered.GetStatus();
    registered = visualTree->Register(activation.Schema());
    if (!registered) return registered.GetStatus();
    return count + 5U;
}

bool XamlDependencyPropertyBridge::HandlesDependencyProperty(
    const XamlResolvedMember& member,
    void* context) noexcept {
    auto* bridge = static_cast<XamlDependencyPropertyBridge*>(context);
    return bridge != nullptr && member.kind == Core::MemberKind::Property &&
        bridge->properties_->Find({member.id}) != nullptr;
}

Base::Result<void> XamlDependencyPropertyBridge::SetDependencyProperty(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    auto* bridge = static_cast<XamlDependencyPropertyBridge*>(context);
    if (bridge == nullptr || services.targetObject != &object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, MessageTargetNotDependencyObject);
    }
    const Core::DependencyPropertyHandle propertyHandle{services.targetMember};
    const Core::DependencyProperty* property =
        bridge->properties_->Find(propertyHandle);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, MessagePropertyNotBridged);
    }
    const XamlDependencyObjectTypeRegistration* type =
        bridge->FindTypeRegistration(services.targetObjectType);
    if (type == nullptr || type->cast == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, MessageTargetNotDependencyObject);
    }
    Core::DependencyObject* dependencyObject = type->cast(object, type->context);
    if (dependencyObject == nullptr ||
        dependencyObject->RuntimeType() != services.targetObjectType ||
        &dependencyObject->PropertyRegistry() != bridge->properties_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, MessageRuntimeMismatch);
    }
    Base::Result<Core::PropertyValue> converted =
        bridge->ConvertValue(value, *property);
    if (!converted) return converted.GetStatus();
    return dependencyObject->SetValue(propertyHandle, converted.Value());
}

} // namespace Aero::Markup
