// Storyboard clocks: property resolution, timelines, and completion.
#include "gui/ViewFrame.hpp"
#include "gui/media/StoryboardHost.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/core/EventRouter.hpp"
#include <Aero/CommandBinding.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/TryCast.hpp>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {


Base::Result<StoryboardHost::ResolvedAnimationProperty>
StoryboardHost::ResolveAnimationProperty(
        ::Aero::DependencyObject& target,
        Base::StringView authoredPath) noexcept {


        Base::Result<Aero::ResolvedAnimationProperty> resolved =
            ResolveAnimationPropertyPath(
                target, authoredPath, (*Metadata()).DependencyProperties());
        if (!resolved) {
            return resolved.GetStatus();
        }
        return ResolvedAnimationProperty{
            resolved.Value().target, resolved.Value().property};
    }

} // namespace Aero

namespace Aero {

using namespace Media::Animation;

Base::Result<std::uint32_t> StoryboardHost::BeginTimeline(
        Timeline& timeline,
        FrameworkElement& triggerOwner,
        const NameScope* names,
        const StoryboardTimingState* inherited,
        Base::Vector<
            Model::AnimationHandle>*
            retainedHandles,
        Controls::DataTemplateTriggerInstance*
            dataTemplateContext) noexcept {


        if (Animations() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard requires the animation manager");
        }
        if (Metadata()->Types().IsDerivedFrom(
                timeline.RuntimeType(),
                TimelineGroup::StaticTypeId())) {
            auto& nested =
                static_cast<TimelineGroup&>(timeline);
            const StoryboardTimingState timing =
                ComposeStoryboardTiming(
                    inherited,
                    nested,
                    timeline.RuntimeType() ==
                        ParallelTimeline::
                            StaticTypeId());
            std::uint32_t count = 0U;
            for (const Base::Ref<Timeline>& child :
                 nested.GetTimelines()) {
                if (!child) continue;
                Base::Result<std::uint32_t> started =
                    BeginTimeline(
                        *child, triggerOwner, names, &timing,
                        retainedHandles,
                        dataTemplateContext);
                if (!started) {
                    if (view != nullptr) {
                        view->ReportUpdateFailure(started.GetStatus());
                    }
                    continue;
                }
                if (count > UINT32_MAX - started.Value()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard child count overflow");
                }
                count += started.Value();
            }
            return count;
        }

        Base::Result<Base::StringView> targetName =
            AnimationAttachedString(
                timeline,
                Storyboard::TargetNameProperty);
        if (!targetName) return targetName.GetStatus();
        Base::Result<Base::StringView> targetPath =
            AnimationAttachedString(
                timeline,
                Storyboard::TargetPropertyProperty);
        if (!targetPath) return targetPath.GetStatus();

        Base::Object* targetObject =
            targetName.Value().Empty()
            ? static_cast<Base::Object*>(
                  &triggerOwner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      targetName.Value())
                : names != nullptr
                    ? names->Find(targetName.Value())
                    : view->loadedDocument.names.Find(
                          targetName.Value());
        if (targetObject == nullptr) {
            targetObject = triggerOwner.FindName(targetName.Value());
        }
        if (targetObject == nullptr) {
            if (Controls::TemplateEngine* templates = ElementTree::TemplatesOf(triggerOwner)) {
                const Controls::Control* control = ::Aero::TryCast<Controls::Control>(&triggerOwner);
                if (control == nullptr) {
                    control = ::Aero::TryCast<Controls::Control>(triggerOwner.GetTemplatedParent());
                }
                if (control != nullptr) {
                    const Controls::TemplateHandle handle = templates->AppliedHandle(*control);
                    if (handle.IsValid()) {
                        targetObject = templates->FindName(handle, targetName.Value());
                    }
                }
            }
        }
        if (targetObject == nullptr && names != nullptr) {
            targetObject = view->loadedDocument.names.Find(
                targetName.Value());
        }
        if (targetObject == nullptr && view != nullptr) {
            targetObject = ViewFrame::FindNameForElement(
                view, targetName.Value(), DependencyObject::StaticTypeId());
        }
        if (targetObject == nullptr ||
            !Metadata()->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard target name does not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<DependencyObject&>(*targetObject);
        Base::Result<ResolvedAnimationProperty> property =
            ResolveAnimationProperty(target, targetPath.Value());
        if (!property) return property.GetStatus();
        DependencyObject& propertyTarget =
            *property.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            property.Value().property;

        Base::Result<Model::AnimationHandle> started =
            Animations()->BeginTimeline(
                propertyTarget,
                propertyHandle,
                timeline,
                EffectiveTimelineTiming(timeline, inherited),
                Metadata());
        return RetainStartedAnimation(
            std::move(started),
            retainedHandles);
    }

} // namespace Aero

namespace Aero {

using namespace Media::Animation;

void StoryboardHost::
CancelStoryboardCompletionSessions(
    Base::Span<const Model::AnimationHandle>
        handles) noexcept
{
    for (std::uint32_t index = 0U;
         index < storyboardCompletionSessions.Size();) {
        bool matches = false;
        for (Model::AnimationHandle sessionHandle :
             storyboardCompletionSessions[index].handles) {
            for (Model::AnimationHandle handle :
                 handles) {
                if (sessionHandle == handle) {
                    matches = true;
                    break;
                }
            }
            if (matches) break;
        }
        if (!matches) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < storyboardCompletionSessions.Size();
             ++next) {
            storyboardCompletionSessions[next - 1U] =
                std::move(
                    storyboardCompletionSessions[next]);
        }
        storyboardCompletionSessions.PopBack();
    }
}

Base::Result<std::uint32_t>
StoryboardHost::ProcessStoryboardCompletions() noexcept
{
    std::uint32_t actionCount = 0U;
    std::uint32_t index = 0U;
    while (index < storyboardCompletionSessions.Size()) {
        StoryboardCompletionSession& session =
            storyboardCompletionSessions[index];
        bool running = false;
        bool filling = false;
        for (Model::AnimationHandle handle :
             session.handles) {
            const Model::AnimationState state =
                Animations()->State(handle);
            if (state ==
                    Model::AnimationState::Active ||
                state ==
                    Model::AnimationState::Paused) {
                running = true;
                break;
            }
            if (state ==
                Model::AnimationState::Filling) {
                filling = true;
            }
        }
        if (running) {
            ++index;
            continue;
        }

        Base::Ref<Storyboard> storyboard =
            session.storyboard;
        for (std::uint32_t next = index + 1U;
             next < storyboardCompletionSessions.Size();
             ++next) {
            storyboardCompletionSessions[next - 1U] =
                std::move(
                    storyboardCompletionSessions[next]);
        }
        storyboardCompletionSessions.PopBack();

        // ClockController.Stop leaves tracks Stopped. That is not a natural
        // completion; WPF does not raise Completed in that case.
        if (!filling) {
            continue;
        }

        for (const StoryboardCompletedSubscription&
                 subscription :
             storyboardCompletedSubscriptions) {
            if (subscription.trigger == nullptr ||
                subscription.owner == nullptr ||
                subscription.trigger->GetStoryboard().Get() !=
                    storyboard.Get()) {
                continue;
            }
            Base::Result<bool> allowed = Interactivity()->ConditionBehaviorsAllowExecution(
                subscription.trigger->GetBehaviors(),
                *subscription.owner,
                subscription.names);
            if (!allowed) return allowed.GetStatus();
            if (!allowed.Value()) continue;
            for (const Base::Ref<
                     Interactivity::TriggerAction>& action :
                 subscription.trigger->GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    ExecuteAnimationAction(
                        *action, *subscription.owner,
                        nullptr, subscription.names);
                if (!executed) {
                    return executed.GetStatus();
                }
                if (actionCount == UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard completed action count overflow");
                }
                ++actionCount;
            }
        }
    }
    return actionCount;
}

} // namespace Aero
