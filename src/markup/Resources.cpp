#include "ResourceSupport.hpp"
#include "SchemaInternal.hpp"

#include <Aero/Application.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Markup/Schema.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>

namespace Aero::Markup {
namespace {

using namespace Aero::Core;


Base::Status InvalidResource(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Base::Result<void> AddResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Core::Value& value,
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
    const ResourceKey& key,
    const Core::Value& value,
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

Base::Result<void> AddApplicationResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Core::Value& value,
    void*) noexcept {
    // This callback is selected through the inherited Application XAML facet,
    // so derived application types are valid scope owners.
    Base::Ref<ResourceDictionary> resources =
        static_cast<Aero::Application&>(scopeOwner).Resources();
    return resources
        ? resources->TryAdd(key, value)
        : Base::Result<void>(InvalidResource(
              "Application resource dictionary is unavailable"));
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

ResourceDictionary* ResolveApplicationScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    Base::Ref<ResourceDictionary> resources =
        static_cast<Aero::Application&>(scopeOwner).Resources();
    return resources.Get();
}

} // namespace

Base::Result<void> ResourceExtension::Register(
    Schema& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML resource extension registration is invalid");
    }
    const PropertyInfo* source =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Source"),
            false);
    const PropertyInfo* merged =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("MergedDictionaries"),
            false);
    const PropertyInfo* entries =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Entries"),
            false);
    if (source == nullptr || merged == nullptr ||
        entries == nullptr ||
        source->ValueType() !=
            TypeOf<Base::ResourceUri>() ||
        merged->ValueType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "ResourceDictionary XAML metadata is incomplete");
    }

    schema_ = &schema;
    Base::Result<void> status =
        Detail::SchemaAccess::AddResourceScope(schema, {
            ResourceDictionary::StaticTypeId(),
            true,
            &AddResource,
            &ResolveDictionaryScope,
            this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = Detail::SchemaAccess::AddResourceScope(schema, {
        FrameworkElement::StaticTypeId(),
        true,
        &AddFrameworkResource,
        &ResolveFrameworkScope,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = Detail::SchemaAccess::AddResourceScope(schema, {
        Aero::Application::StaticTypeId(),
        true,
        &AddApplicationResource,
        &ResolveApplicationScope,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup
