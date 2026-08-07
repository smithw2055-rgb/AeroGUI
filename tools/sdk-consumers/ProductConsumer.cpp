#include <Aero/App.hpp>

#include <type_traits>
#include <utility>

namespace {

class ConsumerApplication : public Aero::Application {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerApplication,
        Aero::Application,
        "urn:aero-sdk-consumer",
        "ConsumerApplication")

public:
    ConsumerApplication() noexcept
        : Application(StaticTypeId()) {}

protected:
    void OnStartup(Aero::StartupEventArgs& args) noexcept override {
        static_cast<void>(args.GetStartupUri());
    }

    void OnExit(Aero::ExitEventArgs& args) noexcept override {
        static_cast<void>(args.GetApplicationExitCode());
    }
};

class ConsumerWindow : public Aero::Window {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerWindow,
        Aero::Window,
        "urn:aero-sdk-consumer",
        "ConsumerWindow")

public:
    ConsumerWindow() noexcept
        : Window(StaticTypeId()) {}

protected:
    void OnClosing(Aero::CancelEventArgs& args) noexcept override {
        Aero::Window::OnClosing(args);
    }
};

[[maybe_unused]] void ConsumeApplicationSdk(
    Aero::Application& application,
    Aero::Window& window) noexcept {
    application.SetMainWindowBorrowed(&window);
    application.SetShutdownMode(
        Aero::ShutdownMode::OnExplicitShutdown);
    static_cast<void>(application.GetMainWindow());
    static_cast<void>(application.GetWindows().GetCount());
    static_cast<void>(application.GetShutdownMode());
    static_cast<void>(window.GetWindowState());
    static_cast<void>(window.GetResizeMode());
    static_cast<void>(window.SourceInitialized());
    static_cast<void>(window.StateChanged());
    static_cast<void>(window.GetIsOpen());
}

[[maybe_unused]] void ConsumeDerivedAppTypes() noexcept {
    ConsumerApplication application;
    ConsumerWindow window;
    static_cast<void>(application.RuntimeType());
    static_cast<void>(window.RuntimeType());
    static_cast<void>(
        static_cast<Aero::Base::Result<int> (Aero::Application::*)() noexcept>(
            &Aero::Application::Run));
    static_cast<void>(
        static_cast<int (*)(const Aero::App::RunOptions&) noexcept>(
            &Aero::App::Run));
}

static_assert(
    std::is_base_of<
        Aero::Application,
        ConsumerApplication>::value,
    "WPF Application must remain derivable");

static_assert(
    std::is_base_of<
        Aero::Window,
        ConsumerWindow>::value,
    "WPF Window must remain derivable");

static_assert(
    std::is_same<
        decltype(std::declval<Aero::Application&>().GetResources()),
        Aero::ResourceDictionary&>::value,
    "Application resources must always be available by reference");

static_assert(
    std::is_same<
        decltype(std::declval<Aero::Application&>().Run()),
        Aero::Base::Result<int>>::value,
    "Application must expose a Result-returning Run boundary");

static_assert(
    std::is_same<
        decltype(std::declval<Aero::Window&>().Show()),
        Aero::Base::Result<void>>::value,
    "Window must expose one Result-returning Show boundary");

} // namespace
