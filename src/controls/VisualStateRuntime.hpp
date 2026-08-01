#pragma once

#include "TemplateAccess.hpp"
#include "runtime/RuntimeFwd.hpp"

namespace Aero::Controls::Detail {

class VisualStateManagerAccess final {
public:
    static Base::Result<VisualStateManager*> Create(Core::EffectiveValueEngine& values,
        TemplateManager& templates, Aero::Detail::AnimationManager& animations,
        Core::DependencyPropertyRegistry& properties) noexcept;
    static Base::Result<bool> GoToState(VisualStateManager& manager, Control& control,
        Base::StringView groupName, Base::StringView stateName,
        bool useTransitions = true) noexcept;
    static Base::Result<bool> ClearState(VisualStateManager& manager, Control& control,
        Base::StringView groupName) noexcept;
    static Base::Result<std::uint32_t> Clear(VisualStateManager& manager,
        Control& control) noexcept;
    static Base::StringView GetCurrentState(const VisualStateManager& manager,
        const Control& control, Base::StringView groupName) noexcept;
};

} // namespace Aero::Controls::Detail
