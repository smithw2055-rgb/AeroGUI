#pragma once

#include <Aero/Interactivity/TriggerAction.hpp>

namespace Aero::Media::Animation {
using ::Aero::Interactivity::TriggerAction;

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
} // namespace Aero::Media::Animation
