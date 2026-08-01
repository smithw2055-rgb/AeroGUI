#include <Aero/Application.hpp>

#include "ApplicationState.hpp"

namespace Aero {
namespace {
thread_local Application* currentApplication = nullptr;
}

Application* Application::Current() noexcept { return currentApplication; }

std::uint32_t WindowCollection::GetCount() const noexcept {
    if (owner_ == nullptr) return 0U;
    const auto* state = static_cast<const App::Detail::ApplicationHostState*>(
        owner_->hostState_);
    if (state != nullptr && state->windowCount != nullptr) {
        return state->windowCount(state->context);
    }
    return owner_->mainWindow_ != nullptr ? 1U : 0U;
}

Window* WindowCollection::GetItem(std::uint32_t index) const noexcept {
    if (owner_ == nullptr) return nullptr;
    const auto* state = static_cast<const App::Detail::ApplicationHostState*>(
        owner_->hostState_);
    if (state != nullptr && state->windowAt != nullptr) {
        return state->windowAt(state->context, index);
    }
    return index == 0U ? owner_->mainWindow_ : nullptr;
}

void Application::SetMainWindow(Window* value) noexcept {
    mainWindowOwner_.Reset();
    mainWindow_ = value;
    if (value != nullptr) {
        Base::Ref<Window> retained = Base::Ref<Window>::TryFromBorrowed(*value);
        if (retained) {
            mainWindowOwner_ = Base::Ref<Base::Object>(std::move(retained));
        }
    }
    auto* state = static_cast<App::Detail::ApplicationHostState*>(hostState_);
    if (state != nullptr && state->setMainWindow != nullptr) {
        state->setMainWindow(state->context, value);
    }
}


void Application::Shutdown(int exitCode) noexcept {
    auto* state = static_cast<App::Detail::ApplicationHostState*>(hostState_);
    if (state != nullptr && state->requestExit != nullptr) state->requestExit(state->context, exitCode);
}

void Application::OnStartup(StartupEventArgs&) noexcept {}
void Application::OnExit(ExitEventArgs&) noexcept {}
void Application::OnActivated(EventArgs&) noexcept {}
void Application::OnDeactivated(EventArgs&) noexcept {}

void Application::Attach(void* hostState, Window* mainWindow) noexcept {
    hostState_ = hostState;
    currentApplication = this;
    if (mainWindow_ == nullptr && mainWindow != nullptr) {
        SetMainWindow(mainWindow);
    }
}

void Application::Detach() noexcept {
    if (currentApplication == this) currentApplication = nullptr;
    hostState_ = nullptr;
    mainWindow_ = nullptr;
    mainWindowOwner_.Reset();
}

void Application::RaiseStartup() noexcept {
    StartupEventArgs args(startupUri_.View());
    OnStartup(args);
}

void Application::RaiseExit(int exitCode) noexcept {
    ExitEventArgs args(exitCode);
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
