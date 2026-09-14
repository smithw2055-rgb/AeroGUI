#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/AnimationTimeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API RectAnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(RectAnimationBase, AnimationTimeline)
public:
    Base::Rect GetFrom() const noexcept { return from_; }
    Base::Rect GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    Base::Rect ResolveFrom(Base::Rect defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    Base::Rect ResolveTo(Base::Rect defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(Base::Rect value) noexcept;
    void SetTo(Base::Rect value) noexcept;

protected:
    explicit RectAnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Rect from_{};
    Base::Rect to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
