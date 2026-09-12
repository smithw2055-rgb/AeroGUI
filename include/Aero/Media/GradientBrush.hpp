#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Brush.hpp>
#include <Aero/Media/GradientStop.hpp>

namespace Aero::Media {

class AERO_GUI_API GradientBrush : public Brush {
    AERO_DECLARE_TYPE(GradientBrush, Brush)
public:
    Span<const Ref<GradientStop>>
        GetGradientStops() const noexcept {
        return stops_.AsSpan();
    }
    void AddGradientStop(
        Ref<GradientStop> stop) noexcept;
    void ClearGradientStops() noexcept;
    BrushMappingMode GetMappingMode() const noexcept;
    void SetMappingMode(BrushMappingMode value) noexcept;
    GradientSpreadMethod GetSpreadMethod() const noexcept;
    void SetSpreadMethod(GradientSpreadMethod value) noexcept;

    AERO_DEPENDENCY_PROPERTY(BrushMappingMode, MappingMode);
    AERO_DEPENDENCY_PROPERTY(GradientSpreadMethod, SpreadMethod);

protected:
    explicit GradientBrush(TypeId runtimeType) noexcept
        : Brush(runtimeType),
          stops_(&Base::GetDefaultAllocator()) {}
    ~GradientBrush() override;
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnGradientStopChanged(Freezable&) noexcept;
    Base::Vector<Ref<GradientStop>> stops_;
    FreezableChangedHandler stopChangedHandler_;
};
} // namespace Aero::Media
