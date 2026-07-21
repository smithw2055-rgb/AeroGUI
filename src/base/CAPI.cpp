#include <Aero/CAPI.h>

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Utf8.hpp>

#include <cstddef>
#include <cstring>

namespace {

static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::Ok) == AERO_STATUS_OK,
    "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::OutOfMemory) ==
    AERO_STATUS_OUT_OF_MEMORY, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::InvalidArgument) ==
    AERO_STATUS_INVALID_ARGUMENT, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::OutOfRange) ==
    AERO_STATUS_OUT_OF_RANGE, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::InvalidUtf8) ==
    AERO_STATUS_INVALID_UTF8, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::NotInitialized) ==
    AERO_STATUS_NOT_INITIALIZED, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::Unsupported) ==
    AERO_STATUS_UNSUPPORTED, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::InternalError) ==
    AERO_STATUS_INTERNAL_ERROR, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::AlreadyExists) ==
    AERO_STATUS_ALREADY_EXISTS, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::InvalidState) ==
    AERO_STATUS_INVALID_STATE, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::NotFound) ==
    AERO_STATUS_NOT_FOUND, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::IdCollision) ==
    AERO_STATUS_ID_COLLISION, "C and C++ status codes must remain aligned");
static_assert(static_cast<std::uint32_t>(Aero::Base::ErrorCode::CycleDetected) ==
    AERO_STATUS_CYCLE_DETECTED, "C and C++ status codes must remain aligned");

AeroStatusCode ValidateUtf8Bridge(AeroStringView text) noexcept {
    if (text.data == nullptr && text.size != 0U) {
        return AERO_STATUS_INVALID_ARGUMENT;
    }
    const Aero::Base::Utf8Validation result = Aero::Base::ValidateUtf8(
        Aero::Base::StringView(text.data, text.size));
    return result.valid ? AERO_STATUS_OK : AERO_STATUS_INVALID_UTF8;
}

} // namespace

extern "C" AeroStatusCode AeroGetBaseApi(
    std::uint32_t requestedAbiVersion,
    std::uint32_t callerStructSize,
    AeroBaseApi* outApi) {
    constexpr std::uint32_t HeaderSize =
        static_cast<std::uint32_t>(offsetof(AeroBaseApi, runtime_version_major));

    if (outApi == nullptr || callerStructSize < HeaderSize) {
        return AERO_STATUS_INVALID_ARGUMENT;
    }
    if (requestedAbiVersion != AERO_BASE_ABI_VERSION_1) {
        return AERO_STATUS_UNSUPPORTED;
    }

    const AeroBaseApi current = {
        static_cast<std::uint32_t>(sizeof(AeroBaseApi)),
        AERO_BASE_ABI_VERSION_1,
        0U,
        1U,
        0U,
        &ValidateUtf8Bridge
    };

    const std::size_t bytesToCopy = callerStructSize < sizeof(AeroBaseApi)
        ? static_cast<std::size_t>(callerStructSize)
        : sizeof(AeroBaseApi);
    std::memcpy(outApi, &current, bytesToCopy);
    return AERO_STATUS_OK;
}
