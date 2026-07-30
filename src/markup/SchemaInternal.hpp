#pragma once

#include "XamlFacetStore.hpp"
#include <Aero/Markup/Schema.hpp>

namespace Aero::Markup {

struct Schema::Impl final {
    Detail::XamlFacetStore facets;
};

namespace Detail {

class SchemaAccess final {
public:
    // Compatibility aggregate input. XamlFacetStore projects it atomically and
    // retains only narrow facet records.
    static Base::Result<void> AddType(
        Schema& schema,
        const XamlTypeFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<void> AddLifecycle(
        Schema& schema,
        const XamlLifecycleFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<void> AddNameScope(
        Schema& schema,
        const XamlNameScopeFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<void> AddResourceScope(
        Schema& schema,
        const XamlResourceScopeFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<void> AddDeferredContent(
        Schema& schema,
        const XamlDeferredContentFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<void> AddImplicitResourceKey(
        Schema& schema,
        const XamlImplicitResourceKeyFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<void> AddPropertyTarget(
        Schema& schema,
        const XamlPropertyTargetFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<void> AddMarkupExtension(
        Schema& schema,
        const XamlMarkupExtensionFacet& registration) noexcept {
        return schema.impl_->facets.TryAdd(
            registration, schema.Types());
    }

    static Base::Result<Core::Value> ConvertText(
        const Schema& schema,
        Core::TypeId type,
        Base::StringView text,
        const ExtensionContext* services = nullptr) noexcept {
        return schema.ConvertText(type, text, services);
    }

    static Base::Result<Core::DependencyObject*>
    ResolvePropertyTarget(
        const Schema& schema,
        Base::Object& object) noexcept {
        return schema.ResolvePropertyTarget(object);
    }

    static Core::MetadataRuntime* Runtime(
        const Schema& schema) noexcept {
        return schema.Runtime();
    }

    static Base::Result<const Core::TypeInfo*> ResolveType(
        const Schema& schema,
        Base::StringView xamlNamespace,
        Base::StringView localName) noexcept {
        return schema.ResolveType(xamlNamespace, localName);
    }
};

} // namespace Detail

} // namespace Aero::Markup
