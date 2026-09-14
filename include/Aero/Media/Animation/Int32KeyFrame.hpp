#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API Int32KeyFrame : public KeyFrame<std::int32_t> {
    AERO_DECLARE_TYPE(Int32KeyFrame, KeyFrameBase)
protected:
    explicit Int32KeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<std::int32_t>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation
