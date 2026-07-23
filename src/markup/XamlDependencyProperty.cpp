#include <Aero/Markup/XamlDependencyProperty.hpp>

#include <Aero/Core/Controls.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>

namespace Aero::Markup {
namespace {

constexpr const char* MessageBridgeFrozen =
    "XAML dependency-property bridge must be configured before schema freeze";
constexpr const char* MessageInvalidTypeRegistration =
    "XAML dependency-object type registration is invalid";
constexpr const char* MessageRegistryNotReady =
    "DependencyPropertyRegistry and TypeRegistry must be frozen before bridging";
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

Base::Result<Base::Ref<Base::Object>> ActivateCoreControl(
    Core::TypeId requestedType,
    const XamlActivationContext& activation,
    Base::IAllocator& allocator,
    void*) noexcept {
    if (!activation.IsCompatible() || activation.dispatcher == nullptr ||
        activation.dependencyProperties == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Core presentation activation services are missing");
    }
    const Base::StringView ns = Core::AeroPresentationNamespaceUri();
#define AERO_ACTIVATE_CONTROL(name, type, ...) \
    if (requestedType == Core::MakeTypeId(ns, Base::StringView(name))) { \
        Base::Result<Base::Ref<Core::type>> made = \
            Base::MakeRefWithAllocator<Core::type>(allocator, \
                *activation.dispatcher, *activation.dependencyProperties, \
                requestedType, __VA_ARGS__ &allocator); \
        if (!made) return made.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(made).Value()); \
    }
    AERO_ACTIVATE_CONTROL("StackPanel", StackPanel, Core::Orientation::Vertical, )
    AERO_ACTIVATE_CONTROL("Canvas", Canvas, )
    AERO_ACTIVATE_CONTROL("Grid", Grid, )
    AERO_ACTIVATE_CONTROL("Border", Border, )
    AERO_ACTIVATE_CONTROL("TextBlock", TextBlock, )
    AERO_ACTIVATE_CONTROL("ContentPresenter", ContentPresenter, )
#undef AERO_ACTIVATE_CONTROL
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Requested type is not a core presentation control");
}

Core::TreeNode* AsCoreTreeNode(Base::Object& object, void*) noexcept {
    return &static_cast<Core::LayoutElement&>(object);
}
Core::LayoutElement* AsCoreLayoutElement(Base::Object& object, void*) noexcept {
    return &static_cast<Core::LayoutElement&>(object);
}
Core::RenderElement* AsCoreRenderElement(Base::Object& object, void*) noexcept {
    return &static_cast<Core::RenderElement&>(object);
}
Core::ContentPresenter* AsCoreContentPresenter(
    Base::Object& object, void*) noexcept {
    return &static_cast<Core::ContentPresenter&>(object);
}
Core::LayoutElement* AsCoreBorder(Base::Object& object, void*) noexcept {
    return &static_cast<Core::Border&>(object);
}
Core::StackPanel* AsCoreStackPanel(Base::Object& object, void*) noexcept {
    return &static_cast<Core::StackPanel&>(object);
}
Core::LayoutElement* AsCoreCollectionOwner(Base::Object& object, void*) noexcept {
    return &static_cast<Core::LayoutElement&>(object);
}

} // namespace

XamlDependencyPropertyBridge::XamlDependencyPropertyBridge(
    XamlSchemaContext& schema,
    Core::DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : schema_(&schema),
      properties_(&properties),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      types_(allocator_) {}

Base::Result<void> XamlDependencyPropertyBridge::TryRegisterType(
    const XamlDependencyObjectTypeRegistration& registration) noexcept {
    if (schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageBridgeFrozen);
    }
    const Core::TypeInfo* type = schema_->Types().FindType(registration.type);
    if (type == nullptr ||
        HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType) ||
        registration.cast == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidTypeRegistration);
    }
    for (const XamlDependencyObjectTypeRegistration& current : types_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML dependency-object type is already registered");
        }
    }
    return types_.TryPushBack(registration);
}

Base::Result<std::uint32_t>
XamlDependencyPropertyBridge::TryRegisterProperties() noexcept {
    if (schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageBridgeFrozen);
    }
    if (!schema_->Types().IsFrozen() || !properties_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageRegistryNotReady);
    }
    if (providerRegistered_) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML dependency-property provider is already registered");
    }

    std::uint32_t registered = 0U;
    for (const Core::TypeInfo& type : schema_->Types().Types()) {
        for (const Core::PropertyInfo& member : type.Properties()) {
            const Core::DependencyProperty* property = properties_->Find(
                type.Id(),
                member.Name());
            if (property == nullptr ||
                schema_->FindMemberAdapter(member.Id()) != nullptr) {
                continue;
            }
            if (registered == UINT32_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
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
            if (!schema_->Types().IsDerivedFrom(current.type, candidate.type)) {
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
            "DependencyObject bridge registrations have no common Meta base");
    }
    Base::Result<void> providerResult =
        Core::TryRegisterDependencyPropertyProvider(
            schema_->Members(), *properties_, providerType);
    if (!providerResult) {
        return providerResult.GetStatus();
    }
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
            if (registration.type == current) {
                return &registration;
            }
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) {
            break;
        }
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Core::PropertyValue>
XamlDependencyPropertyBridge::ConvertValue(
    const XamlValue& value,
    const Core::DependencyProperty& property) const noexcept {
    if (value.Type() == property.ValueType() && !value.IsUnset()) {
        return value;
    }
    switch (value.Kind()) {
    case XamlValueKind::Boolean:
        return Core::PropertyValue::FromBoolean(
            property.ValueType(),
            value.AsBoolean());
    case XamlValueKind::SignedInteger:
        return Core::PropertyValue::FromSignedInteger(
            property.ValueType(),
            value.AsSignedInteger());
    case XamlValueKind::UnsignedInteger:
        return Core::PropertyValue::FromUnsignedInteger(
            property.ValueType(),
            value.AsUnsignedInteger());
    case XamlValueKind::Double:
        return Core::PropertyValue::FromDouble(
            property.ValueType(),
            value.AsDouble());
    case XamlValueKind::Object:
        if (value.IsNullObject()) {
            return Core::PropertyValue::NullObject(property.ValueType());
        }
        return Core::PropertyValue::FromObject(
            value.Type(),
            value.AsObject());
    case XamlValueKind::String:
    case XamlValueKind::Custom:
    case XamlValueKind::None:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageUnsupportedValue);
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError,
        MessageUnsupportedValue);
}

Base::Result<std::uint32_t> TryRegisterCorePresentationXaml(
    XamlDependencyPropertyBridge& bridge) noexcept {
    const Core::TypeId dependencyObjectType = Core::MakeTypeId(
        Core::AeroPresentationNamespaceUri(), Base::StringView("DependencyObject"));
    Base::Result<void> type = bridge.TryRegisterType({
        dependencyObjectType, &AsDependencyObject, nullptr});
    if (!type) return type.GetStatus();
    return bridge.TryRegisterProperties();
}

Base::Result<std::uint32_t> TryRegisterCorePresentationXaml(
    XamlDependencyPropertyBridge& bridge,
    XamlActivationProviderRegistry& activation,
    XamlVisualTreeHost* visualTree) noexcept {
    Base::Result<std::uint32_t> bridged = TryRegisterCorePresentationXaml(bridge);
    if (!bridged) return bridged.GetStatus();
    const Base::StringView ns = Core::AeroPresentationNamespaceUri();
    const Base::StringView controlNames[] = {
        Base::StringView("StackPanel"), Base::StringView("Canvas"),
        Base::StringView("Grid"), Base::StringView("Border"),
        Base::StringView("TextBlock"), Base::StringView("ContentPresenter")
    };
    for (Base::StringView name : controlNames) {
        Base::Result<void> registered = activation.TryRegister({
            Core::MakeTypeId(ns, name), &ActivateCoreControl, nullptr});
        if (!registered) return registered.GetStatus();
    }
    std::uint32_t count = bridged.Value() + 6U;
    if (visualTree == nullptr) return count;

    const Core::TypeId stackPanel = Core::MakeTypeId(ns, Base::StringView("StackPanel"));
    const Core::TypeId canvas = Core::MakeTypeId(ns, Base::StringView("Canvas"));
    const Core::TypeId grid = Core::MakeTypeId(ns, Base::StringView("Grid"));
    const Core::TypeId border = Core::MakeTypeId(ns, Base::StringView("Border"));
    const Core::TypeId textBlock = Core::MakeTypeId(ns, Base::StringView("TextBlock"));
    const Core::TypeId presenter = Core::MakeTypeId(ns, Base::StringView("ContentPresenter"));
    const Core::TypeId visualTypes[] = {
        stackPanel, canvas, grid, border, textBlock, presenter
    };
    for (Core::TypeId type : visualTypes) {
        Base::Result<void> registered = visualTree->TryRegisterType({type,
            &AsCoreTreeNode, &AsCoreLayoutElement, &AsCoreRenderElement, nullptr});
        if (!registered) return registered.GetStatus();
    }
    Base::Result<void> structural = visualTree->TryRegisterContentPresenter(
        {presenter, &AsCoreContentPresenter, nullptr});
    if (!structural) return structural.GetStatus();
    structural = visualTree->TryRegisterContentPresenter(
        {border, nullptr, nullptr, &AsCoreBorder});
    if (!structural) return structural.GetStatus();
    structural = visualTree->TryRegisterCollectionContent({stackPanel,
        Core::MakeMemberId(stackPanel, Core::MemberKind::Property,
            Base::StringView("Children")), &AsCoreStackPanel, nullptr});
    if (!structural) return structural.GetStatus();
    structural = visualTree->TryRegisterCollectionContent({canvas,
        Core::MakeMemberId(canvas, Core::MemberKind::Property,
            Base::StringView("Children")), nullptr, nullptr,
        &AsCoreCollectionOwner, nullptr});
    if (!structural) return structural.GetStatus();
    structural = visualTree->TryRegisterCollectionContent({grid,
        Core::MakeMemberId(grid, Core::MemberKind::Property,
            Base::StringView("Children")), nullptr, nullptr,
        &AsCoreCollectionOwner, nullptr});
    if (!structural) return structural.GetStatus();
    structural = visualTree->Register(activation.Schema());
    if (!structural) return structural.GetStatus();
    return count + 11U;
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
            Base::ErrorCode::InvalidArgument,
            MessageTargetNotDependencyObject);
    }

    const Core::DependencyPropertyHandle propertyHandle{services.targetMember};
    const Core::DependencyProperty* property =
        bridge->properties_->Find(propertyHandle);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessagePropertyNotBridged);
    }

    const XamlDependencyObjectTypeRegistration* type =
        bridge->FindTypeRegistration(services.targetObjectType);
    if (type == nullptr || type->cast == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageTargetNotDependencyObject);
    }
    Core::DependencyObject* dependencyObject = type->cast(
        object,
        type->context);
    if (dependencyObject == nullptr ||
        dependencyObject->RuntimeType() != services.targetObjectType ||
        &dependencyObject->PropertyRegistry() != bridge->properties_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageRuntimeMismatch);
    }

    Base::Result<Core::PropertyValue> converted =
        bridge->ConvertValue(value, *property);
    if (!converted) {
        return converted.GetStatus();
    }
    return dependencyObject->SetValue(
        propertyHandle,
        converted.Value());
}

} // namespace Aero::Markup
