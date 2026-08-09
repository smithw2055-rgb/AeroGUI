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

    auto application = Meta::Register<Aero::Application>(context);
    application
        .Property(
            "StartupUri",
            &Aero::Application::GetStartupUri,
            &Aero::Application::SetStartupUri)
        .Property<
            Base::Ref<Aero::ResourceDictionary>,
            &Aero::Application::SetResources>(
                "Resources",
                Meta::PropertyFlags::Structural)
        .Property(
            "ShutdownMode",
            &Aero::Application::GetShutdownMode,
            &Aero::Application::SetShutdownMode)
        .Factory();
    status = application.Result();
    if (!status) return status.GetStatus();

    auto window = Meta::Register<Aero::Window>(context);
    window
        .Property(Aero::Window::TitleProperty, Meta::FrameworkPropertyMetadata(Base::String{}).AffectsMeasure())
        .Property(Aero::Window::WindowStateProperty, Meta::FrameworkPropertyMetadata(Aero::WindowState::Normal).AffectsRender())
        .Property(Aero::Window::WindowStyleProperty, Meta::FrameworkPropertyMetadata(Aero::WindowStyle::SingleBorderWindow).AffectsMeasure())
        .Property(Aero::Window::ResizeModeProperty, Meta::FrameworkPropertyMetadata(Aero::ResizeMode::CanResize))
        .Property(Aero::Window::SizeToContentProperty, Meta::FrameworkPropertyMetadata(Aero::SizeToContent::Manual).AffectsMeasure())
        .Property(Aero::Window::ShowInTaskbarProperty, Meta::FrameworkPropertyMetadata(true))
        .Property(Aero::Window::TopmostProperty, Meta::FrameworkPropertyMetadata(false))
        .Event(Aero::Window::ClosingEvent, Aero::RoutingStrategy::Direct)
        .Event(Aero::Window::ClosedEvent, Aero::RoutingStrategy::Direct)
        .Event(Aero::Window::ActivatedEvent, Aero::RoutingStrategy::Direct)
        .Event(Aero::Window::DeactivatedEvent, Aero::RoutingStrategy::Direct)
        .Event(Aero::Window::ContentRenderedEvent, Aero::RoutingStrategy::Direct)
        .Event(Aero::Window::SourceInitializedEvent, Aero::RoutingStrategy::Direct)
        .Event(Aero::Window::StateChangedEvent, Aero::RoutingStrategy::Direct)
        .Factory();
    return window.Result();
}

} // namespace Aero::App
