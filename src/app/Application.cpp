#include <Aero/Application.hpp>

#include "ApplicationRuntime.hpp"

namespace Aero {
namespace {

thread_local Application* currentApplication = nullptr;

}

Application* Application::Current() noexcept {
    return currentApplication;
}

void Application::Shutdown(int exitCode) noexcept {
    auto* state = static_cast<App::Detail::ApplicationRuntimeState*>(
        runtimeState_);
    if (state != nullptr && state->requestExit != nullptr) {
        state->requestExit(state->context, exitCode);
    }
}

void Application::Attach(
    void* runtimeState,
    Window* mainWindow) noexcept {
    runtimeState_ = runtimeState;
    if (mainWindow_ == nullptr) {
        mainWindow_ = mainWindow;
    }
    currentApplication = this;
}

void Application::Detach() noexcept {
    if (currentApplication == this) {
        currentApplication = nullptr;
    }
    runtimeState_ = nullptr;
    mainWindow_ = nullptr;
}

} // namespace Aero
