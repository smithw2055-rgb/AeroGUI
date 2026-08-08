#include <Aero/Gui/Application.hpp>
#include <Aero/App.hpp>

#include "DesktopHost.hpp"

namespace Aero {

Base::Result<int> Application::Run() noexcept {
    return Run(App::RunOptions{});
}

Base::Result<int> Application::Run(
    const App::RunOptions& options) noexcept {
    ::Aero::App::DesktopHost host(
        *this,
        MainWindowOwner(),
        options);
    return host.Run();
}

} // namespace Aero
