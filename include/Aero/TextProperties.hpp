#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero {

// Public owner of the WPF-shaped attached Text.* properties used by gallery
// XAML (`aero:Text.Placeholder` on PasswordBox/TextBox). The XAML type name is
// Text; the C++ type is TextProperties because namespace Aero::Text already
// exists for font services.
class AERO_GUI_API TextProperties : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TextProperties, Base::Object, "urn:aero", "Text")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, std::uint32_t>
        PasswordLengthProperty{"PasswordLength"};
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, Base::String>
        PlaceholderProperty{"Placeholder"};
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, Value>
        StrokeProperty{"Stroke"};
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, double>
        StrokeThicknessProperty{"StrokeThickness"};

    static void OnCompatibilityPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

} // namespace Aero
