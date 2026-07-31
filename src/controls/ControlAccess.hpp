#pragma once

#include <Aero/Controls/Base.hpp>

namespace Aero::Controls::Detail {

class ControlAccess final {
public:
    static bool IsTemplateApplied(const Control& control) noexcept {
        return control.templateHandleValue_ != 0U;
    }

    static std::uint64_t TemplateGeneration(const Control& control) noexcept {
        return control.templateGeneration_;
    }

    static UIElement* TemplateRoot(const Control& control) noexcept {
        return control.templateChild_;
    }

    static Base::Result<void> SetTemplateRoot(Control& control, UIElement* child) noexcept {
        return control.SetTemplateChildCore(child);
    }
};

} // namespace Aero::Controls::Detail
