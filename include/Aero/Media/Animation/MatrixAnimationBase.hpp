#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/AnimationTimeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API MatrixAnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(MatrixAnimationBase, AnimationTimeline)
public:
    Base::Transform2D GetFrom() const noexcept { return from_; }
    Base::Transform2D GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    Base::Transform2D ResolveFrom(
        Base::Transform2D defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    Base::Transform2D ResolveTo(
        Base::Transform2D defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(Base::Transform2D value) noexcept;
    void SetTo(Base::Transform2D value) noexcept;

protected:
    explicit MatrixAnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Transform2D from_{};
    Base::Transform2D to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
