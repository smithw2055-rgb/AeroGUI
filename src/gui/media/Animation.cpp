#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>
#include <Aero/Interactivity/ChangePropertyAction.hpp>
#include <Aero/Interactivity/LaunchUriOrFileAction.hpp>
#include "gui/meta/ValueConversion.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Aero::Media::Animation {
namespace {

Base::Result<AnimationTime> ParseClockTime(
    Base::StringView input) noexcept {
    const Base::StringView text =
        ::Aero::Base::ValueConversion::Trim(input);
    if (text.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Animation clock time is empty");
    }
    Base::String owned;
    Base::Result<void> assigned = owned.Assign(text);
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
            if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
                    suffix, "ms")) {
                multiplier = 0.001;
            } else if (!::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
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

bool ContainsTimeline(
    const Timeline& value,
    const Timeline* sought) noexcept {
    if (&value == sought) return true;
    const TimelineGroup* group = ::Aero::TryCast<TimelineGroup>(
        const_cast<Timeline*>(&value));
    if (group == nullptr) return false;
    for (const Base::Ref<Timeline>& child : group->GetTimelines()) {
        if (child && ContainsTimeline(*child, sought)) return true;
    }
    return false;
}

} // namespace

void Timeline::SetBeginTime(
    Base::StringView value) noexcept {
    if (!WritePreamble()) return;
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return;
    Base::Result<void> assigned = beginTimeText_.Assign(value);
    if (!assigned) return;
    beginTimeMicroseconds_ = parsed.Value();
    WritePostscript();
}

void Timeline::SetDuration(
    Base::StringView value) noexcept {
    static_cast<void>(SetDurationChecked(value));
}

Base::Result<void> Timeline::SetDurationChecked(
    Base::StringView value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    Base::Result<AnimationTime> parsed =
        ParseClockTime(value);
    if (!parsed) return parsed.GetStatus();
    Base::Result<void> assigned = durationText_.Assign(value);
    if (!assigned) return assigned.GetStatus();
    durationMicroseconds_ = parsed.Value();
    WritePostscript();
    return {};
}

void Timeline::SetRepeatBehavior(
    Base::StringView value) noexcept {
    if (!WritePreamble()) return;
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(value);
    double repeatCount = 1.0;
    bool repeatForever = false;
    if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Forever")) {
        repeatForever = true;
    } else {
        Base::Result<double> count =
            ::Aero::Base::ValueConversion::ParseDouble(trimmed);
        if (!count || count.Value() <= 0.0) {
            return;
        }
        repeatCount = count.Value();
    }
    Base::Result<void> assigned =
        repeatBehaviorText_.Assign(value);
    if (!assigned) return;
    repeatCount_ = repeatCount;
    repeatForever_ = repeatForever;
    WritePostscript();
}

void Timeline::SetSpeedRatio(double value) noexcept {
    if (!WritePreamble()) return;
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    speedRatio_ = value;
    WritePostscript();
}

void Timeline::SetAutoReverse(bool value) noexcept {
    if (!WritePreamble() || autoReverse_ == value) return;
    autoReverse_ = value;
    WritePostscript();
}

void Timeline::SetFillBehavior(
    FillBehavior value) noexcept {
    if (!WritePreamble() || fillBehavior_ == value) return;
    fillBehavior_ = value;
    WritePostscript();
}

void PowerEase::SetPower(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "PowerEase Power must be nonnegative");
    if (!valid) return;
    SetValue(PowerProperty, value);
    return;
}

void ExponentialEase::SetExponent(
    double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ExponentialEase Exponent must be nonnegative");
    if (!valid) return;
    SetValue(ExponentProperty, value);
    return;
}

void BackEase::SetAmplitude(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BackEase Amplitude must be nonnegative");
    if (!valid) return;
    SetValue(AmplitudeProperty, value);
    return;
}

void BounceEase::SetBounces(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounces must be nonnegative");
    if (!valid) return;
    SetValue(BouncesProperty, value);
    return;
}

void BounceEase::SetBounciness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounciness must be nonnegative");
    if (!valid) return;
    SetValue(BouncinessProperty, value);
    return;
}

void ElasticEase::SetOscillations(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Oscillations must be nonnegative");
    if (!valid) return;
    SetValue(OscillationsProperty, value);
    return;
}

void ElasticEase::SetSpringiness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Springiness must be nonnegative");
    if (!valid) return;
    SetValue(SpringinessProperty, value);
    return;
}

void DoubleAnimationBase::SetFrom(double value) noexcept {
    if (!WritePreamble() || !std::isfinite(value)) {
        return;
    }
    from_ = value;
    hasFrom_ = true;
    WritePostscript();
}

void DoubleAnimationBase::SetTo(double value) noexcept {
    if (!WritePreamble() || !std::isfinite(value)) {
        return;
    }
    to_ = value;
    hasTo_ = true;
    WritePostscript();
}

void DoubleAnimation::SetAccelerationRatio(
    double value) noexcept {
    if (!WritePreamble()) return;
    if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
        value + decelerationRatio_ > 1.0) {
        return;
    }
    accelerationRatio_ = value;
    WritePostscript();
}

void DoubleAnimation::SetDecelerationRatio(
    double value) noexcept {
    if (!WritePreamble()) return;
    if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
        accelerationRatio_ + value > 1.0) {
        return;
    }
    decelerationRatio_ = value;
    WritePostscript();
}

void DoubleAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    if (!WritePreamble() || easing_.Get() == value.Get()) return;
    easing_ = std::move(value);
    WritePostscript();
}


void ColorAnimationBase::SetFrom(
    Base::Color value) noexcept {
    if (!WritePreamble()) return;
    if (!Base::IsFiniteColor(value)) {
        return;
    }
    from_ = value;
    WritePostscript();
}

void ColorAnimationBase::SetTo(
    Base::Color value) noexcept {
    if (!WritePreamble()) return;
    if (!Base::IsFiniteColor(value)) {
        return;
    }
    to_ = value;
    WritePostscript();
}

void ColorAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    if (!WritePreamble() || easing_.Get() == value.Get()) return;
    easing_ = std::move(value);
    WritePostscript();
}


void PointAnimationBase::SetFrom(
    Base::Point value) noexcept {
    if (!WritePreamble()) return;
    if (!std::isfinite(value.x) ||
        !std::isfinite(value.y)) {
        return;
    }
    from_ = value;
    WritePostscript();
}

void PointAnimationBase::SetTo(
    Base::Point value) noexcept {
    if (!WritePreamble()) return;
    if (!std::isfinite(value.x) ||
        !std::isfinite(value.y)) {
        return;
    }
    to_ = value;
    WritePostscript();
}

void
PointAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase>
        value) noexcept {
    if (!WritePreamble() || easing_.Get() == value.Get()) return;
    easing_ = std::move(value);
    WritePostscript();
}


void RectAnimationBase::SetFrom(
    Base::Rect value) noexcept {
    if (!WritePreamble()) return;
    if (!Base::IsFiniteRect(value)) {
        return;
    }
    from_ = value;
    WritePostscript();
}

void RectAnimationBase::SetTo(
    Base::Rect value) noexcept {
    if (!WritePreamble()) return;
    if (!Base::IsFiniteRect(value)) {
        return;
    }
    to_ = value;
    WritePostscript();
}

void RectAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    if (!WritePreamble() || easing_.Get() == value.Get()) return;
    easing_ = std::move(value);
    WritePostscript();
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

void ThicknessAnimationBase::SetFrom(
    Base::Thickness value) noexcept {
    if (!WritePreamble()) return;
    if (!IsFiniteThickness(value)) {
        return;
    }
    from_ = value;
    WritePostscript();
}

void ThicknessAnimationBase::SetTo(
    Base::Thickness value) noexcept {
    if (!WritePreamble()) return;
    if (!IsFiniteThickness(value)) {
        return;
    }
    to_ = value;
    WritePostscript();
}

void
ThicknessAnimation::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    if (!WritePreamble() || easing_.Get() == value.Get()) return;
    easing_ = std::move(value);
    WritePostscript();
}


void KeyFrameBase::SetKeyTime(Base::StringView value) noexcept {
    SetValue(KeyTimeProperty, value);
}

void KeyFrameBase::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    SetValue(EasingFunctionProperty, std::move(value));
}

void KeyFrameBase::SetKeySpline(Base::StringView value) noexcept {
    SetValue(KeySplineProperty, value);
}

void KeyFrameBase::OnKeyTimeChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    auto* frame = ::Aero::TryCast<KeyFrameBase>(&object);
    if (frame == nullptr) return;
    const Base::StringView text = args.GetNewValue().AsString();
    Base::Result<AnimationTime> parsed = ParseClockTime(text);
    if (!parsed) return;
    frame->keyTimeMicroseconds_ = parsed.Value();
}

void KeyFrameBase::OnKeySplineChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    auto* frame = ::Aero::TryCast<KeyFrameBase>(&object);
    if (frame == nullptr) return;
    const Base::StringView value = args.GetNewValue().AsString();
    Base::String owned;
    Base::Result<void> assigned = owned.Assign(value);
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
    if (*cursor != '\0') return;
    frame->controlPoint1X_ = values[0];
    frame->controlPoint1Y_ = values[1];
    frame->controlPoint2X_ = values[2];
    frame->controlPoint2Y_ = values[3];
}

Base::Result<void> TimelineGroup::AddChild(
    Base::Ref<Timeline> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TimelineGroup child cannot be null");
    }
    if (ContainsTimeline(*value, this)) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "TimelineGroup cannot contain itself directly or indirectly");
    }
    if (timelineChangedHandler_.Empty()) {
        timelineChangedHandler_ = FreezableChangedHandler(
            this, &TimelineGroup::OnTimelineChanged);
    }
    Timeline* retained = value.Get();
    if (!retained->IsFrozen()) {
        Base::Result<void> subscribed =
            retained->AddChangedHandlerChecked(timelineChangedHandler_);
        if (!subscribed) return subscribed.GetStatus();
    }
    Base::Result<void> added = timelines_.Add(std::move(value));
    if (!added) {
        if (!retained->IsFrozen()) {
            static_cast<void>(retained->RemoveChangedHandler(
                timelineChangedHandler_));
        }
        return added.GetStatus();
    }
    WritePostscript();
    return {};
}

void TimelineGroup::Clear() noexcept {
    if (!WritePreamble() || timelines_.Empty()) return;
    for (const Base::Ref<Timeline>& timeline : timelines_) {
        if (timeline && !timeline->IsFrozen() &&
            !timelineChangedHandler_.Empty()) {
            static_cast<void>(timeline->RemoveChangedHandler(
                timelineChangedHandler_));
        }
    }
    timelines_.Clear();
    WritePostscript();
}

TimelineGroup::~TimelineGroup() {
    for (const Base::Ref<Timeline>& timeline : timelines_) {
        if (timeline && !timeline->IsFrozen() &&
            !timelineChangedHandler_.Empty()) {
            static_cast<void>(timeline->RemoveChangedHandler(
                timelineChangedHandler_));
        }
    }
}

void TimelineGroup::OnTimelineChanged(Freezable&) noexcept {
    WritePostscript();
}

bool TimelineGroup::FreezeCore(bool isChecking) noexcept {
    for (const Base::Ref<Timeline>& timeline : timelines_) {
        if (!timeline) continue;
        if (isChecking) {
            if (!timeline->CanFreeze()) return false;
        } else {
            static_cast<void>(timeline->Freeze());
        }
    }
    return Timeline::FreezeCore(isChecking);
}

void BeginStoryboard::SetStoryboard(
    Base::Ref<Storyboard> value) noexcept {
    storyboard_ = std::move(value);
    return;
}

void BeginStoryboard::SetName(
    Base::StringView value) noexcept {
    static_cast<void>(name_.Assign(value));
}

} // namespace Aero::Media::Animation

void Aero::Interactivity::ChangePropertyAction::SetTargetName(
    Base::StringView value) noexcept {
    static_cast<void>(targetName_.Assign(value));
}

void Aero::Interactivity::ChangePropertyAction::SetPropertyName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return;
    }
    static_cast<void>(propertyName_.Assign(trimmed));
}

void Aero::Interactivity::ChangePropertyAction::SetValue(
    const Meta::PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return;
    }
    value_ = value;
    return;
}

void Aero::Interactivity::ChangePropertyAction::SetValueBinding(
    Base::Ref<Aero::Data::Binding> value) noexcept {
    valueBinding_ = std::move(value);
}

void Aero::Interactivity::LaunchUriOrFileAction::SetPath(
    Base::StringView value) noexcept {
    static_cast<void>(path_.Assign(value));
}

namespace Aero::Media::Animation {

void
ControllableStoryboardAction::SetBeginStoryboardName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return;
    }
    static_cast<void>(beginStoryboardName_.Assign(trimmed));
}

void SeekStoryboard::SetOffset(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(value);
    Base::Result<AnimationTime> parsed =
        ParseClockTime(trimmed);
    if (!parsed) return;
    Base::Result<void> assigned =
        offsetText_.Assign(trimmed);
    if (!assigned) return;
    offsetMicroseconds_ = parsed.Value();
    return;
}

void EventTrigger::SetRoutedEvent(
    Base::StringView value) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(value);
    if (trimmed.Empty()) {
        return;
    }
    static_cast<void>(routedEvent_.Assign(trimmed));
}

void EventTrigger::SetSourceName(
    Base::StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(value));
}

Base::Result<void> EventTrigger::AddAction(
    Base::Ref<TriggerAction> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "EventTrigger action cannot be null");
    }
    return actions_.PushBack(std::move(value));
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

Base::Result<void> StoryboardCompletedTrigger::AddAction(
    Base::Ref<TriggerAction> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StoryboardCompletedTrigger action cannot be null");
    }
    return actions_.PushBack(std::move(value));
}

void
StoryboardCompletedTrigger::ClearActions() noexcept {
    actions_.Clear();
    return;
}

} // namespace Aero::Media::Animation
