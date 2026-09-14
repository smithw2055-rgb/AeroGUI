#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/AnimationTimeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API PointAnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(PointAnimationBase, AnimationTimeline)
public:
    Base::Point GetFrom() const noexcept { return from_; }
    Base::Point GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    Base::Point ResolveFrom(Base::Point defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    Base::Point ResolveTo(Base::Point defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(Base::Point value) noexcept;
    void SetTo(Base::Point value) noexcept;

protected:
    explicit PointAnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Point from_{};
    Base::Point to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
