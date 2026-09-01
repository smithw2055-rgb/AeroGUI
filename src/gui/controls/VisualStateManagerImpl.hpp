#pragma once

// Source-only VisualStateManager runtime. Not installed under include/Aero.
// Authored VisualStateGroups stay on ControlTemplate; this type only executes
// GoToState against those compiled plans.

#include "gui/templates/TemplateState.hpp"
#include <Aero/Media/Animation.hpp>

namespace Aero::Controls {

using namespace ::Aero::Meta;

class VisualStateManagerImpl {
public:
    VisualStateManagerImpl(
        EffectiveValueEngine& values,
        TemplateEngine& templates,
        Aero::AnimationEngine& animations,
        DependencyPropertyRegistry& properties) noexcept
        : values_(&values),
          templates_(&templates),
          animations_(&animations),
          properties_(&properties) {}

    Base::Result<bool> GoToState(
        Control& control,
        Base::StringView groupName,
        Base::StringView stateName,
        bool useTransitions) noexcept;
    Base::Result<bool> ClearState(
        Control& control,
        Base::StringView groupName) noexcept;
    Base::Result<std::uint32_t> Clear(Control& control) noexcept;
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
        TemplateHandle handle,
        const VisualStatePlan& state,
        ActiveGroup& active) noexcept;
    Base::Result<void> ClearStateValues(
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
    Base::Result<void> ClearStateAnimations(ActiveGroup& active) noexcept;
    void PruneStale() noexcept;
    void RemoveActiveAt(std::uint32_t index) noexcept;
};

} // namespace Aero::Controls
