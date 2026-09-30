#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include "gui/core/ValueConversion.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "gui/core/Describe.hpp"
#include "gui/core/EnumRegistration.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <utility>

namespace Aero::Media::Animation {

Base::Result<std::uint64_t> ParseClockTimeMicroseconds(
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
    return static_cast<std::uint64_t>(
        std::llround(seconds * 1000000.0));
}

namespace {

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

Base::Result<TimeSpan> TimeSpan::TryParse(Base::StringView text) noexcept {
    Base::Result<std::uint64_t> parsed = ParseClockTimeMicroseconds(text);
    if (!parsed) return parsed.GetStatus();
    return TimeSpan::FromMicroseconds(parsed.Value());
}

Base::Result<Duration> Duration::TryParse(Base::StringView text) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(text);
    if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Automatic") ||
        ::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Auto")) {
        return Duration::Automatic();
    }
    if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Forever")) {
        return Duration::Forever();
    }
    Base::Result<TimeSpan> parsed = TimeSpan::TryParse(trimmed);
    if (!parsed) return parsed.GetStatus();
    return Duration::FromTimeSpan(parsed.Value());
}

Base::Result<RepeatBehavior> RepeatBehavior::TryParse(
    Base::StringView text) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(text);
    if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Forever")) {
        return RepeatBehavior::Forever();
    }
    if (!trimmed.Empty()) {
        const char last = trimmed[trimmed.SizeBytes() - 1U];
        if (last == 'x' || last == 'X') {
            const Base::StringView prefix = trimmed.Substr(
                0U, trimmed.SizeBytes() - 1U);
            Base::Result<double> count =
                ::Aero::Base::ValueConversion::ParseDouble(prefix);
            if (!count ||
                !std::isfinite(count.Value()) ||
                count.Value() <= 0.0) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "RepeatBehavior count must be a positive number");
            }
            return RepeatBehavior::Count(count.Value());
        }
    }
    Base::Result<double> count =
        ::Aero::Base::ValueConversion::ParseDouble(trimmed);
    if (count && std::isfinite(count.Value()) && count.Value() > 0.0) {
        return RepeatBehavior::Count(count.Value());
    }
    Base::Result<TimeSpan> duration = TimeSpan::TryParse(trimmed);
    if (!duration) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "RepeatBehavior must be Forever, Nx, a count, or a clock time");
    }
    return RepeatBehavior::FromDuration(duration.Value());
}

Base::Result<KeyTime> KeyTime::TryParse(Base::StringView text) noexcept {
    const Base::StringView trimmed =
        ::Aero::Base::ValueConversion::Trim(text);
    if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Uniform")) {
        return KeyTime::Uniform();
    }
    if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            trimmed, "Paced")) {
        return KeyTime::Paced();
    }
    if (!trimmed.Empty() &&
        trimmed[trimmed.SizeBytes() - 1U] == '%') {
        const Base::StringView prefix = trimmed.Substr(
            0U, trimmed.SizeBytes() - 1U);
        Base::Result<double> percent =
            ::Aero::Base::ValueConversion::ParseDouble(prefix);
        if (!percent ||
            !std::isfinite(percent.Value()) ||
            percent.Value() < 0.0 ||
            percent.Value() > 100.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "KeyTime percent must be between 0 and 100");
        }
        return KeyTime::FromPercent(percent.Value() / 100.0);
    }
    Base::Result<TimeSpan> parsed = TimeSpan::TryParse(trimmed);
    if (!parsed) return parsed.GetStatus();
    return KeyTime::FromTimeSpan(parsed.Value());
}

std::uint64_t KeyTime::ResolveMicroseconds(
    std::uint64_t durationMicroseconds,
    std::uint32_t index,
    std::uint32_t count) const noexcept {
    switch (kind_) {
    case Kind::TimeSpan:
        return timeSpan_.Microseconds();
    case Kind::Percent: {
        if (durationMicroseconds == 0U ||
            durationMicroseconds == UINT64_MAX) {
            return 0U;
        }
        const double scaled =
            percent_ * static_cast<double>(durationMicroseconds);
        if (!std::isfinite(scaled) || scaled <= 0.0) return 0U;
        if (scaled >= static_cast<double>(UINT64_MAX)) return UINT64_MAX;
        return static_cast<std::uint64_t>(std::llround(scaled));
    }
    case Kind::Uniform:
    case Kind::Paced: {
        if (count == 0U ||
            durationMicroseconds == 0U ||
            durationMicroseconds == UINT64_MAX) {
            return 0U;
        }
        const double scaled =
            static_cast<double>(durationMicroseconds) *
            static_cast<double>(index + 1U) /
            static_cast<double>(count);
        if (!std::isfinite(scaled) || scaled <= 0.0) return 0U;
        if (scaled >= static_cast<double>(UINT64_MAX)) return UINT64_MAX;
        return static_cast<std::uint64_t>(std::llround(scaled));
    }
    }
    return 0U;
}

void Timeline::SetBeginTime(TimeSpan value) noexcept {
    SetValue(BeginTimeProperty, value);
}

void Timeline::SetBeginTime(Base::StringView value) noexcept {
    Base::Result<TimeSpan> parsed = TimeSpan::TryParse(value);
    if (!parsed) return;
    SetValue(BeginTimeProperty, parsed.Value());
}

void Timeline::SetDuration(Duration value) noexcept {
    SetValue(DurationProperty, value);
}

void Timeline::SetDuration(Base::StringView value) noexcept {
    Base::Result<Duration> parsed = Duration::TryParse(value);
    if (!parsed) return;
    SetValue(DurationProperty, parsed.Value());
}

void Timeline::SetRepeatBehavior(RepeatBehavior value) noexcept {
    SetValue(RepeatBehaviorProperty, value);
}

void Timeline::SetRepeatBehavior(Base::StringView value) noexcept {
    Base::Result<RepeatBehavior> parsed = RepeatBehavior::TryParse(value);
    if (!parsed) return;
    SetValue(RepeatBehaviorProperty, parsed.Value());
}

void Timeline::SetSpeedRatio(double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    SetValue(SpeedRatioProperty, value);
}

void Timeline::SetAutoReverse(bool value) noexcept {
    SetValue(AutoReverseProperty, value);
}

void Timeline::SetFillBehavior(FillBehavior value) noexcept {
    SetValue(FillBehaviorProperty, value);
}

void EasingFunctionBase::SetPower(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "PowerEase Power must be nonnegative");
    if (!valid) return;
    SetValue(PowerProperty, value);
}

void EasingFunctionBase::SetExponent(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ExponentialEase Exponent must be nonnegative");
    if (!valid) return;
    SetValue(ExponentProperty, value);
}

void EasingFunctionBase::SetAmplitude(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BackEase Amplitude must be nonnegative");
    if (!valid) return;
    SetValue(AmplitudeProperty, value);
}

void EasingFunctionBase::SetBounces(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounces must be nonnegative");
    if (!valid) return;
    SetValue(BouncesProperty, value);
}

void EasingFunctionBase::SetBounciness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "BounceEase Bounciness must be nonnegative");
    if (!valid) return;
    SetValue(BouncinessProperty, value);
}

void EasingFunctionBase::SetOscillations(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Oscillations must be nonnegative");
    if (!valid) return;
    SetValue(OscillationsProperty, value);
}

void EasingFunctionBase::SetSpringiness(double value) noexcept {
    Base::Result<void> valid = ValidateNonNegative(
        value, "ElasticEase Springiness must be nonnegative");
    if (!valid) return;
    SetValue(SpringinessProperty, value);
}

void KeyFrameBase::SetKeyTime(KeyTime value) noexcept {
    SetValue(KeyTimeProperty, value);
}

void KeyFrameBase::SetKeyTime(Base::StringView value) noexcept {
    Base::Result<KeyTime> parsed = KeyTime::TryParse(value);
    if (!parsed) return;
    SetValue(KeyTimeProperty, parsed.Value());
}

void KeyFrameBase::SetEasingFunction(
    Base::Ref<EasingFunctionBase> value) noexcept {
    SetValue(EasingFunctionProperty, std::move(value));
}

void KeyFrameBase::SetKeySpline(Base::StringView value) noexcept {
    SetValue(KeySplineProperty, value);
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

void TimelineGroup::AddChild(
    Base::Ref<Timeline> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    if (!value) { AERO_ASSERT(false); return; }
    if (ContainsTimeline(*value, this)) { AERO_ASSERT(false); return; }
    if (timelineChangedHandler_.Empty()) {
        timelineChangedHandler_ = FreezableChangedHandler(
            this, &TimelineGroup::OnTimelineChanged);
    }
    Timeline* retained = value.Get();
    if (!retained->IsFrozen()) {
        retained->AddChangedHandler(timelineChangedHandler_);
    }
    timelines_.Add(std::move(value));
    WritePostscript();
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
        ParseClockTimeMicroseconds(trimmed);
    if (!parsed) return;
    Base::Result<void> assigned =
        offsetText_.Assign(trimmed);
    if (!assigned) return;
    offsetMicroseconds_ = parsed.Value();
    return;
}

} // namespace Aero::Media::Animation

namespace Aero {

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

void EventTrigger::AddAction(
    Base::Ref<Interactivity::TriggerAction> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    actions_.PushBack(std::move(value));
}

void EventTrigger::ClearActions() noexcept {
    actions_.Clear();
    return;
}

} // namespace Aero

namespace Aero::Media::Animation {

void StoryboardCompletedTrigger::SetStoryboard(
    Base::Ref<Storyboard> value) noexcept {
    storyboard_ = std::move(value);
    return;
}

void StoryboardCompletedTrigger::AddAction(
    Base::Ref<TriggerAction> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    actions_.PushBack(std::move(value));
}

void
StoryboardCompletedTrigger::ClearActions() noexcept {
    actions_.Clear();
    return;
}

} // namespace Aero::Media::Animation

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Interactivity;
using Media::Animation::EventTrigger;
using Media::Animation::StoryboardCompletedTrigger;
namespace {

void AddEventTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<TriggerAction> retained =
        Base::Ref<TriggerAction>::TryFromBorrowed(
            static_cast<TriggerAction&>(*value));
    if (!retained) {
        return;
    }
    static_cast<EventTrigger&>(owner)
        .AddAction(std::move(retained));
}

void ClearEventTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<EventTrigger&>(owner).ClearActions();
    return;
}

void AddStoryboardCompletedTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<TriggerAction> retained =
        Base::Ref<TriggerAction>::TryFromBorrowed(
            static_cast<TriggerAction&>(*value));
    if (!retained) {
        return;
    }
    static_cast<StoryboardCompletedTrigger&>(owner)
        .AddAction(std::move(retained));
}

void ClearStoryboardCompletedTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<StoryboardCompletedTrigger&>(owner)
        .ClearActions();
    return;
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero::Media::Animation {

AERO_DESCRIBE(Timeline) {
    using namespace Aero::Meta;
    Register<Timeline>(context, TypeFlags::Abstract)
            .Property(Timeline::BeginTimeProperty, TimeSpan::Zero())
            .Property(Timeline::DurationProperty, Duration::Automatic())
            .Property(Timeline::RepeatBehaviorProperty, RepeatBehavior::Once())
            .Property(Timeline::SpeedRatioProperty, 1.0)
            .Property(Timeline::AutoReverseProperty, false)
            .Property(Timeline::FillBehaviorProperty, FillBehavior::HoldEnd);
}

AERO_DESCRIBE(AnimationTimeline) {
    using namespace Aero::Meta;
    Register<AnimationTimeline>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(EasingFunctionBase) {
    using namespace Aero::Meta;
    Register<EasingFunctionBase>(context, TypeFlags::Abstract)
            .Property(EasingFunctionBase::EasingModeProperty, EasingMode::EaseOut)
            .Property(EasingFunctionBase::ExponentProperty, 2.0)
            .Property(EasingFunctionBase::PowerProperty, 2.0)
            .Property(EasingFunctionBase::AmplitudeProperty, 1.0)
            .Property(EasingFunctionBase::BouncesProperty, 3.0)
            .Property(EasingFunctionBase::BouncinessProperty, 3.0)
            .Property(EasingFunctionBase::OscillationsProperty, 3.0)
            .Property(EasingFunctionBase::SpringinessProperty, 3.0);
}

AERO_DESCRIBE(KeyFrameBase) {
    using namespace Aero::Meta;
    Register<KeyFrameBase>(context, TypeFlags::Abstract)
            .Property(KeyFrameBase::KeyTimeProperty, KeyTime{})
            .Property(KeyFrameBase::EasingFunctionProperty, Base::Ref<EasingFunctionBase>{}, AffectsRender)
            .Property(KeyFrameBase::KeySplineProperty, FrameworkPropertyMetadata(Base::String{}) .Changed(&KeyFrameBase::OnKeySplineChanged));
}

AERO_DESCRIBE(ObjectKeyFrame) {
    using namespace Aero::Meta;
    Register<ObjectKeyFrame>(context, TypeFlags::Abstract)
            .Property<Value, &ObjectKeyFrame::GetValue, &ObjectKeyFrame::SetValue>("Value", PropertyFlags::AnyValue);
}

AERO_DESCRIBE(TimerTrigger) {
    using namespace Aero::Meta;
    Register<TimerTrigger>(context)
            .Property("TotalTicks", &TimerTrigger::GetTotalTicks, &TimerTrigger::SetTotalTicks)
            .Property<Meta::PropertyValue, &TimerTrigger::GetMillisecondsPerTick, &TimerTrigger::SetMillisecondsPerTick>("MillisecondsPerTick", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(StoryboardCompletedTrigger) {
    using namespace Aero::Meta;
    Register<StoryboardCompletedTrigger>(context)
            .Property("Storyboard", &StoryboardCompletedTrigger::GetStoryboard, &StoryboardCompletedTrigger::SetStoryboard)
            .Content<::Aero::Interactivity::TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddStoryboardCompletedTriggerAction, &::Aero::MetadataSupport::ClearStoryboardCompletedTriggerActions)
            .Factory();
}

} // namespace Aero::Media::Animation

namespace Aero {

AERO_DESCRIBE(EventTrigger) {
    using namespace Aero::Meta;
    Register<EventTrigger>(context)
            .Property("RoutedEvent", &EventTrigger::GetRoutedEvent, &EventTrigger::SetRoutedEvent)
            .Property("EventName", &EventTrigger::GetEventName, &EventTrigger::SetEventName)
            .Property("SourceName", &EventTrigger::GetSourceName, &EventTrigger::SetSourceName)
            .Content<::Aero::Interactivity::TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddEventTriggerAction, &::Aero::MetadataSupport::ClearEventTriggerActions)
            .Factory();
}
} // namespace Aero


// ===== Animation keyframe helpers (MetadataSupport) =====

namespace Aero::MetadataSupport {
using Media::Animation::BooleanAnimationUsingKeyFrames;
using Media::Animation::BooleanKeyFrame;
using Media::Animation::ColorAnimationUsingKeyFrames;
using Media::Animation::ColorKeyFrame;
using Media::Animation::DoubleAnimationUsingKeyFrames;
using Media::Animation::DoubleKeyFrame;
using Media::Animation::Int16AnimationUsingKeyFrames;
using Media::Animation::Int16KeyFrame;
using Media::Animation::Int32AnimationUsingKeyFrames;
using Media::Animation::Int32KeyFrame;
using Media::Animation::Int64AnimationUsingKeyFrames;
using Media::Animation::Int64KeyFrame;
using Media::Animation::MatrixAnimationUsingKeyFrames;
using Media::Animation::MatrixKeyFrame;
using Media::Animation::ObjectAnimationUsingKeyFrames;
using Media::Animation::ObjectKeyFrame;
using Media::Animation::PointAnimationUsingKeyFrames;
using Media::Animation::PointKeyFrame;
using Media::Animation::SizeAnimationUsingKeyFrames;
using Media::Animation::SizeKeyFrame;
using Media::Animation::StringAnimationUsingKeyFrames;
using Media::Animation::StringKeyFrame;
using Media::Animation::ThicknessAnimationUsingKeyFrames;
using Media::Animation::ThicknessKeyFrame;
namespace {

void AddDoubleKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<DoubleKeyFrame> retained =
        Base::Ref<DoubleKeyFrame>::TryFromBorrowed(
            static_cast<DoubleKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<DoubleAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearDoubleKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<DoubleAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddPointKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<PointKeyFrame> retained =
        Base::Ref<PointKeyFrame>::TryFromBorrowed(
            static_cast<PointKeyFrame&>(*value));
    if (!retained) return;
    static_cast<PointAnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearPointKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<PointAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddThicknessKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ThicknessKeyFrame> retained =
        Base::Ref<ThicknessKeyFrame>::
            TryFromBorrowed(
                static_cast<
                    ThicknessKeyFrame&>(
                        *value));
    if (!retained) {
        return;
    }
    static_cast<ThicknessAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearThicknessKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ThicknessAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddColorKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ColorKeyFrame> retained =
        Base::Ref<ColorKeyFrame>::TryFromBorrowed(
            static_cast<ColorKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<ColorAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearColorKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ColorAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddObjectKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ObjectKeyFrame> retained =
        Base::Ref<ObjectKeyFrame>::TryFromBorrowed(
            static_cast<ObjectKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<ObjectAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearObjectKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ObjectAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddBooleanKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<BooleanKeyFrame> retained =
        Base::Ref<BooleanKeyFrame>::TryFromBorrowed(
            static_cast<BooleanKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<BooleanAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearBooleanKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<BooleanAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddInt16KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int16KeyFrame> retained =
        Base::Ref<Int16KeyFrame>::TryFromBorrowed(
            static_cast<Int16KeyFrame&>(*value));
    if (!retained) return;
    static_cast<Int16AnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearInt16KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int16AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddInt32KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int32KeyFrame> retained =
        Base::Ref<Int32KeyFrame>::TryFromBorrowed(
            static_cast<Int32KeyFrame&>(*value));
    if (!retained) return;
    static_cast<Int32AnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearInt32KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int32AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddInt64KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int64KeyFrame> retained =
        Base::Ref<Int64KeyFrame>::TryFromBorrowed(
            static_cast<Int64KeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<Int64AnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearInt64KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int64AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddSizeKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<SizeKeyFrame> retained =
        Base::Ref<SizeKeyFrame>::TryFromBorrowed(
            static_cast<SizeKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<SizeAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearSizeKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<SizeAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddMatrixKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<MatrixKeyFrame> retained =
        Base::Ref<MatrixKeyFrame>::TryFromBorrowed(
            static_cast<MatrixKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<MatrixAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearMatrixKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<MatrixAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddStringKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<StringKeyFrame> retained =
        Base::Ref<StringKeyFrame>::TryFromBorrowed(
            static_cast<StringKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<StringAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearStringKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<StringAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

} // namespace
} // namespace Aero::MetadataSupport

// Concrete animation registration (eases / from-to / keyframes).
// BuiltinModules::PopulateUiAnimation orchestrates DescribeHook lists + this call.

namespace Aero {
using namespace ::Aero::MetadataSupport;
using namespace ::Aero::Meta;

Base::Result<void> PopulateAnimationTypes(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Media::Animation;
    // Prefer public Animation types over Model::* (AnimationEngine.hpp).
    using Media::Animation::ColorAnimation;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimation;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::MatrixAnimation;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::PointAnimation;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::RectAnimation;
    using Media::Animation::RepeatBehavior;
    using Media::Animation::SizeAnimation;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::ThicknessAnimation;
    using Media::Animation::ThicknessKeyFrame;
    // Prefer geometry typedefs over BrushRendering/AnimationModel conversion fns in Animation.
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<Duration>(context)
        .ValueSemantics()
        .TextConverter<&Duration::TryParse>();
    Register<TimeSpan>(context)
        .ValueSemantics()
        .TextConverter<&TimeSpan::TryParse>();
    Register<RepeatBehavior>(context)
        .ValueSemantics()
        .TextConverter<&RepeatBehavior::TryParse>();
    Register<KeyTime>(context)
        .ValueSemantics()
        .TextConverter<&KeyTime::TryParse>();
    DescribeHook<::Aero::Media::Animation::Timeline>::Run(context);
    DescribeHook<::Aero::Media::Animation::AnimationTimeline>::Run(context);
    DescribeHook<::Aero::Media::Animation::TimelineGroup>::Run(context);
    DescribeHook<::Aero::Media::Animation::ParallelTimeline>::Run(context);
    DescribeHook<::Aero::Media::Animation::Storyboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::EasingFunctionBase>::Run(context);

#define AERO_EASE_FACTORY(name, kind) \
    +[]() noexcept -> Base::Result<Base::Ref<Base::Object>> { \
        auto created = Base::MakeRef<EasingFunctionBase>( \
            ::Aero::Meta::MakeTypeId(::Aero::Meta::AeroNamespaceUri(), #name), \
            EasingFunctionBase::Kind::kind); \
        if (!created) return created.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(created).Value()); \
    }
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "SineEase"),
        AERO_EASE_FACTORY(SineEase, Sine));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuadraticEase"),
        AERO_EASE_FACTORY(QuadraticEase, Quadratic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "CubicEase"),
        AERO_EASE_FACTORY(CubicEase, Cubic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuarticEase"),
        AERO_EASE_FACTORY(QuarticEase, Quartic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuinticEase"),
        AERO_EASE_FACTORY(QuinticEase, Quintic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "CircleEase"),
        AERO_EASE_FACTORY(CircleEase, Circle));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "ExponentialEase"),
        AERO_EASE_FACTORY(ExponentialEase, Exponential));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "PowerEase"),
        AERO_EASE_FACTORY(PowerEase, Power));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "BackEase"),
        AERO_EASE_FACTORY(BackEase, Back));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "BounceEase"),
        AERO_EASE_FACTORY(BounceEase, Bounce));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "ElasticEase"),
        AERO_EASE_FACTORY(ElasticEase, Elastic));
#undef AERO_EASE_FACTORY

#define AERO_FROM_TO_BASE(Name, CppType) \
    Register<Name##AnimationBase>(context, TypeFlags::Abstract) \
        .Property<CppType, &Name##AnimationBase::GetFrom, &Name##AnimationBase::SetFrom>("From") \
        .Property<CppType, &Name##AnimationBase::GetTo, &Name##AnimationBase::SetTo>("To");

#define AERO_FROM_TO_ANIMATION(Name, CppType) \
    AERO_FROM_TO_BASE(Name, CppType) \
    Register<Name##Animation>(context) \
        .Property<Base::Ref<EasingFunctionBase>, &Name##Animation::GetEasingFunction, &Name##Animation::SetEasingFunction>("EasingFunction", PropertyFlags::Structural) \
        .Factory();
    AERO_FROM_TO_BASE(Double, double)
    Register<DoubleAnimation>(context)
        .Property<double, &DoubleAnimation::GetAccelerationRatio, &DoubleAnimation::SetAccelerationRatio>("AccelerationRatio")
        .Property<double, &DoubleAnimation::GetDecelerationRatio, &DoubleAnimation::SetDecelerationRatio>("DecelerationRatio")
        .Property<Base::Ref<EasingFunctionBase>, &DoubleAnimation::GetEasingFunction, &DoubleAnimation::SetEasingFunction>("EasingFunction", PropertyFlags::Structural)
        .Factory();
    AERO_FROM_TO_ANIMATION(Color, Color)
    AERO_FROM_TO_ANIMATION(Point, Point)
    AERO_FROM_TO_ANIMATION(Rect, Rect)
    AERO_FROM_TO_ANIMATION(Thickness, Base::Thickness)
    AERO_FROM_TO_ANIMATION(Int16, std::int16_t)
    AERO_FROM_TO_ANIMATION(Int32, std::int32_t)
    AERO_FROM_TO_ANIMATION(Int64, std::int64_t)
    AERO_FROM_TO_ANIMATION(Size, Base::Size)
    AERO_FROM_TO_ANIMATION(Matrix, Base::Transform2D)
#undef AERO_FROM_TO_BASE
#undef AERO_FROM_TO_ANIMATION

#define AERO_KEY_FACTORY(Type, name, interpolation) \
    +[]() noexcept -> Base::Result<Base::Ref<Base::Object>> { \
        auto created = Base::MakeRef<Type>( \
            ::Aero::Meta::MakeTypeId(::Aero::Meta::AeroNamespaceUri(), #name), \
            KeyFrameBase::Interpolation::interpolation); \
        if (!created) return created.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(created).Value()); \
    }
#define AERO_KEYFRAME_ALIAS(Name, Kind) \
    SetObjectFactory(RegisterAlias<Name##KeyFrame>(context, #Kind #Name "KeyFrame"), \
        AERO_KEY_FACTORY(Name##KeyFrame, Kind##Name##KeyFrame, Kind));
#define AERO_KEYFRAME_ALIASES(Name) \
    AERO_KEYFRAME_ALIAS(Name, Linear) \
    AERO_KEYFRAME_ALIAS(Name, Discrete) \
    AERO_KEYFRAME_ALIAS(Name, Easing) \
    AERO_KEYFRAME_ALIAS(Name, Spline)
#define AERO_KEYFRAME_VALUE(Name, CppType) \
    Register<Name##KeyFrame>(context, TypeFlags::Abstract) \
        .Property<CppType, &Name##KeyFrame::GetValue, &Name##KeyFrame::SetValue>("Value");
#define AERO_KEYFRAME_COLLECTION(Name) \
    Register<Name##AnimationUsingKeyFrames>(context) \
        .Content<Name##KeyFrame>("KeyFrames", ContentKind::Collection, &Add##Name##KeyFrame, &Clear##Name##KeyFrames) \
        .Factory();
#define AERO_KEYFRAMES(Name, CppType) \
    AERO_KEYFRAME_VALUE(Name, CppType) \
    AERO_KEYFRAME_ALIASES(Name) \
    AERO_KEYFRAME_COLLECTION(Name)
#define AERO_DISCRETE_KEYFRAMES(Name, CppType) \
    AERO_KEYFRAME_VALUE(Name, CppType) \
    AERO_KEYFRAME_ALIAS(Name, Discrete) \
    AERO_KEYFRAME_COLLECTION(Name)
    DescribeHook<::Aero::Media::Animation::KeyFrameBase>::Run(context);
    AERO_KEYFRAMES(Double, double)
    AERO_KEYFRAMES(Point, Point)
    AERO_KEYFRAMES(Thickness, Thickness)
    AERO_KEYFRAMES(Color, Base::Color)
    AERO_KEYFRAMES(Int16, std::int16_t)
    AERO_KEYFRAMES(Int32, std::int32_t)
    AERO_KEYFRAMES(Int64, std::int64_t)
    AERO_KEYFRAMES(Size, Base::Size)
    AERO_KEYFRAMES(Matrix, Base::Transform2D)
    DescribeHook<::Aero::Media::Animation::ObjectKeyFrame>::Run(context);
    AERO_KEYFRAME_ALIAS(Object, Discrete)
    AERO_KEYFRAME_COLLECTION(Object)
    AERO_DISCRETE_KEYFRAMES(Boolean, bool)
    AERO_DISCRETE_KEYFRAMES(String, Base::String)
#undef AERO_KEYFRAME_ALIAS
#undef AERO_KEYFRAME_ALIASES
#undef AERO_KEYFRAME_VALUE
#undef AERO_KEYFRAME_COLLECTION
#undef AERO_KEYFRAMES
#undef AERO_DISCRETE_KEYFRAMES

    return {};
}

} // namespace Aero
