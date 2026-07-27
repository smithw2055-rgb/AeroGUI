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
};

} // namespace Detail

} // namespace Aero::Markup
