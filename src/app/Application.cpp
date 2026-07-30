#include <Aero/Application.hpp>

namespace Aero {
namespace {

thread_local Application* currentApplication = nullptr;

}

Application* Application::Current() noexcept {
    return currentApplication;
}

void Application::Shutdown(int exitCode) noexcept {
    if (peer_ != nullptr) {
        peer_->RequestExit(exitCode);
    }
}

void Application::Attach(
    App::Detail::IApplicationPeer* peer,
    Window* mainWindow) noexcept {
    peer_ = peer;
    mainWindow_ = mainWindow;
    currentApplication = this;
}

void Application::Detach() noexcept {
    if (currentApplication == this) {
        currentApplication = nullptr;
    }
    peer_ = nullptr;
    mainWindow_ = nullptr;
}

} // namespace Aero
