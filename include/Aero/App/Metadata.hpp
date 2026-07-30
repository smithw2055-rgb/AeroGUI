#pragma once

#include <Aero/Application.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Module.hpp>

namespace Aero::App {

inline constexpr Base::StringView AppMetadataModuleName() noexcept {
    return "Aero.App";
}

namespace Detail {

// App owns the process-level Application XAML object. Window is a WPF-facing
// ContentControl and remains part of the UI/Controls schema; App supplies its
// optional native peer and default launcher lifetime rather than redefining its
// control metadata.
inline Base::Result<void> PopulateAppMetadata(
    Core::MetadataContext& context) noexcept {
    auto application = Core::Describe<::Aero::Application>(context);
    application
        .Factory()
        .Property(
            "StartupUri",
            &::Aero::Application::StartupUri,
            &::Aero::Application::SetStartupUri)
        .Property(
            "Resources",
            &::Aero::Application::Resources,
            &::Aero::Application::SetResources);
    return application.Result();
}

} // namespace Detail

inline ModuleRegistration AppModule() noexcept {
    ModuleRegistration registration = DefineModule(
        AppMetadataModuleName(),
        &Detail::PopulateAppMetadata);
    registration.schemaVersion = 1U;
    return registration;
}

} // namespace Aero::App
