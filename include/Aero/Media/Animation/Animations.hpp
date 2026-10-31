#pragma once

// AnimationTimeline plus the from/to and key-frame templates. XAML names
// such as DoubleAnimation are these specializations, not a class per value.
#include <Aero/Media/Animation/Timeline.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Animation/KeyFrames.hpp>
#include <Aero/Media/Animation/EasingFunctions.hpp>
#include <Aero/Value.hpp>

#include <cmath>
#include <type_traits>
#include <utility>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API AnimationTimeline : public Timeline {
    AERO_DECLARE_TYPE(AnimationTimeline, Timeline)

public:
    virtual Meta::PropertyValue GetCurrentPropertyValue(
        const Meta::PropertyValue& defaultOriginValue,
        const Meta::PropertyValue& defaultDestinationValue,
        double progress,
        AnimationTime sampleTime) const noexcept {
        static_cast<void>(defaultOriginValue);
        static_cast<void>(defaultDestinationValue);
        static_cast<void>(progress);
        static_cast<void>(sampleTime);
        return {};
    }

    virtual bool IsKeyFrameAnimation() const noexcept { return false; }
    virtual AnimationTime GetEffectiveDurationMicroseconds() const noexcept {
        return GetDuration().HasTimeSpan() ? GetDuration().GetTimeSpan().Microseconds() : 0U;
    }

protected:
    explicit AnimationTimeline(Meta::TypeId runtimeType) noexcept : Timeline(runtimeType) {}
};

struct AnimationMath {
    static constexpr double Pi = 3.1415926535897932384626433832795;

    static inline double Clamp01(double value) noexcept {
        return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
    }

    static inline double ApplyAccelerationDeceleration(
        double progress, double acceleration, double deceleration) noexcept {
        const double value = Clamp01(progress);
        if (acceleration <= 0.0 && deceleration <= 0.0) {
            return value;
        }
        const double maximumVelocity =
            1.0 / (1.0 - (acceleration + deceleration) * 0.5);
        if (acceleration > 0.0 && value < acceleration) {
            return 0.5 * maximumVelocity * value * value / acceleration;
        }
        const double beforeDeceleration = 1.0 - deceleration;
        const double accumulatedBeforeDeceleration =
            maximumVelocity * (beforeDeceleration - acceleration * 0.5);
        if (deceleration <= 0.0 || value <= beforeDeceleration) {
            return accumulatedBeforeDeceleration +
                maximumVelocity * (value - beforeDeceleration);
        }
        const double elapsed = value - beforeDeceleration;
        return accumulatedBeforeDeceleration + maximumVelocity * elapsed -
            0.5 * maximumVelocity * elapsed * elapsed / deceleration;
    }

    static inline double CubicBezierCoordinate(
        double t, double first, double second) noexcept {
        const double inverse = 1.0 - t;
        return 3.0 * inverse * inverse * t * first +
            3.0 * inverse * t * t * second + t * t * t;
    }

    static inline double CubicBezierDerivative(
        double t, double first, double second) noexcept {
        const double inverse = 1.0 - t;
        return 3.0 * inverse * inverse * first +
            6.0 * inverse * t * (second - first) +
            3.0 * t * t * (1.0 - second);
    }

    static inline double EvaluateSpline(
        double progress,
        double cp1x, double cp1y,
        double cp2x, double cp2y) noexcept {
        const double target = Clamp01(progress);
        double parameter = target;
        for (std::uint32_t iteration = 0U; iteration < 8U; ++iteration) {
            const double x = CubicBezierCoordinate(parameter, cp1x, cp2x);
            const double derivative = CubicBezierDerivative(parameter, cp1x, cp2x);
            if (std::abs(derivative) < 1.0e-7) break;
            parameter = Clamp01(parameter - (x - target) / derivative);
        }
        double low = 0.0;
        double high = 1.0;
        for (std::uint32_t iteration = 0U; iteration < 12U; ++iteration) {
            const double x = CubicBezierCoordinate(parameter, cp1x, cp2x);
            if (x < target) low = parameter;
            else high = parameter;
            parameter = (low + high) * 0.5;
        }
        return Clamp01(CubicBezierCoordinate(parameter, cp1y, cp2y));
    }
};

template<class T>
inline constexpr bool IsInterpolatable =
    std::is_same_v<T, double> ||
    std::is_same_v<T, Base::Color> ||
    std::is_same_v<T, Base::Point> ||
    std::is_same_v<T, Base::Rect> ||
    std::is_same_v<T, Base::Thickness> ||
    std::is_same_v<T, std::int16_t> ||
    std::is_same_v<T, std::int32_t> ||
    std::is_same_v<T, std::int64_t> ||
    std::is_same_v<T, Base::Size> ||
    std::is_same_v<T, Base::Transform2D>;

template<class T>
inline T LerpAnimationValue(const T& from, const T& to, double progress) noexcept {
    if constexpr (std::is_same_v<T, double>) {
        return from + (to - from) * progress;
    } else if constexpr (std::is_same_v<T, Base::Color>) {
        const float t = static_cast<float>(progress);
        return {
            from.red + (to.red - from.red) * t,
            from.green + (to.green - from.green) * t,
            from.blue + (to.blue - from.blue) * t,
            from.alpha + (to.alpha - from.alpha) * t
        };
    } else if constexpr (std::is_same_v<T, Base::Point>) {
        return {
            from.x + (to.x - from.x) * progress,
            from.y + (to.y - from.y) * progress
        };
    } else if constexpr (std::is_same_v<T, Base::Rect>) {
        return {
            from.x + (to.x - from.x) * progress,
            from.y + (to.y - from.y) * progress,
            from.width + (to.width - from.width) * progress,
            from.height + (to.height - from.height) * progress
        };
    } else if constexpr (std::is_same_v<T, Base::Thickness>) {
        return {
            from.left + (to.left - from.left) * progress,
            from.top + (to.top - from.top) * progress,
            from.right + (to.right - from.right) * progress,
            from.bottom + (to.bottom - from.bottom) * progress
        };
    } else if constexpr (std::is_same_v<T, std::int16_t> || std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t>) {
        const double sampled = static_cast<double>(from) +
            (static_cast<double>(to) - static_cast<double>(from)) * progress;
        if (!std::isfinite(sampled)) return to;
        return static_cast<T>(std::llround(sampled));
    } else if constexpr (std::is_same_v<T, Base::Size>) {
        return {
            from.width + (to.width - from.width) * progress,
            from.height + (to.height - from.height) * progress
        };
    } else if constexpr (std::is_same_v<T, Base::Transform2D>) {
        Base::Transform2D sampled;
        sampled.m11 = from.m11 + (to.m11 - from.m11) * progress;
        sampled.m12 = from.m12 + (to.m12 - from.m12) * progress;
        sampled.m21 = from.m21 + (to.m21 - from.m21) * progress;
        sampled.m22 = from.m22 + (to.m22 - from.m22) * progress;
        sampled.dx = from.dx + (to.dx - from.dx) * progress;
        sampled.dy = from.dy + (to.dy - from.dy) * progress;
        return sampled;
    } else {
        return progress >= 1.0 ? to : from;
    }
}

template<class T> struct FromToName;

#define AERO_FROM_TO_NAME(Tag, CppType) \
    template<> struct FromToName<CppType> { \
        static constexpr Base::StringView BaseName() noexcept { \
            return #Tag "AnimationBase"; \
        } \
        static constexpr Base::StringView ConcreteName() noexcept { \
            return #Tag "Animation"; \
        } \
    };

AERO_FROM_TO_NAME(Double, double)
AERO_FROM_TO_NAME(Color, Base::Color)
AERO_FROM_TO_NAME(Point, Base::Point)
AERO_FROM_TO_NAME(Rect, Base::Rect)
AERO_FROM_TO_NAME(Thickness, Base::Thickness)
AERO_FROM_TO_NAME(Int16, std::int16_t)
AERO_FROM_TO_NAME(Int32, std::int32_t)
AERO_FROM_TO_NAME(Int64, std::int64_t)
AERO_FROM_TO_NAME(Size, Base::Size)
AERO_FROM_TO_NAME(Matrix, Base::Transform2D)
#undef AERO_FROM_TO_NAME

template<class TKeyFrame> struct KeyedAnimationName;

#define AERO_KEYED_ANIMATION_NAME(Frame, NameLiteral) \
    template<> struct KeyedAnimationName<Frame> { \
        static constexpr Base::StringView Name() noexcept { \
            return NameLiteral; \
        } \
    };

AERO_KEYED_ANIMATION_NAME(DoubleKeyFrame, "DoubleAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(PointKeyFrame, "PointAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(ColorKeyFrame, "ColorAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(ThicknessKeyFrame, "ThicknessAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(ObjectKeyFrame, "ObjectAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(BooleanKeyFrame, "BooleanAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(Int16KeyFrame, "Int16AnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(Int32KeyFrame, "Int32AnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(Int64KeyFrame, "Int64AnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(SizeKeyFrame, "SizeAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(MatrixKeyFrame, "MatrixAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(StringKeyFrame, "StringAnimationUsingKeyFrames")
#undef AERO_KEYED_ANIMATION_NAME

template<class T> inline bool AnimationValueAccepted(const T& value) noexcept {
    if constexpr (std::is_same_v<T, double>) {
        return std::isfinite(value);
    } else if constexpr (std::is_same_v<T, Base::Color>) {
        return Base::IsFiniteColor(value);
    } else if constexpr (std::is_same_v<T, Base::Point>) {
        return std::isfinite(value.x) && std::isfinite(value.y);
    } else if constexpr (std::is_same_v<T, Base::Rect>) {
        return Base::IsFiniteRect(value);
    } else if constexpr (std::is_same_v<T, Base::Thickness>) {
        return std::isfinite(value.left) && std::isfinite(value.top) &&
            std::isfinite(value.right) && std::isfinite(value.bottom);
    } else if constexpr (std::is_same_v<T, Base::Size>) {
        return std::isfinite(value.width) && std::isfinite(value.height);
    } else if constexpr (std::is_same_v<T, Base::Transform2D>) {
        return Base::IsFiniteTransform(value);
    } else {
        static_cast<void>(value);
        return true;
    }
}

template<class T>
inline Meta::PropertyValue EncodeAnimationValue(const T& value) noexcept {
    if constexpr (std::is_same_v<T, Meta::PropertyValue>) {
        return value;
    } else {
        using ResultType = decltype(Meta::ValueCodec<T>::Encode(value));
        if constexpr (std::is_same_v<ResultType, Meta::PropertyValue>) {
            return Meta::ValueCodec<T>::Encode(value);
        } else {
            auto encoded = Meta::ValueCodec<T>::Encode(value);
            if (encoded) {
                return *encoded;
            }
            return {};
        }
    }
}

template<class T>
inline T DecodeAnimationValue(const Meta::PropertyValue& propValue, const T& defaultValue = T{}) noexcept {
    if constexpr (std::is_same_v<T, Meta::PropertyValue>) {
        return propValue.IsUnset() ? defaultValue : propValue;
    } else {
        if (auto decoded = Meta::ValueCodec<T>::Decode(propValue)) {
            return *decoded;
        }
        return defaultValue;
    }
}

// Color, Point, Rect and Thickness historically stored the value without
// flipping the presence flag. ResolveFrom keeps that behavior.
template<class T> inline constexpr bool AnimationRecordsPresence =
    !std::is_same_v<T, Base::Color> && !std::is_same_v<T, Base::Point> &&
    !std::is_same_v<T, Base::Rect> && !std::is_same_v<T, Base::Thickness>;

template<class T> class AnimationBase : public AnimationTimeline {
public:
    using BaseType = AnimationTimeline;
    static constexpr Base::StringView StaticMetadataNamespace() noexcept { return Meta::AeroNamespaceUri(); }
    static constexpr Base::StringView StaticMetadataName() noexcept { return FromToName<T>::BaseName(); }
    static constexpr Meta::TypeId StaticTypeId() noexcept {
        return Meta::MakeTypeId(StaticMetadataNamespace(), StaticMetadataName());
    }

    T GetFrom() const noexcept { return from_; }
    T GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    T ResolveFrom(T defaultOriginValue) const noexcept { return hasFrom_ ? from_ : defaultOriginValue; }
    T ResolveTo(T defaultDestinationValue) const noexcept { return hasTo_ ? to_ : defaultDestinationValue; }
    void SetFrom(T value) noexcept {
        if (!WritePreamble() || !AnimationValueAccepted(value)) return;
        from_ = value;
        if constexpr (AnimationRecordsPresence<T>) hasFrom_ = true;
        WritePostscript();
    }
    void SetTo(T value) noexcept {
        if (!WritePreamble() || !AnimationValueAccepted(value)) return;
        to_ = value;
        if constexpr (AnimationRecordsPresence<T>) hasTo_ = true;
        WritePostscript();
    }
    virtual T GetCurrentValue(T defaultOriginValue, T defaultDestinationValue, double progress) const noexcept {
        const T from = ResolveFrom(defaultOriginValue);
        const T to = ResolveTo(defaultDestinationValue);
        return LerpAnimationValue<T>(from, to, progress);
    }

    Meta::PropertyValue GetCurrentPropertyValue(
        const Meta::PropertyValue& defaultOriginValue,
        const Meta::PropertyValue& defaultDestinationValue,
        double progress,
        AnimationTime sampleTime) const noexcept override {
        static_cast<void>(sampleTime);
        const T origin = DecodeAnimationValue<T>(defaultOriginValue);
        const T dest = DecodeAnimationValue<T>(defaultDestinationValue);
        const T sampled = GetCurrentValue(origin, dest, progress);
        if constexpr (std::is_same_v<T, double>) {
            if (!std::isfinite(sampled)) return {};
        }
        return EncodeAnimationValue<T>(sampled);
    }

protected:
    explicit AnimationBase(Meta::TypeId runtimeType) noexcept : AnimationTimeline(runtimeType) {}
    void CloneCore(const Freezable& source) override {
        AnimationTimeline::CloneCore(source);
        const auto& src = static_cast<const AnimationBase<T>&>(source);
        from_ = src.from_;
        to_ = src.to_;
        hasFrom_ = src.hasFrom_;
        hasTo_ = src.hasTo_;
    }

private:
    T from_{};
    T to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

template<class T, bool Enabled = std::is_same_v<T, double>> struct AnimationRatioStorage {};

template<class T> struct AnimationRatioStorage<T, true> {
    double accelerationRatio_ = 0.0;
    double decelerationRatio_ = 0.0;
};

template<class T> class Animation : public AnimationBase<T>,
      private AnimationRatioStorage<T> {
public:
    using BaseType = AnimationBase<T>;
    static constexpr Base::StringView StaticMetadataNamespace() noexcept { return Meta::AeroNamespaceUri(); }
    static constexpr Base::StringView StaticMetadataName() noexcept { return FromToName<T>::ConcreteName(); }
    static constexpr Meta::TypeId StaticTypeId() noexcept {
        return Meta::MakeTypeId(StaticMetadataNamespace(), StaticMetadataName());
    }

    Animation() noexcept : AnimationBase<T>(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept { return easing_; }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept {
        if (!this->WritePreamble() || easing_.Get() == value.Get()) return;
        easing_ = std::move(value);
        this->WritePostscript();
    }
    double GetAccelerationRatio() const noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return this->accelerationRatio_;
        } else { return 0.0; }
    }
    double GetDecelerationRatio() const noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return this->decelerationRatio_;
        } else { return 0.0; }
    }
    void SetAccelerationRatio(double value) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            if (!this->WritePreamble()) return;
            if (!std::isfinite(value) || value < 0.0 || value > 1.0 || value + this->decelerationRatio_ > 1.0) {
                return;
            }
            this->accelerationRatio_ = value;
            this->WritePostscript();
        } else { static_cast<void>(value); }
    }
    void SetDecelerationRatio(double value) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            if (!this->WritePreamble()) return;
            if (!std::isfinite(value) || value < 0.0 || value > 1.0 || this->accelerationRatio_ + value > 1.0) {
                return;
            }
            this->decelerationRatio_ = value;
            this->WritePostscript();
        } else { static_cast<void>(value); }
    }

    Meta::PropertyValue GetCurrentPropertyValue(
        const Meta::PropertyValue& defaultOriginValue,
        const Meta::PropertyValue& defaultDestinationValue,
        double progress,
        AnimationTime sampleTime) const noexcept override {
        double effectiveProgress = progress;
        if constexpr (std::is_same_v<T, double>) {
            if (this->accelerationRatio_ > 0.0 || this->decelerationRatio_ > 0.0) {
                effectiveProgress = AnimationMath::ApplyAccelerationDeceleration(
                    effectiveProgress, this->accelerationRatio_, this->decelerationRatio_);
            }
        }
        if (easing_) {
            effectiveProgress = easing_->Ease(effectiveProgress);
        }
        return AnimationBase<T>::GetCurrentPropertyValue(
            defaultOriginValue, defaultDestinationValue, effectiveProgress, sampleTime);
    }

protected:
    explicit Animation(Meta::TypeId runtimeType) noexcept : AnimationBase<T>(runtimeType) {}
    Base::Ref<Freezable> CreateInstanceCore() const override {
        return Base::MakeRef<Animation<T>>();
    }
    void CloneCore(const Freezable& source) override {
        AnimationBase<T>::CloneCore(source);
        const auto& src = static_cast<const Animation<T>&>(source);
        easing_ = src.easing_;
        if constexpr (std::is_same_v<T, double>) {
            this->accelerationRatio_ = src.accelerationRatio_;
            this->decelerationRatio_ = src.decelerationRatio_;
        }
    }

private:
    Ref<EasingFunctionBase> easing_;
};

template<class TKeyFrame> class AnimationUsingKeyFrames : public AnimationTimeline {
public:
    using BaseType = AnimationTimeline;
    static constexpr Base::StringView StaticMetadataNamespace() noexcept { return Meta::AeroNamespaceUri(); }
    static constexpr Base::StringView StaticMetadataName() noexcept { return KeyedAnimationName<TKeyFrame>::Name(); }
    static constexpr Meta::TypeId StaticTypeId() noexcept {
        return Meta::MakeTypeId(StaticMetadataNamespace(), StaticMetadataName());
    }

    AnimationUsingKeyFrames() noexcept : AnimationTimeline(StaticTypeId()) {}

    void AddKeyFrame(Ref<TKeyFrame> value) noexcept {
        if (!WritePreamble()) { AERO_ASSERT(false); return; }
        if (!value) { AERO_ASSERT(false); return; }
        keyFrames_.PushBack(std::move(value));
        WritePostscript();
    }
    void ClearKeyFrames() noexcept {
        if (!WritePreamble() || keyFrames_.Empty()) return;
        keyFrames_.Clear();
        WritePostscript();
    }
    Span<const Ref<TKeyFrame>> GetKeyFrames() const noexcept { return {keyFrames_.Data(), keyFrames_.Size()}; }

    bool IsKeyFrameAnimation() const noexcept override { return true; }

    AnimationTime GetEffectiveDurationMicroseconds() const noexcept override {
        AnimationTime duration = this->GetDuration().HasTimeSpan() ? this->GetDuration().GetTimeSpan().Microseconds() : 0U;
        if (duration == 0U) {
            for (const auto& kf : keyFrames_) {
                if (kf && kf->GetKeyTime().IsTimeSpan()) {
                    const auto us = kf->GetKeyTime().GetTimeSpan().Microseconds();
                    if (us > duration) duration = us;
                }
            }
        }
        return duration;
    }

    Meta::PropertyValue GetCurrentPropertyValue(
        const Meta::PropertyValue& defaultOriginValue,
        const Meta::PropertyValue& defaultDestinationValue,
        double progress,
        AnimationTime sampleTime) const noexcept override {
        static_cast<void>(defaultDestinationValue);
        using ValueType = typename TKeyFrame::ValueType;
        if (keyFrames_.Empty()) {
            return defaultOriginValue;
        }

        AnimationTime duration = GetEffectiveDurationMicroseconds();
        if (duration == 0U && progress > 0.0) {
            duration = static_cast<AnimationTime>(std::llround(static_cast<double>(sampleTime) / progress));
        }

        const std::uint32_t count = static_cast<std::uint32_t>(keyFrames_.Size());
        struct FrameEntry {
            AnimationTime keyTime;
            const TKeyFrame* frame;
        };
        FrameEntry stackBuf[16];
        FrameEntry* entries = stackBuf;
        Base::Vector<FrameEntry> heapBuf;
        if (count > 16) {
            heapBuf.Resize(count);
            entries = heapBuf.Data();
        }
        std::uint32_t validCount = 0;
        for (std::uint32_t index = 0U; index < count; ++index) {
            const auto& kfRef = keyFrames_[index];
            if (!kfRef) continue;
            entries[validCount++] = {
                kfRef->GetKeyTime().ResolveMicroseconds(duration, index, count),
                kfRef.Get()
            };
        }
        for (std::uint32_t i = 1; i < validCount; ++i) {
            FrameEntry cur = entries[i];
            std::uint32_t j = i;
            while (j > 0 && entries[j - 1].keyTime > cur.keyTime) {
                entries[j] = entries[j - 1];
                --j;
            }
            entries[j] = cur;
        }

        ValueType previous = DecodeAnimationValue<ValueType>(defaultOriginValue);
        AnimationTime previousTime = 0U;
        ValueType sampled = previous;
        bool found = false;

        for (std::uint32_t index = 0U; index < validCount; ++index) {
            const FrameEntry& entry = entries[index];
            const TKeyFrame& frame = *entry.frame;
            const AnimationTime frameKeyTime = entry.keyTime;

            if (sampleTime > frameKeyTime) {
                previous = frame.GetValue();
                previousTime = frameKeyTime;
                sampled = frame.GetValue();
                continue;
            }

            const AnimationTime segmentDuration = (frameKeyTime >= previousTime) ? (frameKeyTime - previousTime) : 0U;
            const double raw = (segmentDuration == 0U)
                ? 1.0
                : static_cast<double>(sampleTime - previousTime) / static_cast<double>(segmentDuration);

            double segmentProgress = 0.0;
            switch (frame.GetInterpolation()) {
            case KeyFrameBase::Interpolation::Discrete:
                segmentProgress = (sampleTime >= frameKeyTime) ? 1.0 : 0.0;
                break;
            case KeyFrameBase::Interpolation::Easing: {
                auto easing = frame.GetEasingFunction();
                segmentProgress = easing ? easing->Ease(raw) : AnimationMath::Clamp01(raw);
                break;
            }
            case KeyFrameBase::Interpolation::Spline:
                segmentProgress = AnimationMath::EvaluateSpline(
                    raw,
                    frame.GetSplineControlPoint1X(),
                    frame.GetSplineControlPoint1Y(),
                    frame.GetSplineControlPoint2X(),
                    frame.GetSplineControlPoint2Y());
                break;
            case KeyFrameBase::Interpolation::Linear:
            default:
                segmentProgress = AnimationMath::Clamp01(raw);
                break;
            }

            sampled = LerpAnimationValue<ValueType>(previous, frame.GetValue(), segmentProgress);
            found = true;
            break;
        }

        if (!found && validCount > 0) {
            sampled = entries[validCount - 1].frame->GetValue();
        }

        return EncodeAnimationValue<ValueType>(sampled);
    }

protected:
    explicit AnimationUsingKeyFrames(Meta::TypeId runtimeType) noexcept : AnimationTimeline(runtimeType) {}
    Base::Ref<Freezable> CreateInstanceCore() const override {
        return Base::MakeRef<AnimationUsingKeyFrames<TKeyFrame>>();
    }
    void CloneCore(const Freezable& source) override {
        AnimationTimeline::CloneCore(source);
        const auto& src = static_cast<const AnimationUsingKeyFrames<TKeyFrame>&>(source);
        keyFrames_.Clear();
        for (const auto& kf : src.keyFrames_) {
            if (kf) keyFrames_.PushBack(kf);
        }
    }

private:
    Base::Vector<Ref<TKeyFrame>> keyFrames_;
};

using DoubleAnimationBase = AnimationBase<double>;
using DoubleAnimation = Animation<double>;
using ColorAnimationBase = AnimationBase<Base::Color>;
using ColorAnimation = Animation<Base::Color>;
using PointAnimationBase = AnimationBase<Base::Point>;
using PointAnimation = Animation<Base::Point>;
using RectAnimationBase = AnimationBase<Base::Rect>;
using RectAnimation = Animation<Base::Rect>;
using ThicknessAnimationBase = AnimationBase<Base::Thickness>;
using ThicknessAnimation = Animation<Base::Thickness>;
using Int16AnimationBase = AnimationBase<std::int16_t>;
using Int16Animation = Animation<std::int16_t>;
using Int32AnimationBase = AnimationBase<std::int32_t>;
using Int32Animation = Animation<std::int32_t>;
using Int64AnimationBase = AnimationBase<std::int64_t>;
using Int64Animation = Animation<std::int64_t>;
using SizeAnimationBase = AnimationBase<Base::Size>;
using SizeAnimation = Animation<Base::Size>;
using MatrixAnimationBase = AnimationBase<Base::Transform2D>;
using MatrixAnimation = Animation<Base::Transform2D>;

using DoubleAnimationUsingKeyFrames = AnimationUsingKeyFrames<DoubleKeyFrame>;
using PointAnimationUsingKeyFrames = AnimationUsingKeyFrames<PointKeyFrame>;
using ColorAnimationUsingKeyFrames = AnimationUsingKeyFrames<ColorKeyFrame>;
using ThicknessAnimationUsingKeyFrames = AnimationUsingKeyFrames<ThicknessKeyFrame>;
using ObjectAnimationUsingKeyFrames = AnimationUsingKeyFrames<ObjectKeyFrame>;
using BooleanAnimationUsingKeyFrames = AnimationUsingKeyFrames<BooleanKeyFrame>;
using Int16AnimationUsingKeyFrames = AnimationUsingKeyFrames<Int16KeyFrame>;
using Int32AnimationUsingKeyFrames = AnimationUsingKeyFrames<Int32KeyFrame>;
using Int64AnimationUsingKeyFrames = AnimationUsingKeyFrames<Int64KeyFrame>;
using SizeAnimationUsingKeyFrames = AnimationUsingKeyFrames<SizeKeyFrame>;
using MatrixAnimationUsingKeyFrames = AnimationUsingKeyFrames<MatrixKeyFrame>;
using StringAnimationUsingKeyFrames = AnimationUsingKeyFrames<StringKeyFrame>;

} // namespace Aero::Media::Animation
