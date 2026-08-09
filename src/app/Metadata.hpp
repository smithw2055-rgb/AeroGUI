#pragma once

#include <Aero/Module.hpp>
#include <AeroApp/App.hpp>

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

} // namespace Aero::App
