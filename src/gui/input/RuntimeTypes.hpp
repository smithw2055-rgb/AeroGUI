#pragma once

#include <Aero/Base/Delegate.hpp>
#include <cstdint>

namespace Aero { class UIElement; }

namespace Aero::Input {

struct CommandBindingHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

struct InputBindingHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

using RequerySuggestedHandler = Base::Delegate<void()>;
using PointerStateChangedHandler = Base::Delegate<void(UIElement&)>;
using PointerCaptureChangedHandler = Base::Delegate<void(std::uint32_t, UIElement*, bool)>;

} // namespace Aero::Input
