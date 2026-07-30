#include <Aero/App.hpp>
#include <Aero/Runtime.hpp>

#include <type_traits>

namespace {

[[maybe_unused]] void ConsumeProductSdk(
    Aero::RuntimeEnvironment& environment) {
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> view =
        environment.CreateView();
    if (view) {
        static_cast<void>(view.Value()->RunFrame());
    }
}

[[maybe_unused]] void ConsumeApplicationSdk(
    Aero::App::Launcher& launcher,
    Aero::Application& application,
    Aero::Window& window) noexcept {
    static_cast<void>(launcher.CurrentApplication());
    static_cast<void>(launcher.MainWindow());
    static_cast<void>(launcher.GetServices());
    static_cast<void>(application.MainWindow());
    static_cast<void>(window.IsOpen());
}

static_assert(
    std::is_same<
        Aero::App::Application,
        Aero::Application>::value,
    "App compatibility name must preserve Application identity");

static_assert(
    std::is_same<
        Aero::App::Window,
        Aero::Window>::value,
    "App compatibility name must preserve Window identity");

} // namespace
