#pragma once

#include <Aero/Module.hpp>

namespace Aero::App::Detail {

AERO_API Base::Result<void> PopulateAppMetadata(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Aero::App::Detail

namespace Aero::App {

inline constexpr Base::StringView AppMetadataModuleName() noexcept {
    return "Aero.App";
}

inline ModuleRegistration AppMetadataModule() noexcept {
    ModuleRegistration module = DefineModule(
        AppMetadataModuleName(),
        &::Aero::App::Detail::PopulateAppMetadata);
    module.schemaVersion = 2U;
    return module;
}

} // namespace Aero::App
