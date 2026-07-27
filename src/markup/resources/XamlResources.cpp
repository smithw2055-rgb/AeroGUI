#include <Aero/Markup/Resources/XamlResources.hpp>

#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Resources.hpp>

namespace Aero::Markup {
namespace {

using namespace Aero::Core;
using namespace Aero::Presentation;

Base::Status InvalidResource(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Base::Result<void> AddResource(
    Base::Object& scopeOwner,
    Base::StringView key,
    const XamlValue& value,
    void*) noexcept {
    if (scopeOwner.RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "XAML resource scope is not a ResourceDictionary");
    }
    return static_cast<ResourceDictionary&>(scopeOwner)
        .TryAdd(key, value);
}

Base::Result<void> AddFrameworkResource(
    Base::Object& scopeOwner,
    Base::StringView key,
    const XamlValue& value,
    void*) noexcept {
    auto* element =
        static_cast<FrameworkElement*>(
            static_cast<Visual&>(scopeOwner)
                .AsFrameworkElement());
    if (element == nullptr) {
        return InvalidResource(
            "XAML resource scope is not a FrameworkElement");
    }
    return element->Resources().TryAdd(key, value);
}

ResourceDictionary* ResolveDictionaryScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    return scopeOwner.RuntimeType() ==
            ResourceDictionary::StaticTypeId()
        ? &static_cast<ResourceDictionary&>(
              scopeOwner)
        : nullptr;
}

ResourceDictionary* ResolveFrameworkScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    Visual& visual =
        static_cast<Visual&>(scopeOwner);
    FrameworkElement* element =
        visual.AsFrameworkElement();
    return element != nullptr
        ? &element->Resources()
        : nullptr;
}

Base::Result<void> SetSource(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void*) noexcept {
    if (object.RuntimeType() !=
            ResourceDictionary::StaticTypeId() ||
        value.Kind() != ValueKind::String) {
        return InvalidResource(
            "ResourceDictionary.Source expects a URI string");
    }
    Base::Result<Base::ResourceUri> uri =
        services.baseUri != nullptr &&
            !services.baseUri->Empty()
        ? Base::ResourceUri::Resolve(
              *services.baseUri,
              value.AsString())
        : Base::ResourceUri::Parse(
              value.AsString());
    if (!uri) return uri.GetStatus();
    return static_cast<ResourceDictionary&>(object)
        .SetSource(uri.Value());
}

Base::Result<void> AddMergedDictionary(
    Base::Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (object.RuntimeType() !=
            ResourceDictionary::StaticTypeId() ||
        value.Kind() != ValueKind::Object ||
        value.IsNullObject() ||
        !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "MergedDictionaries expects ResourceDictionary objects");
    }
    return static_cast<ResourceDictionary&>(object)
        .TryAddMerged(
            static_cast<ResourceDictionary&>(
                *value.AsObject()));
}

Base::Result<void> AddImplicitResource(
    Base::Object& object,
    const XamlValue& value,
    const XamlExtensionContext& services,
    void*) noexcept {
    if (object.RuntimeType() !=
            ResourceDictionary::StaticTypeId() ||
        value.Kind() != ValueKind::Object ||
        value.IsNullObject() ||
        !value.AsObject() ||
        services.schema == nullptr) {
        return InvalidResource(
            "ResourceDictionary entry is invalid");
    }
    Base::Result<ResourceKey> key =
        services.schema->ResolveImplicitResourceKey(
            value.AsObject()->RuntimeType(),
            *value.AsObject());
    if (!key) {
        return InvalidResource(
            "Unkeyed ResourceDictionary entry has no implicit key facet");
    }
    return static_cast<ResourceDictionary&>(object)
        .TryAdd(key.Value(), value);
}

} // namespace

Base::Result<void> XamlResourceExtension::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML resource extension registration is invalid");
    }
    const MetadataPropertyDescriptor* source =
        schema.Descriptors().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Source"),
            false);
    const MetadataPropertyDescriptor* merged =
        schema.Descriptors().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("MergedDictionaries"),
            false);
    const MetadataPropertyDescriptor* entries =
        schema.Descriptors().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Entries"),
            false);
    if (source == nullptr || merged == nullptr ||
        entries == nullptr) {
        return InvalidResource(
            "ResourceDictionary XAML metadata is incomplete");
    }

    schema_ = &schema;
    Base::Result<void> status =
        schema.TryRegisterMemberAdapter({
            source->Id(),
            XamlMemberWriteMode::SetOnce,
            nullptr,
            this,
            &SetSource,
            true});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.TryRegisterMemberAdapter({
        merged->Id(),
        XamlMemberWriteMode::Collection,
        &AddMergedDictionary,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.TryRegisterMemberAdapter({
        entries->Id(),
        XamlMemberWriteMode::Collection,
        nullptr,
        this,
        &AddImplicitResource});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.TryRegisterTypeAdapter({
        ResourceDictionary::StaticTypeId(),
        nullptr,
        nullptr,
        nullptr,
        this,
        false,
        true,
        nullptr,
        &AddResource,
        &ResolveDictionaryScope});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.TryRegisterTypeAdapter({
        FrameworkElement::StaticTypeId(),
        nullptr,
        nullptr,
        nullptr,
        this,
        false,
        true,
        nullptr,
        &AddFrameworkResource,
        &ResolveFrameworkScope});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup
