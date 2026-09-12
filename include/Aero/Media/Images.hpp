#pragma once

#include <Aero/Media/ImageSource.hpp>
#include <Aero/Media/BitmapImage.hpp>
#include <Aero/Media/CroppedBitmap.hpp>

#include <cstdint>

namespace Aero::Media {

enum class Stretch : std::uint8_t {
    None = 0U,
    Fill,
    Uniform,
    UniformToFill
};

enum class StretchDirection : std::uint8_t {
    UpOnly = 0U,
    DownOnly,
    Both
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::Stretch)

AERO_DECLARE_TYPE_ENUM(Aero::Media::StretchDirection)

namespace Aero::Controls {
using Stretch = Aero::Media::Stretch;
using StretchDirection =
    Aero::Media::StretchDirection;
} // namespace Aero::Controls
