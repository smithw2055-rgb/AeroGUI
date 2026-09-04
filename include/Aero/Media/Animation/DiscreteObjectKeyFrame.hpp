#pragma once

#include <Aero/Media/Animation/ObjectKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteObjectKeyFrame : public ObjectKeyFrame {
    AERO_DECLARE_TYPE(DiscreteObjectKeyFrame, ObjectKeyFrame)
public:
    DiscreteObjectKeyFrame() noexcept
        : ObjectKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation
