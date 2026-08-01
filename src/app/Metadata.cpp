#include "Metadata.hpp"

#include <Aero/Application.hpp>
#include <Aero/Meta/Describe.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Window.hpp>

namespace Aero::App {

Base::Result<void> Detail::PopulateAppMetadata(
    Core::MetaRegistration& context) noexcept {
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

    status = Core::Describe<Aero::StartupEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Core::Describe<Aero::ExitEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Core::Describe<Aero::CancelEventArgs>(context).Result();
    if (!status) return status.GetStatus();

    auto windowState = Core::Describe<Aero::WindowState>(context);
    windowState.Value("Normal", Aero::WindowState::Normal).Value("Minimized", Aero::WindowState::Minimized).Value("Maximized", Aero::WindowState::Maximized);
    status = windowState.Result();
    if (!status) return status.GetStatus();
    auto windowStyle = Core::Describe<Aero::WindowStyle>(context);
    windowStyle.Value("None", Aero::WindowStyle::None).Value("SingleBorderWindow", Aero::WindowStyle::SingleBorderWindow).Value("ThreeDBorderWindow", Aero::WindowStyle::ThreeDBorderWindow).Value("ToolWindow", Aero::WindowStyle::ToolWindow);
    status = windowStyle.Result();
    if (!status) return status.GetStatus();
    auto resizeMode = Core::Describe<Aero::ResizeMode>(context);
    resizeMode.Value("NoResize", Aero::ResizeMode::NoResize).Value("CanMinimize", Aero::ResizeMode::CanMinimize).Value("CanResize", Aero::ResizeMode::CanResize).Value("CanResizeWithGrip", Aero::ResizeMode::CanResizeWithGrip);
    status = resizeMode.Result();
    if (!status) return status.GetStatus();
    auto sizeToContent = Core::Describe<Aero::SizeToContent>(context);
    sizeToContent.Value("Manual", Aero::SizeToContent::Manual).Value("Width", Aero::SizeToContent::Width).Value("Height", Aero::SizeToContent::Height).Value("WidthAndHeight", Aero::SizeToContent::WidthAndHeight);
    status = sizeToContent.Result();
    if (!status) return status.GetStatus();

    auto application = Core::Describe<Aero::Application>(context);
    application
        .Property(
            "StartupUri",
            &Aero::Application::GetStartupUri,
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
        .Property(Aero::Window::TitleProperty, Core::PropertyOptions(Base::String{}).AffectsMeasure())
        .Property(Aero::Window::WindowStateProperty, Core::PropertyOptions(Aero::WindowState::Normal).AffectsRender())
        .Property(Aero::Window::WindowStyleProperty, Core::PropertyOptions(Aero::WindowStyle::SingleBorderWindow).AffectsMeasure())
        .Property(Aero::Window::ResizeModeProperty, Core::PropertyOptions(Aero::ResizeMode::CanResize))
        .Property(Aero::Window::SizeToContentProperty, Core::PropertyOptions(Aero::SizeToContent::Manual).AffectsMeasure())
        .Property(Aero::Window::ShowInTaskbarProperty, Core::PropertyOptions(true))
        .Property(Aero::Window::TopmostProperty, Core::PropertyOptions(false))
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
