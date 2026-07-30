#pragma once

#include <Aero/App/ApplicationHost.hpp>
#include <Aero/App/Metadata.hpp>

namespace Aero::App {

// Canonical default application-framework entry point. Launcher composes the
// existing host implementation and guarantees that the WPF-facing App metadata
// module is installed before RuntimeEnvironment freezes its schema.
class AERO_API Launcher final {
public:
    explicit Launcher(
        const LaunchOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept
        : host_(options, allocator),
          appModuleStatus_(
              host_.AddModule(AppModule()).GetStatus()) {}

    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    Base::Result<int> Run() noexcept {
        if (!appModuleStatus_.IsOk()) {
            return appModuleStatus_;
        }
        return host_.Run();
    }

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept {
        if (!appModuleStatus_.IsOk()) {
            return appModuleStatus_;
        }
        return host_.AddModule(registration);
    }

    void RequestExit(int exitCode = 0) noexcept {
        host_.RequestExit(exitCode);
    }

    Application* CurrentApplication() const noexcept {
        return host_.CurrentApplication();
    }

    Window* MainWindow() const noexcept {
        return host_.MainWindow();
    }

private:
    ApplicationHost host_;
    Base::Status appModuleStatus_;
};

} // namespace Aero::App
