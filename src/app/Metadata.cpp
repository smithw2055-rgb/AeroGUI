#include <Aero/App/Metadata.hpp>

#include <Aero/Application.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Window.hpp>

namespace Aero::App {

Base::Result<void> Detail::PopulateAppMetadata(
    Core::MetadataContext& context) noexcept {
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
        .Factory();
    Base::Result<void> status = application.Result();
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
