#include <Aero/Media/Effects.hpp>

#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>
namespace Aero::Media {

std::uint64_t Effect::GetRevision() const noexcept {
    return (*this).Revision();
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

// Metadata registration for the types implemented in this file.
AERO_DESCRIBE(::Aero::Media::Effect) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<Effect>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(::Aero::Media::BlurEffect) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<BlurEffect>(context)
            .Property(BlurEffect::RadiusProperty, 5.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::DropShadowEffect) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<DropShadowEffect>(context)
            .Property(DropShadowEffect::BlurRadiusProperty, 5.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Property(DropShadowEffect::DirectionProperty, 315.0, AffectsRender)
            .Property(DropShadowEffect::ShadowDepthProperty, 5.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Property(DropShadowEffect::OpacityProperty, 1.0, AffectsRender, &ValidateUnitDouble)
            .Property(DropShadowEffect::ColorProperty, Base::Color{ 0.0F, 0.0F, 0.0F, 1.0F}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::PixelateEffect) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<PixelateEffect>(context)
            .Property(PixelateEffect::SizeProperty, 1.0, AffectsRender, &Base::Validate::Positive<double>)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::TintEffect) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<TintEffect>(context)
            .Property(TintEffect::ColorProperty, Base::Color{0.0F, 0.0F, 1.0F, 1.0F}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::DirectionalBlurEffect) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<DirectionalBlurEffect>(context)
            .Property(DirectionalBlurEffect::RadiusProperty, 0.0, AffectsRender, &Base::Validate::NonNegative<double>)
            .Property(DirectionalBlurEffect::AngleProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::ShaderEffect) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<ShaderEffect>(context)
            .Property(ShaderEffect::PixelShaderProperty, FrameworkPropertyMetadata(Base::String{}, AffectsRender).Changed(&ShaderEffect::OnPixelShaderChanged))
            .Factory();
}
