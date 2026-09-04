#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/AnimationTimeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ColorAnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(ColorAnimationBase, AnimationTimeline)
public:
    Base::Color GetFrom() const noexcept { return from_; }
    Base::Color GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    Base::Color ResolveFrom(Base::Color defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    Base::Color ResolveTo(Base::Color defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(Base::Color value) noexcept;
    void SetTo(Base::Color value) noexcept;

protected:
    explicit ColorAnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Color from_{};
    Base::Color to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
