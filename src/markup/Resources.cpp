#include "ResourceSupport.hpp"
#include "SchemaInternal.hpp"

#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Markup/Schema.hpp>
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
        Detail::SchemaAccess::AddType(schema, {
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
    status = Detail::SchemaAccess::AddType(schema, {
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
