#include <Aero/Application.hpp>
#include <Aero/Audio/Audio.hpp>

#include <new>

namespace Aero {
namespace {

thread_local Application* currentApplication = nullptr;

}

Application* Application::Current() noexcept {
    return currentApplication;
}

Base::Result<Audio::Engine*> Application::GetAudioEngine() noexcept {
    if (audio_ != nullptr) return audio_;
    Audio::Engine* created = new (std::nothrow) Audio::Engine();
    if (created == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Audio engine allocation failed");
    }
    audio_ = created;
    return audio_;
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
    delete audio_;
    audio_ = nullptr;
    if (currentApplication == this) {
        currentApplication = nullptr;
    }
    peer_ = nullptr;
    mainWindow_ = nullptr;
}

} // namespace Aero
