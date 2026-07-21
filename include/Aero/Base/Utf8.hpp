#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero::Base {

struct Utf8Validation final {
    bool valid = true;
    std::uint32_t errorOffset = 0;
};

AERO_NODISCARD AERO_API Utf8Validation ValidateUtf8(StringView text) noexcept;

} // namespace Aero::Base
