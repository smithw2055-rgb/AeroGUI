#include <Aero/Integration/HostedGraphics.hpp>

namespace {

Aero::Integration::HostedGraphicsResult Acquire(
    void*,
    Aero::Integration::HostedGraphicsTarget* target) noexcept {
    if (target == nullptr) {
        return Aero::Integration::HostedGraphicsResult::
            InvalidArgument;
    }
    target->colorTarget = 1U;
    target->width = 1U;
    target->height = 1U;
    target->stableId = 1U;
    return Aero::Integration::HostedGraphicsResult::Success;
}

Aero::Integration::HostedGraphicsResult Submit(
    void*,
    const Aero::Integration::HostedGraphicsTarget*,
    const Aero::Integration::HostedGraphicsCommandListView*,
    std::uint64_t) noexcept {
    return Aero::Integration::HostedGraphicsResult::Success;
}

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::Integration::RenderEndpoint>>
CreateHostedEndpoint() noexcept {
    Aero::Integration::HostedGraphicsCallbacks callbacks;
    callbacks.capabilities =
        Aero::Integration::
            HostedGraphicsCapabilityEmbeddedTarget;
    callbacks.acquireTarget = &Acquire;
    callbacks.submit = &Submit;
    return Aero::Integration::CreateHostedEmbeddedEndpoint(
        callbacks);
}

} // namespace
