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
    static Base::Result<void> AddType(
        Schema& schema,
        const XamlTypeFacet& registration) noexcept {
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
};

} // namespace Detail

} // namespace Aero::Markup
