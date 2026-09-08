#include <Aero/Media/Effects.hpp>

#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/media/MediaHelpers.hpp"

namespace Aero::Media {

std::uint64_t Effect::GetRevision() const noexcept {
    return AeroGuiInternal::FreezableRevision(*this);
}

double BlurEffect::GetRadius() const noexcept {
    return GetValue(RadiusProperty);
}

void BlurEffect::SetRadius(
    double value) noexcept {
    SetValue(RadiusProperty, value);
}

double DropShadowEffect::GetBlurRadius() const noexcept {
    return GetValue(BlurRadiusProperty);
}

double DropShadowEffect::GetDirection() const noexcept {
    return GetValue(DirectionProperty);
}

double DropShadowEffect::GetShadowDepth() const noexcept {
    return GetValue(ShadowDepthProperty);
}

double DropShadowEffect::GetOpacity() const noexcept {
    return GetValue(OpacityProperty);
}

Base::Color DropShadowEffect::GetColor() const noexcept {
    return GetValue(ColorProperty);
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
    return GetValue(SizeProperty);
}

void PixelateEffect::SetSize(double value) noexcept {
    SetValue(SizeProperty, value);
}

Base::Color TintEffect::GetColor() const noexcept {
    return GetValue(ColorProperty);
}

void TintEffect::SetColor(Base::Color value) noexcept {
    SetValue(ColorProperty, value);
}

double DirectionalBlurEffect::GetRadius() const noexcept {
    return GetValue(RadiusProperty);
}

void DirectionalBlurEffect::SetRadius(double value) noexcept {
    SetValue(RadiusProperty, value);
}

double DirectionalBlurEffect::GetAngle() const noexcept {
    return GetValue(AngleProperty);
}

void DirectionalBlurEffect::SetAngle(double value) noexcept {
    SetValue(AngleProperty, value);
}

void ShaderEffect::SynchronizePixelShaderCache() const noexcept {
    const StringView current = GetValue(PixelShaderProperty);
    if (current != source_.View()) {
        static_cast<void>(source_.Assign(current));
    }
}

StringView ShaderEffect::GetPixelShader() const noexcept {
    SynchronizePixelShaderCache();
    return source_.View();
}

void ShaderEffect::SetPixelShader(Base::StringView value) noexcept {
    String stored;
    static_cast<void>(stored.Assign(value));
    SetValue(PixelShaderProperty, std::move(stored));
    SynchronizePixelShaderCache();
}

void ShaderEffect::OnPixelShaderChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    static_cast<ShaderEffect&>(object).SynchronizePixelShaderCache();
}

void ShaderEffect::SetBytecode(
    Base::Span<const std::uint8_t> value) noexcept {
    bytecode_.Clear();
    for (std::uint32_t index = 0U; index < value.Size(); ++index) {
        bytecode_.PushBack(value[index]);
    }
}

void ShaderEffect::SetUniform(std::uint32_t index, float value) noexcept {
    if (index >= uniforms_.size()) return;
    uniforms_[index] = value;
    if (index + 1U > uniformCount_) {
        uniformCount_ = index + 1U;
    }
}

} // namespace Aero::Media
