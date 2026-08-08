#include <Aero/Media/Effects.hpp>

#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "media/AnimationInternal.hpp"
#include "media/BrushInternal.hpp"
#include "media/EffectInternal.hpp"
#include "media/TransformInternal.hpp"

namespace Aero::Media {

std::uint64_t Effect::Impl::Revision(
    const Effect& effect) noexcept {
    return Freezable::Impl::Revision(effect);
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
