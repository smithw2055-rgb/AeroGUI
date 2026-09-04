#pragma once

#include <Aero/Media/Animation/ControllableStoryboardAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SeekStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(SeekStoryboard, ControllableStoryboardAction)
public:
    SeekStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
    StringView GetOffset() const noexcept { return offsetText_.View(); }
    AnimationTime GetOffsetMicroseconds() const noexcept { return offsetMicroseconds_; }
    void SetOffset(StringView value) noexcept;

private:
    String offsetText_;
    AnimationTime offsetMicroseconds_ = 0U;
};
} // namespace Aero::Media::Animation
