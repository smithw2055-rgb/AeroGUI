#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API Int16KeyFrame : public KeyFrame<std::int16_t> {
    AERO_DECLARE_TYPE(Int16KeyFrame, KeyFrameBase)
protected:
    explicit Int16KeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<std::int16_t>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation
