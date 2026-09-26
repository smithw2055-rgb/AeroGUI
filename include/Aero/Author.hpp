#pragma once

// Control-author entry. One include for a custom control: dependency
// properties, Register<T>, and input class handlers. Application hosts
// still enter through <AeroApp/App.hpp>; this header does not pull Window.

#include <Aero/ClassHandler.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Meta.hpp>

// class RatingControl : public Aero::Controls::Control {
//     AERO_DECLARE_TYPE(RatingControl, Aero::Controls::Control)
// public:
//     AERO_DEPENDENCY_PROPERTY(double, Rating);
//     static void DescribeComponent(
//         Aero::Meta::TypeBuilder<RatingControl>& type) noexcept;
// protected:
//     void OnKeyDown(Aero::KeyEventArgs& args);
// };
//
// void RatingControl::DescribeComponent(
//     Aero::Meta::TypeBuilder<RatingControl>& type) noexcept {
//     type.Property(RatingControl::RatingProperty, 0.0);
//     AERO_ON(RatingControl, &RatingControl::OnKeyDown,
//         Aero::UIElement::KeyDownEvent);
// }
//
// inline constexpr Aero::ModuleRegistration RatingModule =
//     Aero::DefineComponentModule<RatingControl>("App.Rating");
//
// DefineComponentModule registers the type and its default factory.
// DescribeComponent is optional. Input OnMouse* / OnKey* methods are not
// virtual. AERO_ON installs the most-derived handler for that routed event.
// Call the base method by name when the base behavior should still run.
