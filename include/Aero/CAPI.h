#ifndef AERO_CAPI_H_INCLUDED
#define AERO_CAPI_H_INCLUDED

#include <stdint.h>

#if defined(_WIN32) && defined(AERO_BUILD_SHARED)
#  if defined(AERO_BASE_EXPORTS)
#    define AERO_C_API __declspec(dllexport)
#  else
#    define AERO_C_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(AERO_BUILD_SHARED)
#  define AERO_C_API __attribute__((visibility("default")))
#else
#  define AERO_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AERO_BASE_ABI_VERSION_1 = 1u,
    AERO_BASE_ABI_VERSION_LATEST = AERO_BASE_ABI_VERSION_1
};

typedef uint32_t AeroStatusCode;

enum {
    AERO_STATUS_OK = 0u,
    AERO_STATUS_OUT_OF_MEMORY = 1u,
    AERO_STATUS_INVALID_ARGUMENT = 2u,
    AERO_STATUS_OUT_OF_RANGE = 3u,
    AERO_STATUS_INVALID_UTF8 = 4u,
    AERO_STATUS_NOT_INITIALIZED = 5u,
    AERO_STATUS_UNSUPPORTED = 6u,
    AERO_STATUS_INTERNAL_ERROR = 7u
};

typedef struct AeroStringView {
    const char* data;
    uint32_t size;
} AeroStringView;

typedef AeroStatusCode (*AeroValidateUtf8Fn)(AeroStringView text);

typedef struct AeroBaseApi {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t runtime_version_major;
    uint32_t runtime_version_minor;
    uint32_t runtime_version_patch;
    AeroValidateUtf8Fn validate_utf8;
} AeroBaseApi;

AERO_C_API AeroStatusCode AeroGetBaseApi(
    uint32_t requested_abi_version,
    uint32_t caller_struct_size,
    AeroBaseApi* out_api);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AERO_CAPI_H_INCLUDED */
