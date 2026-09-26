#pragma once

// Media playback trigger actions.
#include <Aero/Interactivity/TriggerAction.hpp>

namespace Aero::Media::Animation {

using ::Aero::Interactivity::TriggerAction;

class AERO_GUI_API PlayMediaAction : public TriggerAction {
    AERO_DECLARE_TYPE(PlayMediaAction, TriggerAction)
public:
    PlayMediaAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept { (void)targetName_.Assign(value); }

private:
    String targetName_;
};

class AERO_GUI_API PauseMediaAction : public TriggerAction {
    AERO_DECLARE_TYPE(PauseMediaAction, TriggerAction)
public:
    PauseMediaAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept { (void)targetName_.Assign(value); }

private:
    String targetName_;
};

class AERO_GUI_API StopMediaAction : public TriggerAction {
    AERO_DECLARE_TYPE(StopMediaAction, TriggerAction)
public:
    StopMediaAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept { (void)targetName_.Assign(value); }

private:
    String targetName_;
};

} // namespace Aero::Media::Animation
