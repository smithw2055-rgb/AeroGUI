#include <Aero/Media/Brushes.hpp>
#include "BrushRendering.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"

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
    // Do not emit transparent gradient/image tessellation. Apart from being
    // unnecessary work, zero-alpha FillRect commands can overwrite earlier
    // layers on backends that use opaque UI batches. This is especially
    // visible for hover overlays authored with Brush.Opacity="0".
    if (brush->GetOpacity() <= 0.0) {
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
                const TileMode tileMode = imageBrush.GetTileMode();
                const bool flipX =
                    (tileMode == TileMode::FlipX ||
                     tileMode == TileMode::FlipXY) &&
                    (column & 1) != 0;
                const bool flipY =
                    (tileMode == TileMode::FlipY ||
                     tileMode == TileMode::FlipXY) &&
                    (row & 1) != 0;
                const double sampleX = flipX ? 1.0 - relativeX : relativeX;
                const double sampleY = flipY ? 1.0 - relativeY : relativeY;
                uv.x += uv.width * sampleX;
                uv.y += uv.height * sampleY;
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

        Base::Transform2D inverse;
        const bool hasInverse = gradient.GetRelativeTransform() &&
            InvertTransform(
                gradient.GetRelativeTransform()->GetMatrix(), inverse);

        const Point quadPoints[4] = {
            {bounds.x, bounds.y},
            {bounds.x + bounds.width, bounds.y},
            {bounds.x + bounds.width, bounds.y + bounds.height},
            {bounds.x, bounds.y + bounds.height}
        };

        // With a relative transform the geometry is warped, so fall back to
        // the single-quad approximation. Without one, an axis-aligned axis
        // can be split into exact per-stop Gouraud bands.
        const bool axisAligned =
            (std::fabs(axisX) < 1.0e-9 && std::fabs(axisY) > 1.0e-9) ||
            (std::fabs(axisY) < 1.0e-9 && std::fabs(axisX) > 1.0e-9);
        if (!hasInverse && axisAligned) {
            const bool horizontal = std::fabs(axisY) < 1.0e-9;
            const double axis = horizontal ? axisX : axisY;
            const double t0 = horizontal ? bounds.x : bounds.y;
            const double t1 = horizontal
                ? bounds.x + bounds.width
                : bounds.y + bounds.height;
            const double tMin = std::min(
                (t0 - (horizontal ? start.x : start.y)) / axis,
                (t1 - (horizontal ? start.x : start.y)) / axis);
            const double tMax = std::max(
                (t0 - (horizontal ? start.x : start.y)) / axis,
                (t1 - (horizontal ? start.x : start.y)) / axis);

            // Collect breakpoints where the color function may change slope.
            Base::Vector<double> boundaries;
            const GradientSpreadMethod spread = gradient.GetSpreadMethod();
            const auto stops = gradient.GetGradientStops();
            const int periodStart = static_cast<int>(std::floor(tMin)) - 1;
            const int periodEnd = static_cast<int>(std::ceil(tMax)) + 1;
            for (int period = periodStart; period <= periodEnd; ++period) {
                for (const Base::Ref<GradientStop>& stop : stops) {
                    if (!stop) continue;
                    if (spread == GradientSpreadMethod::Reflect) {
                        boundaries.PushBack(static_cast<double>(period) +
                            stop->GetOffset());
                        boundaries.PushBack(static_cast<double>(period) +
                            1.0 - stop->GetOffset());
                    } else if (spread == GradientSpreadMethod::Repeat) {
                        boundaries.PushBack(static_cast<double>(period) +
                            stop->GetOffset());
                    } else {
                        boundaries.PushBack(stop->GetOffset());
                    }
                }
            }
            boundaries.PushBack(tMin);
            boundaries.PushBack(tMax);
            std::sort(
                boundaries.Data(),
                boundaries.Data() + static_cast<std::size_t>(boundaries.Size()));
            const auto uniqueEnd = std::unique(
                boundaries.Data(),
                boundaries.Data() + static_cast<std::size_t>(boundaries.Size()),
                [](double a, double b) {
                    return std::fabs(a - b) < 1.0e-9;
                });
            const std::size_t boundaryCount =
                static_cast<std::size_t>(uniqueEnd - boundaries.Data());
            const std::size_t bandLimit = 512U;
            if (boundaryCount > 2U && boundaryCount <= bandLimit) {
                for (std::uint32_t i = 0U; i + 1U < boundaryCount; ++i) {
                    const double a = std::clamp(
                        boundaries[i], tMin, tMax);
                    const double b = std::clamp(
                        boundaries[i + 1U], tMin, tMax);
                    if (b - a < 1.0e-9) continue;
                    const Color ca = ::Aero::Media::SampleBrush(brush, a);
                    const Color cb = ::Aero::Media::SampleBrush(brush, b);
                    const double pos0 = (horizontal ? start.x : start.y) + a * axis;
                    const double pos1 = (horizontal ? start.x : start.y) + b * axis;
                    Point bandPoints[4];
                    if (horizontal) {
                        bandPoints[0] = {pos0, bounds.y};
                        bandPoints[1] = {pos1, bounds.y};
                        bandPoints[2] = {pos1, bounds.y + bounds.height};
                        bandPoints[3] = {pos0, bounds.y + bounds.height};
                    } else {
                        bandPoints[0] = {bounds.x, pos0};
                        bandPoints[1] = {bounds.x + bounds.width, pos0};
                        bandPoints[2] = {bounds.x + bounds.width, pos1};
                        bandPoints[3] = {bounds.x, pos1};
                    }
                    Color bandColors[4];
                    if (horizontal) {
                        bandColors[0] = ca;
                        bandColors[1] = cb;
                        bandColors[2] = cb;
                        bandColors[3] = ca;
                    } else {
                        bandColors[0] = ca;
                        bandColors[1] = ca;
                        bandColors[2] = cb;
                        bandColors[3] = cb;
                    }
                    Base::Result<void> painted =
                        builder.FillGradientQuad(
                            bandPoints, bandColors);
                    if (!painted) return painted.GetStatus();
                }
                return {};
            }
        }

        Color quadColors[4];
        for (int i = 0; i < 4; ++i) {
            Point samplePoint = quadPoints[i];
            if (hasInverse) {
                samplePoint = TransformPoint(inverse, samplePoint);
            }
            const double position =
                ((samplePoint.x - start.x) * axisX +
                 (samplePoint.y - start.y) * axisY) /
                axisLengthSquared;
            quadColors[i] = ::Aero::Media::SampleBrush(
                brush, position, {},
                {(quadPoints[i].x - bounds.x) / bounds.width,
                 (quadPoints[i].y - bounds.y) / bounds.height},
                Base::Size{bounds.width, bounds.height});
        }
        return builder.FillGradientQuad(quadPoints, quadColors);
    }
    if (brush->RuntimeType() ==
        RadialGradientBrush::StaticTypeId()) {
        const auto& gradient =
            *static_cast<RadialGradientBrush*>(
                brush.Get());
        Point center = gradient.GetCenter();
        Point origin = gradient.GetGradientOrigin();
        double radiusX = std::max(std::fabs(gradient.GetRadiusX()), 1.0e-6);
        double radiusY = std::max(std::fabs(gradient.GetRadiusY()), 1.0e-6);
        if (gradient.GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            center = {
                bounds.x + center.x * bounds.width,
                bounds.y + center.y * bounds.height};
            origin = {
                bounds.x + origin.x * bounds.width,
                bounds.y + origin.y * bounds.height};
            radiusX *= bounds.width;
            radiusY *= bounds.height;
        }

        const double fx = (origin.x - center.x) / radiusX;
        const double fy = (origin.y - center.y) / radiusY;
        const bool hasFocal = (fx * fx + fy * fy) > 1.0e-6;

        auto sampleRadialAt = [&](double px, double py) noexcept -> Color {
            const double u = (px - center.x) / radiusX;
            const double v = (py - center.y) / radiusY;
            double t = 0.0;
            if (hasFocal) {
                const double dx = u - fx;
                const double dy = v - fy;
                const double a = dx * dx + dy * dy;
                const double b = 2.0 * (fx * dx + fy * dy);
                const double c = fx * fx + fy * fy - 1.0;
                const double disc = b * b - 4.0 * a * c;
                if (disc >= 0.0 && a > 1.0e-9) {
                    const double root = (-b + std::sqrt(disc)) / (2.0 * a);
                    t = root > 0.0 ? 1.0 / root : 0.0;
                } else {
                    t = std::sqrt(u * u + v * v);
                }
            } else {
                t = std::sqrt(u * u + v * v);
            }
            return ::Aero::Media::SampleBrush(
                brush, t, {},
                {(px - bounds.x) / bounds.width, (py - bounds.y) / bounds.height},
                Base::Size{bounds.width, bounds.height});
        };

        constexpr int gridCols = 16;
        constexpr int gridRows = 16;
        for (int r = 0; r < gridRows; ++r) {
            const double y0 = bounds.y + bounds.height * (static_cast<double>(r) / gridRows);
            const double y1 = bounds.y + bounds.height * (static_cast<double>(r + 1) / gridRows);
            for (int c = 0; c < gridCols; ++c) {
                const double x0 = bounds.x + bounds.width * (static_cast<double>(c) / gridCols);
                const double x1 = bounds.x + bounds.width * (static_cast<double>(c + 1) / gridCols);
                const Point quadPoints[4] = {
                    {x0, y0},
                    {x1, y0},
                    {x1, y1},
                    {x0, y1}
                };
                const Color quadColors[4] = {
                    sampleRadialAt(x0, y0),
                    sampleRadialAt(x1, y0),
                    sampleRadialAt(x1, y1),
                    sampleRadialAt(x0, y1)
                };
                Base::Result<void> drawn = builder.FillGradientQuad(quadPoints, quadColors);
                if (!drawn) return drawn.GetStatus();
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
