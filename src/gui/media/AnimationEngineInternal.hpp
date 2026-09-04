#pragma once

// Shared internals for AnimationEngine translation units (Track + helpers).

#include "gui/media/AnimationEngine.hpp"
#include "gui/media/AnimationModel.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Value.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <cstdint>
#include <utility>

namespace Aero::Media::Animation::EngineDetail {

inline constexpr double Pi = 3.1415926535897932384626433832795;

inline Base::Status InvalidAnimation(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

inline double Clamp01(double value) noexcept {
    return std::max(0.0, std::min(1.0, value));
}

inline double ApplyAccelerationDeceleration(
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
    const double beforeDeceleration =
        1.0 - deceleration;
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

inline double EaseOutBounce(double value) noexcept {
    constexpr double n1 = 7.5625;
    constexpr double d1 = 2.75;
    if (value < 1.0 / d1) {
        return n1 * value * value;
    }
    if (value < 2.0 / d1) {
        value -= 1.5 / d1;
        return n1 * value * value + 0.75;
    }
    if (value < 2.5 / d1) {
        value -= 2.25 / d1;
        return n1 * value * value + 0.9375;
    }
    value -= 2.625 / d1;
    return n1 * value * value + 0.984375;
}

inline double CubicBezierCoordinate(
    double t, double first, double second) noexcept {
    const double inverse = 1.0 - t;
    return 3.0 * inverse * inverse * t * first +
        3.0 * inverse * t * t * second + t * t * t;
}

inline double CubicBezierDerivative(
    double t, double first, double second) noexcept {
    const double inverse = 1.0 - t;
    return 3.0 * inverse * inverse * first +
        6.0 * inverse * t * (second - first) +
        3.0 * t * t * (1.0 - second);
}

template<class TKeyFrame>
double EvaluateSpline(
    double progress,
    const TKeyFrame& frame) noexcept {
    const double target = Clamp01(progress);
    double parameter = target;
    for (std::uint32_t iteration = 0U; iteration < 8U; ++iteration) {
        const double x = CubicBezierCoordinate(
            parameter, frame.controlPoint1X, frame.controlPoint2X);
        const double derivative = CubicBezierDerivative(
            parameter, frame.controlPoint1X, frame.controlPoint2X);
        if (std::abs(derivative) < 1.0e-7) break;
        parameter = Clamp01(parameter - (x - target) / derivative);
    }
    double low = 0.0;
    double high = 1.0;
    for (std::uint32_t iteration = 0U; iteration < 12U; ++iteration) {
        const double x = CubicBezierCoordinate(
            parameter, frame.controlPoint1X, frame.controlPoint2X);
        if (x < target) low = parameter;
        else high = parameter;
        parameter = (low + high) * 0.5;
    }
    return Clamp01(CubicBezierCoordinate(
        parameter, frame.controlPoint1Y, frame.controlPoint2Y));
}

inline bool IsTimingValid(
    const ::Aero::Media::Animation::Model::TimelineTiming& timing) noexcept {
    return std::isfinite(timing.speedRatio) &&
        timing.speedRatio > 0.0 &&
        (timing.repeat.forever ||
            (std::isfinite(timing.repeat.count) &&
             timing.repeat.count > 0.0));
}

inline std::int64_t LerpInteger(
    std::int64_t from,
    std::int64_t to,
    double progress) noexcept {
    const double sampled = static_cast<double>(from) +
        (static_cast<double>(to) - static_cast<double>(from)) * progress;
    if (!std::isfinite(sampled)) {
        return to;
    }
    return static_cast<std::int64_t>(std::llround(sampled));
}

inline Base::Transform2D LerpMatrix(
    Base::Transform2D from,
    Base::Transform2D to,
    double progress) noexcept {
    Base::Transform2D sampled;
    sampled.m11 = from.m11 + (to.m11 - from.m11) * progress;
    sampled.m12 = from.m12 + (to.m12 - from.m12) * progress;
    sampled.m21 = from.m21 + (to.m21 - from.m21) * progress;
    sampled.m22 = from.m22 + (to.m22 - from.m22) * progress;
    sampled.dx = from.dx + (to.dx - from.dx) * progress;
    sampled.dy = from.dy + (to.dy - from.dy) * progress;
    return sampled;
}

template<class T>
Base::Result<Meta::PropertyValue> EncodeSignedInteger(
    std::int64_t value) noexcept {
    const std::int64_t minimum =
        static_cast<std::int64_t>(std::numeric_limits<T>::min());
    const std::int64_t maximum =
        static_cast<std::int64_t>(std::numeric_limits<T>::max());
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    return Meta::ValueCodec<T>::Encode(static_cast<T>(value));
}

inline Base::Result<Meta::PropertyValue> EncodeIntegerWidth(
    std::int64_t value,
    ::Aero::Media::Animation::Model::IntegerAnimationWidth width) noexcept {
    switch (width) {
    case ::Aero::Media::Animation::Model::IntegerAnimationWidth::Int16:
        return EncodeSignedInteger<std::int16_t>(value);
    case ::Aero::Media::Animation::Model::IntegerAnimationWidth::Int32:
        return EncodeSignedInteger<std::int32_t>(value);
    case ::Aero::Media::Animation::Model::IntegerAnimationWidth::Int64:
        return EncodeSignedInteger<std::int64_t>(value);
    }
    return EncodeSignedInteger<std::int32_t>(value);
}

} // namespace Aero::Media::Animation::EngineDetail

namespace Aero {

struct AnimationEngine::Track {
    enum class Kind : std::uint8_t {
        Double,
        CustomDouble,
        Color,
        Point,
        Rect,
        Thickness,
        DoubleKeyFrames,
        ColorKeyFrames,
        PointKeyFrames,
        ThicknessKeyFrames,
        Integer,
        IntegerKeyFrames,
        Size,
        SizeKeyFrames,
        Matrix,
        MatrixKeyFrames,
        Discrete
    };

    explicit Track(Base::IAllocator* allocator) noexcept
        : doubleFrames(allocator),
          colorFrames(allocator),
          pointFrames(allocator),
          thicknessFrames(allocator),
          integerFrames(allocator),
          sizeFrames(allocator),
          matrixFrames(allocator),
          discreteFrames(allocator) {}

    Track(Track&&) noexcept = default;
    Track& operator=(Track&&) noexcept = default;
    Track(const Track&) = delete;
    Track& operator=(const Track&) = delete;

    AnimationHandle handle;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    TimelineTiming timing;
    EasingFunction easing;
    double accelerationRatio = 0.0;
    double decelerationRatio = 0.0;
    Kind kind = Kind::Double;
    AnimationState state = AnimationState::Active;
    AnimationTime startTimeMicroseconds = 0U;
    AnimationTime pauseTimeMicroseconds = 0U;
    AnimationTime seekOffsetMicroseconds = 0U;
    AnimationTime accumulatedPauseMicroseconds = 0U;
    double from = 0.0;
    double to = 0.0;
    double baseValue = 0.0;
    double defaultDestinationValue = 0.0;
    Base::Ref<
        ::Aero::Media::Animation::DoubleAnimationBase>
        customDouble;
    Base::Color fromColor;
    Base::Color toColor;
    Base::Point fromPoint;
    Base::Point toPoint;
    Base::Rect fromRect;
    Base::Rect toRect;
    Base::Thickness fromThickness;
    Base::Thickness toThickness;
    std::int64_t fromInteger = 0;
    std::int64_t toInteger = 0;
    IntegerAnimationWidth integerWidth = IntegerAnimationWidth::Int32;
    Base::Size fromSize{};
    Base::Size toSize{};
    Base::Transform2D fromMatrix{};
    Base::Transform2D toMatrix{};
    Base::Vector<DoubleKeyFrame> doubleFrames;
    Base::Vector<ColorKeyFrame> colorFrames;
    Base::Vector<PointKeyFrame> pointFrames;
    Base::Vector<ThicknessKeyFrame> thicknessFrames;
    Base::Vector<IntegerKeyFrame> integerFrames;
    Base::Vector<SizeKeyFrame> sizeFrames;
    Base::Vector<MatrixKeyFrame> matrixFrames;
    Base::Vector<DiscreteAnimationKeyFrame> discreteFrames;
    Meta::PropertyValue discreteBaseValue;
    bool valueApplied = false;
    bool completedCounted = false;
    bool pendingInitialSample = true;
};


} // namespace Aero
