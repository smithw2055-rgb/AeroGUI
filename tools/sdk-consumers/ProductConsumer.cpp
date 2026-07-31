#include <Aero/App.hpp>

#include <type_traits>

namespace {

class ConsumerApplication final : public Aero::Application {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerApplication,
        Aero::Application,
        "urn:aero-sdk-consumer",
        "ConsumerApplication")

public:
    ConsumerApplication() noexcept
        : Application(StaticTypeId()) {}
};

class ConsumerWindow final : public Aero::Window {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerWindow,
        Aero::Window,
        "urn:aero-sdk-consumer",
        "ConsumerWindow")

public:
    ConsumerWindow() noexcept
        : Window(StaticTypeId()) {}
};

[[maybe_unused]] void ConsumeApplicationSdk(
    Aero::App::Launcher& launcher,
    Aero::Application& application,
    Aero::Window& window) noexcept {
    static_cast<void>(launcher.CurrentApplication());
    static_cast<void>(launcher.MainWindow());
    application.SetMainWindow(&window);
    application.SetShutdownMode(
        Aero::ShutdownMode::OnExplicitShutdown);
    static_cast<void>(application.MainWindow());
    static_cast<void>(application.GetShutdownMode());
    static_cast<void>(window.IsOpen());
}

[[maybe_unused]] void ConsumeDerivedAppTypes() noexcept {
    ConsumerApplication application;
    ConsumerWindow window;
    static_cast<void>(application.RuntimeType());
    static_cast<void>(window.RuntimeType());
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

} // namespace
