#include <Aero/Application.hpp>

#include "ApplicationRuntime.hpp"

namespace Aero {
namespace {
thread_local Application* currentApplication = nullptr;
}

Application* Application::Current() noexcept { return currentApplication; }

std::uint32_t WindowCollection::GetCount() const noexcept {
    return owner_ != nullptr && owner_->mainWindow_ != nullptr ? 1U : 0U;
}

Window* WindowCollection::GetItem(std::uint32_t index) const noexcept {
    return index == 0U && owner_ != nullptr ? owner_->mainWindow_ : nullptr;
}


void Application::Shutdown(int exitCode) noexcept {
    auto* state = static_cast<App::Detail::ApplicationRuntimeState*>(runtimeState_);
    if (state != nullptr && state->requestExit != nullptr) state->requestExit(state->context, exitCode);
}

void Application::OnStartup(StartupEventArgs&) noexcept {}
void Application::OnExit(ExitEventArgs&) noexcept {}
void Application::OnActivated(EventArgs&) noexcept {}
void Application::OnDeactivated(EventArgs&) noexcept {}

void Application::Attach(void* runtimeState, Window* mainWindow) noexcept {
    runtimeState_ = runtimeState;
    if (mainWindow_ == nullptr) mainWindow_ = mainWindow;
    currentApplication = this;
}

void Application::Detach() noexcept {
    if (currentApplication == this) currentApplication = nullptr;
    runtimeState_ = nullptr;
    mainWindow_ = nullptr;
}

void Application::RaiseStartup() noexcept {
    StartupEventArgs args;
    args.startupUri = startupUri_.View();
    OnStartup(args);
}

void Application::RaiseExit(int exitCode) noexcept {
    ExitEventArgs args;
    args.applicationExitCode = exitCode;
    OnExit(args);
}

void Application::RaiseActivated() noexcept {
    EventArgs args;
    OnActivated(args);
}

void Application::RaiseDeactivated() noexcept {
    EventArgs args;
    OnDeactivated(args);
}

} // namespace Aero
