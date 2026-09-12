#pragma once

#include <Aero/Base/Config.hpp>

namespace Aero::Threading {

class Dispatcher;

class AERO_GUI_API DispatcherReentrancyGuard {
public:
    DispatcherReentrancyGuard() noexcept = default;
    DispatcherReentrancyGuard(
        DispatcherReentrancyGuard&& other) noexcept;
    DispatcherReentrancyGuard& operator=(
        DispatcherReentrancyGuard&& other) noexcept;
    ~DispatcherReentrancyGuard();

    DispatcherReentrancyGuard(
        const DispatcherReentrancyGuard&) = delete;
    DispatcherReentrancyGuard& operator=(
        const DispatcherReentrancyGuard&) = delete;

    bool Active() const noexcept {
        return dispatcher_ != nullptr;
    }

    void Release() noexcept;

private:
    friend class Dispatcher;

    explicit DispatcherReentrancyGuard(
        Dispatcher* dispatcher) noexcept
        : dispatcher_(dispatcher) {}

    Dispatcher* dispatcher_ = nullptr;
};

} // namespace Aero::Threading
