#include <Aero/Animation.hpp>

#include <Aero/Value.hpp>

#include <cmath>
#include <cstdlib>

namespace Aero::Media::Animation {
namespace {

Base::Result<AnimationTime> ParseClockTime(
    Base::StringView input) noexcept {
    const Base::StringView text =
        ::Aero::Base::Detail::ValueConversion::Trim(input);
    if (text.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Animation clock time is empty");
    }
    Base::String owned;
    Base::Result<void> assigned = owned.TryAssign(text);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = owned.CStr();
    char* end = nullptr;
    double seconds = 0.0;
    const char* firstColon = nullptr;
    const char* secondColon = nullptr;
    for (const char* scan = cursor; *scan != '\0'; ++scan) {
        if (*scan == ':') {
            if (firstColon == nullptr) firstColon = scan;
            else if (secondColon == nullptr) secondColon = scan;
            else {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Animation clock time has too many fields");
            }
        }
    }
    if (firstColon != nullptr) {
        const double first = std::strtod(cursor, &end);
        if (end != firstColon) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Animation clock time contains invalid hours");
        }
        const char* middle = firstColon + 1;
        const double second = std::strtod(middle, &end);
        if ((secondColon == nullptr && *end != '\0') ||
            (secondColon != nullptr && end != secondColon)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Animation clock time contains invalid minutes");
        }
        if (secondColon != nullptr) {
            const double third = std::strtod(secondColon + 1, &end);
            if (*end != '\0') {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Animation clock time contains invalid seconds");
            }
            seconds = first * 3600.0 + second * 60.0 + third;
        } else {
            seconds = first * 60.0 + second;
        }
    } else {
        seconds = std::strtod(cursor, &end);
        double multiplier = 1.0;
        if (*end != '\0') {
            const Base::StringView suffix(
                end,
                static_cast<std::uint32_t>(
                    owned.CStr() + owned.SizeBytes() - end));
            if (::Aero::Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                    suffix, "ms")) {
                multiplier = 0.001;
            } else if (!::Aero::Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                           suffix, "s")) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Animation clock time suffix is invalid");
            }
        }
        seconds *= multiplier;
    }
    if (!std::isfinite(seconds) || seconds < 0.0 ||
        seconds > static_cast<double>(UINT64_MAX) / 1000000.0) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Animation clock time is outside the supported range");
    }
    return static_cast<AnimationTime>(
        std::llround(seconds * 1000000.0));
}

Base::Result<void> ValidateNonNegative(
    double value,
    const char* message) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed, message);
    }
    return {};
}

} // namespace

void Timeline::SetBeginTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned = beginTimeText_.TryAssign(value);
    if (!assigned) return;
    beginTimeMicroseconds_ = parsed.Value();
    return;
}

void Timeline::SetDuration(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned = durationText_.TryAssign(value);
    if (!assigned) return;
    durationMicroseconds_ = parsed.Value();
    return;
}

void Timeline::SetRepeatBehavior(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::Detail::ValueConversion::Trim(value);
    double repeatCount = 1.0;
    bool repeatForever = false;
    if (::Aero::Base::Detail::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Forever")) {
        repeatForever = true;
    } else {
        Base::Result<double> count =
            ::Aero::Base::Detail::ValueConversion::ParseDouble(trimmed);
        if (!count || count.Value() <= 0.0) {
            return;
        }
        repeatCount = count.Value();
    }
    Base::Result<void> assigned =
        repeatBehaviorText_.TryAssign(value);
    if (!assigned) return;
    repeatCount_ = repeatCount;
    repeatForever_ = repeatForever;
    return;
}

void Timeline::SetSpeedRatio(double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    speedRatio_ = value;
    return;
}

void Timeline::SetAutoReverse(bool value) noexcept {
    autoReverse_ = value;
    return;
}

void Timeline::SetFillBehavior(
    FillBehavior value) noexcept {
    fillBehavior_ = value;
    return;
}

void PowerEase::SetPower(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "PowerEase Power must be nonnegative");
    if (!valid) return;
    SetPowerValue(value);
    return;
}

void ExponentialEase::SetExponent(
    double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ExponentialEase Exponent must be nonnegative");
    if (!valid) return;
    SetPowerValue(value);
    return;
}

void BackEase::SetAmplitude(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BackEase Amplitude must be nonnegative");
    if (!valid) return;
    SetAmplitudeValue(value);
    return;
}

void BounceEase::SetBounces(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounces must be nonnegative");
    if (!valid) return;
    SetOscillationsValue(value);
    return;
}

void BounceEase::SetBounciness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounciness must be nonnegative");
    if (!valid) return;
    SetSpringinessValue(value);
    return;
}

void ElasticEase::SetOscillations(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Oscillations must be nonnegative");
    if (!valid) return;
    SetOscillationsValue(value);
    return;
}

void ElasticEase::SetSpringiness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Springiness must be nonnegative");
    if (!valid) return;
    SetSpringinessValue(value);
    return;
}

void DoubleAnimation::SetFrom(double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    from_ = value;
    return;
}

void DoubleAnimation::SetTo(double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    to_ = value;
    return;
}

void DoubleAnimation::SetAccelerationRatio(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
        value + decelerationRatio_ > 1.0) {
        return;
    }
    accelerationRatio_ = value;
    return;
}

void DoubleAnimation::SetDecelerationRatio(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
        accelerationRatio_ + value > 1.0) {
        return;
    }
    decelerationRatio_ = value;
    return;
}

void DoubleAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return;
}


void ColorAnimation::SetFrom(
    Base::Color value) noexcept {
    if (!Base::IsFiniteColor(value)) {
        return;
    }
    from_ = value;
    return;
}

void ColorAnimation::SetTo(
    Base::Color value) noexcept {
    if (!Base::IsFiniteColor(value)) {
        return;
    }
    to_ = value;
    return;
}

void ColorAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return;
}


void PointAnimation::SetFrom(
    Base::Point value) noexcept {
    if (!std::isfinite(value.x) ||
        !std::isfinite(value.y)) {
        return;
    }
    from_ = value;
    return;
}

void PointAnimation::SetTo(
    Base::Point value) noexcept {
    if (!std::isfinite(value.x) ||
        !std::isfinite(value.y)) {
        return;
    }
    to_ = value;
    return;
}

void
PointAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase>
        value) noexcept {
    easing_ = std::move(value);
    return;
}


void RectAnimation::SetFrom(
    Base::Rect value) noexcept {
    if (!Base::IsFiniteRect(value)) {
        return;
    }
    from_ = value;
    return;
}

void RectAnimation::SetTo(
    Base::Rect value) noexcept {
    if (!Base::IsFiniteRect(value)) {
        return;
    }
    to_ = value;
    return;
}

void RectAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return;
}


namespace {

bool IsFiniteThickness(
    Base::Thickness value) noexcept {
    return std::isfinite(value.left) &&
        std::isfinite(value.top) &&
        std::isfinite(value.right) &&
        std::isfinite(value.bottom);
}

} // namespace

void ThicknessAnimation::SetFrom(
    Base::Thickness value) noexcept {
    if (!IsFiniteThickness(value)) {
        return;
    }
    from_ = value;
    return;
}

void ThicknessAnimation::SetTo(
    Base::Thickness value) noexcept {
    if (!IsFiniteThickness(value)) {
        return;
    }
    to_ = value;
    return;
}

void
ThicknessAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return;
}


void DoubleKeyFrame::SetValue(double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    value_ = value;
    return;
}

void DoubleKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned = keyTimeText_.TryAssign(value);
    if (!assigned) return;
    keyTimeMicroseconds_ = parsed.Value();
    return;
}

void EasingDoubleKeyFrame::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return;
}

void SplineDoubleKeyFrame::SetKeySpline(
    Base::StringView value) noexcept {
    Base::String owned;
    Base::Result<void> assigned = owned.TryAssign(value);
    if (!assigned) return;
    const char* cursor = owned.CStr();
    double values[4]{};
    for (std::uint32_t index = 0U; index < 4U; ++index) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        char* end = nullptr;
        values[index] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[index])) {
            return;
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return;
    }
    assigned = keySpline_.TryAssign(value);
    if (!assigned) return;
    SetSplineControlPoints(
        values[0], values[1], values[2], values[3]);
    return;
}

Base::Result<void>
DoubleAnimationUsingKeyFrames::TryAddKeyFrame(
    Base::Ref<DoubleKeyFrame> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Double key frame cannot be null");
    }
    return keyFrames_.TryPushBack(std::move(value));
}

void
DoubleAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return;
}

void ThicknessKeyFrame::SetValue(
    Base::Thickness value) noexcept {
    if (!IsFiniteThickness(value)) {
        return;
    }
    value_ = value;
    return;
}

void ThicknessKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned =
        keyTimeText_.TryAssign(value);
    if (!assigned) return;
    keyTimeMicroseconds_ = parsed.Value();
    return;
}

void
EasingThicknessKeyFrame::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return;
}

Base::Result<void>
ThicknessAnimationUsingKeyFrames::TryAddKeyFrame(
    Base::Ref<ThicknessKeyFrame> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Thickness key frame cannot be null");
    }
    return keyFrames_.TryPushBack(
        std::move(value));
}

void
ThicknessAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return;
}

void ColorKeyFrame::SetValue(
    Base::Color value) noexcept {
    if (!Base::IsFiniteColor(value)) {
        return;
    }
    value_ = value;
    return;
}

void ColorKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned =
        keyTimeText_.TryAssign(value);
    if (!assigned) return;
    keyTimeMicroseconds_ = parsed.Value();
    return;
}

void
EasingColorKeyFrame::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return;
}

void
SplineColorKeyFrame::SetKeySpline(
    Base::StringView value) noexcept {
    Base::String owned;
    Base::Result<void> assigned =
        owned.TryAssign(value);
    if (!assigned) return;
    const char* cursor = owned.CStr();
    double values[4]{};
    for (std::uint32_t index = 0U;
         index < 4U;
         ++index) {
        while (*cursor == ' ' || *cursor == ',') {
            ++cursor;
        }
        char* end = nullptr;
        values[index] = std::strtod(cursor, &end);
        if (end == cursor ||
            !std::isfinite(values[index])) {
            return;
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') {
        ++cursor;
    }
    if (*cursor != '\0') {
        return;
    }
    assigned = keySpline_.TryAssign(value);
    if (!assigned) return;
    SetSplineControlPoints(
        values[0], values[1], values[2], values[3]);
    return;
}

Base::Result<void>
ColorAnimationUsingKeyFrames::TryAddKeyFrame(
    Base::Ref<ColorKeyFrame> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Color key frame cannot be null");
    }
    return keyFrames_.TryPushBack(
        std::move(value));
}

void
ColorAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return;
}

void DiscreteObjectKeyFrame::SetValue(
    const Meta::PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return;
    }
    value_ = value;
    return;
}

void DiscreteObjectKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned = keyTimeText_.TryAssign(value);
    if (!assigned) return;
    keyTimeMicroseconds_ = parsed.Value();
    return;
}

Base::Result<void>
ObjectAnimationUsingKeyFrames::TryAddKeyFrame(
    Base::Ref<DiscreteObjectKeyFrame> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Object key frame cannot be null");
    }
    return keyFrames_.TryPushBack(std::move(value));
}

void
ObjectAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return;
}

void DiscreteBooleanKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned = keyTimeText_.TryAssign(value);
    if (!assigned) return;
    keyTimeMicroseconds_ = parsed.Value();
    return;
}

Base::Result<void>
BooleanAnimationUsingKeyFrames::TryAddKeyFrame(
    Base::Ref<DiscreteBooleanKeyFrame> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Boolean key frame cannot be null");
    }
    return keyFrames_.TryPushBack(std::move(value));
}

void
BooleanAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return;
}

Base::Result<void> Storyboard::TryAddTimeline(
    Base::Ref<Timeline> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Storyboard timeline cannot be null");
    }
    return timelines_.TryPushBack(std::move(value));
}

void Storyboard::ClearTimelines() noexcept {
    timelines_.Clear();
    return;
}

void BeginStoryboard::SetStoryboard(
    Base::Ref<Storyboard> value) noexcept {
    storyboard_ = std::move(value);
    return;
}

void BeginStoryboard::SetName(
    Base::StringView value) noexcept {
    return;
}

void ChangePropertyAction::SetTargetName(
    Base::StringView value) noexcept {
    return;
}

void ChangePropertyAction::SetPropertyName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::Detail::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return;
    }
    return;
}

void ChangePropertyAction::SetValue(
    const Meta::PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return;
    }
    value_ = value;
    return;
}

void LaunchUriOrFileAction::SetPath(
    Base::StringView value) noexcept {
    return;
}

void
ControllableStoryboardAction::SetBeginStoryboardName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::Detail::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return;
    }
    return;
}

void SeekStoryboard::SetOffset(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::Detail::ValueConversion::Trim(value);
    Base::Result<AnimationTime> parsed =
        ParseClockTime(trimmed);
    if (!parsed) return;
    Base::Result<void> assigned =
        offsetText_.TryAssign(trimmed);
    if (!assigned) return;
    offsetMicroseconds_ = parsed.Value();
    return;
}

void EventTrigger::SetRoutedEvent(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::Detail::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return;
    }
    return;
}

void EventTrigger::SetSourceName(
    Base::StringView value) noexcept {
    return;
}

Base::Result<void> EventTrigger::TryAddAction(
    Base::Ref<TriggerAction> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "EventTrigger action cannot be null");
    }
    return actions_.TryPushBack(std::move(value));
}

void EventTrigger::ClearActions() noexcept {
    actions_.Clear();
    return;
}

void StoryboardCompletedTrigger::SetStoryboard(
    Base::Ref<Storyboard> value) noexcept {
    storyboard_ = std::move(value);
    return;
}

Base::Result<void> StoryboardCompletedTrigger::TryAddAction(
    Base::Ref<TriggerAction> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StoryboardCompletedTrigger action cannot be null");
    }
    return actions_.TryPushBack(std::move(value));
}

void
StoryboardCompletedTrigger::ClearActions() noexcept {
    actions_.Clear();
    return;
}

} // namespace Aero::Media::Animation
