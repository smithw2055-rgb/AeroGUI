#pragma once

#include <Aero/Freezable.hpp>
#include <Aero/Media/Brush.hpp>
#include <Aero/Media/DashStyle.hpp>

#include <cstdint>

namespace Aero::Media {

enum class PenLineJoin : std::uint8_t { Miter = 0U, Bevel, Round };
enum class PenLineCap : std::uint8_t { Flat = 0U, Square, Round, Triangle };

class AERO_GUI_API Pen : public Freezable {
    AERO_DECLARE_TYPE(Pen, Freezable)
public:
    Pen() noexcept : Freezable(StaticTypeId()) {}
    ~Pen() override = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

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

    inline static constexpr DependencyProperty<Ref<Brush>> BrushProperty{"Brush"};
    inline static constexpr DependencyProperty<double> ThicknessProperty{"Thickness"};
    inline static constexpr DependencyProperty<Ref<DashStyle>> DashStyleProperty{"DashStyle"};
    inline static constexpr DependencyProperty<PenLineJoin> LineJoinProperty{"LineJoin"};
    inline static constexpr DependencyProperty<PenLineCap> StartLineCapProperty{"StartLineCap"};
    inline static constexpr DependencyProperty<PenLineCap> EndLineCapProperty{"EndLineCap"};
    inline static constexpr DependencyProperty<double> MiterLimitProperty{"MiterLimit"};

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
