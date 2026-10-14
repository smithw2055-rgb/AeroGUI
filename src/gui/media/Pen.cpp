#include <Aero/Media/Pen.hpp>
#include "gui/core/Describe.hpp"
#include "gui/core/ValueConversion.hpp"
#include <utility>

namespace Aero::Media {

Ref<Brush> Pen::GetBrush() const noexcept {
    return brush_;
}

void Pen::SetBrush(Ref<Brush> value) noexcept {
    if (!WritePreamble() || brush_.Get() == value.Get()) return;
    if (brush_) {
        (void)brush_->RemoveChangedHandler(brushChangedHandler_);
    }
    brush_ = std::move(value);
    if (brush_) {
        brushChangedHandler_ = FreezableChangedHandler(
            this, &Pen::OnBrushChanged);
        (void)brush_->AddChangedHandler(brushChangedHandler_);
    }
    WritePostscript();
}

double Pen::GetThickness() const noexcept {
    return GetValue(ThicknessProperty);
}

void Pen::SetThickness(double value) noexcept {
    SetValue(ThicknessProperty, value);
}

Ref<DashStyle> Pen::GetDashStyle() const noexcept {
    return dashStyle_;
}

void Pen::SetDashStyle(Ref<DashStyle> value) noexcept {
    if (!WritePreamble() || dashStyle_.Get() == value.Get()) return;
    if (dashStyle_) {
        (void)dashStyle_->RemoveChangedHandler(dashStyleChangedHandler_);
    }
    dashStyle_ = std::move(value);
    if (dashStyle_) {
        dashStyleChangedHandler_ = FreezableChangedHandler(
            this, &Pen::OnDashStyleChanged);
        (void)dashStyle_->AddChangedHandler(dashStyleChangedHandler_);
    }
    WritePostscript();
}

PenLineJoin Pen::GetLineJoin() const noexcept {
    return GetValue(LineJoinProperty);
}

void Pen::SetLineJoin(PenLineJoin value) noexcept {
    SetValue(LineJoinProperty, value);
}

PenLineCap Pen::GetStartLineCap() const noexcept {
    return GetValue(StartLineCapProperty);
}

void Pen::SetStartLineCap(PenLineCap value) noexcept {
    SetValue(StartLineCapProperty, value);
}

PenLineCap Pen::GetEndLineCap() const noexcept {
    return GetValue(EndLineCapProperty);
}

void Pen::SetEndLineCap(PenLineCap value) noexcept {
    SetValue(EndLineCapProperty, value);
}

double Pen::GetMiterLimit() const noexcept {
    return GetValue(MiterLimitProperty);
}

void Pen::SetMiterLimit(double value) noexcept {
    SetValue(MiterLimitProperty, value);
}

void Pen::OnBrushChanged(Freezable&) noexcept {
    WritePostscript();
}

void Pen::OnDashStyleChanged(Freezable&) noexcept {
    WritePostscript();
}

bool Pen::FreezeCore(bool isChecking) noexcept {
    if (brush_) {
        if (isChecking) {
            if (!brush_->CanFreeze()) return false;
        } else {
            static_cast<void>(brush_->Freeze());
        }
    }
    if (dashStyle_) {
        if (isChecking) {
            if (!dashStyle_->CanFreeze()) return false;
        } else {
            static_cast<void>(dashStyle_->Freeze());
        }
    }
    return Freezable::FreezeCore(isChecking);
}

} // namespace Aero::Media

// Metadata registration for the types implemented in this file.
namespace Aero::Media {

AERO_DESCRIBE(DashStyle) {
    using namespace Aero::Meta;
    Register<DashStyle>(context)
            .Factory();
}

AERO_DESCRIBE(Pen) {
    using namespace Aero::Meta;
    Register<Pen>(context)
            .Property<Base::Ref<Brush>, &Pen::GetBrush, &Pen::SetBrush>("Brush", PropertyFlags::None)
            .Property(Pen::ThicknessProperty, 1.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::NonNegative<double>)
            .Property<Base::Ref<DashStyle>, &Pen::GetDashStyle, &Pen::SetDashStyle>("DashStyle", PropertyFlags::None)
            .Property(Pen::LineJoinProperty, PenLineJoin::Miter)
            .Property(Pen::StartLineCapProperty, PenLineCap::Flat)
            .Property(Pen::EndLineCapProperty, PenLineCap::Flat)
            .Property(Pen::MiterLimitProperty, 10.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::NonNegative<double>)
            .Factory();
}

} // namespace Aero::Media

