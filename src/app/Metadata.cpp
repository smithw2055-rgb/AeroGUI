#include "Metadata.hpp"

#include <Aero/Application.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Window.hpp>

namespace Aero::App {

Base::Result<void> Detail::PopulateAppMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    auto shutdownMode = Meta::Register<Aero::ShutdownMode>(context);
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

    status = Meta::Register<Aero::StartupEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<Aero::ExitEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<Aero::CancelEventArgs>(context).Result();
    if (!status) return status.GetStatus();

    auto windowState = Meta::Register<Aero::WindowState>(context);
    windowState.Value("Normal", Aero::WindowState::Normal).Value("Minimized", Aero::WindowState::Minimized).Value("Maximized", Aero::WindowState::Maximized);
    status = windowState.Result();
    if (!status) return status.GetStatus();
    auto windowStyle = Meta::Register<Aero::WindowStyle>(context);
    windowStyle.Value("None", Aero::WindowStyle::None).Value("SingleBorderWindow", Aero::WindowStyle::SingleBorderWindow).Value("ThreeDBorderWindow", Aero::WindowStyle::ThreeDBorderWindow).Value("ToolWindow", Aero::WindowStyle::ToolWindow);
    status = windowStyle.Result();
    if (!status) return status.GetStatus();
    auto resizeMode = Meta::Register<Aero::ResizeMode>(context);
    resizeMode.Value("NoResize", Aero::ResizeMode::NoResize).Value("CanMinimize", Aero::ResizeMode::CanMinimize).Value("CanResize", Aero::ResizeMode::CanResize).Value("CanResizeWithGrip", Aero::ResizeMode::CanResizeWithGrip);
    status = resizeMode.Result();
    if (!status) return status.GetStatus();
    auto sizeToContent = Meta::Register<Aero::SizeToContent>(context);
    sizeToContent.Value("Manual", Aero::SizeToContent::Manual).Value("Width", Aero::SizeToContent::Width).Value("Height", Aero::SizeToContent::Height).Value("WidthAndHeight", Aero::SizeToContent::WidthAndHeight);
    status = sizeToContent.Result();
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
        .Property(Aero::Window::TitleProperty, Meta::PropertyOptions(Base::String{}).AffectsMeasure())
        .Property(Aero::Window::WindowStateProperty, Meta::PropertyOptions(Aero::WindowState::Normal).AffectsRender())
        .Property(Aero::Window::WindowStyleProperty, Meta::PropertyOptions(Aero::WindowStyle::SingleBorderWindow).AffectsMeasure())
        .Property(Aero::Window::ResizeModeProperty, Meta::PropertyOptions(Aero::ResizeMode::CanResize))
        .Property(Aero::Window::SizeToContentProperty, Meta::PropertyOptions(Aero::SizeToContent::Manual).AffectsMeasure())
        .Property(Aero::Window::ShowInTaskbarProperty, Meta::PropertyOptions(true))
        .Property(Aero::Window::TopmostProperty, Meta::PropertyOptions(false))
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
