#pragma once

#include <Aero/Interactivity/TriggerAction.hpp>
using Aero::Interactivity::TriggerAction;
#include <Aero/Media/Animation.hpp>
#include <utility>

namespace Aero::Media::Animation {

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

class AERO_GUI_API ControllableStoryboardAction : public TriggerAction {
    AERO_DECLARE_TYPE(ControllableStoryboardAction, TriggerAction)
public:
    StringView GetBeginStoryboardName() const noexcept {
        return beginStoryboardName_.View();
    }
    void SetBeginStoryboardName(StringView value) noexcept;

protected:
    explicit ControllableStoryboardAction(Meta::TypeId runtimeType) noexcept
        : TriggerAction(runtimeType) {}

private:
    String beginStoryboardName_;
};

class AERO_GUI_API PauseStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(PauseStoryboard, ControllableStoryboardAction)
public:
    PauseStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};

class AERO_GUI_API ResumeStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(ResumeStoryboard, ControllableStoryboardAction)
public:
    ResumeStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};

class AERO_GUI_API StopStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(StopStoryboard, ControllableStoryboardAction)
public:
    StopStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};

class AERO_GUI_API RemoveStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(RemoveStoryboard, ControllableStoryboardAction)
public:
    RemoveStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};

class AERO_GUI_API SeekStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(SeekStoryboard, ControllableStoryboardAction)
public:
    SeekStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
    StringView GetOffset() const noexcept { return offsetText_.View(); }
    AnimationTime GetOffsetMicroseconds() const noexcept { return offsetMicroseconds_; }
    void SetOffset(StringView value) noexcept;

private:
    String offsetText_;
    AnimationTime offsetMicroseconds_ = 0U;
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(
    Aero::Media::Animation::ControlStoryboardAction::Option)
