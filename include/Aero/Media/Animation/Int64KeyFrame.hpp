#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API Int64KeyFrame : public KeyFrame<std::int64_t> {
    AERO_DECLARE_TYPE(Int64KeyFrame, KeyFrameBase)
protected:
    explicit Int64KeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<std::int64_t>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation
