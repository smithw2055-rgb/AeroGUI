#pragma once

// Source-only focus queue next to InputRouter.
// Not installed under include/Aero. Included from ViewState.hpp after ViewState.

namespace Aero {

class FocusHost {
public:
    explicit FocusHost(ViewState& owner) noexcept;
    void Bind() noexcept;

    ViewState* view = nullptr;
    Aero::InputRouter* input = nullptr;
    Base::Vector<Base::WeakRef<Aero::UIElement>> pendingFocusTargets;

    Base::Result<void> QueueFocus(Aero::UIElement& target) noexcept;
    Base::Result<std::uint32_t> ProcessPendingFocus() noexcept;
};

} // namespace Aero
