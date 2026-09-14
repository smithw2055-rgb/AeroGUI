#pragma once

#include <Aero/Media/Animation/AnimationTimeline.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API Int16AnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(Int16AnimationBase, AnimationTimeline)
public:
    std::int16_t GetFrom() const noexcept { return from_; }
    std::int16_t GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    std::int16_t ResolveFrom(std::int16_t defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    std::int16_t ResolveTo(std::int16_t defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(std::int16_t value) noexcept;
    void SetTo(std::int16_t value) noexcept;

protected:
    explicit Int16AnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    std::int16_t from_ = 0;
    std::int16_t to_ = 0;
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
