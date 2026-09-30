#include "Metadata.hpp"

#include <AeroApp/Application.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Resources.hpp>
#include <AeroApp/Window.hpp>

namespace Aero::App {

Base::Result<void> PopulateAppMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    Base::Result<void> status;
    status = Meta::Register<Aero::StartupEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<Aero::ExitEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<Aero::CancelEventArgs>(context).Result();
    if (!status) return status.GetStatus();

    auto application = Meta::Register<Application>(context);
    application
        .Property(
            "StartupUri",
            &Application::GetStartupUri,
            &Application::SetStartupUri)
        .Property<
            Base::Ref<Aero::ResourceDictionary>,
            &Application::SetResources>(
                "Resources",
                Meta::PropertyFlags::Structural)
        .Property(
            "ShutdownMode",
            &Application::GetShutdownMode,
            &Application::SetShutdownMode)
        .Factory();
    status = application.Result();
    if (!status) return status.GetStatus();

    auto window = Meta::Register<Window>(context);
    window
        .Property(Window::TitleProperty, Meta::FrameworkPropertyMetadata(Base::String{}).AffectsMeasure())
        .Property(Window::WindowStateProperty, Meta::FrameworkPropertyMetadata(WindowState::Normal).AffectsRender())
        .Property(Window::WindowStyleProperty, Meta::FrameworkPropertyMetadata(WindowStyle::SingleBorderWindow).AffectsMeasure())
        .Property(Window::ResizeModeProperty, Meta::FrameworkPropertyMetadata(ResizeMode::CanResize))
        .Property(Window::SizeToContentProperty, Meta::FrameworkPropertyMetadata(SizeToContent::Manual).AffectsMeasure())
        .Property(Window::ShowInTaskbarProperty, Meta::FrameworkPropertyMetadata(true))
        .Property(Window::TopmostProperty, Meta::FrameworkPropertyMetadata(false))
        .Property(Window::DialogResultProperty, Meta::FrameworkPropertyMetadata(::Aero::Nullable<bool>{}))
        .Property(Window::OwnerProperty, Meta::FrameworkPropertyMetadata(Base::Ref<Window>{}))
        .Event(Window::ClosingEvent, Aero::RoutingStrategy::Direct)
        .Event(Window::ClosedEvent, Aero::RoutingStrategy::Direct)
        .Event(Window::ActivatedEvent, Aero::RoutingStrategy::Direct)
        .Event(Window::DeactivatedEvent, Aero::RoutingStrategy::Direct)
        .Event(Window::ContentRenderedEvent, Aero::RoutingStrategy::Direct)
        .Event(Window::SourceInitializedEvent, Aero::RoutingStrategy::Direct)
        .Event(Window::StateChangedEvent, Aero::RoutingStrategy::Direct)
        .Factory();
    return window.Result();
}

ModuleRegistration AppMetadataModule() noexcept {
    static const Markup::ResourceScopeRegistration resourceScopes[] = {{
        Meta::MakeTypeId(Meta::AeroNamespaceUri(), "Application"),
        &AddApplicationResource,
        &ResolveApplicationResources,
        nullptr,
        true,
        XamlFacetAbiVersion}};
    ModuleRegistration module = DefineModule(
        AppMetadataModuleName(),
        &PopulateAppMetadata);
    module.schemaVersion = 2U;
    module.resourceScopes = resourceScopes;
    return module;
}

} // namespace Aero::App
