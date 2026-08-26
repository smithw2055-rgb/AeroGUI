#pragma once

#include <Aero/Media/Transform3D.hpp>

namespace Aero::Media {

/// Camera-like perspective for descendants. Depth/Offset are consumed by
/// collapse, not by GetTransform3D() (which is identity). Inherited along the
/// visual tree via Transform3DContext — not via DP Inherits().
class AERO_GUI_API PerspectiveTransform3D : public Transform3D {
    AERO_DECLARE_TYPE(PerspectiveTransform3D, Transform3D)
public:
    PerspectiveTransform3D() noexcept : Transform3D(StaticTypeId()) {}

    double GetDepth() const noexcept {
        return GetValueOr(DepthProperty, Base::DefaultPerspectiveDepth);
    }
    double GetOffsetX() const noexcept { return GetValueOr(OffsetXProperty, 0.0); }
    double GetOffsetY() const noexcept { return GetValueOr(OffsetYProperty, 0.0); }

    void SetDepth(double value) noexcept { SetValue(DepthProperty, value); }
    void SetOffsetX(double value) noexcept { SetValue(OffsetXProperty, value); }
    void SetOffsetY(double value) noexcept { SetValue(OffsetYProperty, value); }

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    inline static constexpr DependencyProperty<double> DepthProperty{"Depth"};
    inline static constexpr DependencyProperty<double> OffsetXProperty{"OffsetX"};
    inline static constexpr DependencyProperty<double> OffsetYProperty{"OffsetY"};
};

} // namespace Aero::Media
