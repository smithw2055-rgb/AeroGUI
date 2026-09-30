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
    DesktopHost(
        Application& application,
        Base::Ref<Window> window,
        const RunOptions& options) noexcept;
    ~DesktopHost() noexcept;

    DesktopHost(const DesktopHost&) = delete;
    DesktopHost& operator=(const DesktopHost&) = delete;

    Base::Result<int> Run() noexcept;

    // Lifecycle gates used by the directly owned state below. Keeping these
    // operations on DesktopHost preserves the public Application/Window
    // friendship boundary without naming source-only state in public headers.
    static Base::Result<void> AttachApplication(
        Application& application,
        void* hostState,
        Window* mainWindow) noexcept;
    static void DetachApplication(
        Application& application) noexcept;
    static void RaiseApplicationStartup(
        Application& application) noexcept;
    static void RaiseApplicationExit(
        Application& application,
        int exitCode) noexcept;
    static void AttachMainWindow(
        Application& application,
        Window* window) noexcept;
    static void AdoptApplicationResources(
        Application& application,
        ::Aero::ResourceDictionary&& resources) noexcept;
    static void AttachWindow(
        Window& window,
        void* hostState) noexcept;
    static void DetachWindow(
        Window& window) noexcept;
    static void NotifyWindowSourceInitialized(
        Window& window) noexcept;
    static void NotifyWindowContentRendered(
        Window& window) noexcept;
    static void NotifyWindowClosed(
        Window& window) noexcept;
    static bool WindowComponentRequested(
        const Window& window) noexcept;
    static Base::StringView WindowComponentUri(
        const Window& window) noexcept;

private:
    DesktopHostState* state_ = nullptr;
};

} // namespace Aero::App
