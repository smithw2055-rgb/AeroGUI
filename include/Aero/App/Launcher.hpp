#pragma once

#include <Aero/App/ApplicationHost.hpp>
#include <Aero/App/Services.hpp>

namespace Aero::App {

// Canonical default application-framework entry point. Launcher composes the
// existing host implementation and owns optional process services separately
// from the WPF Application object.
class AERO_API Launcher final {
public:
    explicit Launcher(
        const LaunchOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept
        : host_(options, allocator) {}

    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    Base::Result<int> Run() noexcept {
        return host_.Run();
    }

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept {
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
};

} // namespace Aero::App
