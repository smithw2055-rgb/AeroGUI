#include <Aero/Media/DrawingContext.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Geometries.hpp>

#include "render/DisplayList.hpp"
#include "BrushRendering.hpp"
#include "GeometryFlatten.hpp"
#include "FlattenSinks.inl"
#include "StrokeTessellate.hpp"

namespace Aero::Media {
namespace {


void AssertOk(Base::Result<void> result) noexcept {
    AERO_ASSERT(result);
    (void)result;
}

Result<void> EmitTriangles(
    ::Aero::Render::DisplayListBuilder& builder,
    const Base::Vector<Point>& vertices,
    const Base::Vector<std::uint32_t>& indices,
    Color color) noexcept {
    if (color.alpha <= 0.0F || indices.Size() < 3U) return {};
    return builder.DrawTriangles(
        vertices.Data(), static_cast<std::uint32_t>(vertices.Size()),
        indices.Data(), static_cast<std::uint32_t>(indices.Size()),
        color);
}

// P4.4: stroke paint resolution shared by the standalone stroke path and
// the unified fill+stroke path. Returns false when stroking is a no-op.
// (No failure modes: early-outs are success.)
bool ResolveStrokePaint(
    const Pen& pen,
    Base::Ref<Brush>& brushOut,
    Color& colorOut,
    bool& spatialOut) noexcept {
    Base::Ref<Brush> brush = pen.GetBrush();
    if (!brush || pen.GetThickness() <= 0.0) return false;
    const bool spatial = IsSpatialGradientBrush(brush.Get());
    Color color{};
    if (!spatial) {
        color = SampleBrush(brush);
        if (color.alpha <= 0.0F) return false;
    } else if (brush->GetOpacity() <= 0.0) {
        return false;
    }
    brushOut = brush;
    colorOut = color;
    spatialOut = spatial;
    return true;
}

Result<void> StrokeContours(
    ::Aero::Render::DisplayListBuilder& builder,
    const Pen& pen,
    const Base::Ref<Brush>& brush,
    Color color,
    bool spatial,
    Rect geometryBounds,
    const Base::Vector<Point>& points,
    const Base::Vector<std::uint32_t>& starts,
    const Base::Vector<std::uint32_t>& counts,
    const Base::Vector<std::uint8_t>& closed) noexcept {
    Base::Vector<double> dashes;
    double dashOffset = 0.0;
    if (Ref<DashStyle> style = pen.GetDashStyle()) {
        const Base::Span<const double> values = style->GetDashes();
        for (std::uint32_t index = 0U; index < values.Size(); ++index) {
            dashes.PushBack(values[index]);
        }
        dashOffset = style->GetOffset();
    }
    Base::Vector<Point> vertices;
    Base::Vector<std::uint32_t> indices;
    Result<void> stroked = TessellateStroke(
        points,
        starts,
        counts,
        closed,
        pen.GetThickness(),
        0.0,
        1.0,
        pen.GetLineJoin(),
        pen.GetStartLineCap(),
        pen.GetEndLineCap(),
        pen.GetMiterLimit(),
        {dashes.Data(), dashes.Size()},
        dashOffset,
        vertices,
        indices);
    if (!stroked) return stroked.GetStatus();
    if (spatial) {
        Rect bounds = geometryBounds;
        const double half = pen.GetThickness() * 0.5;
        if (half > 0.0) {
            bounds.x -= half;
            bounds.y -= half;
            bounds.width += half * 2.0;
            bounds.height += half * 2.0;
        }
        return PaintBrushGeometry(
            builder,
            brush,
            vertices.AsSpan(),
            indices.AsSpan(),
            bounds);
    }
    return EmitTriangles(builder, vertices, indices, color);
}

Result<void> StrokePenGeometry(
    ::Aero::Render::DisplayListBuilder& builder,
    const Pen& pen,
    const Geometry& geometry) noexcept {
    Base::Ref<Brush> brush;
    Color color{};
    bool spatial = false;
    if (!ResolveStrokePaint(pen, brush, color, spatial)) return {};
    Base::Vector<Point> points;
    Base::Vector<std::uint32_t> starts;
    Base::Vector<std::uint32_t> counts;
    Base::Vector<std::uint8_t> closed;
    StrokeContourSink sink(points, starts, counts, closed);
    geometry.Flatten(sink);
    sink.Finish();
    return StrokeContours(
        builder, pen, brush, color, spatial, geometry.GetBounds(), points,
        starts, counts, closed);
}

} // namespace

void DrawingContext::PushClip(
    Base::Rect clip) noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .PushClip(clip));
}

void DrawingContext::PopClip() noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .PopClip());
}

void DrawingContext::PushOpacity(
    double opacity) noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .PushOpacity(opacity));
}

void DrawingContext::PopOpacity() noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .PopOpacity());
}

void DrawingContext::PushTransform(
    Base::Transform2D transform) noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .PushTransform(transform));
}

void DrawingContext::PopTransform() noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .PopTransform());
}

void DrawingContext::DrawRectangle(
    Base::Rect bounds,
    Base::Color color) noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .FillRect(bounds, color));
}

void DrawingContext::DrawRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush) noexcept {
    AssertOk(Media::PaintBrushRect(
        ::Aero::Render::DrawingBridge::Builder(*this),
        brush,
        bounds));
}

void DrawingContext::DrawRectangle(
    const Base::Ref<Media::Brush>& fill,
    const Base::Ref<Media::Brush>& stroke,
    Base::Rect bounds,
    double strokeThickness) noexcept {
    DrawRectangle(bounds, fill);
    if (!stroke || strokeThickness <= 0.0) {
        return;
    }
    DrawRectangleOutline(bounds, stroke, strokeThickness);
}

void DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    Base::Color color,
    double radius) noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .FillRoundedRect(bounds, color, radius));
}

void DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double radius) noexcept {
    AssertOk(Media::PaintBrushRect(
        ::Aero::Render::DrawingBridge::Builder(*this),
        brush,
        bounds,
        radius));
}

void DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    Base::Color color,
    double thickness) noexcept {
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .StrokeRect(bounds, color, thickness));
}

void DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double thickness) noexcept {
    const Base::Color color =
        Media::SampleBrush(brush);
    if (color.alpha > 0.0F) {
        AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
              .StrokeRect(bounds, color, thickness));
    }
}

void DrawingContext::DrawLine(
    const Ref<Pen>& pen,
    Base::Point start,
    Base::Point end) noexcept {
    if (!pen) return;
    LineGeometry line;
    line.SetStartPoint(start);
    line.SetEndPoint(end);
    AssertOk(StrokePenGeometry(
        ::Aero::Render::DrawingBridge::Builder(*this),
        *pen,
        line));
}

void DrawingContext::DrawGeometry(
    const Ref<Brush>& brush,
    const Ref<Pen>& pen,
    const Geometry& geometry) noexcept {
    auto& builder = ::Aero::Render::DrawingBridge::Builder(*this);
    if (brush && pen) {
        Base::Vector<Point> fillPoints;
        Base::Vector<FillContour> fillContours;
        Base::Vector<Point> strokePoints;
        Base::Vector<std::uint32_t> starts;
        Base::Vector<std::uint32_t> counts;
        Base::Vector<std::uint8_t> closed;
        FlattenGeometryContours(
            geometry, fillPoints, fillContours, strokePoints, starts,
            counts, closed);
        Base::Vector<Point> vertices;
        Base::Vector<std::uint32_t> indices;
        Result<void> filled = TessellateFillContours(
            fillPoints, fillContours, vertices, indices);
        if (!filled) { AERO_ASSERT(false); return; }
        Result<void> drawn = PaintBrushGeometry(
            builder,
            brush,
            vertices.AsSpan(),
            indices.AsSpan(),
            geometry.GetBounds());
        if (!drawn) { AERO_ASSERT(false); return; }
        Base::Ref<Brush> strokeBrush;
        Color strokeColor{};
        bool strokeSpatial = false;
        if (!ResolveStrokePaint(
                *pen, strokeBrush, strokeColor, strokeSpatial)) {
            return;
        }
        AssertOk(StrokeContours(
            builder, *pen, strokeBrush, strokeColor, strokeSpatial,
            geometry.GetBounds(), strokePoints, starts, counts, closed));
        return;
    }
    if (brush) {
        Base::Vector<Point> vertices;
        Base::Vector<std::uint32_t> indices;
        Result<void> filled = TessellateGeometryFill(
            geometry, vertices, indices);
        if (!filled) { AERO_ASSERT(false); return; }
        Result<void> drawn = PaintBrushGeometry(
            builder,
            brush,
            vertices.AsSpan(),
            indices.AsSpan(),
            geometry.GetBounds());
        if (!drawn) { AERO_ASSERT(false); return; }
    }
    if (!pen) return;
    AssertOk(StrokePenGeometry(builder, *pen, geometry));
}

void DrawingContext::DrawEllipse(
    const Ref<Brush>& brush,
    const Ref<Pen>& pen,
    Base::Point center,
    double radiusX,
    double radiusY) noexcept {
    EllipseGeometry ellipse;
    ellipse.SetCenter(center);
    ellipse.SetRadiusX(radiusX);
    ellipse.SetRadiusY(radiusY);
    DrawGeometry(brush, pen, ellipse);
}

void DrawingContext::DrawRoundedRectangle(
    const Ref<Brush>& brush,
    const Ref<Pen>& pen,
    Base::Rect bounds,
    double radiusX,
    double radiusY) noexcept {
    RectangleGeometry rectGeom;
    rectGeom.SetRect(bounds);
    rectGeom.SetRadiusX(radiusX);
    rectGeom.SetRadiusY(radiusY);
    DrawGeometry(brush, pen, rectGeom);
}

void DrawingContext::DrawImage(
    std::uint64_t imageId,
    Base::Rect bounds,
    Base::Rect sourceUv) noexcept {
    if (imageId == 0U || bounds.width <= 0.0 || bounds.height <= 0.0) return;
    AssertOk(::Aero::Render::DrawingBridge::Builder(*this)
        .DrawImage(imageId, bounds, sourceUv));
}

} // namespace Aero::Media
