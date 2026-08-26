#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <utility>

namespace Aero {

FocusHost::FocusHost(ViewState& owner) noexcept
    : view(&owner),
      pendingFocusTargets(owner.allocator) {}

void FocusHost::Bind() noexcept {
    input = view != nullptr ? view->input : nullptr;
}

Base::Result<void> FocusHost::QueueFocus(Aero::UIElement& target) noexcept {
        Base::Ref<Aero::UIElement> retained =
            Base::Ref<Aero::UIElement>::FromBorrowed(target);
        for (const Base::WeakRef<Aero::UIElement>& pending :
             pendingFocusTargets) {
            Base::Ref<Aero::UIElement> existing = pending.Lock();
            if (existing.Get() == &target) return {};
        }
        return pendingFocusTargets.PushBack(
            Base::WeakRef<Aero::UIElement>(retained));
    }

Base::Result<std::uint32_t> FocusHost::ProcessPendingFocus() noexcept {
        if (input == nullptr && view != nullptr) {
            input = view->input;
        }
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
            Base::Result<bool> focused = input->SetFocus(target.Get());
            if (!focused) return focused.GetStatus();
            if (focused.Value()) ++focusedCount;
        }
        Base::Result<void> resized =
            pendingFocusTargets.Resize(output);
        if (!resized) return resized.GetStatus();
        return focusedCount;
    }


} // namespace Aero
