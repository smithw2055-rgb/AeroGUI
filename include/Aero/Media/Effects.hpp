#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>

namespace Aero { class FrameworkElement; }

namespace Aero::Media {

class AERO_API Effect : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Effect, ::Aero::Freezable)
public:
    struct Impl;

protected:
    explicit Effect(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}

private:
    friend struct Impl;
};

class AERO_API BlurEffect : public Effect {
    AERO_DECLARE_TYPE(BlurEffect, Effect)
public:
    BlurEffect() noexcept : Effect(StaticTypeId()) {}

    double GetRadius() const noexcept;
    void SetRadius(double value) noexcept;

    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"};
};

class AERO_API DropShadowEffect : public Effect {
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
class AERO_API PixelateEffect : public Effect {
    AERO_DECLARE_TYPE(PixelateEffect, Effect)
public:
    PixelateEffect() noexcept : Effect(StaticTypeId()) {}

    double GetSize() const noexcept;
    void SetSize(double value) noexcept;

    inline static constexpr DependencyProperty<double> SizeProperty{"Size"};
};

} // namespace Aero::Media
