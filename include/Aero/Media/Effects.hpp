#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero { class FrameworkElement; }
namespace Aero::Internal { class EffectPrivate; }

namespace Aero::Media {

class AERO_API Effect : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(Effect, ::Aero::DependencyObject)
public:
protected:
    explicit Effect(Meta::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    void OnPropertyInvalidated(
        Meta::PropertyInvalidationFlags flags) noexcept override;

private:
    friend class ::Aero::Internal::EffectPrivate;
    Aero::FrameworkElement* owner_ = nullptr;
};

class AERO_API BlurEffect final : public Effect {
    AERO_DECLARE_TYPE(BlurEffect, Effect)
public:
    BlurEffect() noexcept : Effect(StaticTypeId()) {}

    double GetRadius() const noexcept;
    void SetRadius(double value) noexcept;

    inline static constexpr Members::Property<double> RadiusProperty{"Radius"};
};

class AERO_API DropShadowEffect final : public Effect {
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

    inline static constexpr Members::Property<double> BlurRadiusProperty{"BlurRadius"};
    inline static constexpr Members::Property<double> DirectionProperty{"Direction"};
    inline static constexpr Members::Property<double> ShadowDepthProperty{"ShadowDepth"};
    inline static constexpr Members::Property<double> OpacityProperty{"Opacity"};
    inline static constexpr Members::Property<Base::Color> ColorProperty{"Color"};
};

// Gallery's custom ShaderEffect contract. Rendering backends receive the
// authored pixel size through the effect snapshot; Size remains a dependency
// property so bindings can update it at runtime.
class AERO_API PixelateEffect final : public Effect {
    AERO_DECLARE_TYPE(PixelateEffect, Effect)
public:
    PixelateEffect() noexcept : Effect(StaticTypeId()) {}

    double GetSize() const noexcept;
    void SetSize(double value) noexcept;

    inline static constexpr Members::Property<double> SizeProperty{"Size"};
};

} // namespace Aero::Media
