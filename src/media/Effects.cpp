#include <Aero/Media/Effects.hpp>

#include "gui/GuiPrivate.hpp"
#include <Aero/FrameworkElement.hpp>

namespace Aero::Media {

void Effect::OnPropertyInvalidated(
    Meta::PropertyInvalidationFlags flags) noexcept {
    DependencyObject::OnPropertyInvalidated(flags);
    if (owner_ != nullptr) {
        static_cast<void>(
            Aero::GuiPrivate::Detail::ElementPrivate::
                InvalidateRenderState(*owner_));
    }
}

double BlurEffect::GetRadius() const noexcept {
    return GetValueOr(RadiusProperty, 5.0);
}

void BlurEffect::SetRadius(
    double value) noexcept {
    SetValue(RadiusProperty, value);
}

double DropShadowEffect::GetBlurRadius() const noexcept {
    return GetValueOr(BlurRadiusProperty, 5.0);
}

double DropShadowEffect::GetDirection() const noexcept {
    return GetValueOr(DirectionProperty, 315.0);
}

double DropShadowEffect::GetShadowDepth() const noexcept {
    return GetValueOr(ShadowDepthProperty, 5.0);
}

double DropShadowEffect::GetOpacity() const noexcept {
    return GetValueOr(OpacityProperty, 1.0);
}

Base::Color DropShadowEffect::GetColor() const noexcept {
    return GetValueOr(
        ColorProperty,
        Base::Color{0.0F, 0.0F, 0.0F, 1.0F});
}

void DropShadowEffect::SetBlurRadius(
    double value) noexcept {
    SetValue(BlurRadiusProperty, value);
}

void DropShadowEffect::SetDirection(
    double value) noexcept {
    SetValue(DirectionProperty, value);
}

void DropShadowEffect::SetShadowDepth(
    double value) noexcept {
    SetValue(ShadowDepthProperty, value);
}

void DropShadowEffect::SetOpacity(
    double value) noexcept {
    SetValue(OpacityProperty, value);
}

void DropShadowEffect::SetColor(
    Base::Color value) noexcept {
    SetValue(ColorProperty, value);
}

double PixelateEffect::GetSize() const noexcept {
    return GetValueOr(SizeProperty, 1.0);
}

void PixelateEffect::SetSize(double value) noexcept {
    SetValue(SizeProperty, value);
}

} // namespace Aero::Media
