#pragma once

#include <Aero/App.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::App {

namespace Detail {

class ApplicationAccess final {
public:
    static void Attach(
        Application& application,
        void* runtimeState,
        Window* mainWindow) noexcept;
    static void Detach(Application& application) noexcept;
    static void RaiseStartup(Application& application) noexcept;
    static void RaiseExit(
        Application& application,
        int exitCode) noexcept;
};

class WindowAccess final {
public:
    static void Attach(Window& window, void* runtimeState) noexcept;
    static void Detach(Window& window) noexcept;
    static void NotifySourceInitialized(Window& window) noexcept;
    static void NotifyContentRendered(Window& window) noexcept;
    static void NotifyClosed(Window& window) noexcept;
};

// Private implementation of the optional desktop application framework.
// It is deliberately not installed and does not form a second authoring API.
class DesktopHost final {
public:
    explicit DesktopHost(const RunOptions& options) noexcept;
    DesktopHost(
        Application& application,
        Base::Ref<Window> window,
        const RunOptions& options) noexcept;
    ~DesktopHost() noexcept;

    DesktopHost(const DesktopHost&) = delete;
    DesktopHost& operator=(const DesktopHost&) = delete;

    Base::Result<int> Run() noexcept;



private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Detail

} // namespace Aero::App
