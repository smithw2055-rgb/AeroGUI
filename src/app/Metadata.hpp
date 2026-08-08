#pragma once

#include <Aero/Module.hpp>
#include <Aero/Gui/Application.hpp>

namespace Aero::App {

inline Base::Result<void> AddApplicationResource(
    Base::Object& owner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    return static_cast<Application&>(owner).GetResources().Add(key, value);
}

inline ResourceDictionary* ResolveApplicationResources(
    Base::Object& owner,
    void*) noexcept {
    return &static_cast<Application&>(owner).GetResources();
}

Base::Result<void> PopulateAppMetadata(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Aero::App

namespace Aero::App {

inline constexpr Base::StringView AppMetadataModuleName() noexcept {
    return "Aero.App";
}

inline ModuleRegistration AppMetadataModule() noexcept {
    static const Markup::ResourceScopeRegistration resourceScopes[] = {{
        Meta::MakeTypeId(Meta::AeroNamespaceUri(), "Application"),
        &AddApplicationResource,
        &ResolveApplicationResources,
        nullptr,
        true,
        XamlFacetAbiVersion}};
    ModuleRegistration module = DefineModule(
        AppMetadataModuleName(),
        &::Aero::App::PopulateAppMetadata);
    module.schemaVersion = 2U;
    module.resourceScopes = resourceScopes;
    return module;
}

} // namespace Aero::App
