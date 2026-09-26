#include <Aero/Media/Pen.hpp>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/EventTrigger.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
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
AERO_DESCRIBE(::Aero::Media::DashStyle) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media;
    Register<DashStyle>(context)
            .Factory();
}

AERO_DESCRIBE(::Aero::Media::Pen) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media;
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
