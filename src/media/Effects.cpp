#include <Aero/Media/Effects.hpp>

#include <Aero/FrameworkElement.hpp>

namespace Aero::Media {

Base::Result<void> Effect::OnPropertyInvalidated(
    Core::PropertyInvalidationFlags flags) noexcept {
    Base::Result<void> base =
        DependencyObject::OnPropertyInvalidated(flags);
    if (!base) return base.GetStatus();
    return owner_ != nullptr
        ? owner_->InvalidateRender()
        : Base::Result<void>();
}

double BlurEffect::Radius() const noexcept {
    return GetValueOr(RadiusProperty, 5.0);
}

Base::Result<void> BlurEffect::SetRadius(
    double value) noexcept {
    return SetValue(RadiusProperty, value);
}

double DropShadowEffect::BlurRadius() const noexcept {
    return GetValueOr(BlurRadiusProperty, 5.0);
}

double DropShadowEffect::Direction() const noexcept {
    return GetValueOr(DirectionProperty, 315.0);
}

double DropShadowEffect::ShadowDepth() const noexcept {
    return GetValueOr(ShadowDepthProperty, 5.0);
}

double DropShadowEffect::Opacity() const noexcept {
    return GetValueOr(OpacityProperty, 1.0);
}

Base::Color DropShadowEffect::Color() const noexcept {
    return GetValueOr(
        ColorProperty,
        Base::Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Base::Result<void> DropShadowEffect::SetBlurRadius(
    double value) noexcept {
    return SetValue(BlurRadiusProperty, value);
}

Base::Result<void> DropShadowEffect::SetDirection(
    double value) noexcept {
    return SetValue(DirectionProperty, value);
}

Base::Result<void> DropShadowEffect::SetShadowDepth(
    double value) noexcept {
    return SetValue(ShadowDepthProperty, value);
}

Base::Result<void> DropShadowEffect::SetOpacity(
    double value) noexcept {
    return SetValue(OpacityProperty, value);
}

Base::Result<void> DropShadowEffect::SetColor(
    Base::Color value) noexcept {
    return SetValue(ColorProperty, value);
}

double PixelateEffect::Size() const noexcept {
    return GetValueOr(SizeProperty, 1.0);
}

Base::Result<void> PixelateEffect::SetSize(double value) noexcept {
    return SetValue(SizeProperty, value);
}

} // namespace Aero::Media
