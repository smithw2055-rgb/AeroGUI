#pragma once

#include <Aero/VisualStateManager.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero {
class AnimationEngine;
namespace Meta {
class EffectiveValueEngine;
class DependencyPropertyRegistry;
}
namespace Controls {
class Control;
class TemplateEngine;

class VisualStateManagerExecution {
public:
    static Base::Ref<VisualStateManager> Create(
        Meta::EffectiveValueEngine& values,
        TemplateEngine& templates,
        AnimationEngine& animations,
        Meta::DependencyPropertyRegistry& properties) noexcept;

    static bool GoToState(
        VisualStateManager& manager,
        Control& control,
        Base::StringView groupName,
        Base::StringView stateName,
        bool useTransitions = true) noexcept;

    static bool ClearState(
        VisualStateManager& manager,
        Control& control,
        Base::StringView groupName) noexcept;

    static std::uint32_t Clear(
        VisualStateManager& manager,
        Control& control) noexcept;

    static Base::StringView CurrentState(
        const VisualStateManager& manager,
        const Control& control,
        Base::StringView groupName) noexcept;
};

} // namespace Controls
} // namespace Aero
