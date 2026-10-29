#include <Aero/Media/Effects.hpp>

#include "gui/core/Describe.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include <utility>

namespace Aero::Media {

std::uint64_t Effect::GetRevision() const noexcept {
    return (*this).Revision();
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

// Metadata registration for the types implemented in this file.
namespace Aero::Media {

AERO_DESCRIBE(Effect) {
    using namespace Aero::Meta;
    Register<Effect>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(BlurEffect) {
    using namespace Aero::Meta;
    Register<BlurEffect>(context)
            .Property(BlurEffect::RadiusProperty, 5.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Factory();
}

AERO_DESCRIBE(DropShadowEffect) {
    using namespace Aero::Meta;
    Register<DropShadowEffect>(context)
            .Property(DropShadowEffect::BlurRadiusProperty, 5.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Property(DropShadowEffect::DirectionProperty, 315.0, AffectsRender)
            .Property(DropShadowEffect::ShadowDepthProperty, 5.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Property(DropShadowEffect::OpacityProperty, 1.0, AffectsRender, &ValidateUnitDouble)
            .Property(DropShadowEffect::ColorProperty, Base::Color{ 0.0F, 0.0F, 0.0F, 1.0F}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(PixelateEffect) {
    using namespace Aero::Meta;
    Register<PixelateEffect>(context)
            .Property(PixelateEffect::SizeProperty, 1.0, AffectsRender, &Base::Validate::Positive<double>)
            .Factory();
}

AERO_DESCRIBE(TintEffect) {
    using namespace Aero::Meta;
    Register<TintEffect>(context)
            .Property(TintEffect::ColorProperty, Base::Color{0.0F, 0.0F, 1.0F, 1.0F}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(DirectionalBlurEffect) {
    using namespace Aero::Meta;
    Register<DirectionalBlurEffect>(context)
            .Property(DirectionalBlurEffect::RadiusProperty, 0.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Property(DirectionalBlurEffect::AngleProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(ShaderEffect) {
    using namespace Aero::Meta;
    Register<ShaderEffect>(context)
            .Property(ShaderEffect::PixelShaderProperty, FrameworkPropertyMetadata(Base::String{}, AffectsRender).Changed(&ShaderEffect::OnPixelShaderChanged))
            .Factory();
}

} // namespace Aero::Media

