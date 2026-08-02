#pragma once

#include <Aero/App.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Internal {

class DesktopPrivate {
public:
    static void Attach(
        ::Aero::Application& application,
        void* hostState,
        ::Aero::Window* mainWindow) noexcept;
    static void Detach(::Aero::Application& application) noexcept;
    static void RaiseStartup(::Aero::Application& application) noexcept;
    static void RaiseExit(
        ::Aero::Application& application,
        int exitCode) noexcept;

    static void Attach(::Aero::Window& window, void* hostState) noexcept;
    static void Detach(::Aero::Window& window) noexcept;
    static void NotifySourceInitialized(::Aero::Window& window) noexcept;
    static void NotifyContentRendered(::Aero::Window& window) noexcept;
    static void NotifyClosed(::Aero::Window& window) noexcept;
};

// Private implementation of the optional desktop application framework.
// It is deliberately not installed and does not form a second authoring API.
class DesktopHost {
public:
    explicit DesktopHost(const ::Aero::App::RunOptions& options) noexcept;
    DesktopHost(
        ::Aero::Application& application,
        Base::Ref<::Aero::Window> window,
        const ::Aero::App::RunOptions& options) noexcept;
    ~DesktopHost() noexcept;

    DesktopHost(const DesktopHost&) = delete;
    DesktopHost& operator=(const DesktopHost&) = delete;

    Base::Result<int> Run() noexcept;



private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Internal

namespace Aero::App::Detail {
using ::Aero::Internal::DesktopPrivate;
using ::Aero::Internal::DesktopHost;
} // namespace Aero::App::Detail
