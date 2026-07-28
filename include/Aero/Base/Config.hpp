#pragma once

#include <cstddef>
#include <cstdint>

// Incremented when the Base::Object virtual interface changed for Meta RTTI.
#define AERO_BASE_ABI_VERSION 2

#if defined(_WIN32) && defined(AERO_BUILD_SHARED)
// CMake emits per-target export tables for the C++ surface. Avoid applying a
// single module's dllexport/dllimport state to declarations from dependencies
// included in the same translation unit.
#  define AERO_API
#elif defined(__GNUC__) && defined(AERO_BUILD_SHARED)
#  define AERO_API __attribute__((visibility("default")))
#else
#  define AERO_API
#endif

#if defined(_MSC_VER)
#  define AERO_FORCE_INLINE __forceinline
#else
#  define AERO_FORCE_INLINE inline __attribute__((always_inline))
#endif

#if defined(_MSC_VER)
static_assert(_MSVC_LANG >= 201703L,
    "AeroGUI requires a compiler operating in ISO C++17 mode or newer");
#else
static_assert(__cplusplus >= 201703L,
    "AeroGUI requires a compiler operating in ISO C++17 mode or newer");
#endif
