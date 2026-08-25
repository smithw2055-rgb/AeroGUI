#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

Base::Result<void> ViewState::QueueFocus(Aero::UIElement& target) noexcept {
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

Base::Result<std::uint32_t> ViewState::ProcessPendingFocus() noexcept {
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
