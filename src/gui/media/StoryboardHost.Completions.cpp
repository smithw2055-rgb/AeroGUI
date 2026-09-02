#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

void StoryboardHost::
CancelStoryboardCompletionSessions(
    Base::Span<const Aero::Media::Animation::Model::AnimationHandle>
        handles) noexcept
{
    for (std::uint32_t index = 0U;
         index < storyboardCompletionSessions.Size();) {
        bool matches = false;
        for (Aero::Media::Animation::Model::AnimationHandle sessionHandle :
             storyboardCompletionSessions[index].handles) {
            for (Aero::Media::Animation::Model::AnimationHandle handle :
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
        for (Aero::Media::Animation::Model::AnimationHandle handle :
             session.handles) {
            const Aero::Media::Animation::Model::AnimationState state =
                animations->State(handle);
            if (state ==
                    Aero::Media::Animation::Model::AnimationState::Active ||
                state ==
                    Aero::Media::Animation::Model::AnimationState::Paused) {
                running = true;
                break;
            }
            if (state ==
                Aero::Media::Animation::Model::AnimationState::Filling) {
                filling = true;
            }
        }
        if (running) {
            ++index;
            continue;
        }

        Base::Ref<MediaAnimation::Storyboard> storyboard =
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
            Base::Result<bool> allowed = interactivity->ConditionBehaviorsAllowExecution(
                subscription.trigger->GetBehaviors(),
                *subscription.owner,
                subscription.names);
            if (!allowed) return allowed.GetStatus();
            if (!allowed.Value()) continue;
            for (const Base::Ref<
                     Aero::Interactivity::TriggerAction>& action :
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
