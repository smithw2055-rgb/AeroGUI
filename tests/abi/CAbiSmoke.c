#include <Aero/CAPI.h>

#include <stddef.h>
#include <string.h>

int main(void) {
    AeroBaseApi api;
    (void)memset(&api, 0, sizeof(api));

    if (AeroGetBaseApi(AERO_BASE_ABI_VERSION_LATEST,
            (uint32_t)sizeof(api), &api) != AERO_STATUS_OK) {
        return 1;
    }
    if (api.struct_size < sizeof(AeroBaseApi) ||
        api.abi_version != AERO_BASE_ABI_VERSION_1 ||
        api.validate_utf8 == NULL) {
        return 2;
    }

    {
        const AeroStringView valid = {"AeroGUI", 7u};
        if (api.validate_utf8(valid) != AERO_STATUS_OK) {
            return 3;
        }
    }

    {
        const unsigned char invalid_bytes[] = {0xC0u, 0xAFu};
        const AeroStringView invalid = {
            (const char*)invalid_bytes,
            (uint32_t)sizeof(invalid_bytes)
        };
        if (api.validate_utf8(invalid) != AERO_STATUS_INVALID_UTF8) {
            return 4;
        }
    }

    {
        AeroBaseApi prefix;
        (void)memset(&prefix, 0xCD, sizeof(prefix));
        if (AeroGetBaseApi(AERO_BASE_ABI_VERSION_1,
            (uint32_t)offsetof(AeroBaseApi, runtime_version_major),
                &prefix) != AERO_STATUS_OK) {
            return 5;
        }
        if (prefix.abi_version != AERO_BASE_ABI_VERSION_1) {
            return 6;
        }
        if (prefix.runtime_version_major != 0xCDCDCDCDu) {
            return 7;
        }
    }

    {
        AeroBaseApi partial;
        (void)memset(&partial, 0xCD, sizeof(partial));
        if (AeroGetBaseApi(AERO_BASE_ABI_VERSION_1,
                (uint32_t)offsetof(AeroBaseApi, runtime_version_major) + 1u,
                &partial) != AERO_STATUS_OK) {
            return 8;
        }
        if (partial.runtime_version_major != 0xCDCDCDCDu) {
            return 9;
        }
        if (AeroGetBaseApi(AERO_BASE_ABI_VERSION_1,
                (uint32_t)offsetof(AeroBaseApi, runtime_version_minor),
                &partial) != AERO_STATUS_OK ||
            partial.runtime_version_major != 0u ||
            partial.runtime_version_minor != 0xCDCDCDCDu) {
            return 10;
        }
    }

    if (AeroGetBaseApi(999u, (uint32_t)sizeof(api), &api) !=
        AERO_STATUS_UNSUPPORTED) {
        return 11;
    }
    if (AeroGetBaseApi(AERO_BASE_ABI_VERSION_1,
            (uint32_t)sizeof(api), NULL) != AERO_STATUS_INVALID_ARGUMENT) {
        return 12;
    }
    return 0;
}
