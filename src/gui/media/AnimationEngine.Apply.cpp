#include "gui/media/AnimationEngineCommon.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Value.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <utility>
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Media::Animation::Model;
using namespace Aero::Media;
using namespace Aero::Media::Animation::EngineSupport;

double AnimationEngine::Ease(
    double progress,
    const EasingFunction& easing) noexcept {
    if (auto ease = MakeEasingFunction(easing)) {
        return ease->Ease(progress);
    }
    return Clamp01(progress);
}

bool AnimationEngine::ApplyTrack(
    Track& track,
    AnimationTime nowMicroseconds) noexcept {
    if (track.state == AnimationState::Stopped ||
        track.state == AnimationState::Filling) {
        return false;
    }
    AnimationTime sampledNow = nowMicroseconds;
    if (track.pendingInitialSample) {
        // Automatic clocks keep t=0 until the first presented frame.
        // Manual clocks (View::Update / AdvanceBy) must sample elapsed time
        // on the same tick, otherwise a GeneratedDuration fade stays frozen
        // at `from` (Menu3D CircledArrow Opacity 1 after Unchecked).
        if (automaticTickingEnabled_ ||
            nowMicroseconds <= track.startTimeMicroseconds) {
            sampledNow = track.startTimeMicroseconds;
        } else {
            track.pendingInitialSample = false;
        }
    }
    if (!track.pendingInitialSample &&
        track.state == AnimationState::Paused) {
        sampledNow = track.pauseTimeMicroseconds;
    }
    const AnimationTime elapsedClock =
        sampledNow >= track.startTimeMicroseconds
        ? sampledNow - track.startTimeMicroseconds
        : 0U;
    const AnimationTime unpausedClock =
        elapsedClock >= track.accumulatedPauseMicroseconds
        ? elapsedClock - track.accumulatedPauseMicroseconds
        : 0U;
    const long double scaled =
        static_cast<long double>(unpausedClock) *
        track.timing.speedRatio +
        static_cast<long double>(track.seekOffsetMicroseconds);
    AnimationTime localTime = scaled >=
            static_cast<long double>(UINT64_MAX)
        ? UINT64_MAX
        : static_cast<AnimationTime>(scaled);
    if (localTime < track.timing.beginTimeMicroseconds) {
        return false;
    }
    localTime -= track.timing.beginTimeMicroseconds;

    const AnimationTime duration =
        track.timing.durationMicroseconds;
    const long double cycleDuration = static_cast<long double>(
        duration) * (track.timing.autoReverse ? 2.0L : 1.0L);
    const long double activeDuration = track.timing.repeat.forever
        ? static_cast<long double>(UINT64_MAX)
        : cycleDuration * track.timing.repeat.count;
    const bool completed = duration == 0U ||
        (!track.timing.repeat.forever &&
         static_cast<long double>(localTime) >= activeDuration);

    double progress = 1.0;
    AnimationTime sampleTime = duration;
    if (!completed && duration != 0U) {
        long double within = std::fmod(
            static_cast<long double>(localTime),
            cycleDuration);
        if (track.timing.autoReverse &&
            within > static_cast<long double>(duration)) {
            within = cycleDuration - within;
        }
        within = std::max(
            0.0L,
            std::min(
                within,
                static_cast<long double>(duration)));
        sampleTime =
            static_cast<AnimationTime>(within);
        progress = static_cast<double>(sampleTime) /
            static_cast<double>(duration);
    } else if (completed && track.timing.autoReverse) {
        progress = 0.0;
        sampleTime = 0U;
    }

    if (!track.timeline) return false;
    Meta::PropertyValue value = track.timeline->GetCurrentPropertyValue(
        track.originValue, track.destinationValue, progress, sampleTime);
    if (value.IsUnset()) return false;

    const Meta::DependencyProperty* targetProperty =
        DependencyObjectAccess::PropertyRegistry(track.target).Find(
            track.property);
    if (targetProperty != nullptr &&
        targetProperty->ValueType() ==
            Brush::StaticTypeId() &&
        value.Type() == Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Meta::ValueCodec<Base::Color>::Decode(
                value);
        if (!color) return false;
        Base::Ref<Brush> brush =
            MakeSolidColorBrush(*color);
        if (!brush) return false;
        value = Meta::PropertyValue::FromObject(
            Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush)));
    }
    if (targetProperty != nullptr &&
        targetProperty->ValueType() ==
            Meta::TypeOf<Length>() &&
        value.Type() == Meta::TypeOf<double>()) {
        Base::Result<double> numeric =
            Meta::ValueCodec<double>::Decode(value);
        if (!numeric) return false;
        Base::Result<Meta::PropertyValue> length =
            Meta::ValueCodec<Length>::Encode(
                Length::Pixels(*numeric));
        if (!length) return false;
        value = std::move(*length);
    }
    if (UIElement* element = TryCast<UIElement>(track.target)) {
        if (track.property == UIElement::OpacityProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) element->SetAnimatedOpacity(*decoded);
        } else if (track.property == UIElement::VisibilityProperty.Handle()) {
            Base::Result<Visibility> decoded =
                Meta::ValueCodec<Visibility>::Decode(value);
            if (decoded) element->SetAnimatedVisibility(*decoded);
        }
    }
    if (FrameworkElement* framework = TryCast<FrameworkElement>(track.target)) {
        if (track.property == FrameworkElement::WidthProperty.Handle()) {
            Base::Result<Length> decoded = Meta::ValueCodec<Length>::Decode(value);
            if (decoded) framework->SetAnimatedWidth(*decoded);
        } else if (track.property == FrameworkElement::HeightProperty.Handle()) {
            Base::Result<Length> decoded = Meta::ValueCodec<Length>::Decode(value);
            if (decoded) framework->SetAnimatedHeight(*decoded);
        } else if (track.property == FrameworkElement::MinWidthProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMinWidth(*decoded);
        } else if (track.property == FrameworkElement::MinHeightProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMinHeight(*decoded);
        } else if (track.property == FrameworkElement::MaxWidthProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMaxWidth(*decoded);
        } else if (track.property == FrameworkElement::MaxHeightProperty.Handle()) {
            Base::Result<double> decoded = Meta::ValueCodec<double>::Decode(value);
            if (decoded) framework->SetAnimatedMaxHeight(*decoded);
        } else if (track.property == FrameworkElement::MarginProperty.Handle()) {
            Base::Result<Thickness> decoded =
                Meta::ValueCodec<Thickness>::Decode(value);
            if (decoded) framework->SetAnimatedMargin(*decoded);
        }
    }
    Base::Result<void> applied = values_->SetAnimationValue(
        *track.target, track.property, value);
    if (!applied) return false;
    track.valueApplied = true;
    ++diagnostics_.appliedValueCount;

    if (completed) {
        if (!track.completedCounted) {
            ++diagnostics_.completedCount;
            track.completedCounted = true;
        }
        if (track.timing.fillBehavior == FillBehavior::Stop) {
            ClearTrackValue(track);
            track.state = AnimationState::Stopped;
        } else {
            track.state = AnimationState::Filling;
        }
    }
    return true;
}

} // namespace Aero
