#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>

namespace Aero { class FrameworkElement; }

namespace Aero::Media {

class AERO_GUI_API Effect : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Effect, ::Aero::Freezable)
public:

protected:
    explicit Effect(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
};

class AERO_GUI_API BlurEffect : public Effect {
    AERO_DECLARE_TYPE(BlurEffect, Effect)
public:
    BlurEffect() noexcept : Effect(StaticTypeId()) {}

    double GetRadius() const noexcept;
    void SetRadius(double value) noexcept;

    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"};
};

class AERO_GUI_API DropShadowEffect : public Effect {
    AERO_DECLARE_TYPE(DropShadowEffect, Effect)
public:
    DropShadowEffect() noexcept : Effect(StaticTypeId()) {}

    double GetBlurRadius() const noexcept;
    double GetDirection() const noexcept;
    double GetShadowDepth() const noexcept;
    double GetOpacity() const noexcept;
    Base::Color GetColor() const noexcept;

    void SetBlurRadius(double value) noexcept;
    void SetDirection(double value) noexcept;
    void SetShadowDepth(double value) noexcept;
    void SetOpacity(double value) noexcept;
    void SetColor(Base::Color value) noexcept;

    inline static constexpr DependencyProperty<double> BlurRadiusProperty{"BlurRadius"};
    inline static constexpr DependencyProperty<double> DirectionProperty{"Direction"};
    inline static constexpr DependencyProperty<double> ShadowDepthProperty{"ShadowDepth"};
    inline static constexpr DependencyProperty<double> OpacityProperty{"Opacity"};
    inline static constexpr DependencyProperty<Base::Color> ColorProperty{"Color"};
};

// Gallery's custom ShaderEffect contract. Rendering backends receive the
// authored pixel size through the effect snapshot; Size remains a dependency
// property so bindings can update it at runtime.
class AERO_GUI_API PixelateEffect : public Effect {
    AERO_DECLARE_TYPE(PixelateEffect, Effect)
public:
    PixelateEffect() noexcept : Effect(StaticTypeId()) {}

    double GetSize() const noexcept;
    void SetSize(double value) noexcept;

    inline static constexpr DependencyProperty<double> SizeProperty{"Size"};
};

// AeroGUIExtensions-compatible color multiply effect. The GPU channel that
// applies the tint is implemented by the rendering backends.
class AERO_GUI_API TintEffect : public Effect {
    AERO_DECLARE_TYPE(TintEffect, Effect)
public:
    TintEffect() noexcept : Effect(StaticTypeId()) {}

    Base::Color GetColor() const noexcept {
        return GetValueOr(ColorProperty, Base::Color{0.0F, 0.0F, 1.0F, 1.0F});
    }
    void SetColor(Base::Color value) noexcept {
        SetValue(ColorProperty, value);
    }

    inline static constexpr DependencyProperty<Base::Color> ColorProperty{"Color"};
};

// AeroGUIExtensions-compatible angled blur effect. The GPU channel is
// implemented by the rendering backends.
class AERO_GUI_API DirectionalBlurEffect : public Effect {
    AERO_DECLARE_TYPE(DirectionalBlurEffect, Effect)
public:
    DirectionalBlurEffect() noexcept : Effect(StaticTypeId()) {}

    double GetRadius() const noexcept {
        return GetValueOr(RadiusProperty, 0.0);
    }
    void SetRadius(double value) noexcept {
        SetValue(RadiusProperty, value);
    }
    double GetAngle() const noexcept {
        return GetValueOr(AngleProperty, 0.0);
    }
    void SetAngle(double value) noexcept {
        SetValue(AngleProperty, value);
    }

    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"};
    inline static constexpr DependencyProperty<double> AngleProperty{"Angle"};
};

} // namespace Aero::Media
