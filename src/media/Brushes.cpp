#include <Aero/Media/Brushes.hpp>
#include "BrushRendering.hpp"
#include "media/MediaPrivate.hpp"

#include <algorithm>
#include <cmath>

namespace Aero::Media {

double Brush::GetOpacity() const noexcept {
    return GetValueOr(OpacityProperty, 1.0);
}

void Brush::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    DependencyObject::OnPropertyInvalidated(flags);
    if (owner_ != nullptr) (void)owner_->InvalidateVisual();
}

void Brush::SetOpacity(
    double value) noexcept {
    SetValue(OpacityProperty, value);
}

Color SolidColorBrush::GetColor() const noexcept {
    return GetValueOr(ColorProperty, initialColor_);
}

void SolidColorBrush::SetColor(
    Color value) noexcept {
    SetValue(ColorProperty, value);
}

double GradientStop::GetOffset() const noexcept {
    return GetValueOr(OffsetProperty, 0.0);
}

Color GradientStop::GetColor() const noexcept {
    return GetValueOr(ColorProperty, Color{});
}

void GradientStop::SetOffset(
    double value) noexcept {
    SetValue(OffsetProperty, value);
}

void GradientStop::SetColor(
    Color value) noexcept {
    SetValue(ColorProperty, value);
}

void GradientStop::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    DependencyObject::OnPropertyInvalidated(flags);
    FrameworkElement* visualOwner =
        owner_ != nullptr
        ? ::Aero::Media::Detail::BrushPrivate::Owner(*owner_)
        : nullptr;
    if (visualOwner != nullptr) (void)visualOwner->InvalidateVisual();
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
        stops_.PushBack(std::move(stop));
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
        stops_.PushBack(std::move(stop));
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

BrushMappingMode GradientBrush::GetMappingMode() const noexcept {
    return GetValueOr(
        MappingModeProperty,
        BrushMappingMode::RelativeToBoundingBox);
}

void GradientBrush::SetMappingMode(
    BrushMappingMode value) noexcept {
    SetValue(MappingModeProperty, value);
}

Point LinearGradientBrush::GetStartPoint() const noexcept {
    return GetValueOr(StartPointProperty, Point{0.0, 0.0});
}

Point LinearGradientBrush::GetEndPoint() const noexcept {
    return GetValueOr(EndPointProperty, Point{1.0, 1.0});
}

void LinearGradientBrush::SetStartPoint(
    Point value) noexcept {
    SetValue(StartPointProperty, value);
}

void LinearGradientBrush::SetEndPoint(
    Point value) noexcept {
    SetValue(EndPointProperty, value);
}

Point RadialGradientBrush::GetCenter() const noexcept {
    return GetValueOr(CenterProperty, Point{0.5, 0.5});
}

Point RadialGradientBrush::GetGradientOrigin() const noexcept {
    return GetValueOr(
        GradientOriginProperty, Point{0.5, 0.5});
}

double RadialGradientBrush::GetRadiusX() const noexcept {
    return GetValueOr(RadiusXProperty, 0.5);
}

double RadialGradientBrush::GetRadiusY() const noexcept {
    return GetValueOr(RadiusYProperty, 0.5);
}

void RadialGradientBrush::SetCenter(
    Point value) noexcept {
    SetValue(CenterProperty, value);
}

void RadialGradientBrush::SetGradientOrigin(
    Point value) noexcept {
    SetValue(GradientOriginProperty, value);
}

void RadialGradientBrush::SetRadiusX(
    double value) noexcept {
    SetValue(RadiusXProperty, value);
}

void RadialGradientBrush::SetRadiusY(
    double value) noexcept {
    SetValue(RadiusYProperty, value);
}

Base::Ref<ImageSource>
ImageBrush::GetSource() const noexcept {
    return GetValueOr(
        ImageSourceProperty,
        Base::Ref<ImageSource>{});
}

Stretch ImageBrush::GetStretch() const noexcept {
    return GetValueOr(
        StretchProperty, Stretch::Fill);
}

Rect ImageBrush::GetViewbox() const noexcept {
    return GetValueOr(
        ViewboxProperty,
        Rect{0.0, 0.0, 1.0, 1.0});
}

Rect ImageBrush::GetViewport() const noexcept {
    return GetValueOr(
        ViewportProperty,
        Rect{0.0, 0.0, 1.0, 1.0});
}

TileMode ImageBrush::GetTileMode() const noexcept {
    return GetValueOr(
        TileModeProperty, TileMode::None);
}

void ImageBrush::SetSource(
    Base::Ref<ImageSource> value) noexcept {
    SetValue(
        ImageSourceProperty, std::move(value));
}

void ImageBrush::SetStretch(
    Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

void ImageBrush::SetViewbox(
    Rect value) noexcept {
    SetValue(ViewboxProperty, value);
}

void ImageBrush::SetViewport(
    Rect value) noexcept {
    SetValue(ViewportProperty, value);
}

void ImageBrush::SetTileMode(
    TileMode value) noexcept {
    SetValue(TileModeProperty, value);
}

Base::Result<Base::Ref<Brush>>
MakeSolidColorBrush(Color color) noexcept {
    Base::Result<Base::Ref<SolidColorBrush>> made =
        Base::MakeRef<SolidColorBrush>();
    if (!made) return made.GetStatus();
    made.Value()->SetColor(color);
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
        Point start = gradient.GetStartPoint();
        Point end = gradient.GetEndPoint();
        if (gradient.GetMappingMode() ==
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
                    ::Aero::Media::Detail::SampleGradient(gradient, position));
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
        const Point center = gradient.GetCenter();
        const Point origin = gradient.GetGradientOrigin();
        const double radiusX =
            std::max(std::fabs(gradient.GetRadiusX()),
                     1.0e-6);
        const double radiusY =
            std::max(std::fabs(gradient.GetRadiusY()),
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
                        ::Aero::Media::Detail::SampleGradient(gradient, position));
                if (!painted) {
                    return painted.GetStatus();
                }
            }
        }
        return {};
    }
    const Color color = ::Aero::Media::Detail::SampleBrush(brush);
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
