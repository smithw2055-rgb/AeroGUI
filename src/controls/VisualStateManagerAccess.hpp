#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Styling.hpp>
#include "../runtime/RuntimeFwd.hpp"

namespace Aero::Controls::Detail {

class VisualStateManagerAccess final {
public:
    static Base::Result<VisualStateManager*> Create(
        Core::EffectiveValueEngine& values,
        TemplateManager& templates,
        Aero::Detail::AnimationManager& animations,
        Core::DependencyPropertyRegistry& properties) noexcept;
};

} // namespace Aero::Controls::Detail
