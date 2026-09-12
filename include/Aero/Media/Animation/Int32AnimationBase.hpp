#pragma once

#include <Aero/Media/Animation/AnimationTimeline.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API Int32AnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(Int32AnimationBase, AnimationTimeline)
public:
    std::int32_t GetFrom() const noexcept { return from_; }
    std::int32_t GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    std::int32_t ResolveFrom(std::int32_t defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    std::int32_t ResolveTo(std::int32_t defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(std::int32_t value) noexcept;
    void SetTo(std::int32_t value) noexcept;

protected:
    explicit Int32AnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    std::int32_t from_ = 0;
    std::int32_t to_ = 0;
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
