#pragma once

#include <Aero/Value.hpp>
#include <cstdint>

namespace Aero::Input {

enum class InputScope : std::uint8_t {
    Default = 0U,
    Url,
    EmailSmtpAddress,
    Digits,
    Number,
    Password,
    TelephoneNumber
};

} // namespace Aero::Input

AERO_DECLARE_TYPE_ENUM(Aero::Input::InputScope)
