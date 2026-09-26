#pragma once

#include <Aero/Animatable.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Brushes.hpp>

#include <cstdint>

namespace Aero::Media {

enum class PenLineJoin : std::uint8_t { Miter = 0U, Bevel, Round };
enum class PenLineCap : std::uint8_t { Flat = 0U, Square, Round, Triangle };

class AERO_GUI_API DashStyle : public Animatable {
    AERO_DECLARE_TYPE(DashStyle, Animatable)
public:
    DashStyle() noexcept : Animatable(StaticTypeId()) {}

    Span<const double> GetDashes() const noexcept { return {dashes_.Data(), dashes_.Size()}; }
    double GetOffset() const noexcept { return offset_; }

    void SetDashes(Span<const double> value) noexcept;
    void SetOffset(double value) noexcept;

private:
    Base::Vector<double> dashes_;
    double offset_ = 0.0;
};

class AERO_GUI_API Pen : public Animatable {
    AERO_DECLARE_TYPE(Pen, Animatable)
public:
    Pen() noexcept : Animatable(StaticTypeId()) {}
    ~Pen() override = default;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    Ref<Brush> GetBrush() const noexcept;
    void SetBrush(Ref<Brush> value) noexcept;
    double GetThickness() const noexcept;
    void SetThickness(double value) noexcept;
    Ref<DashStyle> GetDashStyle() const noexcept;
    void SetDashStyle(Ref<DashStyle> value) noexcept;
    PenLineJoin GetLineJoin() const noexcept;
    void SetLineJoin(PenLineJoin value) noexcept;
    PenLineCap GetStartLineCap() const noexcept;
    void SetStartLineCap(PenLineCap value) noexcept;
    PenLineCap GetEndLineCap() const noexcept;
    void SetEndLineCap(PenLineCap value) noexcept;
    double GetMiterLimit() const noexcept;
    void SetMiterLimit(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<Brush>, Brush);
    AERO_DEPENDENCY_PROPERTY(double, Thickness);
    AERO_DEPENDENCY_PROPERTY(Ref<DashStyle>, DashStyle);
    AERO_DEPENDENCY_PROPERTY(PenLineJoin, LineJoin);
    AERO_DEPENDENCY_PROPERTY(PenLineCap, StartLineCap);
    AERO_DEPENDENCY_PROPERTY(PenLineCap, EndLineCap);
    AERO_DEPENDENCY_PROPERTY(double, MiterLimit);

protected:
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnBrushChanged(Freezable&) noexcept;
    void OnDashStyleChanged(Freezable&) noexcept;
    Ref<Brush> brush_;
    Ref<DashStyle> dashStyle_;
    FreezableChangedHandler brushChangedHandler_;
    FreezableChangedHandler dashStyleChangedHandler_;
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::PenLineJoin)
AERO_DECLARE_TYPE_ENUM(Aero::Media::PenLineCap)
