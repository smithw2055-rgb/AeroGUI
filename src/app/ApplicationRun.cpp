#include <Aero/Application.hpp>
#include <Aero/App.hpp>

#include "DesktopHost.hpp"

#include <utility>

namespace Aero {

int Application::Run() noexcept {
    return Run(App::RunOptions{});
}

int Application::Run(
    const App::RunOptions& options) noexcept {
    ::Aero::App::Detail::DesktopHost host(*this, {}, options);
    Base::Result<int> result = host.Run();
    return result ? result.Value() : -1;
}

int Application::Run(
    Base::Ref<Window> window) noexcept {
    return Run(std::move(window), App::RunOptions{});
}

int Application::Run(
    Base::Ref<Window> window,
    const App::RunOptions& options) noexcept {
    if (!window) return -1;
    ::Aero::App::Detail::DesktopHost host(
        *this, std::move(window), options);
    Base::Result<int> result = host.Run();
    return result ? result.Value() : -1;
}

} // namespace Aero
