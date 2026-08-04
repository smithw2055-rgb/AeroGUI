#include <Aero/Application.hpp>
#include <Aero/App.hpp>

#include "DesktopHost.hpp"

#include <utility>

namespace Aero {

int Application::Run() noexcept {
    Base::Result<int> result = RunChecked();
    return result ? result.Value() : -1;
}

int Application::Run(
    const App::RunOptions& options) noexcept {
    Base::Result<int> result = RunChecked(options);
    return result ? result.Value() : -1;
}

int Application::Run(
    Base::Ref<Window> window) noexcept {
    Base::Result<int> result = RunChecked(std::move(window));
    return result ? result.Value() : -1;
}

int Application::Run(
    Base::Ref<Window> window,
    const App::RunOptions& options) noexcept {
    Base::Result<int> result = RunChecked(
        std::move(window), options);
    return result ? result.Value() : -1;
}

Base::Result<int> Application::RunChecked() noexcept {
    return RunChecked(App::RunOptions{});
}

Base::Result<int> Application::RunChecked(
    const App::RunOptions& options) noexcept {
    ::Aero::App::Detail::DesktopHost host(*this, {}, options);
    return host.Run();
}

Base::Result<int> Application::RunChecked(
    Base::Ref<Window> window) noexcept {
    return RunChecked(std::move(window), App::RunOptions{});
}

Base::Result<int> Application::RunChecked(
    Base::Ref<Window> window,
    const App::RunOptions& options) noexcept {
    if (!window) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Application.RunChecked requires a Window");
    }
    ::Aero::App::Detail::DesktopHost host(
        *this, std::move(window), options);
    return host.Run();
}

} // namespace Aero
