#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/AnimationTimeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ThicknessAnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(ThicknessAnimationBase, AnimationTimeline)
public:
    Base::Thickness GetFrom() const noexcept { return from_; }
    Base::Thickness GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    Base::Thickness ResolveFrom(Base::Thickness defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    Base::Thickness ResolveTo(Base::Thickness defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(Base::Thickness value) noexcept;
    void SetTo(Base::Thickness value) noexcept;

protected:
    explicit ThicknessAnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Thickness from_{};
    Base::Thickness to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
