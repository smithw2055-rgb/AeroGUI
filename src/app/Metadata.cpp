#include <Aero/App/Metadata.hpp>

#include <Aero/Application.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Resources.hpp>

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
    return application.Result();
}

} // namespace Aero::App
