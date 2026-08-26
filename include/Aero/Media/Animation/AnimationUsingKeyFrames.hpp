#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Animation/AnimationTimeline.hpp>
#include <utility>

namespace Aero::Media::Animation {

template<typename TKeyFrame>
class AnimationUsingKeyFrames : public AnimationTimeline {
public:
    Result<void> AddKeyFrame(Ref<TKeyFrame> value) noexcept {
        Result<void> writable = WritePreamble();
        if (!writable) return writable.GetStatus();
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Key frame cannot be null");
        }
        Result<void> added = keyFrames_.PushBack(std::move(value));
        if (!added) return added.GetStatus();
        WritePostscript();
        return {};
    }
    void ClearKeyFrames() noexcept {
        if (!WritePreamble() || keyFrames_.Empty()) return;
        keyFrames_.Clear();
        WritePostscript();
    }
    Span<const Ref<TKeyFrame>> GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

protected:
    explicit AnimationUsingKeyFrames(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Vector<Ref<TKeyFrame>> keyFrames_;
};

} // namespace Aero::Media::Animation
