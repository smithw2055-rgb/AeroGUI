#include "Metadata.hpp"

#include <Aero/Application.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Window.hpp>

namespace Aero::App {

Base::Result<void> Detail::PopulateAppMetadata(
    Core::MetadataContext& context) noexcept {
    auto shutdownMode = Core::Describe<Aero::ShutdownMode>(context);
    shutdownMode
        .Value(
            "OnLastWindowClose",
            Aero::ShutdownMode::OnLastWindowClose)
        .Value(
            "OnMainWindowClose",
            Aero::ShutdownMode::OnMainWindowClose)
        .Value(
            "OnExplicitShutdown",
            Aero::ShutdownMode::OnExplicitShutdown);
    Base::Result<void> status = shutdownMode.Result();
    if (!status) return status.GetStatus();

    auto application = Core::Describe<Aero::Application>(context);
    application
        .Property(
            "StartupUri",
            &Aero::Application::StartupUri,
            &Aero::Application::SetStartupUri)
        .Property<
            Base::Ref<Aero::ResourceDictionary>,
            &Aero::Application::SetResources>(
                "Resources",
                Core::PropertyFlags::Structural)
        .Property(
            "ShutdownMode",
            &Aero::Application::GetShutdownMode,
            &Aero::Application::SetShutdownMode)
        .Factory();
    status = application.Result();
    if (!status) return status.GetStatus();

    auto window = Core::Describe<Aero::Window>(context);
    window
        .Property(
            Aero::Window::TitleProperty,
            Core::PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Factory();
    return window.Result();
}

} // namespace Aero::App
