#include "render/DisplayList.hpp"
#include <Aero/Shapes.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Pen.hpp>

#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/media/MediaState.hpp"
#include "gui/media/BrushRendering.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Shapes {

using namespace ::Aero::Media;
using namespace ::Aero::Render;

namespace {

struct ImageBrushGeometry {
    Rect destination;
    Rect sourceUv;
};

double AlignmentFactor(HorizontalAlignment alignment) noexcept {
    switch (alignment) {
    case HorizontalAlignment::Left:
        return 0.0;
    case HorizontalAlignment::Right:
        return 1.0;
    default:
        return 0.5;
    }
}

double AlignmentFactor(VerticalAlignment alignment) noexcept {
    switch (alignment) {
    case VerticalAlignment::Top:
        return 0.0;
    case VerticalAlignment::Bottom:
        return 1.0;
    default:
        return 0.5;
    }
}

ImageBrushGeometry FitImageBrush(
    const ImageBrush& brush,
    Rect destination,
    Rect sourceUv) noexcept {
    const double sourceWidth =
        static_cast<double>(
            brush.
                GetPixelWidth()) *
        std::fabs(sourceUv.width);
    const double sourceHeight =
        static_cast<double>(
            brush.
                GetPixelHeight()) *
        std::fabs(sourceUv.height);
    if (sourceWidth <= 0.0 ||
        sourceHeight <= 0.0 ||
        destination.width <= 0.0 ||
        destination.height <= 0.0 ||
        brush.GetStretch() == Stretch::Fill) {
        return {destination, sourceUv};
    }
    if (brush.GetStretch() == Stretch::None) {
        destination.x +=
            (destination.width - sourceWidth) *
            AlignmentFactor(brush.GetAlignmentX());
        destination.y +=
            (destination.height - sourceHeight) *
            AlignmentFactor(brush.GetAlignmentY());
        destination.width = sourceWidth;
        destination.height = sourceHeight;
        return {destination, sourceUv};
    }
    const double scaleX =
        destination.width / sourceWidth;
    const double scaleY =
        destination.height / sourceHeight;
    if (brush.GetStretch() ==
        Stretch::UniformToFill) {
        const double scale =
            std::max(scaleX, scaleY);
        const double drawnWidth =
            sourceWidth * scale;
        const double drawnHeight =
            sourceHeight * scale;
        sourceUv.x +=
            sourceUv.width *
            (1.0 -
             destination.width / drawnWidth) *
            AlignmentFactor(brush.GetAlignmentX());
        sourceUv.y +=
            sourceUv.height *
            (1.0 -
             destination.height / drawnHeight) *
            AlignmentFactor(brush.GetAlignmentY());
        sourceUv.width *=
            destination.width / drawnWidth;
        sourceUv.height *=
            destination.height / drawnHeight;
        return {destination, sourceUv};
    }
    const double scale =
        std::min(scaleX, scaleY);
    const double fittedWidth =
        sourceWidth * scale;
    const double fittedHeight =
        sourceHeight * scale;
    destination.x +=
        (destination.width - fittedWidth) *
        AlignmentFactor(brush.GetAlignmentX());
    destination.y +=
        (destination.height - fittedHeight) *
        AlignmentFactor(brush.GetAlignmentY());
    destination.width = fittedWidth;
    destination.height = fittedHeight;
    return {destination, sourceUv};
}

[[maybe_unused]] Base::Result<void> PaintImageBrush(
    ::Aero::Media::DrawingContext& context,
    const ImageBrush& brush,
    Rect bounds) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const RenderImageId image =
        brush.
            GetRenderImageId();
    if (image == InvalidRenderImageId ||
        brush.
            GetPixelWidth() == 0U ||
        brush.
            GetPixelHeight() == 0U) {
        return {};
    }
    const double pixelWidth = static_cast<double>(
        brush.GetPixelWidth());
    const double pixelHeight = static_cast<double>(
        brush.GetPixelHeight());
    const Rect authoredViewbox = brush.GetViewbox();
    Rect sourceUv = brush.GetViewboxUnits() ==
            BrushMappingMode::Absolute
        ? Rect{
            authoredViewbox.x / pixelWidth,
            authoredViewbox.y / pixelHeight,
            authoredViewbox.width / pixelWidth,
            authoredViewbox.height / pixelHeight}
        : authoredViewbox;
    sourceUv = {
        std::clamp(sourceUv.x, 0.0, 1.0),
        std::clamp(sourceUv.y, 0.0, 1.0),
        std::clamp(sourceUv.width, 0.0, 1.0),
        std::clamp(sourceUv.height, 0.0, 1.0)};
    sourceUv.width = std::min(
        sourceUv.width, 1.0 - sourceUv.x);
    sourceUv.height = std::min(
        sourceUv.height, 1.0 - sourceUv.y);
    if (sourceUv.width <= 0.0 ||
        sourceUv.height <= 0.0) {
        return {};
    }

    const Rect authoredViewport =
        brush.GetViewport();
    Rect cell = brush.GetViewportUnits() ==
            BrushMappingMode::Absolute
        ? Rect{
            bounds.x + authoredViewport.x,
            bounds.y + authoredViewport.y,
            authoredViewport.width,
            authoredViewport.height}
        : Rect{
            bounds.x + authoredViewport.x * bounds.width,
            bounds.y + authoredViewport.y * bounds.height,
            authoredViewport.width * bounds.width,
            authoredViewport.height * bounds.height};
    if (cell.width <= 0.0 ||
        cell.height <= 0.0) {
        return {};
    }
    const Color tint{
        1.0F, 1.0F, 1.0F,
        static_cast<float>(brush.GetOpacity())};
    const Base::Ref<Transform> relativeTransform =
        brush.GetRelativeTransform();
    const bool transformed = relativeTransform &&
        bounds.width > 0.0 && bounds.height > 0.0;
    const bool clipped = transformed ||
        brush.GetTileMode() != TileMode::None;
    Base::Result<void> status;
    if (clipped) status = builder.PushClip(bounds);
    if (!status) return status.GetStatus();
    if (transformed) {
        Base::Transform2D toNormalized;
        toNormalized.m11 = 1.0 / bounds.width;
        toNormalized.m22 = 1.0 / bounds.height;
        toNormalized.dx = -bounds.x / bounds.width;
        toNormalized.dy = -bounds.y / bounds.height;
        Base::Transform2D fromNormalized;
        fromNormalized.m11 = bounds.width;
        fromNormalized.m22 = bounds.height;
        fromNormalized.dx = bounds.x;
        fromNormalized.dy = bounds.y;
        const Base::Transform2D absolute = ComposeTransforms(
            ComposeTransforms(
                toNormalized,
                relativeTransform->GetMatrix()),
            fromNormalized);
        status = builder.PushTransform(absolute);
        if (!status) {
            static_cast<void>(builder.PopClip());
            return status.GetStatus();
        }
    }

    if (brush.GetTileMode() == TileMode::None) {
        const ImageBrushGeometry geometry =
            FitImageBrush(brush, cell, sourceUv);
        status = builder.DrawImage(
            image, geometry.destination, geometry.sourceUv, tint);
    } else {
        constexpr std::uint32_t maxTiles = 4096U;
        std::uint32_t tileCount = 0U;
        const std::int64_t firstColumn = static_cast<std::int64_t>(
            std::floor((bounds.x - cell.x) / cell.width));
        const std::int64_t firstRow = static_cast<std::int64_t>(
            std::floor((bounds.y - cell.y) / cell.height));
        for (std::int64_t row = firstRow;
             cell.y + static_cast<double>(row) * cell.height < bounds.y + bounds.height &&
             tileCount < maxTiles && status; ++row) {
            for (std::int64_t column = firstColumn;
                 cell.x + static_cast<double>(column) * cell.width < bounds.x + bounds.width &&
                 tileCount < maxTiles; ++column) {
                Rect tile{
                    cell.x + static_cast<double>(column) * cell.width,
                    cell.y + static_cast<double>(row) * cell.height,
                    cell.width,
                    cell.height};
                ImageBrushGeometry geometry =
                    FitImageBrush(brush, tile, sourceUv);
                const bool oddColumn = (column & 1) != 0;
                const bool oddRow = (row & 1) != 0;
                const TileMode mode = brush.GetTileMode();
                if (oddColumn &&
                    (mode == TileMode::FlipX || mode == TileMode::FlipXY)) {
                    geometry.sourceUv.x += geometry.sourceUv.width;
                    geometry.sourceUv.width = -geometry.sourceUv.width;
                }
                if (oddRow &&
                    (mode == TileMode::FlipY || mode == TileMode::FlipXY)) {
                    geometry.sourceUv.y += geometry.sourceUv.height;
                    geometry.sourceUv.height = -geometry.sourceUv.height;
                }
                status = builder.DrawImage(
                    image, geometry.destination, geometry.sourceUv, tint);
                if (!status) break;
                ++tileCount;
            }
        }
    }
    const Base::Status paintStatus = status
        ? Base::Status::Ok()
        : status.GetStatus();
    if (transformed) {
        Base::Result<void> popped = builder.PopTransform();
        if (status && !popped) status = popped.GetStatus();
    }
    if (clipped) {
        Base::Result<void> popped = builder.PopClip();
        if (status && !popped) status = popped.GetStatus();
    }
    return paintStatus.IsOk() ? status : Base::Result<void>(paintStatus);
}

} // namespace

Base::Ref<Brush> Shape::GetFill() const noexcept {
    return GetValueOr(
        FillProperty, Base::Ref<Brush>{});
}

Base::Ref<Brush> Shape::GetStroke() const noexcept {
    return GetValueOr(
        StrokeProperty, Base::Ref<Brush>{});
}

double Shape::GetStrokeThickness() const noexcept {
    return GetValueOr(StrokeThicknessProperty, 1.0);
}

Stretch Shape::GetStretch() const noexcept {
    return GetValueOr(StretchProperty, Stretch::Fill);
}

void Shape::SetFill(
    Base::Ref<Brush> value) noexcept {
    SetValue(FillProperty, std::move(value));
}

void Shape::SetStroke(
    Base::Ref<Brush> value) noexcept {
    SetValue(StrokeProperty, std::move(value));
}

Base::Ref<Media::Pen> Shape::GetPen() const noexcept {
    return GetValueOr(PenProperty, Base::Ref<Media::Pen>{});
}

void Shape::SetPen(Base::Ref<Media::Pen> value) noexcept {
    SetValue(PenProperty, std::move(value));
}

void Shape::SetStrokeThickness(double value) noexcept {
    SetValue(StrokeThicknessProperty, value);
}

void Shape::SetStretch(Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

double Rectangle::GetRadiusX() const noexcept {
    return GetValueOr(RadiusXProperty, 0.0);
}

double Rectangle::GetRadiusY() const noexcept {
    return GetValueOr(RadiusYProperty, 0.0);
}

void Rectangle::SetRadiusX(double value) noexcept {
    SetValue(RadiusXProperty, value);
}

void Rectangle::SetRadiusY(double value) noexcept {
    SetValue(RadiusYProperty, value);
}

Size Rectangle::MeasureOverride(
    Size) noexcept {
    const double stroke =
        std::max(0.0, GetStrokeThickness());
    return Size{stroke * 2.0, stroke * 2.0};
}

void Rectangle::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const Size renderSize = GetRenderSize();
    if (renderSize.width <= 0.0 ||
        renderSize.height <= 0.0) {
        return;
    }

    const Rect bounds{
        0.0, 0.0, renderSize.width, renderSize.height};
    const double radius = std::max(
        0.0, std::min(GetRadiusX(), GetRadiusY()));
    Base::Ref<Brush> fillBrush = GetFill();
    if (fillBrush) {
        static_cast<void>(::Aero::Media::PaintBrushRect(
            builder, fillBrush, bounds, radius));
    }

    const Color stroke = ::Aero::Media::SampleBrush(GetStroke());
    const double thickness = GetStrokeThickness();
    if (stroke.alpha > 0.0F && thickness > 0.0) {
        static_cast<void>(builder.StrokeRect(
            bounds, stroke, thickness, radius));
    }
}

Size Ellipse::MeasureOverride(
    Size) noexcept {
    const double stroke =
        std::max(0.0, GetStrokeThickness());
    return Size{stroke * 2.0, stroke * 2.0};
}

void Ellipse::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const Size renderSize = GetRenderSize();
    if (renderSize.width <= 0.0 ||
        renderSize.height <= 0.0) {
        return;
    }

    auto paintEllipse = [&](Rect bounds, Color color) noexcept
        -> Base::Result<void> {
        if (bounds.width <= 0.0 || bounds.height <= 0.0 ||
            color.alpha <= 0.0F) {
            return {};
        }
        Transform2D transform;
        transform.m11 = bounds.width;
        transform.m22 = bounds.height;
        transform.dx = bounds.x;
        transform.dy = bounds.y;
        Base::Result<void> pushed =
            builder.PushTransform(transform);
        if (!pushed) return pushed.GetStatus();
        Base::Result<void> painted = builder.FillRoundedRect(
            Rect{0.0, 0.0, 1.0, 1.0}, color, 0.5);
        Base::Result<void> popped = builder.PopTransform();
        if (!painted) return painted.GetStatus();
        return popped;
    };

    auto paintEllipseBrush = [&](Rect bounds, const Base::Ref<Brush>& brush)
        noexcept -> Base::Result<void> {
        if (!brush || bounds.width <= 0.0 || bounds.height <= 0.0) {
            return {};
        }
        Transform2D transform;
        transform.m11 = bounds.width;
        transform.m22 = bounds.height;
        transform.dx = bounds.x;
        transform.dy = bounds.y;
        Base::Result<void> pushed = builder.PushTransform(transform);
        if (!pushed) return pushed.GetStatus();
        Base::Result<void> painted = ::Aero::Media::PaintBrushRect(
            builder,
            brush,
            Rect{0.0, 0.0, 1.0, 1.0},
            0.5,
            GetFlowDirection() == FlowDirection::RightToLeft);
        Base::Result<void> popped = builder.PopTransform();
        if (!painted) return painted.GetStatus();
        return popped;
    };

    const Base::Ref<Brush> fillBrush = GetFill();
    const Base::Ref<Brush> strokeBrush = GetStroke();
    const bool spatialFill =
        ::Aero::Media::IsSpatialGradientBrush(fillBrush.Get());
    const Color fill = spatialFill
        ? Color{}
        : ::Aero::Media::SampleBrush(fillBrush);
    const double thickness = std::max(0.0, GetStrokeThickness());
    const Rect bounds{0.0, 0.0, renderSize.width, renderSize.height};
    const bool hasFill = spatialFill || fill.alpha > 0.0F;
    if (hasFill) {
        Base::Result<void> painted = spatialFill
            ? paintEllipseBrush(bounds, fillBrush)
            : paintEllipse(bounds, fill);
        if (!painted) return;
    }
    if (strokeBrush && thickness > 0.0) {
        // Match AeroGUI Ellipse: fill/stroke as rounded-rect primitives, not
        // Flatten+TessellateStroke. A 200px clock bezel was emitting hundreds
        // of GPU gradient triangles per frame and stalling first paint.
        const double half = thickness * 0.5;
        const Rect strokeBounds{
            half, half,
            std::max(0.0, renderSize.width - thickness),
            std::max(0.0, renderSize.height - thickness)};
        if (strokeBounds.width > 0.0 && strokeBounds.height > 0.0) {
            const double radius = std::min(
                strokeBounds.width, strokeBounds.height) * 0.5;
            static_cast<void>(::Aero::Media::PaintBrushRoundedStroke(
                builder,
                strokeBrush,
                strokeBounds,
                thickness,
                radius,
                GetFlowDirection() == FlowDirection::RightToLeft));
        }
    }
}

namespace {

void StrokeLineSegment(
    DisplayListBuilder& builder,
    Point start,
    Point end,
    Color color,
    double thickness) noexcept {
    if (color.alpha <= 0.0F || thickness <= 0.0) return;
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double length = std::hypot(dx, dy);
    if (!(length > 0.0) || !std::isfinite(length)) return;
    const double cosine = dx / length;
    const double sine = dy / length;
    Transform2D transform;
    transform.m11 = cosine;
    transform.m12 = sine;
    transform.m21 = -sine;
    transform.m22 = cosine;
    transform.dx = start.x;
    transform.dy = start.y;
    Base::Result<void> pushed = builder.PushTransform(transform);
    if (!pushed) return;
    static_cast<void>(builder.FillRect(
        Rect{0.0, -thickness * 0.5, length, thickness}, color));
    static_cast<void>(builder.PopTransform());
}

Base::Result<void> ParsePoints(
    StringView text,
    Base::Vector<Point>& points) noexcept {
    points.Clear();
    const char* cursor = text.Data();
    const char* const end = text.Data() + text.SizeBytes();
    auto skipSeparators = [&cursor, end]() noexcept {
        while (cursor < end &&
               (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' ||
                *cursor == '\r' || *cursor == ',')) {
            ++cursor;
        }
    };
    for (;;) {
        skipSeparators();
        if (cursor >= end) break;
        char* afterX = nullptr;
        const double x = std::strtod(cursor, &afterX);
        if (afterX == cursor || !std::isfinite(x)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Shape point list is invalid");
        }
        cursor = afterX;
        skipSeparators();
        if (cursor >= end) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Shape point list is invalid");
        }
        char* afterY = nullptr;
        const double y = std::strtod(cursor, &afterY);
        if (afterY == cursor || !std::isfinite(y)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Shape point list is invalid");
        }
        cursor = afterY;
        Base::Result<void> stored = points.PushBack(Point{x, y});
        if (!stored) return stored.GetStatus();
    }
    return {};
}

Size PointsExtent(
    Span<const Point> points,
    double stroke) noexcept {
    if (points.Empty()) {
        return Size{stroke * 2.0, stroke * 2.0};
    }
    double minX = points[0].x;
    double minY = points[0].y;
    double maxX = points[0].x;
    double maxY = points[0].y;
    for (std::uint32_t index = 1U; index < points.Size(); ++index) {
        minX = std::min(minX, points[index].x);
        minY = std::min(minY, points[index].y);
        maxX = std::max(maxX, points[index].x);
        maxY = std::max(maxY, points[index].y);
    }
    return Size{
        std::max(0.0, maxX - minX) + stroke * 2.0,
        std::max(0.0, maxY - minY) + stroke * 2.0};
}

void FillPointFan(
    DisplayListBuilder& builder,
    Span<const Point> points,
    Color fill) noexcept {
    if (fill.alpha <= 0.0F || points.Size() < 3U) return;
    for (std::uint32_t index = 1U; index + 1U < points.Size(); ++index) {
        const Point quad[4] = {
            points[0], points[index], points[index + 1U], points[index + 1U]};
        const Color colors[4] = {fill, fill, fill, fill};
        static_cast<void>(builder.FillGradientQuad(quad, colors));
    }
}

} // namespace

double Line::GetX1() const noexcept { return GetValueOr(X1Property, 0.0); }
double Line::GetY1() const noexcept { return GetValueOr(Y1Property, 0.0); }
double Line::GetX2() const noexcept { return GetValueOr(X2Property, 0.0); }
double Line::GetY2() const noexcept { return GetValueOr(Y2Property, 0.0); }
void Line::SetX1(double value) noexcept { SetValue(X1Property, value); }
void Line::SetY1(double value) noexcept { SetValue(Y1Property, value); }
void Line::SetX2(double value) noexcept { SetValue(X2Property, value); }
void Line::SetY2(double value) noexcept { SetValue(Y2Property, value); }

Size Line::MeasureOverride(Size) noexcept {
    const double stroke = std::max(0.0, GetStrokeThickness());
    const double width = std::fabs(GetX2() - GetX1());
    const double height = std::fabs(GetY2() - GetY1());
    return Size{width + stroke * 2.0, height + stroke * 2.0};
}

void Line::OnRender(::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    StrokeLineSegment(
        builder,
        Point{GetX1(), GetY1()},
        Point{GetX2(), GetY2()},
        ::Aero::Media::SampleBrush(GetStroke()),
        GetStrokeThickness());
}

FillRule Polygon::GetFillRule() const noexcept {
    return GetValueOr(FillRuleProperty, FillRule::EvenOdd);
}
void Polygon::SetFillRule(FillRule value) noexcept {
    SetValue(FillRuleProperty, value);
}
Span<const Point> Polygon::GetPoints() const noexcept {
    return points_.AsSpan();
}
Result<void> Polygon::SetPoints(Span<const Point> points) noexcept {
    points_.Clear();
    Result<void> stored = points_.Append(points);
    if (stored) {
        static_cast<void>(InvalidateMeasure());
        static_cast<void>(InvalidateVisual());
    }
    return stored;
}
Result<void> Polygon::AddPoint(Point point) noexcept {
    Result<void> stored = points_.PushBack(point);
    if (stored) {
        static_cast<void>(InvalidateMeasure());
        static_cast<void>(InvalidateVisual());
    }
    return stored;
}
void Polygon::ClearPoints() noexcept {
    if (points_.Empty()) return;
    points_.Clear();
    static_cast<void>(InvalidateMeasure());
    static_cast<void>(InvalidateVisual());
}
Result<void> Polygon::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePoints(text, parsed);
    if (!status) return status.GetStatus();
    return SetPoints(parsed.AsSpan());
}
Size Polygon::MeasureOverride(Size) noexcept {
    return PointsExtent(points_.AsSpan(), std::max(0.0, GetStrokeThickness()));
}
void Polygon::OnRender(::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    FillPointFan(builder, points_.AsSpan(), ::Aero::Media::SampleBrush(GetFill()));
    const Color stroke = ::Aero::Media::SampleBrush(GetStroke());
    const double thickness = GetStrokeThickness();
    if (points_.Size() < 2U) return;
    for (std::uint32_t index = 0U; index + 1U < points_.Size(); ++index) {
        StrokeLineSegment(
            builder, points_[index], points_[index + 1U], stroke, thickness);
    }
    StrokeLineSegment(
        builder, points_[points_.Size() - 1U], points_[0], stroke, thickness);
}

Span<const Point> Polyline::GetPoints() const noexcept {
    return points_.AsSpan();
}
Result<void> Polyline::SetPoints(Span<const Point> points) noexcept {
    points_.Clear();
    Result<void> stored = points_.Append(points);
    if (stored) {
        static_cast<void>(InvalidateMeasure());
        static_cast<void>(InvalidateVisual());
    }
    return stored;
}
Result<void> Polyline::AddPoint(Point point) noexcept {
    Result<void> stored = points_.PushBack(point);
    if (stored) {
        static_cast<void>(InvalidateMeasure());
        static_cast<void>(InvalidateVisual());
    }
    return stored;
}
void Polyline::ClearPoints() noexcept {
    if (points_.Empty()) return;
    points_.Clear();
    static_cast<void>(InvalidateMeasure());
    static_cast<void>(InvalidateVisual());
}
Result<void> Polyline::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePoints(text, parsed);
    if (!status) return status.GetStatus();
    return SetPoints(parsed.AsSpan());
}
Size Polyline::MeasureOverride(Size) noexcept {
    return PointsExtent(points_.AsSpan(), std::max(0.0, GetStrokeThickness()));
}
void Polyline::OnRender(::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const Color stroke = ::Aero::Media::SampleBrush(GetStroke());
    const double thickness = GetStrokeThickness();
    for (std::uint32_t index = 0U; index + 1U < points_.Size(); ++index) {
        StrokeLineSegment(
            builder, points_[index], points_[index + 1U], stroke, thickness);
    }
}

} // namespace Aero::Shapes
