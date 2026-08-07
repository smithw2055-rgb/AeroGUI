#include <Aero/Application.hpp>
#include <Aero/App.hpp>

#include "DesktopHost.hpp"

#include <utility>

namespace Aero {

Base::Result<int> Application::Run() noexcept {
    return Run(App::RunOptions{});
}

Base::Result<int> Application::Run(
    const App::RunOptions& options) noexcept {
    ::Aero::App::Detail::DesktopHost host(*this, {}, options);
    return host.Run();
}

Base::Result<int> Application::Run(
    Base::Ref<Window> window) noexcept {
    return Run(std::move(window), App::RunOptions{});
}

Base::Result<int> Application::Run(
    Base::Ref<Window> window,
    const App::RunOptions& options) noexcept {
    if (!window) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Application.Run requires a Window");
    }
    ::Aero::App::Detail::DesktopHost host(
        *this, std::move(window), options);
    return host.Run();
}

} // namespace Aero
