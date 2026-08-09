#include <Aero/Media/Brushes.hpp>
#include "BrushRendering.hpp"
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/media/AnimationRuntime.hpp"
#include "gui/media/BrushRuntime.hpp"
#include "gui/media/EffectRuntime.hpp"
#include "gui/media/TransformRuntime.hpp"
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace Aero::Media {

double Brush::GetOpacity() const noexcept {
    return GetValueOr(OpacityProperty, 1.0);
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

Base::Result<void> GradientBrush::AddGradientStop(
    Base::Ref<GradientStop> stop) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!stop) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GradientStop cannot be null");
    }
    if (stopChangedHandler_.Empty()) {
        stopChangedHandler_ = FreezableChangedHandler(
            this, &GradientBrush::OnGradientStopChanged);
    }
    GradientStop* retained = stop.Get();
    if (!retained->IsFrozen()) {
        Base::Result<void> subscribed =
            retained->AddChangedHandlerChecked(stopChangedHandler_);
        if (!subscribed) return subscribed.GetStatus();
    }
    Base::Result<void> added =
        stops_.PushBack(std::move(stop));
    if (!added) {
        if (!retained->IsFrozen()) {
            static_cast<void>(
                retained->RemoveChangedHandler(stopChangedHandler_));
        }
        return added.GetStatus();
    }
    WritePostscript();
    return {};
}

Base::Result<void> GradientStopCollection::Add(
    Base::Ref<GradientStop> stop) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!stop) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GradientStopCollection item cannot be null");
    }
    if (stopChangedHandler_.Empty()) {
        stopChangedHandler_ = FreezableChangedHandler(
            this, &GradientStopCollection::OnStopChanged);
    }
    GradientStop* retained = stop.Get();
    if (!retained->IsFrozen()) {
        Base::Result<void> subscribed =
            retained->AddChangedHandlerChecked(stopChangedHandler_);
        if (!subscribed) return subscribed.GetStatus();
    }
    Base::Result<void> added =
        stops_.PushBack(std::move(stop));
    if (!added) {
        if (!retained->IsFrozen()) {
            static_cast<void>(
                retained->RemoveChangedHandler(stopChangedHandler_));
        }
        return added.GetStatus();
    }
    if (!changed_.Empty()) {
        changed_.Invoke({
            Collections::ItemsChangeAction::Add,
            UINT32_MAX,
            stops_.Size() - 1U,
            0U,
            1U});
    }
    WritePostscript();
    return {};
}

GradientBrush::~GradientBrush() {
    for (const Base::Ref<GradientStop>& stop : stops_) {
        if (stop && !stopChangedHandler_.Empty()) {
            static_cast<void>(
                stop->RemoveChangedHandler(stopChangedHandler_));
        }
    }
}

void GradientBrush::ClearGradientStops() noexcept {
    if (!WritePreamble() || stops_.Empty()) return;
    for (const Base::Ref<GradientStop>& stop : stops_) {
        if (stop && !stopChangedHandler_.Empty()) {
            static_cast<void>(
                stop->RemoveChangedHandler(stopChangedHandler_));
        }
    }
    stops_.Clear();
    WritePostscript();
}

void GradientBrush::OnGradientStopChanged(Freezable&) noexcept {
    WritePostscript();
}

bool GradientBrush::FreezeCore(bool isChecking) noexcept {
    for (const Base::Ref<GradientStop>& stop : stops_) {
        if (!stop) continue;
        if (isChecking) {
            if (!stop->CanFreeze()) return false;
        } else {
            static_cast<void>(stop->Freeze());
        }
    }
    return Brush::FreezeCore(isChecking);
}

GradientStopCollection::~GradientStopCollection() {
    for (const Base::Ref<GradientStop>& stop : stops_) {
        if (stop && !stopChangedHandler_.Empty()) {
            static_cast<void>(
                stop->RemoveChangedHandler(stopChangedHandler_));
        }
    }
}

void GradientStopCollection::Clear() noexcept {
    if (!WritePreamble() || stops_.Empty()) return;
    const std::uint32_t count = stops_.Size();
    for (const Base::Ref<GradientStop>& stop : stops_) {
        if (stop && !stopChangedHandler_.Empty()) {
            static_cast<void>(
                stop->RemoveChangedHandler(stopChangedHandler_));
        }
    }
    stops_.Clear();
    if (!changed_.Empty()) {
        changed_.Invoke({
            Collections::ItemsChangeAction::Reset,
            0U, 0U, count, 0U});
    }
    WritePostscript();
}

void GradientStopCollection::OnStopChanged(Freezable&) noexcept {
    WritePostscript();
}

bool GradientStopCollection::FreezeCore(bool isChecking) noexcept {
    for (const Base::Ref<GradientStop>& stop : stops_) {
        if (!stop) continue;
        if (isChecking) {
            if (!stop->CanFreeze()) return false;
        } else {
            static_cast<void>(stop->Freeze());
        }
    }
    if (!isChecking) changed_.Reset();
    return Freezable::FreezeCore(isChecking);
}

std::uint64_t Brush::Access::Revision(const Brush& brush) noexcept {
    return Freezable::Access::Revision(brush);
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

GradientSpreadMethod GradientBrush::GetSpreadMethod() const noexcept {
    return GetValueOr(
        SpreadMethodProperty,
        GradientSpreadMethod::Pad);
}

void GradientBrush::SetSpreadMethod(
    GradientSpreadMethod value) noexcept {
    SetValue(SpreadMethodProperty, value);
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

BrushMappingMode ImageBrush::GetViewboxUnits() const noexcept {
    return GetValueOr(
        ViewboxUnitsProperty,
        BrushMappingMode::RelativeToBoundingBox);
}

BrushMappingMode ImageBrush::GetViewportUnits() const noexcept {
    return GetValueOr(
        ViewportUnitsProperty,
        BrushMappingMode::RelativeToBoundingBox);
}

TileMode ImageBrush::GetTileMode() const noexcept {
    return GetValueOr(
        TileModeProperty, TileMode::None);
}

HorizontalAlignment ImageBrush::GetAlignmentX() const noexcept {
    return GetValueOr(
        AlignmentXProperty, HorizontalAlignment::Center);
}

VerticalAlignment ImageBrush::GetAlignmentY() const noexcept {
    return GetValueOr(
        AlignmentYProperty, VerticalAlignment::Center);
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

void ImageBrush::SetViewboxUnits(
    BrushMappingMode value) noexcept {
    SetValue(ViewboxUnitsProperty, value);
}

void ImageBrush::SetViewportUnits(
    BrushMappingMode value) noexcept {
    SetValue(ViewportUnitsProperty, value);
}

void ImageBrush::SetTileMode(
    TileMode value) noexcept {
    SetValue(TileModeProperty, value);
}

void ImageBrush::SetAlignmentX(
    HorizontalAlignment value) noexcept {
    SetValue(AlignmentXProperty, value);
}

void ImageBrush::SetAlignmentY(
    VerticalAlignment value) noexcept {
    SetValue(AlignmentYProperty, value);
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
    if (brush->RuntimeType() == ImageBrush::StaticTypeId()) {
        const auto& imageBrush =
            *static_cast<ImageBrush*>(brush.Get());
        const Render::RenderImageId image =
            BrushPrivate::RuntimeImage(imageBrush);
        const std::uint32_t pixelWidth =
            BrushPrivate::PixelWidth(imageBrush);
        const std::uint32_t pixelHeight =
            BrushPrivate::PixelHeight(imageBrush);
        if (image == Render::InvalidRenderImageId ||
            pixelWidth == 0U || pixelHeight == 0U) {
            return {};
        }
        Rect source = imageBrush.GetViewbox();
        if (imageBrush.GetViewboxUnits() ==
            BrushMappingMode::Absolute) {
            source.x /= static_cast<double>(pixelWidth);
            source.y /= static_cast<double>(pixelHeight);
            source.width /= static_cast<double>(pixelWidth);
            source.height /= static_cast<double>(pixelHeight);
        }
        source.x = std::clamp(source.x, 0.0, 1.0);
        source.y = std::clamp(source.y, 0.0, 1.0);
        source.width = std::clamp(source.width, 0.0, 1.0 - source.x);
        source.height = std::clamp(source.height, 0.0, 1.0 - source.y);
        if (source.width <= 0.0 || source.height <= 0.0) return {};
        Rect viewport = imageBrush.GetViewport();
        if (imageBrush.GetViewportUnits() ==
            BrushMappingMode::RelativeToBoundingBox) {
            viewport = {
                bounds.x + viewport.x * bounds.width,
                bounds.y + viewport.y * bounds.height,
                viewport.width * bounds.width,
                viewport.height * bounds.height};
        } else {
            viewport.x += bounds.x;
            viewport.y += bounds.y;
        }
        if (!Aero::IsFinite(viewport) ||
            imageBrush.GetTileMode() == TileMode::None ||
            viewport.width <= 0.0 || viewport.height <= 0.0) {
            viewport = bounds;
        }
        const bool tiled = imageBrush.GetTileMode() != TileMode::None;
        const std::int32_t firstColumn = tiled
            ? static_cast<std::int32_t>(std::floor(
                (bounds.x - viewport.x) / viewport.width))
            : 0;
        const std::int32_t requestedLastColumn = tiled
            ? static_cast<std::int32_t>(std::ceil(
                (bounds.x + bounds.width - viewport.x) / viewport.width))
            : 1;
        const std::int32_t lastColumn = std::min(
            requestedLastColumn, firstColumn + 256);
        const std::int32_t firstRow = tiled
            ? static_cast<std::int32_t>(std::floor(
                (bounds.y - viewport.y) / viewport.height))
            : 0;
        const std::int32_t requestedLastRow = tiled
            ? static_cast<std::int32_t>(std::ceil(
                (bounds.y + bounds.height - viewport.y) / viewport.height))
            : 1;
        const std::int32_t lastRow = std::min(
            requestedLastRow, firstRow + 256);
        for (std::int32_t row = firstRow; row < lastRow; ++row) {
            for (std::int32_t column = firstColumn;
                 column < lastColumn; ++column) {
                Rect tile{
                    viewport.x + column * viewport.width,
                    viewport.y + row * viewport.height,
                    viewport.width, viewport.height};
                tile = Aero::Intersect(tile, bounds);
                if (tile.width <= 0.0 || tile.height <= 0.0) continue;
                Rect uv = source;
                const double relativeX =
                    (tile.x - (viewport.x + column * viewport.width)) /
                    viewport.width;
                const double relativeY =
                    (tile.y - (viewport.y + row * viewport.height)) /
                    viewport.height;
                uv.x += uv.width * relativeX;
                uv.y += uv.height * relativeY;
                uv.width *= tile.width / viewport.width;
                uv.height *= tile.height / viewport.height;
                const Point center{
                    (tile.x - bounds.x + tile.width * 0.5) / bounds.width,
                    (tile.y - bounds.y + tile.height * 0.5) / bounds.height};
                const Color tint = SampleBrush(
                    brush, 0.5,
                    {1.0F, 1.0F, 1.0F,
                     static_cast<float>(imageBrush.GetOpacity())},
                    center, Base::Size{bounds.width, bounds.height});
                Base::Result<void> drawn =
                    builder.DrawImage(image, tile, uv, tint);
                if (!drawn) return drawn.GetStatus();
            }
        }
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
            Point samplePoint{centerX, centerY};
            Base::Transform2D inverse;
            if (gradient.GetRelativeTransform() &&
                InvertTransform(
                    gradient.GetRelativeTransform()->GetMatrix(), inverse)) {
                samplePoint = TransformPoint(inverse, samplePoint);
            }
            const double position =
                ((samplePoint.x - start.x) * axisX +
                 (samplePoint.y - start.y) * axisY) /
                axisLengthSquared;
            Base::Result<void> painted =
                builder.FillRect(
                    band,
                    ::Aero::Media::SampleBrush(
                        brush, position, {},
                        {(centerX - bounds.x) / bounds.width,
                         (centerY - bounds.y) / bounds.height},
                        Base::Size{bounds.width, bounds.height}));
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
                Point sample{x, y};
                Base::Transform2D inverse;
                if (gradient.GetRelativeTransform() &&
                    InvertTransform(
                        gradient.GetRelativeTransform()->GetMatrix(), inverse)) {
                    sample = TransformPoint(inverse, sample);
                }

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
                    (sample.x - center.x) / radiusX -
                    focalX;
                const double normalizedY =
                    (sample.y - center.y) / radiusY -
                    focalY;
                const double position =
                    std::hypot(normalizedX, normalizedY);
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
                        ::Aero::Media::SampleBrush(
                            brush, position, {},
                            {x, y},
                            Base::Size{bounds.width, bounds.height}));
                if (!painted) {
                    return painted.GetStatus();
                }
            }
        }
        return {};
    }
    const Color color = ::Aero::Media::SampleBrush(brush);
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
