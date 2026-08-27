#include <Aero/Media/Pen.hpp>

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
        (void)brush_->AddChangedHandlerChecked(brushChangedHandler_);
    }
    WritePostscript();
}

double Pen::GetThickness() const noexcept {
    return GetValueOr(ThicknessProperty, 1.0);
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
        (void)dashStyle_->AddChangedHandlerChecked(dashStyleChangedHandler_);
    }
    WritePostscript();
}

PenLineJoin Pen::GetLineJoin() const noexcept {
    return GetValueOr(LineJoinProperty, PenLineJoin::Miter);
}

void Pen::SetLineJoin(PenLineJoin value) noexcept {
    SetValue(LineJoinProperty, value);
}

PenLineCap Pen::GetStartLineCap() const noexcept {
    return GetValueOr(StartLineCapProperty, PenLineCap::Flat);
}

void Pen::SetStartLineCap(PenLineCap value) noexcept {
    SetValue(StartLineCapProperty, value);
}

PenLineCap Pen::GetEndLineCap() const noexcept {
    return GetValueOr(EndLineCapProperty, PenLineCap::Flat);
}

void Pen::SetEndLineCap(PenLineCap value) noexcept {
    SetValue(EndLineCapProperty, value);
}

double Pen::GetMiterLimit() const noexcept {
    return GetValueOr(MiterLimitProperty, 10.0);
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
