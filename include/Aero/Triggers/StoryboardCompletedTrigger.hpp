#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_API StoryboardCompletedTrigger : public Base::Object {
    AERO_DECLARE_TYPE(StoryboardCompletedTrigger, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<Storyboard> GetStoryboard() const noexcept { return storyboard_; }
    void SetStoryboard(Base::Ref<Storyboard> value) noexcept;
    Base::Result<void> AddAction(Base::Ref<TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Base::Span<const Base::Ref<TriggerAction>> GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }

private:
    Base::Ref<Storyboard> storyboard_;
    Base::Vector<Base::Ref<TriggerAction>> actions_;
};

} // namespace Aero::Media::Animation
