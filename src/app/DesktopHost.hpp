#pragma once

#include <AeroApp/App.hpp>

#include <cstddef>
#include <cstdint>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::App {

struct DesktopHostState;

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

    // Lifecycle gates used by the directly owned state below. Keeping these
    // operations on DesktopHost preserves the public Application/Window
    // friendship boundary without naming source-only state in public headers.
    static Base::Result<void> AttachApplication(
        ::Aero::Application& application,
        void* hostState,
        ::Aero::Window* mainWindow) noexcept;
    static void DetachApplication(
        ::Aero::Application& application) noexcept;
    static void RaiseApplicationStartup(
        ::Aero::Application& application) noexcept;
    static void RaiseApplicationExit(
        ::Aero::Application& application,
        int exitCode) noexcept;
    static void AttachMainWindow(
        ::Aero::Application& application,
        ::Aero::Window* window) noexcept;
    static void AdoptApplicationResources(
        ::Aero::Application& application,
        ::Aero::ResourceDictionary&& resources) noexcept;
    static void AttachWindow(
        ::Aero::Window& window,
        void* hostState) noexcept;
    static void DetachWindow(
        ::Aero::Window& window) noexcept;
    static void NotifyWindowSourceInitialized(
        ::Aero::Window& window) noexcept;
    static void NotifyWindowContentRendered(
        ::Aero::Window& window) noexcept;
    static void NotifyWindowClosed(
        ::Aero::Window& window) noexcept;
    static bool WindowComponentRequested(
        const ::Aero::Window& window) noexcept;
    static Base::StringView WindowComponentUri(
        const ::Aero::Window& window) noexcept;

private:
    alignas(std::max_align_t) std::uint8_t stateStorage_[131072]{};
    DesktopHostState* state_ = nullptr;
};

} // namespace Aero::App
