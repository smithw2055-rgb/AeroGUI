#pragma once

#include <Aero/Media/Animation/AnimationTimeline.hpp>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API Int64AnimationBase : public AnimationTimeline {
    AERO_DECLARE_TYPE(Int64AnimationBase, AnimationTimeline)
public:
    std::int64_t GetFrom() const noexcept { return from_; }
    std::int64_t GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    std::int64_t ResolveFrom(std::int64_t defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    std::int64_t ResolveTo(std::int64_t defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(std::int64_t value) noexcept;
    void SetTo(std::int64_t value) noexcept;

protected:
    explicit Int64AnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    std::int64_t from_ = 0;
    std::int64_t to_ = 0;
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

} // namespace Aero::Media::Animation
