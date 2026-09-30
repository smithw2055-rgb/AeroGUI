#pragma once

// Src-only resource walk used by FrameworkElement::FindResource /
// TryFindResource and ViewFrame. Markup::ResourceResolver stays public.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Resources.hpp>

namespace Aero {

class FrameworkElement;

class ResourceResolver {
public:
    static Result<ResourceValue> Lookup(const FrameworkElement* element,
        const ResourceKey& key, const ResourceDictionary* templateResources,
        const ResourceEnvironment& environment) noexcept;
    static Result<ResourceValue> Lookup(const FrameworkElement* element,
        StringView key, const ResourceDictionary* templateResources, const ResourceEnvironment& environment) noexcept;
    static Result<ResourceValue> Lookup(const FrameworkElement* element,
        Meta::TypeId key, const ResourceDictionary* templateResources, const ResourceEnvironment& environment) noexcept;
};

} // namespace Aero
