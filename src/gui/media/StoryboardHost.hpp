#pragma once

// Source-only storyboard session host next to AnimationEngine.
// Not installed under include/Aero. Included from ViewState.hpp after ViewState.

namespace Aero {

class StoryboardHost {
public:
    explicit StoryboardHost(ViewState& owner) noexcept;
    void Bind() noexcept;

    ViewState* view = nullptr;
    Base::IAllocator* allocator = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Aero::AnimationEngine* animations = nullptr;
    Aero::InputRouter* input = nullptr;
    Aero::StyleEngine* styles = nullptr;
    InteractivityEngine* interactivity = nullptr;

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

    Base::Result<void> ExecuteAnimationAction(
        Aero::Interactivity::TriggerAction& action,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState* dataTemplateContext = nullptr,
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
        Aero::Controls::DataTemplateTriggerState* dataTemplateContext =
            nullptr) noexcept;
    Base::Result<std::uint32_t> StartContentElementAnimations(
        Aero::FrameworkContentElement& content,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept;
    Base::Result<std::uint32_t> StartLoadedAnimations(
        Aero::Media::Visual* visual,
        const Aero::NameScope* names = nullptr) noexcept;
};

} // namespace Aero
