#pragma once

#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Value.hpp>
#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero {

class ResourceDictionary;
class ResourceKey;

namespace Markup {

using AddResourceCallback = Result<void> (*)(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void* context) noexcept;

using ResolveResourceScopeCallback = ResourceDictionary* (*)(
    Base::Object& scopeOwner,
    void* context) noexcept;

// A module-level XAML capability. Product modules use this descriptor to
// teach the platform-neutral Gui schema about resource-owning root objects
// without creating a link dependency from AeroGui back to the product DLL.
struct ResourceScopeRegistration {
    Meta::TypeId type = Meta::InvalidTypeId;
    AddResourceCallback addResource = nullptr;
    ResolveResourceScopeCallback resolve = nullptr;
    void* context = nullptr;
    bool inherited = true;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

} // namespace Markup
} // namespace Aero
