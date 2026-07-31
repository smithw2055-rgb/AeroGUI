#pragma once

#include <cstdint>

namespace Aero::Controls::Detail {

struct TemplateHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

} // namespace Aero::Controls::Detail
