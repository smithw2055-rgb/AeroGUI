#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/HorizontalAlignment.hpp>
#include <Aero/Media/Transform.hpp>

#include <cstdint>

namespace Aero::Media {
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::DependencyPropertyRef;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::PropertyInvalidationFlags;
using ::Aero::Meta::TypeId;
using Color = Base::Color;
using Point = Base::Point;
using Rect = Base::Rect;
using ::Aero::HorizontalAlignment;
using ::Aero::VerticalAlignment;

enum class TileMode : std::uint8_t {
    None = 0U,
    Tile,
    FlipX,
    FlipY,
    FlipXY
};

enum class BrushMappingMode : std::uint8_t {
    RelativeToBoundingBox = 0U,
    Absolute
};

enum class GradientSpreadMethod : std::uint8_t {
    Pad = 0U,
    Reflect,
    Repeat
};

class AERO_GUI_API Brush : public Freezable {
    AERO_DECLARE_TYPE(Brush, Freezable)
public:

    double GetOpacity() const noexcept;
    void SetOpacity(double value) noexcept;
    Ref<Base::Object> GetShader() const noexcept {
        return GetValueOr(
            ShaderProperty, Ref<Base::Object>{});
    }
    void SetShader(
        Ref<Base::Object> value) noexcept {
        SetValue(ShaderProperty, std::move(value));
    }
    Ref<Transform> GetRelativeTransform() const noexcept {
        return GetValueOr(
            RelativeTransformProperty,
            Ref<Transform>{});
    }
    void SetRelativeTransform(
        Ref<Transform> value) noexcept {
        SetValue(RelativeTransformProperty, std::move(value));
    }

    std::uint64_t GetRevision() const noexcept;

    inline static constexpr DependencyProperty<double> OpacityProperty{"Opacity"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ShaderProperty{"Shader"};
    inline static constexpr DependencyProperty<Ref<Transform>> RelativeTransformProperty{"RelativeTransform"};

protected:
    explicit Brush(TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
    ~Brush() override = default;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
};
} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::TileMode)
AERO_DECLARE_TYPE_ENUM(Aero::Media::BrushMappingMode)
AERO_DECLARE_TYPE_ENUM(Aero::Media::GradientSpreadMethod)
