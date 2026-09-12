#pragma once

// Source-only storyboard session host next to AnimationEngine.
// Not installed under include/Aero. Included from ViewFrame.hpp after ViewFrame.

#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Media/Animation/Storyboard.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>

namespace Aero {

class StoryboardHost {
public:
    explicit StoryboardHost(ViewFrame& owner) noexcept;
    void Bind() noexcept;

    ViewFrame* view = nullptr;

    Base::IAllocator* Allocator() const noexcept;
    ::Aero::Meta::Registry* Metadata() const noexcept;
    Aero::AnimationEngine* Animations() const noexcept;
    Aero::InputRouter* Input() const noexcept;
    Aero::StyleEngine* Styles() const noexcept;
    InteractivityEngine* Interactivity() const noexcept;

    struct StoryboardSession {
        explicit StoryboardSession(Base::IAllocator* allocator) noexcept;

        Aero::FrameworkElement* owner = nullptr;
        Base::String name;
        Base::Vector<Aero::Media::Animation::Model::AnimationHandle> handles;
    };
    Base::Vector<StoryboardSession> storyboardSessions;

    struct StoryboardCompletionSession {
        explicit StoryboardCompletionSession(Base::IAllocator* allocator) noexcept;

        Base::Ref<MediaAnimation::Storyboard> storyboard;
        Aero::FrameworkElement* owner = nullptr;
        Base::Vector<Aero::Media::Animation::Model::AnimationHandle> handles;
    };
    struct StoryboardCompletedSubscription {
        MediaAnimation::StoryboardCompletedTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;
    };
    Base::Vector<StoryboardCompletionSession> storyboardCompletionSessions;
    Base::Vector<StoryboardCompletedSubscription> storyboardCompletedSubscriptions;

    struct AnimationEventState {
        StoryboardHost* runtime = nullptr;
        MediaAnimation::EventTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;

        Base::Result<bool> EvaluateComparison(
            const Aero::Interactivity::ComparisonCondition& condition) noexcept;
        Base::Result<bool> BehaviorsAllowExecution() noexcept;
        void Invoke(Base::Object*, Aero::RoutedEventArgs&) noexcept;
    };
    struct AnimationEventSubscription {
        Base::Object* source = nullptr;
        Aero::Media::Visual* visualOwner = nullptr;
        Aero::RoutedEventHandle event;
        Aero::RoutedEventHandler handler;
        AnimationEventState* context = nullptr;
        bool contentSource = false;
    };
    Base::Vector<AnimationEventSubscription> animationEventSubscriptions;
    Base::Status eventTriggerStatus;

    struct PendingLoadedTrigger {
        MediaAnimation::EventTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;
    };
    Base::Vector<PendingLoadedTrigger> pendingLoadedTriggers;
    Base::Result<void> FlushPendingLoadedTriggers() noexcept;

    Base::Result<bool> StartEventTrigger(
        MediaAnimation::EventTrigger& trigger,
        Base::Object& defaultSource,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept;
    void ClearEventTriggersFor(Aero::Media::Visual& fragmentRoot) noexcept;
    void ClearEventTriggers() noexcept;

    Base::Result<void> ExecuteAnimationAction(
        Aero::Interactivity::TriggerAction& action,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerInstance* dataTemplateContext = nullptr,
        const Aero::NameScope* names = nullptr) noexcept;
    void CancelStoryboardCompletionSessions(
        Base::Span<const Aero::Media::Animation::Model::AnimationHandle>
            handles) noexcept;
    Base::Result<std::uint32_t> ProcessStoryboardCompletions() noexcept;

    Base::Result<Base::StringView> AnimationAttachedString(
        MediaAnimation::Timeline& timeline,
        Meta::DependencyPropertyHandle property) noexcept;

    struct ResolvedAnimationProperty {
        ::Aero::DependencyObject* target = nullptr;
        Meta::DependencyPropertyHandle property;
    };
    Base::Result<ResolvedAnimationProperty> ResolveAnimationProperty(
        ::Aero::DependencyObject& target,
        Base::StringView authoredPath) noexcept;

    struct StoryboardTimingState {
        Aero::Media::Animation::AnimationTime beginTimeMicroseconds = 0U;
        Aero::Media::Animation::AnimationTime durationMicroseconds = 0U;
        Aero::Media::Animation::Model::RepeatBehavior repeat;
        double speedRatio = 1.0;
        bool hasDuration = false;
        bool hasRepeat = false;
        bool autoReverse = false;
        bool preservesChildDuration = false;
    };
    StoryboardTimingState ComposeStoryboardTiming(
        const StoryboardTimingState* inherited,
        const MediaAnimation::Timeline& storyboard,
        bool preservesChildDuration) noexcept;
    Aero::Media::Animation::Model::TimelineTiming EffectiveTimelineTiming(
        const MediaAnimation::Timeline& timeline,
        const StoryboardTimingState* inherited) noexcept;
    Base::Result<std::uint32_t> RetainStartedAnimation(
        Base::Result<Aero::Media::Animation::Model::AnimationHandle> started,
        Base::Vector<Aero::Media::Animation::Model::AnimationHandle>*
            retainedHandles) noexcept;
    Base::Result<std::uint32_t> BeginTimeline(
        MediaAnimation::Timeline& timeline,
        Aero::FrameworkElement& triggerOwner,
        const Aero::NameScope* names = nullptr,
        const StoryboardTimingState* inherited = nullptr,
        Base::Vector<Aero::Media::Animation::Model::AnimationHandle>*
            retainedHandles = nullptr,
        Aero::Controls::DataTemplateTriggerInstance* dataTemplateContext =
            nullptr) noexcept;
    Base::Result<std::uint32_t> StartContentElementAnimations(
        Aero::FrameworkContentElement& content,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept;
    Base::Result<std::uint32_t> StartLoadedAnimations(
        Aero::Media::Visual* visual,
        const Aero::NameScope* names = nullptr) noexcept;
};

// Shared keyframe-schedule helper for StoryboardHost TUs
// (merged from StoryboardHostCommon.hpp; no ViewFrame re-include needed
// as this header is consumed after ViewFrame).
namespace StoryboardSupport {

template<class TAnimation>
inline Aero::Media::Animation::KeyframeSchedule
MakeKeyframeSchedule(
    const TAnimation& animation,
    Aero::Media::Animation::AnimationTime authoredDuration) noexcept {
    return Aero::Media::Animation::MakeSchedule(
        animation.GetKeyFrames(), authoredDuration);
}

} // namespace StoryboardSupport

using StoryboardSupport::MakeKeyframeSchedule;

} // namespace Aero
