#include <Aero/Media/Brushes.hpp>
#include "BrushRendering.hpp"

#include <algorithm>
#include <cmath>

namespace Aero::Media {

double Brush::Opacity() const noexcept {
    return GetValueOr(OpacityProperty, 1.0);
}

Base::Result<void> Brush::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    Base::Result<void> base =
        DependencyObject::OnPropertyInvalidated(
            flags);
    if (!base) return base.GetStatus();
    return owner_ != nullptr
        ? owner_->InvalidateRender()
        : Base::Result<void>();
}

Base::Result<void> Brush::SetOpacity(
    double value) noexcept {
    return SetValue(OpacityProperty, value);
}

Color SolidColorBrush::GetColor() const noexcept {
    return GetValueOr(ColorProperty, initialColor_);
}

Base::Result<void> SolidColorBrush::SetColor(
    Color value) noexcept {
    return SetValue(ColorProperty, value);
}

double GradientStop::Offset() const noexcept {
    return GetValueOr(OffsetProperty, 0.0);
}

Color GradientStop::GetColor() const noexcept {
    return GetValueOr(ColorProperty, Color{});
}

Base::Result<void> GradientStop::SetOffset(
    double value) noexcept {
    return SetValue(OffsetProperty, value);
}

Base::Result<void> GradientStop::SetColor(
    Color value) noexcept {
    return SetValue(ColorProperty, value);
}

Base::Result<void> GradientStop::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    Base::Result<void> base =
        DependencyObject::OnPropertyInvalidated(
            flags);
    if (!base) return base.GetStatus();
    FrameworkElement* visualOwner =
        owner_ != nullptr
        ? owner_->Owner()
        : nullptr;
    return visualOwner != nullptr
        ? visualOwner->InvalidateRender()
        : Base::Result<void>();
}

Base::Result<void> GradientBrush::AddGradientStop(
    Base::Ref<GradientStop> stop) noexcept {
    if (!stop) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GradientStop cannot be null");
    }
    GradientStop* retained = stop.Get();
    retained->SetOwner(this);
    Base::Result<void> added =
        stops_.TryPushBack(std::move(stop));
    if (!added) {
        retained->SetOwner(nullptr);
    }
    return added;
}

Base::Result<void> GradientStopCollection::Add(
    Base::Ref<GradientStop> stop) noexcept {
    if (!stop) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GradientStopCollection item cannot be null");
    }
    Base::Result<void> added =
        stops_.TryPushBack(std::move(stop));
    if (!added) return added.GetStatus();
    if (!changed_.Empty()) {
        changed_.Invoke({
            Collections::ItemsChangeAction::Add,
            UINT32_MAX,
            stops_.Size() - 1U,
            0U,
            1U});
    }
    return {};
}

GradientBrush::~GradientBrush() {
    ClearGradientStops();
}

void GradientBrush::ClearGradientStops() noexcept {
    for (const Base::Ref<GradientStop>& stop :
         stops_) {
        if (stop) stop->SetOwner(nullptr);
    }
    stops_.Clear();
}

BrushMappingMode GradientBrush::MappingMode() const noexcept {
    return GetValueOr(
        MappingModeProperty,
        BrushMappingMode::RelativeToBoundingBox);
}

Base::Result<void> GradientBrush::SetMappingMode(
    BrushMappingMode value) noexcept {
    return SetValue(MappingModeProperty, value);
}

Color GradientBrush::Sample(
    double position) const noexcept {
    const auto stops = GradientStops();
    if (stops.Empty()) return {};
    position = std::clamp(position, 0.0, 1.0);
    const GradientStop* lower = nullptr;
    const GradientStop* upper = nullptr;
    for (const Base::Ref<GradientStop>& stop : stops) {
        if (!stop) continue;
        if (stop->Offset() <= position &&
            (lower == nullptr ||
             stop->Offset() >= lower->Offset())) {
            lower = stop.Get();
        }
        if (stop->Offset() >= position &&
            (upper == nullptr ||
             stop->Offset() <= upper->Offset())) {
            upper = stop.Get();
        }
    }
    if (lower == nullptr) lower = upper;
    if (upper == nullptr) upper = lower;
    if (lower == nullptr || upper == nullptr) return {};
    const double span = upper->Offset() - lower->Offset();
    const float amount = span > 0.0
        ? static_cast<float>(std::clamp(
            (position - lower->Offset()) / span,
            0.0, 1.0))
        : 0.0F;
    const Color a = lower->GetColor();
    const Color b = upper->GetColor();
    Color result{
        a.red + (b.red - a.red) * amount,
        a.green + (b.green - a.green) * amount,
        a.blue + (b.blue - a.blue) * amount,
        a.alpha + (b.alpha - a.alpha) * amount};
    result.alpha *= static_cast<float>(Opacity());
    return result;
}

Point LinearGradientBrush::StartPoint() const noexcept {
    return GetValueOr(StartPointProperty, Point{0.0, 0.0});
}

Point LinearGradientBrush::EndPoint() const noexcept {
    return GetValueOr(EndPointProperty, Point{1.0, 1.0});
}

Base::Result<void> LinearGradientBrush::SetStartPoint(
    Point value) noexcept {
    return SetValue(StartPointProperty, value);
}

Base::Result<void> LinearGradientBrush::SetEndPoint(
    Point value) noexcept {
    return SetValue(EndPointProperty, value);
}

Point RadialGradientBrush::Center() const noexcept {
    return GetValueOr(CenterProperty, Point{0.5, 0.5});
}

Point RadialGradientBrush::GradientOrigin() const noexcept {
    return GetValueOr(
        GradientOriginProperty, Point{0.5, 0.5});
}

double RadialGradientBrush::RadiusX() const noexcept {
    return GetValueOr(RadiusXProperty, 0.5);
}

double RadialGradientBrush::RadiusY() const noexcept {
    return GetValueOr(RadiusYProperty, 0.5);
}

Base::Result<void> RadialGradientBrush::SetCenter(
    Point value) noexcept {
    return SetValue(CenterProperty, value);
}

Base::Result<void> RadialGradientBrush::SetGradientOrigin(
    Point value) noexcept {
    return SetValue(GradientOriginProperty, value);
}

Base::Result<void> RadialGradientBrush::SetRadiusX(
    double value) noexcept {
    return SetValue(RadiusXProperty, value);
}

Base::Result<void> RadialGradientBrush::SetRadiusY(
    double value) noexcept {
    return SetValue(RadiusYProperty, value);
}

Base::Ref<ImageSource>
ImageBrush::Source() const noexcept {
    return GetValueOr(
        ImageSourceProperty,
        Base::Ref<ImageSource>{});
}

Stretch ImageBrush::GetStretch() const noexcept {
    return GetValueOr(
        StretchProperty, Stretch::Fill);
}

Rect ImageBrush::Viewbox() const noexcept {
    return GetValueOr(
        ViewboxProperty,
        Rect{0.0, 0.0, 1.0, 1.0});
}

Rect ImageBrush::Viewport() const noexcept {
    return GetValueOr(
        ViewportProperty,
        Rect{0.0, 0.0, 1.0, 1.0});
}

TileMode ImageBrush::GetTileMode() const noexcept {
    return GetValueOr(
        TileModeProperty, TileMode::None);
}

Base::Result<void> ImageBrush::SetSource(
    Base::Ref<ImageSource> value) noexcept {
    return SetValue(
        ImageSourceProperty, std::move(value));
}

Base::Result<void> ImageBrush::SetStretch(
    Stretch value) noexcept {
    return SetValue(StretchProperty, value);
}

Base::Result<void> ImageBrush::SetViewbox(
    Rect value) noexcept {
    return SetValue(ViewboxProperty, value);
}

Base::Result<void> ImageBrush::SetViewport(
    Rect value) noexcept {
    return SetValue(ViewportProperty, value);
}

Base::Result<void> ImageBrush::SetTileMode(
    TileMode value) noexcept {
    return SetValue(TileModeProperty, value);
}

Color SampleBrush(
    const Base::Ref<Brush>& brush,
    double position,
    Color fallback) noexcept {
    if (!brush) return fallback;
    if (brush->RuntimeType() ==
        SolidColorBrush::StaticTypeId()) {
        Color sampled =
            static_cast<SolidColorBrush*>(
                brush.Get())->GetColor();
        sampled.alpha *=
            static_cast<float>(brush->Opacity());
        return sampled;
    }
    if (brush->RuntimeType() ==
            LinearGradientBrush::StaticTypeId() ||
        brush->RuntimeType() ==
            RadialGradientBrush::StaticTypeId()) {
        return static_cast<GradientBrush*>(
            brush.Get())->Sample(position);
    }
    return fallback;
}

Base::Result<Base::Ref<Brush>>
MakeSolidColorBrush(Color color) noexcept {
    Base::Result<Base::Ref<SolidColorBrush>> made =
        Base::MakeRef<SolidColorBrush>();
    if (!made) return made.GetStatus();
    Base::Result<void> assigned =
        made.Value()->SetColor(color);
    if (!assigned) return assigned.GetStatus();
    return Base::Ref<Brush>(
        std::move(made).Value());
}

Base::Result<void> PaintBrushRect(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double cornerRadius) noexcept {
    if (!brush || bounds.width <= 0.0 ||
        bounds.height <= 0.0) {
        return {};
    }
    if (brush->RuntimeType() ==
        LinearGradientBrush::StaticTypeId()) {
        const auto& gradient =
            *static_cast<LinearGradientBrush*>(
                brush.Get());
        Point start = gradient.StartPoint();
        Point end = gradient.EndPoint();
        if (gradient.MappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            start = {
                bounds.x + start.x * bounds.width,
                bounds.y + start.y * bounds.height};
            end = {
                bounds.x + end.x * bounds.width,
                bounds.y + end.y * bounds.height};
        }
        const double axisX = end.x - start.x;
        const double axisY = end.y - start.y;
        const double axisLengthSquared = std::max(
            axisX * axisX + axisY * axisY, 1.0e-12);
        const bool horizontal =
            std::fabs(axisX) >= std::fabs(axisY);
        constexpr std::uint32_t bandCount = 96U;
        for (std::uint32_t index = 0U;
             index < bandCount; ++index) {
            const double begin =
                static_cast<double>(index) /
                bandCount;
            const double finish =
                static_cast<double>(index + 1U) /
                bandCount;
            const Rect band = horizontal
                ? Rect{
                    bounds.x + begin * bounds.width,
                    bounds.y,
                    (finish - begin) *
                        bounds.width + 0.5,
                    bounds.height}
                : Rect{
                    bounds.x,
                    bounds.y + begin * bounds.height,
                    bounds.width,
                    (finish - begin) *
                        bounds.height + 0.5};
            const double centerX = band.x + band.width * 0.5;
            const double centerY = band.y + band.height * 0.5;
            const double position =
                ((centerX - start.x) * axisX +
                 (centerY - start.y) * axisY) /
                axisLengthSquared;
            Base::Result<void> painted =
                builder.FillRect(
                    band,
                    gradient.Sample(position));
            if (!painted) {
                return painted.GetStatus();
            }
        }
        return {};
    }
    if (brush->RuntimeType() ==
        RadialGradientBrush::StaticTypeId()) {
        const auto& gradient =
            *static_cast<RadialGradientBrush*>(
                brush.Get());
        const Point center = gradient.Center();
        const Point origin = gradient.GradientOrigin();
        const double radiusX =
            std::max(std::fabs(gradient.RadiusX()),
                     1.0e-6);
        const double radiusY =
            std::max(std::fabs(gradient.RadiusY()),
                     1.0e-6);
        constexpr std::uint32_t columns = 20U;
        constexpr std::uint32_t rows = 10U;
        for (std::uint32_t row = 0U;
             row < rows; ++row) {
            const double beginY =
                static_cast<double>(row) / rows;
            const double endY =
                static_cast<double>(row + 1U) / rows;
            for (std::uint32_t column = 0U;
                 column < columns; ++column) {
                const double beginX =
                    static_cast<double>(column) /
                    columns;
                const double endX =
                    static_cast<double>(column + 1U) /
                    columns;
                const double x =
                    (beginX + endX) * 0.5;
                const double y =
                    (beginY + endY) * 0.5;

                // WPF's common centered radial gradient maps
                // normalized ellipse distance directly to the
                // gradient stops. Preserve a displaced gradient
                // origin by shifting the sampling ray by the same
                // normalized amount; this keeps the focal highlight
                // useful without flattening it to one midpoint color.
                const double focalX =
                    (origin.x - center.x) / radiusX;
                const double focalY =
                    (origin.y - center.y) / radiusY;
                const double normalizedX =
                    (x - center.x) / radiusX -
                    focalX;
                const double normalizedY =
                    (y - center.y) / radiusY -
                    focalY;
                const double position = std::clamp(
                    std::hypot(
                        normalizedX, normalizedY),
                    0.0,
                    1.0);
                Base::Result<void> painted =
                    builder.FillRect(
                        Rect{
                            bounds.x +
                                beginX * bounds.width,
                            bounds.y +
                                beginY * bounds.height,
                            (endX - beginX) *
                                bounds.width + 0.25,
                            (endY - beginY) *
                                bounds.height + 0.25},
                        gradient.Sample(position));
                if (!painted) {
                    return painted.GetStatus();
                }
            }
        }
        return {};
    }
    const Color color = SampleBrush(brush);
    if (color.alpha <= 0.0F) return {};
    const double effectiveRadius = std::min(
        std::max(0.0, cornerRadius),
        std::min(
            bounds.width,
            bounds.height) * 0.5);
    return effectiveRadius > 0.0
        ? builder.FillRoundedRect(
            bounds, color, effectiveRadius)
        : builder.FillRect(bounds, color);
}

} // namespace Aero::Media
