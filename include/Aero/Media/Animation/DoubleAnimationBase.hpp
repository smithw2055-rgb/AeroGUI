#pragma once

#include <Aero/Media/Animation/AnimationTimeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DoubleAnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(DoubleAnimationBase, AnimationTimeline)
public:
    ~DoubleAnimationBase() override = default;
    double GetFrom() const noexcept { return from_; }
    double GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    double ResolveFrom(double defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    double ResolveTo(double defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(double value) noexcept;
    void SetTo(double value) noexcept;

    double GetCurrentValue(
        double defaultOriginValue,
        double defaultDestinationValue,
        double progress) const noexcept {
        return GetCurrentValueCore(
            defaultOriginValue,
            defaultDestinationValue,
            progress);
    }

protected:
    explicit DoubleAnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}
    virtual double GetCurrentValueCore(
        double defaultOriginValue,
        double defaultDestinationValue,
        double progress) const noexcept = 0;

private:
    double from_ = 0.0;
    double to_ = 0.0;
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
