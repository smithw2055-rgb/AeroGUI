#pragma once

#include <cstdint>

namespace Aero::Core {

using TypeId = std::uint64_t;
using MemberId = std::uint64_t;

inline constexpr TypeId InvalidTypeId = 0U;
inline constexpr MemberId InvalidMemberId = 0U;

} // namespace Aero::Core
