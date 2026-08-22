#pragma once

#include <Aero/Interactivity/TriggerAction.hpp>
using Aero::Interactivity::TriggerAction;

namespace Aero::Media::Animation {

// Plays the targeted MediaElement. TargetName is authored in XAML to reference
// the element; if left empty the engine resolves the source of the event that
// triggered the action.
class AERO_GUI_API PlayMediaAction : public TriggerAction {
    AERO_DECLARE_TYPE(PlayMediaAction, TriggerAction)
public:
    PlayMediaAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept {
        (void)targetName_.Assign(value);
    }

private:
    String targetName_;
};

// Pauses the targeted MediaElement.
class AERO_GUI_API PauseMediaAction : public TriggerAction {
    AERO_DECLARE_TYPE(PauseMediaAction, TriggerAction)
public:
    PauseMediaAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept {
        (void)targetName_.Assign(value);
    }

private:
    String targetName_;
};

// Stops the targeted MediaElement.
class AERO_GUI_API StopMediaAction : public TriggerAction {
    AERO_DECLARE_TYPE(StopMediaAction, TriggerAction)
public:
    StopMediaAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept {
        (void)targetName_.Assign(value);
    }

private:
    String targetName_;
};

} // namespace Aero::Media::Animation