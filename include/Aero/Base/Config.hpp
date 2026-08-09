#pragma once

#include <cstddef>
#include <cstdint>

// Historical SDK umbrella headers used these macros to produce different class
// definitions from the same public header. That model is no longer supported:
// every translation unit must see one invariant public type definition.
#if defined(AERO_SDK_SURFACE_ONLY) || \
    defined(AERO_MODULE_SDK_AUTHORING_ONLY)
#  error "AeroGUI public type-shaping SDK macros have been removed"
#endif

// Incremented when the Base::Object virtual interface changed for Meta RTTI.
#define AERO_BASE_ABI_VERSION 2

#if defined(_WIN32) && defined(AERO_BUILD_SHARED)
#  if defined(AERO_BASE_EXPORTS)
#    define AERO_BASE_API __declspec(dllexport)
#  else
#    define AERO_BASE_API __declspec(dllimport)
#  endif
#  if defined(AERO_AUDIO_EXPORTS)
#    define AERO_AUDIO_API __declspec(dllexport)
#  else
#    define AERO_AUDIO_API __declspec(dllimport)
#  endif
#  if defined(AERO_GUI_EXPORTS)
#    define AERO_GUI_API __declspec(dllexport)
#    define AERO_GUI_INTERNAL_API __declspec(dllexport)
#  else
#    define AERO_GUI_API __declspec(dllimport)
#    define AERO_GUI_INTERNAL_API __declspec(dllimport)
#  endif
#  if defined(AERO_APP_EXPORTS)
#    define AERO_APP_API __declspec(dllexport)
#  else
#    define AERO_APP_API __declspec(dllimport)
#  endif
#  if defined(AERO_RENDER_D3D11_EXPORTS)
#    define AERO_RENDER_D3D11_API __declspec(dllexport)
#  else
#    define AERO_RENDER_D3D11_API __declspec(dllimport)
#  endif
#  if defined(AERO_RENDER_OPENGL33_EXPORTS)
#    define AERO_RENDER_OPENGL33_API __declspec(dllexport)
#  else
#    define AERO_RENDER_OPENGL33_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(AERO_BUILD_SHARED)
#  define AERO_BASE_API __attribute__((visibility("default")))
#  define AERO_AUDIO_API __attribute__((visibility("default")))
#  define AERO_GUI_API __attribute__((visibility("default")))
#  define AERO_GUI_INTERNAL_API __attribute__((visibility("default")))
#  define AERO_APP_API __attribute__((visibility("default")))
#  define AERO_RENDER_D3D11_API __attribute__((visibility("default")))
#  define AERO_RENDER_OPENGL33_API __attribute__((visibility("default")))
#else
#  define AERO_BASE_API
#  define AERO_AUDIO_API
#  define AERO_GUI_API
#  define AERO_GUI_INTERNAL_API
#  define AERO_APP_API
#  define AERO_RENDER_D3D11_API
#  define AERO_RENDER_OPENGL33_API
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
