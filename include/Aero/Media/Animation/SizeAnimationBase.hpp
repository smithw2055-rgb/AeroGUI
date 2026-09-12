#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/AnimationTimeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SizeAnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(SizeAnimationBase, AnimationTimeline)
public:
    Base::Size GetFrom() const noexcept { return from_; }
    Base::Size GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    Base::Size ResolveFrom(Base::Size defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    Base::Size ResolveTo(Base::Size defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(Base::Size value) noexcept;
    void SetTo(Base::Size value) noexcept;

protected:
    explicit SizeAnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Size from_{};
    Base::Size to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
