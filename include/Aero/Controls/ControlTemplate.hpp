#pragma once

#include <Aero/FrameworkTemplate.hpp>

namespace Aero::Controls {

class AERO_GUI_API ControlTemplate : public Aero::FrameworkTemplate {
    AERO_DECLARE_TYPE(ControlTemplate, FrameworkTemplate)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    ControlTemplate() noexcept = default;
    ~ControlTemplate() noexcept override = default;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
};

} // namespace Aero::Controls
