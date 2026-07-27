#pragma once

#include <cstdint>

namespace Aero {

inline constexpr std::uint32_t VersionMajor = 0U;
inline constexpr std::uint32_t VersionMinor = 2U;
inline constexpr std::uint32_t VersionPatch = 0U;

// Public binary and descriptor contracts. These values are intentionally
// independent from the product semantic version so cache/tool compatibility can
// be diagnosed precisely.
inline constexpr std::uint32_t CppAbiVersion = 1U;
inline constexpr std::uint32_t ModuleAbiVersion = 2U;
inline constexpr std::uint32_t XamlFacetAbiVersion = 8U;
inline constexpr std::uint32_t XamlSchemaAbiVersion = 8U;
inline constexpr std::uint32_t RuntimeAbiVersion = 2U;

} // namespace Aero
