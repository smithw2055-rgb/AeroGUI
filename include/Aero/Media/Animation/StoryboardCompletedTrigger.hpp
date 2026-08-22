#pragma once

#include <Aero/Interactivity/TriggerAction.hpp>
using Aero::Interactivity::TriggerAction;
#include <Aero/Media/Animation.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API StoryboardCompletedTrigger : public Base::Object {
    AERO_DECLARE_TYPE(StoryboardCompletedTrigger, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Ref<Storyboard> GetStoryboard() const noexcept { return storyboard_; }
    void SetStoryboard(Ref<Storyboard> value) noexcept;
    Result<void> AddAction(Ref<TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Span<const Ref<TriggerAction>> GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }
    Result<void> AddConditionBehavior(Ref<Base::Object> value) noexcept {
        return value ? behaviors_.PushBack(std::move(value))
                     : Result<void>(Base::Status::Failure(
                           Base::ErrorCode::InvalidArgument,
                           "StoryboardCompletedTrigger behavior cannot be null"));
    }
    void ClearConditionBehaviors() noexcept { behaviors_.Clear(); }
    Span<const Ref<Base::Object>> GetBehaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

private:
    Ref<Storyboard> storyboard_;
    Base::Vector<Ref<TriggerAction>> actions_;
    Base::Vector<Ref<Base::Object>> behaviors_;
};

} // namespace Aero::Media::Animation
