#pragma once

#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Media/Animation/Storyboard.hpp>
#include <Aero/Triggers/TriggerBase.hpp>

namespace Aero::Media::Animation {

using ::Aero::Interactivity::TriggerAction;

class AERO_GUI_API StoryboardCompletedTrigger : public ::Aero::TriggerBase {
    AERO_DECLARE_TYPE(StoryboardCompletedTrigger, ::Aero::TriggerBase)
public:
    StoryboardCompletedTrigger() noexcept
        : StoryboardCompletedTrigger(StaticTypeId()) {}
    Ref<Storyboard> GetStoryboard() const noexcept { return storyboard_; }
    void SetStoryboard(Ref<Storyboard> value) noexcept;
    void AddAction(Ref<TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Span<const Ref<TriggerAction>> GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }
    void AddConditionBehavior(Ref<Base::Object> value) noexcept {
        if (!value) { AERO_ASSERT(false); return; }
        behaviors_.PushBack(std::move(value));
    }
    void ClearConditionBehaviors() noexcept { behaviors_.Clear(); }
    Span<const Ref<Base::Object>> GetBehaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

protected:
    explicit StoryboardCompletedTrigger(Meta::TypeId runtimeType) noexcept
        : ::Aero::TriggerBase(runtimeType) {}

private:
    Ref<Storyboard> storyboard_;
    Base::Vector<Ref<TriggerAction>> actions_;
    Base::Vector<Ref<Base::Object>> behaviors_;
};

} // namespace Aero::Media::Animation
