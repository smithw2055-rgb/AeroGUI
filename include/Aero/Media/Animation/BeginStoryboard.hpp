#pragma once

#include <Aero/Media/Animation/Storyboard.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>

namespace Aero::Media::Animation {
using ::Aero::Interactivity::TriggerAction;

class AERO_GUI_API BeginStoryboard : public TriggerAction {
    AERO_DECLARE_TYPE(BeginStoryboard, TriggerAction)
public:
    BeginStoryboard() noexcept : TriggerAction(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    StringView GetName() const noexcept { return name_.View(); }
    Ref<Storyboard> GetStoryboard() const noexcept { return storyboard_; }
    void SetName(StringView value) noexcept;
    void SetStoryboard(Ref<Storyboard> value) noexcept;

private:
    String name_;
    Ref<Storyboard> storyboard_;
};
} // namespace Aero::Media::Animation
