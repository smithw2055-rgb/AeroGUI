#pragma once

#include <Aero/Media/Animation/StringKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteStringKeyFrame : public StringKeyFrame {
    AERO_DECLARE_TYPE(DiscreteStringKeyFrame, StringKeyFrame)
public:
    DiscreteStringKeyFrame() noexcept
        : StringKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation
