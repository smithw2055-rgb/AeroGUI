#pragma once

// Helpers shared by the object writer and XamlMarkupExtensions.cpp.
// Control and engine headers stay in the translation units that use them.

#include "gui/markup/MarkupExtensionContract.hpp"
#include <Aero/Base/String.hpp>

namespace Aero::Controls {
class ControlTemplate;
}

namespace Aero::Markup {

Base::Result<long double> ReadConstantBindingNumber(
    const Meta::Value& value) noexcept;
Base::Result<Meta::Value> ConvertConstantBindingValue(
    const Meta::Value& value,
    Meta::TypeId targetType) noexcept;
Base::Result<ProvidedValue> CreateMultiBindingValue(
    Data::MultiBinding& binding,
    const ExtensionServices& services) noexcept;
Base::Result<void> CaptureControlTemplateChildName(
    Controls::ControlTemplate& controlTemplate,
    const Aero::NameScope* nameScope,
    Base::Object& target,
    Base::String& storage) noexcept;

} // namespace Aero::Markup
