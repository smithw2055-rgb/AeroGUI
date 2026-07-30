#pragma once

#include <Aero/App/ApplicationHost.hpp>
#include <Aero/App/Metadata.hpp>
#include <Aero/App/Services.hpp>

namespace Aero::App {

// Canonical default application-framework entry point. Launcher composes the
// existing host implementation, installs App metadata before schema freeze and
// owns optional process services separately from the WPF Application object.
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

    Services& GetServices() noexcept {
        return services_;
    }

    const Services& GetServices() const noexcept {
        return services_;
    }

private:
    ApplicationHost host_;
    Services services_;
    Base::Status appModuleStatus_;
};

} // namespace Aero::App
