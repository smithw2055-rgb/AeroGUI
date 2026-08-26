#include <Aero/Media/Effects.hpp>

#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/media/MediaState.hpp"

namespace Aero::Media {

std::uint64_t EffectRuntime::Revision(
    const Effect& effect) noexcept {
    return AeroGuiInternal::FreezableRevision(effect);
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

Base::Color TintEffect::GetColor() const noexcept {
    return GetValueOr(ColorProperty, Base::Color{0.0F, 0.0F, 1.0F, 1.0F});
}

void TintEffect::SetColor(Base::Color value) noexcept {
    SetValue(ColorProperty, value);
}

double DirectionalBlurEffect::GetRadius() const noexcept {
    return GetValueOr(RadiusProperty, 0.0);
}

void DirectionalBlurEffect::SetRadius(double value) noexcept {
    SetValue(RadiusProperty, value);
}

double DirectionalBlurEffect::GetAngle() const noexcept {
    return GetValueOr(AngleProperty, 0.0);
}

void DirectionalBlurEffect::SetAngle(double value) noexcept {
    SetValue(AngleProperty, value);
}

void ShaderEffect::SynchronizePixelShaderCache() const noexcept {
    const String current = GetValueOr(PixelShaderProperty, String{});
    if (current.View() != source_.View()) {
        static_cast<void>(source_.Assign(current.View()));
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

Base::Result<void> ShaderEffect::SetBytecode(
    Base::Span<const std::uint8_t> value) noexcept {
    bytecode_.Clear();
    for (std::uint32_t index = 0U; index < value.Size(); ++index) {
        Base::Result<void> added = bytecode_.PushBack(value[index]);
        if (!added) return added.GetStatus();
    }
    return {};
}

void ShaderEffect::SetUniform(std::uint32_t index, float value) noexcept {
    if (index >= uniforms_.size()) return;
    uniforms_[index] = value;
    if (index + 1U > uniformCount_) {
        uniformCount_ = index + 1U;
    }
}

} // namespace Aero::Media
