#pragma once

#include <Aero/Base/StringView.hpp>
#include <Aero/Events/EventArgs.hpp>

namespace Aero {

struct StartupEventArgs : EventArgs {
    AERO_DECLARE_TYPE(StartupEventArgs, EventArgs)
public:
    StartupEventArgs() noexcept : EventArgs(StaticTypeId()) {}
    explicit StartupEventArgs(StringView startupUri) noexcept
        : EventArgs(StaticTypeId()), startupUri_(startupUri) {}

    StringView GetStartupUri() const noexcept { return startupUri_; }

private:
    StringView startupUri_;
};

struct ExitEventArgs : EventArgs {
    AERO_DECLARE_TYPE(ExitEventArgs, EventArgs)
public:
    ExitEventArgs() noexcept : EventArgs(StaticTypeId()) {}
    explicit ExitEventArgs(int applicationExitCode) noexcept
        : EventArgs(StaticTypeId()),
          applicationExitCode_(applicationExitCode) {}

    int GetApplicationExitCode() const noexcept {
        return applicationExitCode_;
    }

private:
    int applicationExitCode_ = 0;
};

} // namespace Aero
