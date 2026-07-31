#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero { class FrameworkElement; }

namespace Aero::Media {

class AERO_API Effect : public Core::DependencyObject {
    AERO_DECLARE_TYPE(Effect, Core::DependencyObject)
public:
    Aero::FrameworkElement* Owner() const noexcept { return owner_; }
    void SetOwner(Aero::FrameworkElement* owner) noexcept { owner_ = owner; }

protected:
    explicit Effect(Core::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    Base::Result<void> OnPropertyInvalidated(
        Core::PropertyInvalidationFlags flags) noexcept override;

private:
    Aero::FrameworkElement* owner_ = nullptr;
};

class AERO_API BlurEffect final : public Effect {
    AERO_DECLARE_TYPE(BlurEffect, Effect)
public:
    BlurEffect() noexcept : Effect(StaticTypeId()) {}

    double Radius() const noexcept;
    Base::Result<void> SetRadius(double value) noexcept;

    inline static constexpr Members::Property<double> RadiusProperty{"Radius"};
};

class AERO_API DropShadowEffect final : public Effect {
    AERO_DECLARE_TYPE(DropShadowEffect, Effect)
public:
    DropShadowEffect() noexcept : Effect(StaticTypeId()) {}

    double BlurRadius() const noexcept;
    double Direction() const noexcept;
    double ShadowDepth() const noexcept;
    double Opacity() const noexcept;
    Base::Color Color() const noexcept;

    Base::Result<void> SetBlurRadius(double value) noexcept;
    Base::Result<void> SetDirection(double value) noexcept;
    Base::Result<void> SetShadowDepth(double value) noexcept;
    Base::Result<void> SetOpacity(double value) noexcept;
    Base::Result<void> SetColor(Base::Color value) noexcept;

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

    double Size() const noexcept;
    Base::Result<void> SetSize(double value) noexcept;

    inline static constexpr Members::Property<double> SizeProperty{"Size"};
};

} // namespace Aero::Media
