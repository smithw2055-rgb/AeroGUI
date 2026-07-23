#pragma once

#include <cstdint>

namespace Aero::Base {

using MetaTypeId = std::uint64_t;
using MetaMemberId = std::uint64_t;

inline constexpr MetaTypeId InvalidMetaTypeId = 0U;
inline constexpr MetaMemberId InvalidMetaMemberId = 0U;

} // namespace Aero::Base
