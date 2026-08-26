#pragma once

#include <Aero/Media/Geometry.hpp>

#include <cstdint>

namespace Aero::Media {

enum class GeometryCombineMode : std::uint8_t {
    Union = 0U,
    Intersect,
    Xor,
    Exclude
};

class AERO_GUI_API CombinedGeometry : public Geometry {
    AERO_DECLARE_TYPE(CombinedGeometry, Geometry)
public:
    CombinedGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Ref<Geometry> GetGeometry1() const noexcept { return geometry1_; }
    Ref<Geometry> GetGeometry2() const noexcept { return geometry2_; }
    GeometryCombineMode GetGeometryCombineMode() const noexcept {
        return GetValueOr(
            GeometryCombineModeProperty, GeometryCombineMode::Union);
    }
    void SetGeometry1(Ref<Geometry> value) noexcept;
    void SetGeometry2(Ref<Geometry> value) noexcept;
    void SetGeometryCombineMode(GeometryCombineMode value) noexcept {
        SetValue(GeometryCombineModeProperty, value);
    }
    inline static constexpr DependencyProperty<GeometryCombineMode>
        GeometryCombineModeProperty{"GeometryCombineMode"};
protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;
    bool FreezeCore(bool isChecking) noexcept override;
private:
    void OnChildChanged(Freezable&) noexcept;
    void AttachChild(Ref<Geometry>& slot, Ref<Geometry> value) noexcept;
    Ref<Geometry> geometry1_;
    Ref<Geometry> geometry2_;
    FreezableChangedHandler childChangedHandler_;
};
} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::GeometryCombineMode)
