#include "gui/media/AnimationEngineCommon.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include <Aero/Layout.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Value.hpp>

namespace Aero {

namespace Model = Aero::Media::Animation::Model;

namespace {

static Base::Result<double> DecodeDoubleOrLength(
    const Meta::PropertyValue& value) noexcept {
    if (value.Type() == Meta::TypeOf<Length>()) {
        Base::Result<Length> length = Meta::ValueCodec<Length>::Decode(value);
        if (!length) return length.GetStatus();
        return length.Value().isAuto ? 0.0 : length.Value().value;
    }
    return Meta::ValueCodec<double>::Decode(value);
}

} // namespace

Base::Result<AnimationHandle> AnimationEngine::BeginTimeline(
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    const Media::Animation::Timeline& timeline,
    const TimelineTiming& timing,
    Meta::Registry* metadata) noexcept {
    const Meta::TypeId type = timeline.RuntimeType();
    if (type == Media::Animation::DoubleAnimation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::DoubleAnimation&>(timeline);
        Model::DoubleAnimation runtime = Media::Animation::Double(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (metadata != nullptr && metadata->Types().IsDerivedFrom(type, Media::Animation::DoubleAnimationBase::StaticTypeId())) {
        auto& animation = static_cast<const Media::Animation::DoubleAnimationBase&>(timeline);
        Base::Result<Meta::PropertyValue> current = target.GetValue(property);
        if (!current) return current.GetStatus();
        Base::Result<double> origin = DecodeDoubleOrLength(current.Value());
        if (!origin) return origin.GetStatus();

        Model::CustomDoubleAnimation runtime;
        runtime.animation = Base::Ref<Media::Animation::DoubleAnimationBase>::TryFromBorrowed(
            const_cast<Media::Animation::DoubleAnimationBase&>(animation));
        if (!runtime.animation) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Custom DoubleAnimation is not reference-counted");
        }
        runtime.defaultOriginValue = origin.Value();
        runtime.defaultDestinationValue = animation.ResolveTo(origin.Value());
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::ColorAnimation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::ColorAnimation&>(timeline);
        Model::ColorAnimation runtime = Media::Animation::Color(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::PointAnimation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::PointAnimation&>(timeline);
        Model::PointAnimation runtime = Media::Animation::Point(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::RectAnimation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::RectAnimation&>(timeline);
        Model::RectAnimation runtime = Media::Animation::Rect(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::ThicknessAnimation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::ThicknessAnimation&>(timeline);
        Model::ThicknessAnimation runtime = Media::Animation::Thickness(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::Int16Animation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::Int16Animation&>(timeline);
        Model::IntegerAnimation runtime = Media::Animation::Integer16(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::Int32Animation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::Int32Animation&>(timeline);
        Model::IntegerAnimation runtime = Media::Animation::Integer32(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::Int64Animation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::Int64Animation&>(timeline);
        Model::IntegerAnimation runtime = Media::Animation::Integer64(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::SizeAnimation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::SizeAnimation&>(timeline);
        Model::SizeAnimation runtime = Media::Animation::Size(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::MatrixAnimation::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::MatrixAnimation&>(timeline);
        Model::MatrixAnimation runtime = Media::Animation::Matrix(authored);
        runtime.timing = timing;
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::DoubleAnimationUsingKeyFrames::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::DoubleAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Model::DoubleKeyFrame> frames =
            Media::Animation::BuildSortedKeyFrames<Model::DoubleKeyFrame>(
                authored, timing.durationMicroseconds);
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetAnimationBaseValueInternal(property);
        if (!base || base.Value().IsUnset()) {
            base = target.GetValue(property);
        }
        if (!base) return base.GetStatus();
        Base::Result<double> decoded = DecodeDoubleOrLength(base.Value());
        Model::DoubleKeyFrameAnimation runtime;
        if (decoded) {
            runtime.baseValue = decoded.Value();
        } else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) {
            runtime.baseValue = frames.Front().value;
        } else {
            return decoded.GetStatus();
        }
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::ColorAnimationUsingKeyFrames::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::ColorAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Model::ColorKeyFrame> frames =
            Media::Animation::BuildSortedKeyFrames<Model::ColorKeyFrame>(
                authored, timing.durationMicroseconds);
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetValue(property);
        if (!base) return base.GetStatus();
        Base::Result<Base::Color> decoded = Meta::ValueCodec<Base::Color>::Decode(base.Value());
        if (!decoded) return decoded.GetStatus();
        Model::ColorKeyFrameAnimation runtime;
        runtime.baseValue = decoded.Value();
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::PointAnimationUsingKeyFrames::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::PointAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Model::PointKeyFrame> frames =
            Media::Animation::BuildSortedKeyFrames<Model::PointKeyFrame>(
                authored, timing.durationMicroseconds);
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetAnimationBaseValueInternal(property);
        if (!base || base.Value().IsUnset()) {
            base = target.GetValue(property);
        }
        if (!base) return base.GetStatus();
        Base::Result<Base::Point> decoded = Meta::ValueCodec<Base::Point>::Decode(base.Value());
        Model::PointKeyFrameAnimation runtime;
        if (decoded) {
            runtime.baseValue = decoded.Value();
        } else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) {
            runtime.baseValue = frames.Front().value;
        } else {
            return decoded.GetStatus();
        }
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::ThicknessAnimationUsingKeyFrames::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::ThicknessAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Model::ThicknessKeyFrame> frames =
            Media::Animation::BuildSortedKeyFrames<Model::ThicknessKeyFrame>(
                authored, timing.durationMicroseconds);
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetValue(property);
        if (!base) return base.GetStatus();
        Base::Result<Base::Thickness> decoded = Meta::ValueCodec<Base::Thickness>::Decode(base.Value());
        Model::ThicknessKeyFrameAnimation runtime;
        if (decoded) {
            runtime.baseValue = decoded.Value();
        } else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) {
            runtime.baseValue = frames.Front().value;
        } else {
            return decoded.GetStatus();
        }
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::Int16AnimationUsingKeyFrames::StaticTypeId() ||
        type == Media::Animation::Int32AnimationUsingKeyFrames::StaticTypeId() ||
        type == Media::Animation::Int64AnimationUsingKeyFrames::StaticTypeId()) {
        Model::IntegerAnimationWidth width = Model::IntegerAnimationWidth::Int32;
        Base::Vector<Model::IntegerKeyFrame> frames;
        if (type == Media::Animation::Int16AnimationUsingKeyFrames::StaticTypeId()) {
            width = Model::IntegerAnimationWidth::Int16;
            auto& authored = static_cast<const Media::Animation::Int16AnimationUsingKeyFrames&>(timeline);
            frames = Media::Animation::BuildSortedKeyFrames<Model::IntegerKeyFrame>(
                authored, timing.durationMicroseconds);
        } else if (type == Media::Animation::Int64AnimationUsingKeyFrames::StaticTypeId()) {
            width = Model::IntegerAnimationWidth::Int64;
            auto& authored = static_cast<const Media::Animation::Int64AnimationUsingKeyFrames&>(timeline);
            frames = Media::Animation::BuildSortedKeyFrames<Model::IntegerKeyFrame>(
                authored, timing.durationMicroseconds);
        } else {
            auto& authored = static_cast<const Media::Animation::Int32AnimationUsingKeyFrames&>(timeline);
            frames = Media::Animation::BuildSortedKeyFrames<Model::IntegerKeyFrame>(
                authored, timing.durationMicroseconds);
        }
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetValue(property);
        if (!base) return base.GetStatus();
        Model::IntegerKeyFrameAnimation runtime;
        runtime.width = width;
        if (width == Model::IntegerAnimationWidth::Int16) {
            auto decoded = Meta::ValueCodec<std::int16_t>::Decode(base.Value());
            if (decoded) runtime.baseValue = decoded.Value();
            else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) runtime.baseValue = frames.Front().value;
            else return decoded.GetStatus();
        } else if (width == Model::IntegerAnimationWidth::Int64) {
            auto decoded = Meta::ValueCodec<std::int64_t>::Decode(base.Value());
            if (decoded) runtime.baseValue = decoded.Value();
            else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) runtime.baseValue = frames.Front().value;
            else return decoded.GetStatus();
        } else {
            auto decoded = Meta::ValueCodec<std::int32_t>::Decode(base.Value());
            if (decoded) runtime.baseValue = decoded.Value();
            else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) runtime.baseValue = frames.Front().value;
            else return decoded.GetStatus();
        }
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::SizeAnimationUsingKeyFrames::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::SizeAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Model::SizeKeyFrame> frames =
            Media::Animation::BuildSortedKeyFrames<Model::SizeKeyFrame>(
                authored, timing.durationMicroseconds);
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetValue(property);
        if (!base) return base.GetStatus();
        Base::Result<Base::Size> decoded = Meta::ValueCodec<Base::Size>::Decode(base.Value());
        Model::SizeKeyFrameAnimation runtime;
        if (decoded) {
            runtime.baseValue = decoded.Value();
        } else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) {
            runtime.baseValue = frames.Front().value;
        } else {
            return decoded.GetStatus();
        }
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::MatrixAnimationUsingKeyFrames::StaticTypeId()) {
        auto& authored = static_cast<const Media::Animation::MatrixAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Model::MatrixKeyFrame> frames =
            Media::Animation::BuildSortedKeyFrames<Model::MatrixKeyFrame>(
                authored, timing.durationMicroseconds);
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetValue(property);
        if (!base) return base.GetStatus();
        Base::Result<Base::Transform2D> decoded = Meta::ValueCodec<Base::Transform2D>::Decode(base.Value());
        Model::MatrixKeyFrameAnimation runtime;
        if (decoded) {
            runtime.baseValue = decoded.Value();
        } else if (!frames.Empty() && frames.Front().keyTimeMicroseconds == 0U) {
            runtime.baseValue = frames.Front().value;
        } else {
            return decoded.GetStatus();
        }
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    if (type == Media::Animation::BooleanAnimationUsingKeyFrames::StaticTypeId() ||
        type == Media::Animation::ObjectAnimationUsingKeyFrames::StaticTypeId() ||
        type == Media::Animation::StringAnimationUsingKeyFrames::StaticTypeId()) {
        Base::Vector<Model::DiscreteAnimationKeyFrame> frames;
        if (type == Media::Animation::BooleanAnimationUsingKeyFrames::StaticTypeId()) {
            auto& authored = static_cast<const Media::Animation::BooleanAnimationUsingKeyFrames&>(timeline);
            frames = Media::Animation::BuildSortedKeyFramesWith<Model::DiscreteAnimationKeyFrame>(
                authored, timing.durationMicroseconds,
                [](const Media::Animation::BooleanKeyFrame& frame,
                   Media::Animation::AnimationTime duration,
                   std::uint32_t keyIndex, std::uint32_t count) {
                    Model::DiscreteAnimationKeyFrame runtime;
                    runtime.keyTimeMicroseconds = Media::Animation::ResolveKeyTime(
                        frame.GetKeyTime(), duration, keyIndex, count);
                    Base::Result<Meta::PropertyValue> encoded = Meta::ValueCodec<bool>::Encode(frame.GetValue());
                    if (encoded) runtime.value = std::move(encoded).Value();
                    return runtime;
                });
        } else if (type == Media::Animation::StringAnimationUsingKeyFrames::StaticTypeId()) {
            auto& authored = static_cast<const Media::Animation::StringAnimationUsingKeyFrames&>(timeline);
            frames = Media::Animation::BuildSortedKeyFramesWith<Model::DiscreteAnimationKeyFrame>(
                authored, timing.durationMicroseconds,
                [](const Media::Animation::StringKeyFrame& frame,
                   Media::Animation::AnimationTime duration,
                   std::uint32_t keyIndex, std::uint32_t count) {
                    Model::DiscreteAnimationKeyFrame runtime;
                    runtime.keyTimeMicroseconds = Media::Animation::ResolveKeyTime(
                        frame.GetKeyTime(), duration, keyIndex, count);
                    Base::Result<Meta::PropertyValue> encoded = Meta::ValueCodec<Base::String>::Encode(frame.GetValue());
                    if (encoded) runtime.value = std::move(encoded).Value();
                    return runtime;
                });
        } else {
            auto& authored = static_cast<const Media::Animation::ObjectAnimationUsingKeyFrames&>(timeline);
            frames = Media::Animation::BuildSortedKeyFramesWith<Model::DiscreteAnimationKeyFrame>(
                authored, timing.durationMicroseconds,
                [&](const Media::Animation::ObjectKeyFrame& frame,
                    Media::Animation::AnimationTime duration,
                    std::uint32_t keyIndex, std::uint32_t count) {
                    Model::DiscreteAnimationKeyFrame runtime;
                    runtime.keyTimeMicroseconds = Media::Animation::ResolveKeyTime(
                        frame.GetKeyTime(), duration, keyIndex, count);
                    runtime.value = frame.GetValue();
                    const Meta::DependencyProperty* targetProperty =
                        DependencyObjectAccess::PropertyRegistry(target).Find(property);
                    if (targetProperty != nullptr && runtime.value.IsNullObject() &&
                        runtime.value.Type() != targetProperty->ValueType()) {
                        runtime.value = Meta::PropertyValue::NullObject(targetProperty->ValueType());
                    }
                    return runtime;
                });
        }
        if (frames.Empty()) return Model::AnimationHandle{};
        Base::Result<Meta::PropertyValue> base = target.GetValue(property);
        if (!base) return base.GetStatus();
        Model::DiscreteAnimation runtime;
        runtime.baseValue = base.Value();
        runtime.timing = timing;
        if (runtime.timing.durationMicroseconds == 0U && !frames.Empty()) {
            runtime.timing.durationMicroseconds = frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        return Begin(target, property, runtime);
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Storyboard contains an unsupported Timeline type");
}

} // namespace Aero
