#include <Aero/Controls/Templates.hpp>

#include <Aero/Core/Metadata/ValueCodec.hpp>
#include <Aero/Media/Transforms.hpp>

#include <algorithm>
#include <utility>
#include "RuntimeManagers.hpp"
#include "../ui/RuntimeManagers.hpp"

namespace Aero::Controls {
namespace {

struct AnimationTarget final {
    DependencyObject* object = nullptr;
    DependencyPropertyHandle property;
};

Base::StringView NormalizePropertyPath(
    Base::StringView path) noexcept {
    if (path.SizeBytes() >= 2U &&
        path[0] == '(' &&
        path[path.SizeBytes() - 1U] == ')') {
        return path.Substr(1U, path.SizeBytes() - 2U);
    }
    return path;
}

Base::Result<AnimationTarget> ResolveAnimationTarget(
    Control& control,
    TemplateHandle handle,
    const Media::Animation::Timeline& timeline,
    TemplateManager& templates,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<PropertyValue> targetNameValue =
        timeline.GetValue(
            Media::Animation::Storyboard::TargetNameProperty.Handle());
    if (!targetNameValue) return targetNameValue.GetStatus();
    if (targetNameValue.Value().Kind() !=
        ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Storyboard target name is not a string");
    }
    const Base::StringView targetName =
        targetNameValue.Value().AsString();
    DependencyObject* target = targetName.Empty()
        ? static_cast<DependencyObject*>(&control)
        : templates.FindName(
              handle, targetName);
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
    if (authoredPathValue.Value().Kind() !=
        ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Storyboard target property is not a string");
    }
    const Base::StringView authoredPath =
        authoredPathValue.Value().AsString();
    if (authoredPath.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Storyboard target property is required");
    }
    Base::StringView path =
        authoredPath;
    bool compoundParenthesizedPath = false;
    for (std::uint32_t index = 0U;
         index + 1U < path.SizeBytes(); ++index) {
        if (path[index] == ')' &&
            (path[index + 1U] == '.' ||
             path[index + 1U] == '[')) {
            compoundParenthesizedPath = true;
            break;
        }
    }
    if (!compoundParenthesizedPath) {
        path = NormalizePropertyPath(path);
    }

    std::uint32_t indexedOpen = UINT32_MAX;
    std::uint32_t indexedClose = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < path.SizeBytes(); ++index) {
        if (path[index] == '[' &&
            indexedOpen == UINT32_MAX) {
            indexedOpen = index;
        } else if (path[index] == ']' &&
                   indexedOpen != UINT32_MAX) {
            indexedClose = index;
            break;
        }
    }
    bool nestedTargetResolved = false;
    if (indexedOpen != UINT32_MAX) {
        if (indexedClose == UINT32_MAX ||
            indexedClose == indexedOpen + 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "VisualState indexed Storyboard path has an invalid index");
        }
        std::uint64_t parsedIndex = 0U;
        for (std::uint32_t index = indexedOpen + 1U;
             index < indexedClose; ++index) {
            if (path[index] < '0' ||
                path[index] > '9') {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "VisualState indexed Storyboard path index must be numeric");
            }
            parsedIndex =
                parsedIndex * 10U +
                static_cast<std::uint64_t>(
                    path[index] - '0');
            if (parsedIndex > UINT32_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "VisualState indexed Storyboard path index is too large");
            }
        }
        const Base::StringView beforeIndex =
            path.Substr(0U, indexedOpen);
        Base::StringView terminalPath =
            path.Substr(
                indexedClose + 1U,
                path.SizeBytes() -
                    indexedClose - 1U);
        if (!terminalPath.Empty() &&
            terminalPath[0] == '.') {
            terminalPath = terminalPath.Substr(
                1U,
                terminalPath.SizeBytes() - 1U);
        }
        terminalPath =
            NormalizePropertyPath(terminalPath);
        const bool transformChildren =
            beforeIndex ==
                Base::StringView(
                    "(UIElement.RenderTransform).(TransformGroup.Children)") ||
            beforeIndex ==
                Base::StringView(
                    "RenderTransform.Children") ||
            beforeIndex ==
                Base::StringView(
                    "(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
            beforeIndex ==
                Base::StringView(
                    "LayoutTransform.Children") ||
            beforeIndex ==
                Base::StringView(
                    "(TransformGroup.Children)");
        if (!transformChildren) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "VisualState indexed Storyboard collection is not supported");
        }

        Base::Ref<Transform> transform;
        if (beforeIndex ==
                Base::StringView(
                    "(TransformGroup.Children)") &&
            properties.Types().IsDerivedFrom(
                target->RuntimeType(),
                TransformGroup::StaticTypeId())) {
            transform =
                Base::Ref<Transform>::TryFromBorrowed(
                    *static_cast<Transform*>(target));
        } else {
            const bool layoutPath =
                beforeIndex ==
                    Base::StringView(
                        "(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
                beforeIndex ==
                    Base::StringView(
                        "LayoutTransform.Children");
            if (layoutPath) {
                if (!properties.Types().IsDerivedFrom(
                        target->RuntimeType(),
                        FrameworkElement::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidArgument,
                        "VisualState LayoutTransform target is not a FrameworkElement");
                }
                transform =
                    static_cast<FrameworkElement*>(
                        target)->LayoutTransform();
            } else {
                if (!properties.Types().IsDerivedFrom(
                        target->RuntimeType(),
                        UIElement::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidArgument,
                        "VisualState RenderTransform target is not a UIElement");
                }
                transform =
                    static_cast<UIElement*>(
                        target)->RenderTransform();
            }
        }
        if (!transform ||
            !properties.Types().IsDerivedFrom(
                transform->RuntimeType(),
                TransformGroup::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "VisualState transform path has no TransformGroup");
        }
        auto& group =
            static_cast<TransformGroup&>(*transform);
        const auto children = group.Children();
        if (parsedIndex >= children.Size() ||
            !children[static_cast<std::uint32_t>(
                parsedIndex)]) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "VisualState TransformGroup index is out of range");
        }
        target =
            children[static_cast<std::uint32_t>(
                parsedIndex)].Get();
        path = terminalPath;
        nestedTargetResolved = true;
    }

    constexpr Base::StringView TransformPrefix(
        "RenderTransform.");
    if (!nestedTargetResolved &&
        path.SizeBytes() > TransformPrefix.SizeBytes() &&
        path.Substr(0U, TransformPrefix.SizeBytes()) ==
            TransformPrefix) {
        if (!properties.Types().IsDerivedFrom(
                target->RuntimeType(),
                UIElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RenderTransform animation target is not a UIElement");
        }
        Base::Ref<Transform> transform =
            static_cast<UIElement*>(target)->RenderTransform();
        if (!transform) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "RenderTransform animation target has no transform");
        }
        target = transform.Get();
        path = path.Substr(
            TransformPrefix.SizeBytes(),
            path.SizeBytes() - TransformPrefix.SizeBytes());
    } else if (!nestedTargetResolved &&
               compoundParenthesizedPath &&
               path.SizeBytes() >= 7U &&
               path[0] == '(' &&
               path[path.SizeBytes() - 1U] == ')') {
        std::uint32_t separator = UINT32_MAX;
        for (std::uint32_t index = 1U;
             index + 2U < path.SizeBytes();
             ++index) {
            if (path[index] == ')' &&
                path[index + 1U] == '.' &&
                path[index + 2U] == '(') {
                separator = index;
                break;
            }
        }
        if (separator == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "VisualState compound Storyboard path is malformed");
        }
        Base::StringView ownerPath =
            path.Substr(1U, separator - 1U);
        std::uint32_t ownerDot = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < ownerPath.SizeBytes(); ++index) {
            if (ownerPath[index] == '.') {
                ownerDot = index;
            }
        }
        const Base::StringView ownerProperty =
            ownerDot == UINT32_MAX
            ? ownerPath
            : ownerPath.Substr(
                  ownerDot + 1U,
                  ownerPath.SizeBytes() -
                      ownerDot - 1U);
        const DependencyProperty* owner =
            properties.Find(
                target->RuntimeType(),
                ownerProperty);
        if (owner == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "VisualState compound object property was not found");
        }
        Base::Result<PropertyValue> ownerValue =
            target->GetValue(owner->Handle());
        if (!ownerValue ||
            ownerValue.Value().Kind() !=
                ValueKind::Object ||
            !ownerValue.Value().AsObject() ||
            !properties.Types().IsDerivedFrom(
                ownerValue.Value().AsObject()->
                    RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "VisualState compound object property has no DependencyObject value");
        }
        target =
            static_cast<DependencyObject*>(
                ownerValue.Value().AsObject().Get());
        const std::uint32_t terminalStart =
            separator + 3U;
        path = path.Substr(
            terminalStart,
            path.SizeBytes() -
                terminalStart - 1U);
        nestedTargetResolved = true;
    } else if (!nestedTargetResolved &&
               path.SizeBytes() > 6U &&
               path.Substr(
                   path.SizeBytes() - 6U, 6U) ==
                   Base::StringView(".Color")) {
        path = path.Substr(0U, path.SizeBytes() - 6U);
    } else if (!nestedTargetResolved) {
        for (std::uint32_t index = 0U;
             index < path.SizeBytes(); ++index) {
            if (path[index] == '.') {
                path = path.Substr(
                    index + 1U,
                    path.SizeBytes() - index - 1U);
            }
        }
    }
    path = NormalizePropertyPath(path);
    std::uint32_t ownerDot = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < path.SizeBytes(); ++index) {
        if (path[index] == '.') {
            ownerDot = index;
        }
    }
    if (ownerDot != UINT32_MAX) {
        path = path.Substr(
            ownerDot + 1U,
            path.SizeBytes() - ownerDot - 1U);
    }

    const DependencyProperty* property =
        properties.Find(target->RuntimeType(), path);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "VisualState Storyboard target property was not found");
    }
    return AnimationTarget{target, property->Handle()};
}

Aero::Detail::Animation::TimelineTiming ComposeTiming(
    const Media::Animation::Timeline& timeline,
    const Aero::Detail::Animation::TimelineTiming& parent) noexcept {
    Aero::Detail::Animation::TimelineTiming timing = timeline.Timing();
    if (UINT64_MAX - timing.beginTimeMicroseconds <
        parent.beginTimeMicroseconds) {
        timing.beginTimeMicroseconds = UINT64_MAX;
    } else {
        timing.beginTimeMicroseconds +=
            parent.beginTimeMicroseconds;
    }
    if (timing.durationMicroseconds == 0U) {
        timing.durationMicroseconds =
            parent.durationMicroseconds;
    }
    if (parent.repeat.forever) {
        timing.repeat = parent.repeat;
    }
    timing.speedRatio *= parent.speedRatio;
    timing.autoReverse =
        timing.autoReverse || parent.autoReverse;
    return timing;
}

} // namespace

std::uint32_t VisualStateManager::FindActive(
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

const VisualStateGroup* VisualStateManager::FindGroup(
    const ControlTemplate& plan,
    Base::StringView groupName) noexcept {
    for (const VisualStateGroup& group :
        plan.VisualStateGroups()) {
        if (group.name.View() == groupName) return &group;
    }
    return nullptr;
}

const VisualState* VisualStateManager::FindState(
    const VisualStateGroup& group,
    Base::StringView stateName) noexcept {
    for (const VisualState& state : group.states) {
        if (state.name.View() == stateName) return &state;
    }
    return nullptr;
}

const VisualTransition* VisualStateManager::FindTransition(
    const VisualStateGroup& group,
    Base::StringView fromState,
    Base::StringView toState) noexcept {
    const VisualTransition* best = nullptr;
    std::uint32_t bestScore = 0U;
    for (const VisualTransition& transition :
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

Base::Result<void> VisualStateManager::ApplyState(
    TemplateHandle handle,
    const VisualState& state) noexcept {
    for (const VisualStateSetter& setter : state.setters) {
        DependencyObject* target =
            templates_->FindName(handle, setter.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Visual state target name was not found");
        }
    }
    std::uint32_t appliedCount = 0U;
    for (const VisualStateSetter& setter : state.setters) {
        DependencyObject* target =
            templates_->FindName(handle, setter.targetName.View());
        Base::Result<void> applied = values_->SetAnimationValue(
            *target, setter.property, setter.value);
        if (!applied) {
            for (std::uint32_t index = 0U;
                index < appliedCount; ++index) {
                const VisualStateSetter& rollback =
                    state.setters[index];
                DependencyObject* rollbackTarget =
                    templates_->FindName(
                        handle, rollback.targetName.View());
                if (rollbackTarget != nullptr) {
                    static_cast<void>(
                        values_->ClearAnimationValue(
                            *rollbackTarget, rollback.property));
                }
            }
            return applied.GetStatus();
        }
        ++appliedCount;
    }
    return {};
}

Base::Result<void> VisualStateManager::ClearStateValues(
    TemplateHandle handle,
    const VisualState& state) noexcept {
    for (const VisualStateSetter& setter : state.setters) {
        DependencyObject* target =
            templates_->FindName(handle, setter.targetName.View());
        if (target == nullptr) continue;
        Base::Result<void> cleared =
            values_->ClearAnimationValue(
                *target, setter.property);
        if (!cleared) return cleared.GetStatus();
    }
    return {};
}

void VisualStateManager::RemoveActiveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != active_.Size()) {
        active_[index] =
            std::move(active_[active_.Size() - 1U]);
    }
    active_.PopBack();
}

void VisualStateManager::PruneStale() noexcept {
    for (std::uint32_t index = active_.Size();
        index > 0U; --index) {
        TemplateHandle handle;
        handle.value = active_[index - 1U].templateValue;
        if (templates_->AppliedTemplate(handle) == nullptr) {
            static_cast<void>(
                ClearStateAnimations(active_[index - 1U]));
            RemoveActiveAt(index - 1U);
        }
    }
}

Base::Result<void> VisualStateManager::ClearStateAnimations(
    ActiveGroup& active) noexcept {
    Base::Status first;
    for (Aero::Detail::Animation::AnimationHandle animation :
         active.animations) {
        Base::Result<void> removed =
            animations_->Remove(animation);
        if (!removed && first.IsOk()) {
            first = removed.GetStatus();
        }
    }
    active.animations.Clear();
    return first.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(first);
}

Base::Result<void> VisualStateManager::StartStateAnimations(
    Control& control,
    TemplateHandle handle,
    const VisualState& state,
    ActiveGroup& active,
    const Aero::Detail::Animation::TimelineTiming& parent) noexcept {
    if (!state.storyboard) return {};
    return StartStoryboardAnimations(
        control,
        handle,
        *state.storyboard,
        active,
        parent);
}

Base::Result<void> VisualStateManager::StartStoryboardAnimations(
    Control& control,
    TemplateHandle handle,
    Media::Animation::Storyboard& root,
    ActiveGroup& active,
    const Aero::Detail::Animation::TimelineTiming& parent) noexcept {
    const auto startTimeline =
        [&](const auto& self,
            Media::Animation::Timeline& timeline,
            const Aero::Detail::Animation::TimelineTiming& parent)
            -> Base::Result<void> {
        if (timeline.RuntimeType() ==
            Media::Animation::Storyboard::StaticTypeId()) {
            auto& storyboard =
                static_cast<Media::Animation::Storyboard&>(timeline);
            const Aero::Detail::Animation::TimelineTiming timing =
                ComposeTiming(storyboard, parent);
            for (const Base::Ref<Media::Animation::Timeline>& child :
                 storyboard.Timelines()) {
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
        if (!resolved) return resolved.GetStatus();

        Base::Result<Aero::Detail::Animation::AnimationHandle> started =
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "VisualState Storyboard contains an unsupported Timeline");
        if (timeline.RuntimeType() ==
            Media::Animation::DoubleAnimation::StaticTypeId()) {
            auto& authored =
                static_cast<Media::Animation::DoubleAnimation&>(timeline);
            Aero::Detail::Animation::DoubleAnimation runtime =
                authored.RuntimeAnimation();
            runtime.timing = ComposeTiming(authored, parent);
            started = animations_->Begin(
                *resolved.Value().object,
                resolved.Value().property,
                runtime);
        } else if (timeline.RuntimeType() ==
                   Media::Animation::ColorAnimation::StaticTypeId()) {
            auto& authored =
                static_cast<Media::Animation::ColorAnimation&>(timeline);
            Aero::Detail::Animation::ColorAnimation runtime =
                authored.RuntimeAnimation();
            runtime.timing = ComposeTiming(authored, parent);
            started = animations_->Begin(
                *resolved.Value().object,
                resolved.Value().property,
                runtime);
        } else if (timeline.RuntimeType() ==
                   Media::Animation::DoubleAnimationUsingKeyFrames::
                       StaticTypeId()) {
            auto& authored = static_cast<
                Media::Animation::DoubleAnimationUsingKeyFrames&>(
                    timeline);
            Base::Vector<Aero::Detail::Animation::DoubleKeyFrame>
                frames;
            for (const Base::Ref<Media::Animation::DoubleKeyFrame>&
                     frame : authored.KeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.TryPushBack(
                        frame->RuntimeFrame());
                if (!appended) {
                    return appended.GetStatus();
                }
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Aero::Detail::Animation::DoubleKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            if (frames.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "VisualState double key-frame animation has no frames");
            }
            Base::Result<PropertyValue> base =
                resolved.Value().object->GetValue(
                    resolved.Value().property);
            if (!base) return base.GetStatus();
            Base::Result<double> decoded =
                ValueCodec<double>::Decode(
                    base.Value());
            if (!decoded) return decoded.GetStatus();
            Aero::Detail::Animation::DoubleKeyFrameAnimation runtime;
            runtime.baseValue = decoded.Value();
            runtime.timing =
                ComposeTiming(authored, parent);
            if (runtime.timing.durationMicroseconds == 0U) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            started = animations_->Begin(
                *resolved.Value().object,
                resolved.Value().property,
                runtime);
        } else if (
            timeline.RuntimeType() ==
                Media::Animation::ObjectAnimationUsingKeyFrames::
                    StaticTypeId() ||
            timeline.RuntimeType() ==
                Media::Animation::BooleanAnimationUsingKeyFrames::
                    StaticTypeId()) {
            Base::Vector<
                Aero::Detail::Animation::DiscreteAnimationKeyFrame>
                frames;
            if (timeline.RuntimeType() ==
                Media::Animation::ObjectAnimationUsingKeyFrames::
                    StaticTypeId()) {
                auto& authored = static_cast<
                    Media::Animation::ObjectAnimationUsingKeyFrames&>(
                        timeline);
                for (const Base::Ref<
                         Media::Animation::DiscreteObjectKeyFrame>&
                         frame : authored.KeyFrames()) {
                    if (!frame) continue;
                    Aero::Detail::Animation::DiscreteAnimationKeyFrame
                        runtime;
                    runtime.keyTimeMicroseconds =
                        frame->KeyTimeMicroseconds();
                    runtime.value = frame->Value();
                    Base::Result<void> appended =
                        frames.TryPushBack(
                            std::move(runtime));
                    if (!appended) {
                        return appended.GetStatus();
                    }
                }
            } else {
                auto& authored = static_cast<
                    Media::Animation::BooleanAnimationUsingKeyFrames&>(
                        timeline);
                for (const Base::Ref<
                         Media::Animation::DiscreteBooleanKeyFrame>&
                         frame : authored.KeyFrames()) {
                    if (!frame) continue;
                    Base::Result<PropertyValue> encoded =
                        ValueCodec<bool>::Encode(
                            frame->Value());
                    if (!encoded) {
                        return encoded.GetStatus();
                    }
                    Aero::Detail::Animation::DiscreteAnimationKeyFrame
                        runtime;
                    runtime.keyTimeMicroseconds =
                        frame->KeyTimeMicroseconds();
                    runtime.value =
                        std::move(encoded).Value();
                    Base::Result<void> appended =
                        frames.TryPushBack(
                            std::move(runtime));
                    if (!appended) {
                        return appended.GetStatus();
                    }
                }
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Aero::Detail::Animation::DiscreteAnimationKeyFrame
                    current =
                        std::move(frames[index]);
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        std::move(
                            frames[position - 1U]);
                    --position;
                }
                frames[position] =
                    std::move(current);
            }
            if (frames.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "VisualState discrete key-frame animation has no frames");
            }
            Base::Result<PropertyValue> base =
                resolved.Value().object->GetValue(
                    resolved.Value().property);
            if (!base) return base.GetStatus();
            Aero::Detail::Animation::DiscreteAnimation runtime;
            runtime.baseValue = base.Value();
            runtime.timing =
                ComposeTiming(timeline, parent);
            if (runtime.timing.durationMicroseconds == 0U) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            started = animations_->Begin(
                *resolved.Value().object,
                resolved.Value().property,
                runtime);
        }
        if (!started) return started.GetStatus();
        return active.animations.TryPushBack(
            started.Value());
    };

    Aero::Detail::Animation::TimelineTiming rootTiming;
    rootTiming = parent;
    Base::Result<void> started =
        startTimeline(
            startTimeline,
            root,
            rootTiming);
    if (!started) {
        static_cast<void>(ClearStateAnimations(active));
        return started.GetStatus();
    }
    return {};
}

Base::Result<void> VisualStateManager::CaptureTransitionValues(
    TemplateHandle handle,
    const VisualState& next,
    Base::Vector<TransitionValue>& output) noexcept {
    output.Clear();
    for (const VisualStateSetter& setter :
         next.setters) {
        DependencyObject* target =
            templates_->FindName(
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
        Base::Result<void> appended =
            output.TryPushBack(std::move(value));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> VisualStateManager::StartTransitionAnimations(
    Control& control,
    TemplateHandle handle,
    const VisualTransition& transition,
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
        ? transition.generatedEasingFunction
              ->RuntimeEasing()
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
        Base::Result<double> fromDouble =
            ValueCodec<double>::Decode(value.from);
        Base::Result<double> toDouble =
            ValueCodec<double>::Decode(value.to);
        if (fromDouble && toDouble) {
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
        } else {
            Base::Result<Base::Color> fromColor =
                ValueCodec<Base::Color>::Decode(
                    value.from);
            Base::Result<Base::Color> toColor =
                ValueCodec<Base::Color>::Decode(
                    value.to);
            if (!fromColor || !toColor) {
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
        }
        if (!started) {
            static_cast<void>(
                ClearStateAnimations(active));
            return started.GetStatus();
        }
        Base::Result<void> retained =
            active.animations.TryPushBack(
                started.Value());
        if (!retained) {
            static_cast<void>(
                animations_->Remove(
                    started.Value()));
            static_cast<void>(
                ClearStateAnimations(active));
            return retained.GetStatus();
        }
    }
    return {};
}

Base::Result<bool> VisualStateManager::GoToState(
    Control& control,
    Base::StringView groupName,
    Base::StringView stateName,
    bool useTransitions) noexcept {
    if (groupName.Empty() || stateName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Visual state group and state names are required");
    }
    PruneStale();
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    const ControlTemplate* plan =
        templates_->AppliedTemplate(handle);
    if (!handle.IsValid() || plan == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Control does not have an applied template");
    }
    const VisualStateGroup* group =
        FindGroup(*plan, groupName);
    const VisualState* next =
        group != nullptr ? FindState(*group, stateName) : nullptr;
    if (next == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Visual state was not found");
    }

    std::uint32_t activeIndex = FindActive(handle, groupName);
    if (activeIndex != UINT32_MAX &&
        active_[activeIndex].stateName.View() == stateName) {
        return false;
    }
    Base::String nextGroup;
    Base::Result<void> groupAssigned =
        nextGroup.TryAssign(groupName);
    if (!groupAssigned) return groupAssigned.GetStatus();
    Base::String nextName;
    Base::Result<void> nameAssigned =
        nextName.TryAssign(stateName);
    if (!nameAssigned) return nameAssigned.GetStatus();
    bool addedRecord = false;
    if (activeIndex == UINT32_MAX) {
        ActiveGroup active;
        active.templateValue = handle.value;
        active.groupName = std::move(nextGroup);
        Base::Result<void> appended =
            active_.TryPushBack(std::move(active));
        if (!appended) return appended.GetStatus();
        activeIndex = active_.Size() - 1U;
        addedRecord = true;
    }

    const VisualState* previous = nullptr;
    if (!active_[activeIndex].stateName.Empty()) {
        previous = FindState(
            *group, active_[activeIndex].stateName.View());
    }
    const Base::StringView previousName =
        active_[activeIndex].stateName.View();
    const VisualTransition* transition =
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
                handle,
                *next,
                transitionValues);
        if (!captured) {
            if (addedRecord) {
                RemoveActiveAt(activeIndex);
            }
            return captured.GetStatus();
        }
    }
    if (previous != nullptr) {
        Base::Result<void> animationsCleared =
            ClearStateAnimations(active_[activeIndex]);
        if (!animationsCleared) {
            if (addedRecord) RemoveActiveAt(activeIndex);
            return animationsCleared.GetStatus();
        }
        Base::Result<void> cleared =
            ClearStateValues(handle, *previous);
        if (!cleared) {
            if (addedRecord) RemoveActiveAt(activeIndex);
            return cleared.GetStatus();
        }
    }
    Base::Result<void> applied = ApplyState(handle, *next);
    if (!applied) {
        if (previous != nullptr) {
            static_cast<void>(ApplyState(handle, *previous));
        }
        if (addedRecord) RemoveActiveAt(activeIndex);
        return applied.GetStatus();
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
    Aero::Detail::Animation::TimelineTiming stateTiming;
    if (animated && transition != nullptr) {
        stateTiming.beginTimeMicroseconds =
            transition->generatedDurationMicroseconds;
        if (transition->storyboard) {
            stateTiming.beginTimeMicroseconds =
                std::max(
                    stateTiming.beginTimeMicroseconds,
                    transition->storyboard->Timing()
                        .durationMicroseconds);
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
            ClearStateValues(handle, *next));
        if (previous != nullptr) {
            static_cast<void>(
                ApplyState(handle, *previous));
            static_cast<void>(
                StartStateAnimations(
                    control, handle, *previous,
                    active_[activeIndex]));
        }
        if (addedRecord) RemoveActiveAt(activeIndex);
        return animated.GetStatus();
    }
    active_[activeIndex].stateName = std::move(nextName);
    return true;
}

Base::Result<bool> VisualStateManager::ClearState(
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
    const VisualStateGroup* group = plan != nullptr
        ? FindGroup(*plan, groupName) : nullptr;
    const VisualState* state = group != nullptr
        ? FindState(*group, active_[activeIndex].stateName.View())
        : nullptr;
    if (state != nullptr) {
        Base::Result<void> animationsCleared =
            ClearStateAnimations(active_[activeIndex]);
        if (!animationsCleared) {
            return animationsCleared.GetStatus();
        }
        Base::Result<void> cleared =
            ClearStateValues(handle, *state);
        if (!cleared) return cleared.GetStatus();
    }
    RemoveActiveAt(activeIndex);
    return true;
}

Base::Result<std::uint32_t> VisualStateManager::Clear(
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
        Base::Result<bool> cleared = ClearState(
            control, active_[index - 1U].groupName.View());
        if (!cleared) return cleared.GetStatus();
        if (cleared.Value()) ++clearedCount;
    }
    return clearedCount;
}

Base::StringView VisualStateManager::CurrentState(
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
