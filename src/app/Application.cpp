#include <AeroApp/Application.hpp>
#include <AeroApp/App.hpp>

#include "ApplicationHost.hpp"
#include "DesktopHost.hpp"

#include <atomic>
#include <new>
#include <utility>

namespace Aero::App {
namespace {
std::atomic<Application*> currentApplication{nullptr};
}

Application::Application(Meta::TypeId runtimeType) noexcept
    : runtimeType_(runtimeType) {}

Application::~Application() noexcept {
    Detach();
}

Window::Window(Meta::TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {}

Window::~Window() noexcept {
    host_ = nullptr;
}

Application* Application::Current() noexcept {
    return currentApplication.load(std::memory_order_acquire);
}

std::uint32_t WindowCollection::GetCount() const noexcept {
    if (owner_ == nullptr) return 0U;
    if (owner_->host_ != nullptr) {
        return owner_->host_->WindowCount();
    }
    return owner_->mainWindow_ != nullptr ? 1U : 0U;
}

Window* WindowCollection::GetItem(std::uint32_t index) const noexcept {
    if (owner_ == nullptr) return nullptr;
    if (owner_->host_ != nullptr) {
        return owner_->host_->WindowAt(index);
    }
    return index == 0U ? owner_->mainWindow_ : nullptr;
}

void Application::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    if (!value) {
        resources_ = ResourceDictionary{};
        return;
    }
    Base::Result<ResourceDictionary> shared = value->Share();
    if (shared) {
        resources_ = std::move(shared).Value();
    }
}

void Application::SetMainWindow(
    Base::Ref<Window> value) noexcept {
    mainWindow_ = value.Get();
    mainWindowOwner_ = Base::Ref<Base::Object>(std::move(value));
    if (host_ != nullptr) {
        host_->SetMainWindow(mainWindow_);
    }
}

void Application::AttachMainWindow(Window* value) noexcept {
    mainWindowOwner_.Reset();
    mainWindow_ = value;
    if (host_ != nullptr) {
        host_->SetMainWindow(value);
    }
}

Base::Ref<Window> Application::MainWindowOwner() noexcept {
    return mainWindow_ != nullptr
        ? Base::Ref<Window>::TryFromBorrowed(*mainWindow_)
        : Base::Ref<Window>{};
}

void Application::Shutdown(int exitCode) noexcept {
    if (host_ != nullptr) {
        host_->RequestExit(exitCode);
    }
}

void Application::OnStartup(StartupEventArgs&) noexcept {}
void Application::OnExit(ExitEventArgs&) noexcept {}
void Application::OnActivated(EventArgs&) noexcept {}
void Application::OnDeactivated(EventArgs&) noexcept {}

Base::Result<void> Application::Attach(
    IApplicationHost* host,
    Window* mainWindow) noexcept {
    Application* expected = nullptr;
    if (!currentApplication.compare_exchange_strong(
            expected,
            this,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "A default Application is already active in this process");
    }
    host_ = host;
    if (mainWindow_ == nullptr && mainWindow != nullptr) {
        AttachMainWindow(mainWindow);
    }
    return {};
}

void Application::Detach() noexcept {
    Application* expected = this;
    static_cast<void>(currentApplication.compare_exchange_strong(
        expected,
        nullptr,
        std::memory_order_acq_rel,
        std::memory_order_acquire));
    host_ = nullptr;
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

Base::Result<int> Application::Run() noexcept {
    return Run(RunOptions{});
}

Base::Result<int> Application::Run(
    const RunOptions& options) noexcept {
    DesktopHost host(
        *this,
        MainWindowOwner(),
        options);
    return host.Run();
}

void Application::AdoptResources(
    ResourceDictionary&& resources) noexcept {
    resources_ = std::move(resources);
}

} // namespace Aero::App
