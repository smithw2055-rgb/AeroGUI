#include <Aero/Animation.hpp>

#include <Aero/Meta/ValueConversion.hpp>

#include <cmath>
#include <cstdlib>

namespace Aero::Media::Animation {
namespace {

Base::Result<AnimationTime> ParseClockTime(
    Base::StringView input) noexcept {
    const Base::StringView text =
        Core::ValueConversion::Trim(input);
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
            if (Core::ValueConversion::EqualsAsciiInsensitive(
                    suffix, "ms")) {
                multiplier = 0.001;
            } else if (!Core::ValueConversion::EqualsAsciiInsensitive(
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

Base::Result<void> Timeline::SetBeginTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned = beginTimeText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    beginTimeMicroseconds_ = parsed.Value();
    return {};
}

Base::Result<void> Timeline::SetDuration(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned = durationText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    durationMicroseconds_ = parsed.Value();
    return {};
}

Base::Result<void> Timeline::SetRepeatBehavior(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        Core::ValueConversion::Trim(value);
    double repeatCount = 1.0;
    bool repeatForever = false;
    if (Core::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Forever")) {
        repeatForever = true;
    } else {
        Base::Result<double> count =
            Core::ValueConversion::ParseDouble(trimmed);
        if (!count || count.Value() <= 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "RepeatBehavior must be Forever or a positive count");
        }
        repeatCount = count.Value();
    }
    Base::Result<void> assigned =
        repeatBehaviorText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    repeatCount_ = repeatCount;
    repeatForever_ = repeatForever;
    return {};
}

Base::Result<void> Timeline::SetSpeedRatio(double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Timeline SpeedRatio must be positive");
    }
    speedRatio_ = value;
    return {};
}

Base::Result<void> Timeline::SetAutoReverse(bool value) noexcept {
    autoReverse_ = value;
    return {};
}

Base::Result<void> Timeline::SetFillBehavior(
    FillBehavior value) noexcept {
    fillBehavior_ = value;
    return {};
}

Base::Result<void> PowerEase::SetPower(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "PowerEase Power must be nonnegative");
    if (!valid) return valid.GetStatus();
    SetPowerValue(value);
    return {};
}

Base::Result<void> ExponentialEase::SetExponent(
    double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ExponentialEase Exponent must be nonnegative");
    if (!valid) return valid.GetStatus();
    SetPowerValue(value);
    return {};
}

Base::Result<void> BackEase::SetAmplitude(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BackEase Amplitude must be nonnegative");
    if (!valid) return valid.GetStatus();
    SetAmplitudeValue(value);
    return {};
}

Base::Result<void> BounceEase::SetBounces(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounces must be nonnegative");
    if (!valid) return valid.GetStatus();
    SetOscillationsValue(value);
    return {};
}

Base::Result<void> BounceEase::SetBounciness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounciness must be nonnegative");
    if (!valid) return valid.GetStatus();
    SetSpringinessValue(value);
    return {};
}

Base::Result<void> ElasticEase::SetOscillations(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Oscillations must be nonnegative");
    if (!valid) return valid.GetStatus();
    SetOscillationsValue(value);
    return {};
}

Base::Result<void> ElasticEase::SetSpringiness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Springiness must be nonnegative");
    if (!valid) return valid.GetStatus();
    SetSpringinessValue(value);
    return {};
}

Base::Result<void> DoubleAnimation::SetFrom(double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "DoubleAnimation From must be finite");
    }
    from_ = value;
    return {};
}

Base::Result<void> DoubleAnimation::SetTo(double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "DoubleAnimation To must be finite");
    }
    to_ = value;
    return {};
}

Base::Result<void> DoubleAnimation::SetAccelerationRatio(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
        value + decelerationRatio_ > 1.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "DoubleAnimation AccelerationRatio must be in [0, 1] and the ratios must not exceed 1");
    }
    accelerationRatio_ = value;
    return {};
}

Base::Result<void> DoubleAnimation::SetDecelerationRatio(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
        accelerationRatio_ + value > 1.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "DoubleAnimation DecelerationRatio must be in [0, 1] and the ratios must not exceed 1");
    }
    decelerationRatio_ = value;
    return {};
}

Base::Result<void> DoubleAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return {};
}


Base::Result<void> ColorAnimation::SetFrom(
    Base::Color value) noexcept {
    if (!Base::IsFiniteColor(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "ColorAnimation From must be finite");
    }
    from_ = value;
    return {};
}

Base::Result<void> ColorAnimation::SetTo(
    Base::Color value) noexcept {
    if (!Base::IsFiniteColor(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "ColorAnimation To must be finite");
    }
    to_ = value;
    return {};
}

Base::Result<void> ColorAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return {};
}


Base::Result<void> PointAnimation::SetFrom(
    Base::Point value) noexcept {
    if (!std::isfinite(value.x) ||
        !std::isfinite(value.y)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "PointAnimation From must be finite");
    }
    from_ = value;
    return {};
}

Base::Result<void> PointAnimation::SetTo(
    Base::Point value) noexcept {
    if (!std::isfinite(value.x) ||
        !std::isfinite(value.y)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "PointAnimation To must be finite");
    }
    to_ = value;
    return {};
}

Base::Result<void>
PointAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase>
        value) noexcept {
    easing_ = std::move(value);
    return {};
}


Base::Result<void> RectAnimation::SetFrom(
    Base::Rect value) noexcept {
    if (!Base::IsFiniteRect(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "RectAnimation From must be finite");
    }
    from_ = value;
    return {};
}

Base::Result<void> RectAnimation::SetTo(
    Base::Rect value) noexcept {
    if (!Base::IsFiniteRect(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "RectAnimation To must be finite");
    }
    to_ = value;
    return {};
}

Base::Result<void> RectAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return {};
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

Base::Result<void> ThicknessAnimation::SetFrom(
    Base::Thickness value) noexcept {
    if (!IsFiniteThickness(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "ThicknessAnimation From must be finite");
    }
    from_ = value;
    return {};
}

Base::Result<void> ThicknessAnimation::SetTo(
    Base::Thickness value) noexcept {
    if (!IsFiniteThickness(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "ThicknessAnimation To must be finite");
    }
    to_ = value;
    return {};
}

Base::Result<void>
ThicknessAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return {};
}


Base::Result<void> DoubleKeyFrame::SetValue(double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Double key-frame value must be finite");
    }
    value_ = value;
    return {};
}

Base::Result<void> DoubleKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned = keyTimeText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    keyTimeMicroseconds_ = parsed.Value();
    return {};
}

Base::Result<void> EasingDoubleKeyFrame::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return {};
}

Base::Result<void> SplineDoubleKeyFrame::SetKeySpline(
    Base::StringView value) noexcept {
    Base::String owned;
    Base::Result<void> assigned = owned.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = owned.CStr();
    double values[4]{};
    for (std::uint32_t index = 0U; index < 4U; ++index) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        char* end = nullptr;
        values[index] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "KeySpline requires four finite coordinates");
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "KeySpline contains trailing text");
    }
    assigned = keySpline_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    SetSplineControlPoints(
        values[0], values[1], values[2], values[3]);
    return {};
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

Base::Result<void>
DoubleAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return {};
}

Base::Result<void> ThicknessKeyFrame::SetValue(
    Base::Thickness value) noexcept {
    if (!IsFiniteThickness(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Thickness key-frame value must be finite");
    }
    value_ = value;
    return {};
}

Base::Result<void> ThicknessKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned =
        keyTimeText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    keyTimeMicroseconds_ = parsed.Value();
    return {};
}

Base::Result<void>
EasingThicknessKeyFrame::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return {};
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

Base::Result<void>
ThicknessAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return {};
}

Base::Result<void> ColorKeyFrame::SetValue(
    Base::Color value) noexcept {
    if (!Base::IsFiniteColor(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Color key-frame value must be finite");
    }
    value_ = value;
    return {};
}

Base::Result<void> ColorKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned =
        keyTimeText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    keyTimeMicroseconds_ = parsed.Value();
    return {};
}

Base::Result<void>
EasingColorKeyFrame::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    easing_ = std::move(value);
    return {};
}

Base::Result<void>
SplineColorKeyFrame::SetKeySpline(
    Base::StringView value) noexcept {
    Base::String owned;
    Base::Result<void> assigned =
        owned.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
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
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "KeySpline requires four finite coordinates");
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') {
        ++cursor;
    }
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "KeySpline contains trailing text");
    }
    assigned = keySpline_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    SetSplineControlPoints(
        values[0], values[1], values[2], values[3]);
    return {};
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

Base::Result<void>
ColorAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return {};
}

Base::Result<void> DiscreteObjectKeyFrame::SetValue(
    const Core::PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Object key-frame value cannot be unset");
    }
    value_ = value;
    return {};
}

Base::Result<void> DiscreteObjectKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned = keyTimeText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    keyTimeMicroseconds_ = parsed.Value();
    return {};
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

Base::Result<void>
ObjectAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return {};
}

Base::Result<void> DiscreteBooleanKeyFrame::SetKeyTime(
    Base::StringView value) noexcept {
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned = keyTimeText_.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    keyTimeMicroseconds_ = parsed.Value();
    return {};
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

Base::Result<void>
BooleanAnimationUsingKeyFrames::ClearKeyFrames() noexcept {
    keyFrames_.Clear();
    return {};
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

Base::Result<void> Storyboard::ClearTimelines() noexcept {
    timelines_.Clear();
    return {};
}

Base::Result<void> BeginStoryboard::SetStoryboard(
    Base::Ref<Storyboard> value) noexcept {
    storyboard_ = std::move(value);
    return {};
}

Base::Result<void> BeginStoryboard::SetName(
    Base::StringView value) noexcept {
    return name_.TryAssign(
        Core::ValueConversion::Trim(value));
}

Base::Result<void> ChangePropertyAction::SetTargetName(
    Base::StringView value) noexcept {
    return targetName_.TryAssign(
        Core::ValueConversion::Trim(value));
}

Base::Result<void> ChangePropertyAction::SetPropertyName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        Core::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "ChangePropertyAction PropertyName cannot be empty");
    }
    return propertyName_.TryAssign(trimmed);
}

Base::Result<void> ChangePropertyAction::SetValue(
    const Core::PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ChangePropertyAction Value cannot be unset");
    }
    value_ = value;
    return {};
}

Base::Result<void> LaunchUriOrFileAction::SetPath(
    Base::StringView value) noexcept {
    return path_.TryAssign(Core::ValueConversion::Trim(value));
}

Base::Result<void>
ControllableStoryboardAction::SetBeginStoryboardName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        Core::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Storyboard action requires BeginStoryboardName");
    }
    return beginStoryboardName_.TryAssign(trimmed);
}

Base::Result<void> SeekStoryboard::SetOffset(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        Core::ValueConversion::Trim(value);
    Base::Result<AnimationTime> parsed =
        ParseClockTime(trimmed);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned =
        offsetText_.TryAssign(trimmed);
    if (!assigned) return assigned.GetStatus();
    offsetMicroseconds_ = parsed.Value();
    return {};
}

Base::Result<void> EventTrigger::SetRoutedEvent(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        Core::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "EventTrigger RoutedEvent cannot be empty");
    }
    return routedEvent_.TryAssign(trimmed);
}

Base::Result<void> EventTrigger::SetSourceName(
    Base::StringView value) noexcept {
    return sourceName_.TryAssign(
        Core::ValueConversion::Trim(value));
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

Base::Result<void> EventTrigger::ClearActions() noexcept {
    actions_.Clear();
    return {};
}

Base::Result<void> StoryboardCompletedTrigger::SetStoryboard(
    Base::Ref<Storyboard> value) noexcept {
    storyboard_ = std::move(value);
    return {};
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

Base::Result<void>
StoryboardCompletedTrigger::ClearActions() noexcept {
    actions_.Clear();
    return {};
}

} // namespace Aero::Media::Animation
