#include "gui/ViewFrame.hpp"
#include "gui/input/InputManager.hpp"
#include <Aero/UIElement.hpp>

namespace Aero {

FocusHost::FocusHost(ViewFrame& owner) noexcept
    : view(&owner),
      pendingFocusTargets(owner.allocator) {}

void FocusHost::Bind() noexcept {
    // Input is read on demand from the ElementTree hub via Input().
}

Aero::InputManager* FocusHost::Input() const noexcept {
    return view != nullptr ? view->Input() : nullptr;
}

void FocusHost::QueueFocus(Aero::UIElement& target) noexcept {
    Base::Ref<Aero::UIElement> retained =
        Base::Ref<Aero::UIElement>::FromBorrowed(target);
    for (const Base::WeakRef<Aero::UIElement>& pending :
         pendingFocusTargets) {
        Base::Ref<Aero::UIElement> existing = pending.Lock();
        if (existing.Get() == &target) return;
    }
    pendingFocusTargets.PushBack(
        Base::WeakRef<Aero::UIElement>(retained));
}

std::uint32_t FocusHost::ProcessPendingFocus() noexcept {
    Aero::InputManager* input = Input();
    if (input == nullptr || pendingFocusTargets.Empty()) return 0U;
    std::uint32_t focusedCount = 0U;
    std::uint32_t output = 0U;
    for (std::uint32_t index = 0U;
         index < pendingFocusTargets.Size(); ++index) {
        Base::Ref<Aero::UIElement> target =
            pendingFocusTargets[index].Lock();
        if (!target) continue;
        if (!target->GetIsLoaded()) {
            if (output != index) {
                pendingFocusTargets[output] =
                    std::move(pendingFocusTargets[index]);
            }
            ++output;
            continue;
        }
        if (!target->GetIsEnabled()) continue;
        if (input->SetFocus(target.Get())) {
            ++focusedCount;
        }
    }
    pendingFocusTargets.Resize(output);
    return focusedCount;
}

} // namespace Aero
