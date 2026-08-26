#pragma once

#include <Aero/Media/Animation/Storyboard.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <utility>

namespace Aero::Media::Animation {
using ::Aero::Interactivity::TriggerAction;

class AERO_GUI_API ControlStoryboardAction : public TriggerAction {
    AERO_DECLARE_TYPE(ControlStoryboardAction, TriggerAction)
public:
    enum class Option : std::uint8_t {
        Play = 0U, Stop, TogglePlayPause, Pause, Resume, SkipToFill
    };
    ControlStoryboardAction() noexcept : TriggerAction(StaticTypeId()) {}
    Ref<Storyboard> GetStoryboard() const noexcept { return storyboard_; }
    void SetStoryboard(Ref<Storyboard> value) noexcept {
        storyboard_ = std::move(value);
    }
    Option GetControlOption() const noexcept { return option_; }
    void SetControlOption(Option value) noexcept { option_ = value; }

private:
    Ref<Storyboard> storyboard_;
    Option option_ = Option::Play;
};
} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(
    Aero::Media::Animation::ControlStoryboardAction::Option)
