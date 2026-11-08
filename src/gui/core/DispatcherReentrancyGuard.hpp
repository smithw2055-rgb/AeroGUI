#pragma once

#include <Aero/Base/Config.hpp>
#include <cstdint>

namespace Aero::Meta {
class DependencyPropertyRegistry;
} // namespace Aero::Meta

namespace Aero::Threading {

class Dispatcher;

std::uint16_t AcquireObjectContextSlot(
    Dispatcher* dispatcher,
    Meta::DependencyPropertyRegistry* registry,
    std::uint16_t registryEpoch = 0U) noexcept;
Dispatcher* ResolveObjectContextDispatcher(std::uint16_t slot) noexcept;
Meta::DependencyPropertyRegistry* ResolveObjectContextRegistry(std::uint16_t slot) noexcept;
std::uint16_t ResolveObjectContextRegistryEpoch(std::uint16_t slot) noexcept;
void InvalidateDispatcherContextSlots(const Dispatcher* dispatcher) noexcept;
void InvalidateRegistryContextSlots(const Meta::DependencyPropertyRegistry* registry) noexcept;

class AERO_GUI_API DispatcherReentrancyGuard {
public:
    DispatcherReentrancyGuard() noexcept = default;
    DispatcherReentrancyGuard(DispatcherReentrancyGuard&& other) noexcept;
    DispatcherReentrancyGuard& operator=(DispatcherReentrancyGuard&& other) noexcept;
    ~DispatcherReentrancyGuard();

    DispatcherReentrancyGuard(const DispatcherReentrancyGuard&) = delete;
    DispatcherReentrancyGuard& operator=(const DispatcherReentrancyGuard&) = delete;

    bool Active() const noexcept { return dispatcher_ != nullptr; }

    void Release() noexcept;

private:
    friend class Dispatcher;

    explicit DispatcherReentrancyGuard(Dispatcher* dispatcher) noexcept : dispatcher_(dispatcher) {}

    Dispatcher* dispatcher_ = nullptr;
};

} // namespace Aero::Threading
