// ===== StaticResourceExtension =====

namespace Aero::Markup {

Base::Result<void> StaticResourceExtension::Register(
    Schema& schema,
    Meta::TypeId staticResourceExtensionType) noexcept {
    return SchemaPrivate::AddMarkupExtension(schema, {
        staticResourceExtensionType,
        &StaticResourceExtension::ProvideValue,
        this});
}

Base::Result<ProvidedValue> StaticResourceExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    StaticResourceExtension* extension =
        static_cast<StaticResourceExtension*>(context);
    if (extension == nullptr ||
        services.schema == nullptr || services.targetObject == nullptr ||
        services.targetMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StaticResource markup extension has no target service context");
    }
    Base::StringView key = TrimAscii(arguments);
    constexpr Base::StringView ResourceKeyPrefix("ResourceKey=");
    if (key.SizeBytes() > ResourceKeyPrefix.SizeBytes() &&
        key.Substr(0U, ResourceKeyPrefix.SizeBytes()) ==
            ResourceKeyPrefix) {
        key = TrimAscii(key.Substr(
            ResourceKeyPrefix.SizeBytes(),
            key.SizeBytes() - ResourceKeyPrefix.SizeBytes()));
    }
    if (key.Empty() || key.Data() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "StaticResource requires a resource key");
    }
    // A Setter is an authored style record rather than a DependencyObject.
    // Resolve its current resource value while the style dictionary is being
    // built; the style finalizer subsequently converts that value for the
    // target dependency property.
    if (services.targetObject->RuntimeType() ==
        Aero::Setter::StaticTypeId()) {
        for (const ResourceDictionary* resources :
             services.ambientResourceChain) {
            if (resources == nullptr) continue;
            Base::Result<Aero::ResourceValue> resource =
                resources->Lookup(key);
            if (resource) {
                return ProvidedValue::FromValue(
                    std::move(resource).Value());
            }
            if (resource.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return resource.GetStatus();
            }
        }
        if (services.fallbackResources != nullptr) {
            Base::Result<Aero::ResourceValue> resource =
                services.fallbackResources->Lookup(key);
            if (resource) {
                return ProvidedValue::FromValue(
                    std::move(resource).Value());
            }
            if (resource.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return resource.GetStatus();
            }
        }
        return ProvidedValue::FromValue(
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }
    Base::Result<::Aero::DependencyObject*> targetResult =
        SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    ::Aero::DependencyObject* target = targetResult.Value();
    const Meta::DependencyPropertyHandle property{services.targetMember};
    if (services.deferredContentOwner != nullptr &&
        services.deferredContentOwner->RuntimeType() ==
            Controls::ControlTemplate::StaticTypeId()) {
        Base::String targetName;
        Base::Result<void> captured = CaptureControlTemplateChildName(
            static_cast<Controls::ControlTemplate&>(
                *services.deferredContentOwner),
            services.nameScope,
            *target,
            targetName);
        if (!captured) return captured.GetStatus();
        Base::Result<void> retained =
            ::Aero::Controls::TemplatePrivate::AddDynamicResource(
                static_cast<Controls::ControlTemplate&>(
                    *services.deferredContentOwner),
                targetName.View(),
                key,
                property);
        return retained
            ? Base::Result<ProvidedValue>(ProvidedValue::Handled())
            : Base::Result<ProvidedValue>(retained.GetStatus());
    }
    const Meta::DependencyProperty* descriptor =
        target->PropertyRegistry().Find(property);
    auto resolveFrom =
        [&](const ResourceDictionary* resources)
            -> Base::Result<Meta::PropertyValue> {
        if (resources == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound, "StaticResource scope is absent");
        }
        Base::Result<Aero::ResourceValue> resource =
            resources->Lookup(key);
        if (resource) {
            return ConvertLookedUpDynamicResource(
                resource.Value(), descriptor);
        }
        return resource.GetStatus();
    };
    for (const ResourceDictionary* resources :
         services.ambientResourceChain) {
        Base::Result<Meta::PropertyValue> value = resolveFrom(resources);
        if (value) {
            return ProvidedValue::FromValue(
                std::move(value).Value());
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    if (services.fallbackResources != nullptr) {
        Base::Result<Meta::PropertyValue> value =
            resolveFrom(services.fallbackResources);
        if (value) {
            return ProvidedValue::FromValue(
                std::move(value).Value());
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    if (services.deferredContentOwner != nullptr) {
        Aero::ResourceDictionary* templateResources = nullptr;
        const Meta::TypeId ownerType =
            services.deferredContentOwner->RuntimeType();
        if (ownerType == ::Aero::DataTemplate::StaticTypeId() ||
            ownerType == ::Aero::HierarchicalDataTemplate::StaticTypeId()) {
            templateResources =
                &static_cast<::Aero::DataTemplate&>(
                    *services.deferredContentOwner).GetResources();
        } else if (
            ownerType == Controls::ItemsPanelTemplate::StaticTypeId()) {
            templateResources =
                &static_cast<Controls::ItemsPanelTemplate&>(
                    *services.deferredContentOwner).GetResources();
        }
        if (templateResources != nullptr) {
            Base::Result<Meta::PropertyValue> value =
                resolveFrom(templateResources);
            if (value) {
                return ProvidedValue::FromValue(
                    std::move(value).Value());
            }
            if (value.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
    }
    // WPF StaticResource fails when the key is absent, but AeroGUI keeps the
    // tree alive by falling back to the property default so the gallery and
    // sample resources never crash the load.
    return ProvidedValue::FromValue(
        MissingDynamicResourceValue(*target, descriptor).Value());
}

} // namespace Aero::Markup
