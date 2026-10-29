#include <Aero/Controls/Control.hpp>
#include <Aero/VisualStateManager.hpp>
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/templates/TemplateInstance.hpp"

#include <Aero/Value.hpp>
#include <Aero/PropertyPath.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include "gui/media/AnimationModel.hpp"
#include "gui/triggers/VisualStateManagerExecution.hpp"
#include <algorithm>
#include <new>
#include <utility>
#include <Aero/Triggers.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Layout.hpp>
#include "gui/core/EnumRegistration.hpp"
#include "gui/core/TypeRegistryCore.hpp"

namespace Aero::Controls {

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

// File-local VSM execution state (merged companion; not exported).
class VisualStateManagerState : public VisualStateManager {
public:
    VisualStateManagerState(
        EffectiveValueEngine& values,
        TemplateEngine& templates,
        Aero::AnimationEngine& animations,
        DependencyPropertyRegistry& properties) noexcept
        : values_(&values),
          templates_(&templates),
          animations_(&animations),
          properties_(&properties) {}

    bool GoToState(
        Control& control,
        Base::StringView groupName,
        Base::StringView stateName,
        bool useTransitions) noexcept;
    bool ClearState(
        Control& control,
        Base::StringView groupName) noexcept;
    std::uint32_t Clear(Control& control) noexcept;
    Base::StringView CurrentState(
        const Control& control,
        Base::StringView groupName) const noexcept;

private:
    struct ActiveGroup {
        std::uint64_t templateValue = 0U;
        Base::String groupName;
        Base::String stateName;
        Base::Vector<Aero::Media::Animation::Model::AnimationHandle> animations;
        std::uint32_t providerOrigin = 0U;
        std::uint32_t nextOrdinal = 0U;
    };

    struct TransitionValue {
        DependencyObject* target = nullptr;
        DependencyPropertyHandle property;
        PropertyValue from;
        PropertyValue to;
    };

    EffectiveValueEngine* values_ = nullptr;
    TemplateEngine* templates_ = nullptr;
    Aero::AnimationEngine* animations_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    Base::Vector<ActiveGroup> active_;
    std::uint32_t goToStateDepth_ = 0U;

    std::uint32_t FindActive(
        TemplateHandle handle,
        Base::StringView groupName) const noexcept;
    static const VisualStateGroupPlan* FindGroup(
        const ControlTemplate& plan,
        Base::StringView groupName) noexcept;
    static const VisualStatePlan* FindState(
        const VisualStateGroupPlan& group,
        Base::StringView stateName) noexcept;
    static const VisualTransitionPlan* FindTransition(
        const VisualStateGroupPlan& group,
        Base::StringView fromState,
        Base::StringView toState) noexcept;
    Base::Result<void> ApplyState(
        Control& control,
        TemplateHandle handle,
        const VisualStatePlan& state,
        ActiveGroup& active) noexcept;
    Base::Result<void> ClearStateValues(
        Control& control,
        TemplateHandle handle,
        const VisualStatePlan& state,
        ActiveGroup& active) noexcept;
    Base::Result<void> StartStateAnimations(
        Control& control,
        TemplateHandle handle,
        const VisualStatePlan& state,
        ActiveGroup& active,
        const Aero::Media::Animation::Model::TimelineTiming& parent = {}) noexcept;
    Base::Result<void> StartStoryboardAnimations(
        Control& control,
        TemplateHandle handle,
        Media::Animation::Storyboard& storyboard,
        ActiveGroup& active,
        const Aero::Media::Animation::Model::TimelineTiming& parent = {}) noexcept;
    Base::Result<void> CaptureTransitionValues(
        Control& control,
        TemplateHandle handle,
        const VisualStatePlan& next,
        Base::Vector<TransitionValue>& values) noexcept;
    Base::Result<void> CaptureStoryboardTransitionValues(
        Control& control,
        TemplateHandle handle,
        const VisualStatePlan& next,
        Base::Vector<TransitionValue>& values) noexcept;
    static Base::Result<void> CaptureStoryboardTimeline(
        Control& control,
        TemplateHandle handle,
        TemplateEngine& templates,
        DependencyPropertyRegistry& properties,
        Media::Animation::Timeline& timeline,
        Base::Vector<TransitionValue>& values,
        bool revertToBase) noexcept;
    Base::Result<void> CaptureStoryboardRevertToBase(
        Control& control,
        TemplateHandle handle,
        const VisualStatePlan& previous,
        Base::Vector<TransitionValue>& values) noexcept;
    Base::Result<void> StartTransitionAnimations(
        Control& control,
        TemplateHandle handle,
        const VisualTransitionPlan& transition,
        Base::Span<const TransitionValue> values,
        ActiveGroup& active) noexcept;
    void ClearStateAnimations(ActiveGroup& active) noexcept;
    void PruneStale() noexcept;
    void RemoveActiveAt(std::uint32_t index) noexcept;
    static PropertyValue BaseValueWithoutAnimation(
        DependencyObject& target,
        DependencyPropertyHandle property,
        DependencyPropertyRegistry& properties) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls {

PropertyValue VisualStateManagerState::BaseValueWithoutAnimation(
    DependencyObject& target,
    DependencyPropertyHandle property,
    DependencyPropertyRegistry& properties) noexcept {
    // Template-expanded values (Menu3D CircledArrow Fill.Opacity="0") are not
    // Local. ReadLocalValue is Unset and Brush.Opacity metadata defaults to 1,
    // so generated Unchecked transitions would tween 1→1 and leave the arrow
    // visible. Animation-base includes template/style/local, excluding HoldEnd.
    Base::Result<PropertyValue> base =
        (target).GetAnimationBaseValueInternal( property);
    if (base && !base.Value().IsUnset()) {
        return base.Value();
    }
    const PropertyValue local = target.ReadLocalValue(property);
    if (!local.IsUnset()) {
        return local;
    }
    const ::Aero::Meta::DependencyProperty* descriptor = properties.Find(property);
    if (descriptor == nullptr) {
        return PropertyValue::Unset();
    }
    const PropertyMetadata* metadata =
        descriptor->MetadataFor(target.RuntimeType());
    return metadata != nullptr
        ? metadata->defaultValue
        : PropertyValue::Unset();
}

} // namespace Aero::Controls

namespace Aero {
using Aero::Controls::TemplateEngine;
using Aero::Controls::TemplateHandle;
using namespace Aero::Media::Animation::Model;
using namespace ::Aero::Meta;
using namespace ::Aero::Media;
using namespace ::Aero::Controls;
namespace {


struct AnimationTarget {
    DependencyObject* object = nullptr;
    DependencyPropertyHandle property;
};

Base::Result<AnimationTarget> ResolveAnimationTarget(
    Control& control,
    TemplateHandle handle,
    const Media::Animation::Timeline& timeline,
    TemplateEngine& templates,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<PropertyValue> targetNameValue =
        timeline.GetValue(
            Media::Animation::Storyboard::TargetNameProperty.Handle());
    if (!targetNameValue) return targetNameValue.GetStatus();
    if (targetNameValue.Value().Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Storyboard target name is not a string");
    }
    const Base::StringView targetName = targetNameValue.Value().AsString();
    DependencyObject* target = targetName.Empty()
        ? static_cast<DependencyObject*>(&control)
        : templates.FindName(handle, targetName);
    if (target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "VisualState Storyboard target name was not found");
    }
    Base::Result<PropertyValue> authoredPathValue =
        timeline.GetValue(
            Media::Animation::Storyboard::TargetPropertyProperty.Handle());
    if (!authoredPathValue) {
        return authoredPathValue.GetStatus();
    }
    Base::StringView authoredPath;
    if (authoredPathValue.Value().Kind() == ValueKind::String) {
        authoredPath = authoredPathValue.Value().AsString();
    } else if (authoredPathValue.Value().Kind() == ValueKind::Object && authoredPathValue.Value().AsObject()) {
        auto* pp = TryCast<PropertyPath>(authoredPathValue.Value().AsObject().Get());
        if (pp != nullptr) {
            authoredPath = pp->Path();
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "VisualState Storyboard target property is not a string or PropertyPath");
        }
    } else {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Storyboard target property is not a string or PropertyPath");
    }
    Base::Result<ResolvedAnimationProperty> resolved =
        ResolveAnimationPropertyPath(*target, authoredPath, properties);
    if (!resolved) {
        return resolved.GetStatus();
    }
    return AnimationTarget{resolved.Value().target, resolved.Value().property};
}


} // namespace

} // namespace Aero

namespace Aero {
using Aero::Controls::TemplateEngine;
using Aero::Controls::TemplateHandle;
using namespace Aero::Media::Animation::Model;
using namespace ::Aero::Meta;
using namespace ::Aero::Media;
using namespace ::Aero::Controls;

Base::Ref<VisualStateManager>
Controls::VisualStateManagerExecution::Create(
    Meta::EffectiveValueEngine& values,
    ::Aero::Controls::TemplateEngine& templates,
    ::Aero::AnimationEngine& animations,
    Meta::DependencyPropertyRegistry& properties) noexcept {
    return Base::MakeRef<VisualStateManagerState>(
        values, templates, animations, properties);
}

} // namespace Aero

namespace Aero::Controls {
using Aero::Controls::TemplateEngine;
using Aero::Controls::TemplateHandle;
using namespace Aero::Media::Animation::Model;
using namespace ::Aero::Meta;
using namespace ::Aero::Media;




std::uint32_t VisualStateManagerState::FindActive(
    TemplateHandle handle,
    Base::StringView groupName) const noexcept {
    for (std::uint32_t index = 0U;
        index < active_.Size(); ++index) {
        if (active_[index].templateValue == handle.value &&
            active_[index].groupName.View() == groupName) {
            return index;
        }
    }
    return UINT32_MAX;
}

const VisualStateGroupPlan* VisualStateManagerState::FindGroup(
    const ControlTemplate& plan,
    Base::StringView groupName) noexcept {
    for (const VisualStateGroupPlan& group :
        FrameworkTemplateState::VisualStateGroups(plan)) {
        if (group.name.View() == groupName) return &group;
    }
    return nullptr;
}

const VisualStatePlan* VisualStateManagerState::FindState(
    const VisualStateGroupPlan& group,
    Base::StringView stateName) noexcept {
    for (const VisualStatePlan& state : group.states) {
        if (state.name.View() == stateName) return &state;
    }
    return nullptr;
}

const VisualTransitionPlan* VisualStateManagerState::FindTransition(
    const VisualStateGroupPlan& group,
    Base::StringView fromState,
    Base::StringView toState) noexcept {
    const VisualTransitionPlan* best = nullptr;
    std::uint32_t bestScore = 0U;
    for (const VisualTransitionPlan& transition :
         group.transitions) {
        if (!transition.from.Empty() &&
            transition.from.View() != fromState) {
            continue;
        }
        if (!transition.to.Empty() &&
            transition.to.View() != toState) {
            continue;
        }
        const std::uint32_t score =
            (!transition.from.Empty() ? 2U : 0U) +
            (!transition.to.Empty() ? 1U : 0U);
        if (best == nullptr || score > bestScore) {
            best = &transition;
            bestScore = score;
        }
    }
    return best;
}

Base::Result<void> VisualStateManagerState::ApplyState(
    Control& control,
    TemplateHandle handle,
    const VisualStatePlan& state,
    ActiveGroup& active) noexcept {
    for (const VisualStateSetterPlan& setter : state.setters) {
        DependencyObject* target = setter.targetName.Empty()
            ? static_cast<DependencyObject*>(&control)
            : templates_->FindName(handle, setter.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Visual state target name was not found");
        }
    }
    if (active.providerOrigin == 0U) {
        Base::Result<std::uint32_t> origin =
            values_->AllocateProviderOrigin();
        if (!origin) return origin.GetStatus();
        active.providerOrigin = origin.Value();
        active.nextOrdinal = 0U;
    }
    for (const VisualStateSetterPlan& setter : state.setters) {
        DependencyObject* target = setter.targetName.Empty()
            ? static_cast<DependencyObject*>(&control)
            : templates_->FindName(handle, setter.targetName.View());
        const PropertyProviderToken token{
            PropertyValueRank::VisualState,
            active.providerOrigin,
            active.nextOrdinal++};
        Base::Result<void> applied = values_->SetProviderContribution(
            *target, setter.property, token, setter.value);
        if (!applied) {
            static_cast<void>(ClearStateValues(control, handle, state, active));
            return applied.GetStatus();
        }
    }
    return {};
}

Base::Result<void> VisualStateManagerState::ClearStateValues(
    Control& control,
    TemplateHandle handle,
    const VisualStatePlan& state,
    ActiveGroup& active) noexcept {
    if (active.providerOrigin == 0U) {
        return {};
    }
    DependencyObject* seenTargets[32];
    std::uint32_t seenCount = 0U;

    for (const VisualStateSetterPlan& setter : state.setters) {
        DependencyObject* target = setter.targetName.Empty()
            ? static_cast<DependencyObject*>(&control)
            : templates_->FindName(handle, setter.targetName.View());
        if (target == nullptr) continue;

        bool alreadyCleared = false;
        for (std::uint32_t i = 0U; i < seenCount; ++i) {
            if (seenTargets[i] == target) {
                alreadyCleared = true;
                break;
            }
        }
        if (alreadyCleared) {
            continue;
        }
        if (seenCount < 32U) {
            seenTargets[seenCount++] = target;
        }

        Base::Result<std::uint32_t> cleared =
            values_->ClearProviderOrigin(
                *target, active.providerOrigin);
        if (!cleared) return cleared.GetStatus();
    }
    active.nextOrdinal = 0U;
    return {};
}

void VisualStateManagerState::RemoveActiveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != active_.Size()) {
        active_[index] =
            std::move(active_[active_.Size() - 1U]);
    }
    active_.PopBack();
}

void VisualStateManagerState::PruneStale() noexcept {
    for (std::uint32_t index = active_.Size();
        index > 0U; --index) {
        TemplateHandle handle;
        handle.value = active_[index - 1U].templateValue;
        if (templates_->AppliedTemplate(handle) == nullptr) {
            RemoveActiveAt(index - 1U);
        }
    }
}

void VisualStateManagerState::ClearStateAnimations(
    ActiveGroup& active) noexcept {
    for (Aero::Media::Animation::Model::AnimationHandle animation :
         active.animations) {
        animations_->Remove(animation);
    }
    active.animations.Clear();
}

Base::Result<void> VisualStateManagerState::StartStateAnimations(
    Control& control,
    TemplateHandle handle,
    const VisualStatePlan& state,
    ActiveGroup& active,
    const Aero::Media::Animation::Model::TimelineTiming& parent) noexcept {
    if (!state.storyboard) return {};
    return StartStoryboardAnimations(
        control,
        handle,
        *state.storyboard,
        active,
        parent);
}

Base::Result<void> VisualStateManagerState::StartStoryboardAnimations(
    Control& control,
    TemplateHandle handle,
    Media::Animation::Storyboard& root,
    ActiveGroup& active,
    const Aero::Media::Animation::Model::TimelineTiming& parent) noexcept {
    const auto startTimeline =
        [&](const auto& self,
            Media::Animation::Timeline& timeline,
            const Aero::Media::Animation::Model::TimelineTiming& parent)
            -> Base::Result<void> {
        if (timeline.RuntimeType() ==
            Media::Animation::Storyboard::StaticTypeId()) {
            auto& storyboard =
                static_cast<Media::Animation::Storyboard&>(timeline);
            Aero::Media::Animation::Model::TimelineTiming effectiveParent = parent;
            if (storyboard.GetRepeatBehavior().IsForever()) {
                effectiveParent.beginTimeMicroseconds = 0U;
            }
            const Aero::Media::Animation::Model::TimelineTiming timing =
                ComposeTiming(storyboard, effectiveParent);
            for (const Base::Ref<Media::Animation::Timeline>& child :
                 storyboard.GetTimelines()) {
                if (!child) continue;
                Base::Result<void> started =
                    self(self, *child, timing);
                if (!started) return started.GetStatus();
            }
            return {};
        }

        Base::Result<AnimationTarget> resolved =
            ResolveAnimationTarget(
                control, handle, timeline,
                *templates_, *properties_);
        if (!resolved) {
            return resolved.GetStatus();
        }

        Aero::Media::Animation::Model::TimelineTiming effectiveParent = parent;
        if (timeline.GetRepeatBehavior().IsForever()) {
            effectiveParent.beginTimeMicroseconds = 0U;
        }
        const Aero::Media::Animation::Model::TimelineTiming composedTiming =
            ComposeTiming(timeline, effectiveParent);

        Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
            animations_->BeginTimeline(
                *resolved.Value().object,
                resolved.Value().property,
                timeline,
                composedTiming);
        if (!started) return started.GetStatus();
        active.animations.PushBack(
            started.Value());
        return {};
    };

    Aero::Media::Animation::Model::TimelineTiming rootTiming;
    rootTiming = parent;
    Base::Result<void> started =
        startTimeline(
            startTimeline,
            root,
            rootTiming);
    if (!started) {
        ClearStateAnimations(active);
        return started.GetStatus();
    }
    return {};
}

Base::Result<void> VisualStateManagerState::CaptureTransitionValues(
    Control& control,
    TemplateHandle handle,
    const VisualStatePlan& next,
    Base::Vector<TransitionValue>& output) noexcept {
    output.Clear();
    for (const VisualStateSetterPlan& setter :
         next.setters) {
        DependencyObject* target = setter.targetName.Empty()
            ? static_cast<DependencyObject*>(&control)
            : templates_->FindName(
                handle,
                setter.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "VisualTransition target name was not found");
        }
        Base::Result<PropertyValue> current =
            target->GetValue(setter.property);
        if (!current) return current.GetStatus();
        TransitionValue value;
        value.target = target;
        value.property = setter.property;
        value.from = current.Value();
        value.to = setter.value;
        output.PushBack(std::move(value));
    }
    return {};
}



// Walks a storyboard (recursively) and, for every animatable leaf timeline,
// resolves its real target and captures the current value (from) plus the
// value the animation drives toward (to). This lets a generated
// VisualTransition (GeneratedDuration with no explicit Storyboard) fade
// between the current and destination values even when the destination
// VisualState expresses its effect through a Storyboard rather than Setters.
// revertToBase uses the local/default value as `to` so empty Unchecked
// states can fade HoldEnd clocks back to rest.
Base::Result<void> VisualStateManagerState::CaptureStoryboardTimeline(
    Control& control,
    TemplateHandle handle,
    TemplateEngine& templates,
    DependencyPropertyRegistry& properties,
    Media::Animation::Timeline& timeline,
    Base::Vector<TransitionValue>& values,
    bool revertToBase) noexcept {
    if (timeline.RuntimeType() ==
        Media::Animation::Storyboard::StaticTypeId()) {
        auto& storyboard =
            static_cast<Media::Animation::Storyboard&>(timeline);
        for (const Base::Ref<Media::Animation::Timeline>& child :
             storyboard.GetTimelines()) {
            if (!child) continue;
            Base::Result<void> captured =
                CaptureStoryboardTimeline(
                    control, handle, templates, properties,
                    *child, values, revertToBase);
            if (!captured) return captured.GetStatus();
        }
        return {};
    }

    if (timeline.GetRepeatBehavior().IsForever()) {
        return {};
    }

    Base::Result<AnimationTarget> resolved =
        ResolveAnimationTarget(
            control, handle, timeline, templates, properties);
    if (!resolved) return resolved.GetStatus();

    DependencyObject* target = resolved.Value().object;
    const DependencyPropertyHandle property =
        resolved.Value().property;
    for (std::uint32_t index = 0U; index < values.Size(); ++index) {
        if (values[index].target == target &&
            values[index].property == property) {
            return {};
        }
    }
    Base::Result<PropertyValue> from =
        target->GetValue(property);
    if (!from) return from.GetStatus();

    if (revertToBase) {
        const PropertyValue to = VisualStateManagerState::BaseValueWithoutAnimation(
            *target, property, properties);
        if (to.IsUnset()) {
            return {};
        }
        values.PushBack(
            TransitionValue{
                target,
                property,
                from.Value(),
                to});
        return {};
    }
    if (timeline.RuntimeType() ==
            Media::Animation::DoubleAnimationUsingKeyFrames::
                StaticTypeId()) {
        auto& authored = static_cast<
            Media::Animation::DoubleAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Aero::Media::Animation::Model::DoubleKeyFrame> frames =
            Aero::Media::Animation::BuildSortedKeyFrames<
                Aero::Media::Animation::Model::DoubleKeyFrame>(
                authored,
                ComposeTiming(authored, {}).durationMicroseconds);
        if (frames.Empty()) {
            return {};
        }
        Base::Result<PropertyValue> to =
            ValueCodec<double>::Encode(frames.Back().value);
        if (!to) return to.GetStatus();
        values.PushBack(
            TransitionValue{
                target,
                property,
                from.Value(),
                std::move(to).Value()});
        return {};
    }
    if (timeline.RuntimeType() ==
            Media::Animation::ColorAnimationUsingKeyFrames::
                StaticTypeId()) {
        auto& authored = static_cast<
            Media::Animation::ColorAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Aero::Media::Animation::Model::ColorKeyFrame> frames =
            Aero::Media::Animation::BuildSortedKeyFrames<
                Aero::Media::Animation::Model::ColorKeyFrame>(
                authored,
                ComposeTiming(authored, {}).durationMicroseconds);
        if (frames.Empty()) {
            return {};
        }
        Base::Result<PropertyValue> to =
            ValueCodec<Base::Color>::Encode(frames.Back().value);
        if (!to) return to.GetStatus();
        values.PushBack(
            TransitionValue{
                target,
                property,
                from.Value(),
                std::move(to).Value()});
        return {};
    }
    if (timeline.RuntimeType() ==
            Media::Animation::DoubleAnimation::StaticTypeId()) {
        auto& authored =
            static_cast<Media::Animation::DoubleAnimation&>(timeline);
        if (!authored.GetHasTo()) {
            return {};
        }
        Base::Result<PropertyValue> to =
            ValueCodec<double>::Encode(authored.GetTo());
        if (!to) return to.GetStatus();
        values.PushBack(
            TransitionValue{
                target,
                property,
                from.Value(),
                std::move(to).Value()});
        return {};
    }
    if (timeline.RuntimeType() ==
            Media::Animation::ColorAnimation::StaticTypeId()) {
        auto& authored =
            static_cast<Media::Animation::ColorAnimation&>(timeline);
        Base::Result<PropertyValue> to =
            ValueCodec<Base::Color>::Encode(authored.GetTo());
        if (!to) return to.GetStatus();
        values.PushBack(
            TransitionValue{
                target,
                property,
                from.Value(),
                std::move(to).Value()});
        return {};
    }
    if (timeline.RuntimeType() ==
            Media::Animation::PointAnimationUsingKeyFrames::
                StaticTypeId()) {
        auto& authored = static_cast<
            Media::Animation::PointAnimationUsingKeyFrames&>(timeline);
        Base::Vector<Aero::Media::Animation::Model::PointKeyFrame> frames =
            Aero::Media::Animation::BuildSortedKeyFrames<
                Aero::Media::Animation::Model::PointKeyFrame>(
                authored,
                ComposeTiming(authored, {}).durationMicroseconds);
        if (frames.Empty()) {
            return {};
        }
        Base::Result<PropertyValue> to =
            ValueCodec<Base::Point>::Encode(frames.Back().value);
        if (!to) return to.GetStatus();
        values.PushBack(
            TransitionValue{
                target,
                property,
                from.Value(),
                std::move(to).Value()});
        return {};
    }
    if (timeline.RuntimeType() ==
            Media::Animation::PointAnimation::StaticTypeId()) {
        auto& authored =
            static_cast<Media::Animation::PointAnimation&>(timeline);
        Base::Result<PropertyValue> to =
            ValueCodec<Base::Point>::Encode(authored.GetTo());
        if (!to) return to.GetStatus();
        values.PushBack(
            TransitionValue{
                target,
                property,
                from.Value(),
                std::move(to).Value()});
        return {};
    }
    // Unsupported timeline kind for a generated transition: leave it to the
    // state's own Storyboard (no fade for this property).
    return {};
}

Base::Result<void>
VisualStateManagerState::CaptureStoryboardTransitionValues(
    Control& control,
    TemplateHandle handle,
    const VisualStatePlan& next,
    Base::Vector<TransitionValue>& values) noexcept {
    if (!next.storyboard) return {};
    const auto& storyboard = *next.storyboard;
    for (const Base::Ref<Media::Animation::Timeline>& child :
         storyboard.GetTimelines()) {
        if (!child) continue;
        Base::Result<void> captured =
            CaptureStoryboardTimeline(
                control, handle, *templates_, *properties_,
                *child, values, false);
        if (!captured) return captured.GetStatus();
    }
    return {};
}

Base::Result<void>
VisualStateManagerState::CaptureStoryboardRevertToBase(
    Control& control,
    TemplateHandle handle,
    const VisualStatePlan& previous,
    Base::Vector<TransitionValue>& values) noexcept {
    if (!previous.storyboard) return {};
    return CaptureStoryboardTimeline(
        control,
        handle,
        *templates_,
        *properties_,
        *previous.storyboard,
        values,
        true);
}

Base::Result<void> VisualStateManagerState::StartTransitionAnimations(
    Control& control,
    TemplateHandle handle,
    const VisualTransitionPlan& transition,
    Base::Span<const TransitionValue> values,
    ActiveGroup& active) noexcept {
    if (transition.storyboard) {
        return StartStoryboardAnimations(
            control,
            handle,
            *transition.storyboard,
            active);
    }
    if (transition.generatedDurationMicroseconds == 0U) {
        return {};
    }
    const EasingFunction easing =
        transition.generatedEasingFunction
        ? Aero::Media::Animation::Easing(
                *transition.generatedEasingFunction)
        : EasingFunction{};
    for (const TransitionValue& value : values) {
        if (value.target == nullptr ||
            !value.property.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Generated VisualTransition target is invalid");
        }
        Base::Result<AnimationHandle> started =
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Generated VisualTransition value type is unsupported");
        if (value.from.Kind() == ValueKind::Double ||
            value.from.Type() == Meta::TypeOf<Length>()) {
            Base::Result<double> fromDouble =
                DecodeDoubleOrLength(value.from);
            Base::Result<double> toDouble =
                DecodeDoubleOrLength(value.to);
            if (fromDouble && toDouble) {
                if (fromDouble.Value() == toDouble.Value()) {
                    continue;
                }
                DoubleAnimation animation;
                animation.from = fromDouble.Value();
                animation.to = toDouble.Value();
                animation.timing.durationMicroseconds =
                    transition.generatedDurationMicroseconds;
                animation.timing.fillBehavior =
                    FillBehavior::HoldEnd;
                animation.easing = easing;
                started = animations_->Begin(
                    *value.target,
                    value.property,
                    animation);
            }
        } else if (value.from.Type() == Meta::TypeOf<Base::Color>()) {
            Base::Result<Base::Color> fromColor =
                ValueCodec<Base::Color>::Decode(
                    value.from);
            Base::Result<Base::Color> toColor =
                ValueCodec<Base::Color>::Decode(
                    value.to);
            if (!fromColor || !toColor) {
                continue;
            }
            if (fromColor.Value() == toColor.Value()) {
                continue;
            }
            ColorAnimation animation;
            animation.from = fromColor.Value();
            animation.to = toColor.Value();
            animation.timing.durationMicroseconds =
                transition.generatedDurationMicroseconds;
            animation.timing.fillBehavior =
                FillBehavior::HoldEnd;
            animation.easing = easing;
            started = animations_->Begin(
                *value.target,
                value.property,
                animation);
        } else if (value.from.Type() == Meta::TypeOf<Base::Point>()) {
            Base::Result<Base::Point> fromPoint =
                ValueCodec<Base::Point>::Decode(
                    value.from);
            Base::Result<Base::Point> toPoint =
                ValueCodec<Base::Point>::Decode(
                    value.to);
            if (!fromPoint || !toPoint) {
                continue;
            }
            if (fromPoint.Value().x == toPoint.Value().x &&
                fromPoint.Value().y == toPoint.Value().y) {
                continue;
            }
            PointAnimation animation;
            animation.from = fromPoint.Value();
            animation.to = toPoint.Value();
            animation.timing.durationMicroseconds =
                transition.generatedDurationMicroseconds;
            animation.timing.fillBehavior =
                FillBehavior::HoldEnd;
            animation.easing = easing;
            started = animations_->Begin(
                *value.target,
                value.property,
                animation);
        } else {
            continue;
        }
        if (!started) {
            ClearStateAnimations(active);
            return started.GetStatus();
        }
        active.animations.PushBack(
                started.Value());
    }
    return {};
}

bool VisualStateManagerState::GoToState(
    Control& control,
    Base::StringView groupName,
    Base::StringView stateName,
    bool useTransitions) noexcept {
    if (stateName.Empty()) {
        return false;
    }
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    const ControlTemplate* plan =
        templates_->AppliedTemplate(handle);
    if (!handle.IsValid() || plan == nullptr) {
        return false;
    }
    const VisualStateGroupPlan* group = groupName.Empty() ? nullptr : FindGroup(*plan, groupName);
    if (group == nullptr && groupName.Empty()) {
        for (const VisualStateGroupPlan& candidate : FrameworkTemplateState::VisualStateGroups(*plan)) {
            if (FindState(candidate, stateName) != nullptr) { group = &candidate; groupName = candidate.name.View(); break; }
        }
    }
    const VisualStatePlan* next = group != nullptr ? FindState(*group, stateName) : nullptr;
    if (next == nullptr) {
        // Support WPF / Silverlight / Aero naming aliases
        Base::StringView fallbackName;
        if (stateName == "PointerOver") fallbackName = "MouseOver";
        else if (stateName == "MouseOver") fallbackName = "PointerOver";
        else if (stateName == "Selected") fallbackName = "SelectedUnfocused";
        else if (stateName == "SelectedUnfocused") fallbackName = "Selected";

        if (!fallbackName.Empty()) {
            if (group != nullptr) {
                next = FindState(*group, fallbackName);
            }
            if (next == nullptr && groupName.Empty()) {
                for (const VisualStateGroupPlan& candidate : FrameworkTemplateState::VisualStateGroups(*plan)) {
                    if (FindState(candidate, fallbackName) != nullptr) {
                        group = &candidate;
                        groupName = candidate.name.View();
                        next = FindState(candidate, fallbackName);
                        break;
                    }
                }
            }
            if (next != nullptr) {
                stateName = fallbackName;
            }
        }
    }
    if (next == nullptr) {
        return false;
    }
    if (goToStateDepth_ != 0U) {
        return false;
    }
    struct DepthGuard {
        std::uint32_t& depth;
        explicit DepthGuard(std::uint32_t& value) noexcept : depth(value) {
            ++depth;
        }
        ~DepthGuard() noexcept { --depth; }
        DepthGuard(const DepthGuard&) = delete;
        DepthGuard& operator=(const DepthGuard&) = delete;
    } depthGuard(goToStateDepth_);

    std::uint32_t activeIndex = FindActive(handle, groupName);
    if (activeIndex != UINT32_MAX &&
        active_[activeIndex].stateName.View() == stateName) {
        return false;
    }
    Base::String nextGroup;
    nextGroup.Assign(groupName);
    Base::String nextName;
    nextName.Assign(stateName);
    bool addedRecord = false;
    if (activeIndex == UINT32_MAX) {
        if (!active_.Empty()) {
            PruneStale();
            activeIndex = FindActive(handle, groupName);
        }
        if (activeIndex == UINT32_MAX) {
            ActiveGroup active;
            active.templateValue = handle.value;
            active.groupName = std::move(nextGroup);
            active_.PushBack(std::move(active));
            activeIndex = active_.Size() - 1U;
            addedRecord = true;
        }
    }

    const VisualStatePlan* previous = nullptr;
    if (!active_[activeIndex].stateName.Empty()) {
        previous = FindState(
            *group, active_[activeIndex].stateName.View());
    }
    const Base::StringView previousName =
        active_[activeIndex].stateName.View();
    const VisualTransitionPlan* transition =
        useTransitions
        ? FindTransition(
              *group,
              previousName,
              stateName)
        : nullptr;
    Base::Vector<TransitionValue>
        transitionValues;
    if (transition != nullptr &&
        transition->generatedDurationMicroseconds != 0U &&
        !transition->storyboard) {
        Base::Result<void> captured =
            CaptureTransitionValues(
                control,
                handle,
                *next,
                transitionValues);
        if (!captured) {
            if (addedRecord) {
                RemoveActiveAt(activeIndex);
            }
            return false;
        }
        captured = CaptureStoryboardTransitionValues(
            control,
            handle,
            *next,
            transitionValues);
        if (!captured) {
            if (addedRecord) {
                RemoveActiveAt(activeIndex);
            }
            return false;
        }
        // Empty destination states (Menu3D CheckStates Unchecked) have no
        // storyboard/setters. WPF generated VisualTransitions still tween
        // the previous storyboard's properties back to their base values,
        // otherwise HoldEnd leaves CircledArrow Fill.Opacity at 1.
        if (previous != nullptr && previous->storyboard) {
            captured = CaptureStoryboardRevertToBase(
                control,
                handle,
                *previous,
                transitionValues);
            if (!captured) {
                if (addedRecord) {
                    RemoveActiveAt(activeIndex);
                }
                return false;
            }
        }
    } else if (previous != nullptr && previous->storyboard && !next->storyboard) {
        // Bug 2.2: When there is no generated duration or no transition, capture revert to base
        // so HoldEnd animation values from previous state get restored to base values.
        Base::Result<void> captured = CaptureStoryboardRevertToBase(
            control,
            handle,
            *previous,
            transitionValues);
        if (!captured) {
            if (addedRecord) {
                RemoveActiveAt(activeIndex);
            }
            return false;
        }
    }
    if (previous != nullptr) {
        ClearStateAnimations(active_[activeIndex]);
        Base::Result<void> cleared =
            ClearStateValues(control, handle, *previous, active_[activeIndex]);
        if (!cleared) {
            if (addedRecord) RemoveActiveAt(activeIndex);
            return false;
        }
        if (!next->storyboard && (transition == nullptr || transition->generatedDurationMicroseconds == 0U)) {
            for (const TransitionValue& value : transitionValues) {
                if (value.target == nullptr ||
                    !value.property.IsValid() ||
                    value.to.IsUnset()) {
                    continue;
                }
                Base::Result<void> restored = values_->SetAnimationValue(
                    *value.target, value.property, value.to);
                if (!restored) {
                    if (addedRecord) RemoveActiveAt(activeIndex);
                    return false;
                }
            }
            if (!transitionValues.Empty() && !values_->IsFlushing()) {
                Base::Result<std::uint32_t> flushed = values_->Flush();
                if (!flushed) {
                    if (addedRecord) RemoveActiveAt(activeIndex);
                    return false;
                }
            }
        }
    }
    Base::Result<void> applied = ApplyState(control, handle, *next, active_[activeIndex]);
    if (!applied) {
        if (previous != nullptr) {
            static_cast<void>(ApplyState(control, handle, *previous, active_[activeIndex]));
        }
        if (addedRecord) RemoveActiveAt(activeIndex);
        return false;
    }
    Base::Result<void> animated =
        Base::Result<void>();
    if (transition != nullptr) {
        animated = StartTransitionAnimations(
            control,
            handle,
            *transition,
            transitionValues.AsSpan(),
            active_[activeIndex]);
    }
    Aero::Media::Animation::Model::TimelineTiming stateTiming;
    if (animated && transition != nullptr) {
        stateTiming.beginTimeMicroseconds =
            transition->generatedDurationMicroseconds;
        if (transition->storyboard) {
            stateTiming.beginTimeMicroseconds =
                std::max(
                    stateTiming.beginTimeMicroseconds,
                    Aero::Media::Animation::Timing(
                        *transition->storyboard).durationMicroseconds);
        }
    }
    if (animated) {
        animated = StartStateAnimations(
            control, handle, *next,
            active_[activeIndex],
            stateTiming);
    }
    if (!animated) {

        static_cast<void>(
            ClearStateAnimations(
                active_[activeIndex]));
        static_cast<void>(
            ClearStateValues(control, handle, *next, active_[activeIndex]));
        if (previous != nullptr) {
            static_cast<void>(
                ApplyState(control, handle, *previous, active_[activeIndex]));
            static_cast<void>(
                StartStateAnimations(
                    control, handle, *previous,
                    active_[activeIndex]));
        }
        if (addedRecord) RemoveActiveAt(activeIndex);
        return false;
    }

    active_[activeIndex].stateName = std::move(nextName);
    return true;
}

bool VisualStateManagerState::ClearState(
    Control& control,
    Base::StringView groupName) noexcept {
    PruneStale();
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    if (!handle.IsValid()) return false;
    const std::uint32_t activeIndex =
        FindActive(handle, groupName);
    if (activeIndex == UINT32_MAX) return false;
    const ControlTemplate* plan =
        templates_->AppliedTemplate(handle);
    const VisualStateGroupPlan* group = plan != nullptr
        ? FindGroup(*plan, groupName) : nullptr;
    const VisualStatePlan* state = group != nullptr
        ? FindState(*group, active_[activeIndex].stateName.View())
        : nullptr;
    if (state != nullptr) {
        ClearStateAnimations(active_[activeIndex]);
        static_cast<void>(
            ClearStateValues(control, handle, *state, active_[activeIndex]));
    }
    RemoveActiveAt(activeIndex);
    return true;
}

std::uint32_t VisualStateManagerState::Clear(
    Control& control) noexcept {
    PruneStale();
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    if (!handle.IsValid()) return 0U;
    std::uint32_t clearedCount = 0U;
    for (std::uint32_t index = active_.Size();
        index > 0U; --index) {
        if (active_[index - 1U].templateValue != handle.value) {
            continue;
        }
        if (ClearState(
            control, active_[index - 1U].groupName.View())) {
            ++clearedCount;
        }
    }
    return clearedCount;
}

Base::StringView VisualStateManagerState::CurrentState(
    const Control& control,
    Base::StringView groupName) const noexcept {
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    if (!handle.IsValid()) return {};
    const std::uint32_t index =
        FindActive(handle, groupName);
    return index != UINT32_MAX
        ? active_[index].stateName.View()
        : Base::StringView{};
}

} // namespace Aero::Controls

namespace Aero {

bool VisualStateManager::GoToState(
    Controls::Control& control,
    Base::StringView stateName,
    bool useTransitions) noexcept {
    auto* manager = static_cast<VisualStateManagerState*>(
        ElementTree::VisualStatesOf(control));
    if (manager == nullptr) return false;
    return manager->GoToState(
        control, {}, stateName, useTransitions);
}

VisualStateManager::~VisualStateManager() noexcept = default;

bool Controls::VisualStateManagerExecution::GoToState(
    VisualStateManager& manager,
    Controls::Control& control,
    Base::StringView groupName,
    Base::StringView stateName,
    bool useTransitions) noexcept {
    auto& runtime = static_cast<VisualStateManagerState&>(manager);
    return runtime.GoToState(control, groupName, stateName, useTransitions);
}

bool Controls::VisualStateManagerExecution::ClearState(
    VisualStateManager& manager,
    Controls::Control& control,
    Base::StringView groupName) noexcept {
    auto& runtime = static_cast<VisualStateManagerState&>(manager);
    return runtime.ClearState(control, groupName);
}

std::uint32_t Controls::VisualStateManagerExecution::Clear(
    VisualStateManager& manager,
    Controls::Control& control) noexcept {
    auto& runtime = static_cast<VisualStateManagerState&>(manager);
    return runtime.Clear(control);
}

Base::StringView Controls::VisualStateManagerExecution::CurrentState(
    const VisualStateManager& manager,
    const Controls::Control& control,
    Base::StringView groupName) noexcept {
    auto& runtime = static_cast<const VisualStateManagerState&>(manager);
    return runtime.CurrentState(control, groupName);
}

} // namespace Aero

// VisualState* metadata registration (who-defines-registers).
namespace Aero {
namespace {

using namespace ::Aero::Meta;
using namespace ::Aero::Media;

void AddGroupState(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualState::StaticTypeId()) {
        return;
    }
    static_cast<VisualStateGroup&>(object).AddState(
        Base::Ref<VisualState>::FromBorrowed(
            *static_cast<VisualState*>(value.Get())));
}

void ClearGroupStates(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(object).ClearStates();
}

void AddGroupTransition(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualTransition::StaticTypeId()) {
        return;
    }
    static_cast<VisualStateGroup&>(object).AddTransition(
        Base::Ref<VisualTransition>::FromBorrowed(
            *static_cast<VisualTransition*>(value.Get())));
}

void ClearGroupTransitions(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(
        object).ClearTransitions();
}

void AddStateContent(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    auto& state =
        static_cast<VisualState&>(object);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        state.AddSetter(value);
        return;
    }
    if (value->RuntimeType() ==
        Media::Animation::Storyboard::StaticTypeId()) {
        state.SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(value.Get())));
        return;
    }
    return;
}

void ClearStateContent(
    Base::Object& object,
    void*) noexcept {
    auto& state =
        static_cast<VisualState&>(object);
    state.ClearSetters();
    state.SetStoryboard({});
}

void SetTransitionStoryboard(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            Media::Animation::Storyboard::StaticTypeId()) {
        return;
    }
    static_cast<VisualTransition&>(
        object).SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(
                    value.Get())));
}

void ClearTransitionStoryboard(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualTransition&>(object).SetStoryboard({});
}

} // namespace

Base::Result<void> PopulateVisualStateMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace ::Aero::Meta;
    using namespace ::Aero::Media;
    Base::Result<void> status;
    status = Meta::Register<VisualStateGroupCollection>(
        context, TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();

    auto visualStateManager =
        Meta::Register<VisualStateManager>(
            context, TypeFlags::Abstract);
    visualStateManager
        .Property(
            VisualStateManager::VisualStateGroupsProperty,
            FrameworkPropertyMetadata(
                Base::Ref<VisualStateGroupCollection>{})
                .Structural());
    status = visualStateManager.Result();
    if (!status) return status.GetStatus();

    auto stateGroup =
        Meta::Register<VisualStateGroup>(context);
    stateGroup
        .Property(
            "Name",
            &VisualStateGroup::GetName,
            &VisualStateGroup::SetName)
        .Content<VisualState>(
            "States",
            ContentKind::Collection,
            &AddGroupState,
            &ClearGroupStates)
        .Collection<VisualTransition>(
            "Transitions",
            &AddGroupTransition,
            &ClearGroupTransitions)
        .Factory();
    status = stateGroup.Result();
    if (!status) return status.GetStatus();

    auto state = Meta::Register<VisualState>(context);
    state
        .Property(
            "Name",
            &VisualState::GetName,
            &VisualState::SetName)
        .Property<
            Base::Ref<Media::Animation::Storyboard>,
            &VisualState::GetStoryboard,
            &VisualState::SetStoryboard>(
            "Storyboard",
            PropertyFlags::Structural)
        .Content<Base::Object>(
            "Content",
            ContentKind::Collection,
            &AddStateContent,
            &ClearStateContent)
        .Factory();
    status = state.Result();
    if (!status) return status.GetStatus();

    auto transition =
        Meta::Register<VisualTransition>(context);
    transition
        .Property(
            "From",
            &VisualTransition::GetFrom,
            &VisualTransition::SetFrom)
        .Property(
            "To",
            &VisualTransition::GetTo,
            &VisualTransition::SetTo)
        .Property(
            "GeneratedDuration",
            &VisualTransition::GetGeneratedDuration,
            &VisualTransition::SetGeneratedDuration)
        .Property<
            Base::Ref<Media::Animation::EasingFunctionBase>,
            &VisualTransition::GetGeneratedEasingFunction,
            &VisualTransition::SetGeneratedEasingFunction>(
            "GeneratedEasingFunction",
            PropertyFlags::Structural)
        .Content<Media::Animation::Storyboard>(
            "Storyboard",
            ContentKind::Single,
            &SetTransitionStoryboard,
            &ClearTransitionStoryboard)
        .Factory();
    status = transition.Result();
    return status;
}

} // namespace Aero
