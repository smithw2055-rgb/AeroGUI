#include <Aero/Media/DrawingContext.hpp>
#include <Aero/Media/DashStyle.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/LineGeometry.hpp>
#include <Aero/Media/Pen.hpp>

#include "DisplayList.hpp"
#include "gui/media/BrushRendering.hpp"
#include "gui/media/GeometryFlatten.hpp"
#include "gui/media/StrokeTessellate.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/media/MediaState.hpp"

namespace Aero::Media {
namespace {

class StrokeContourSink final : public FlattenSink {
public:
    StrokeContourSink(
        Base::Vector<Point>& points,
        Base::Vector<std::uint32_t>& starts,
        Base::Vector<std::uint32_t>& counts,
        Base::Vector<std::uint8_t>& closed) noexcept
        : points_(&points),
          starts_(&starts),
          counts_(&counts),
          closed_(&closed) {}

    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        Result<void> finished = Flush(closedFlag_);
        if (!finished) return finished.GetStatus();
        closedFlag_ = isClosed;
        contour_.Clear();
        return contour_.PushBack(start);
    }
    Result<void> AddPoint(Point point) noexcept override {
        if (contour_.Empty()) {
            return BeginFigure(point, closedFlag_);
        }
        return contour_.PushBack(point);
    }
    Result<void> EndFigure(bool isClosed) noexcept override {
        closedFlag_ = isClosed;
        return Flush(isClosed);
    }
    Result<void> Finish() noexcept { return Flush(closedFlag_); }

private:
    Result<void> Flush(bool closed) noexcept {
        if (contour_.Size() < 2U) {
            contour_.Clear();
            return {};
        }
        Result<void> added = starts_->PushBack(points_->Size());
        if (added) added = counts_->PushBack(contour_.Size());
        if (added) added = closed_->PushBack(closed ? std::uint8_t{1} : std::uint8_t{0});
        if (added) added = points_->Append(contour_.AsSpan());
        contour_.Clear();
        return added;
    }

    Base::Vector<Point> contour_;
    Base::Vector<Point>* points_ = nullptr;
    Base::Vector<std::uint32_t>* starts_ = nullptr;
    Base::Vector<std::uint32_t>* counts_ = nullptr;
    Base::Vector<std::uint8_t>* closed_ = nullptr;
    bool closedFlag_ = false;
};

Result<void> EmitTriangles(
    ::Aero::Render::DisplayListBuilder& builder,
    const Base::Vector<Point>& vertices,
    const Base::Vector<std::uint32_t>& indices,
    Color color) noexcept {
    if (color.alpha <= 0.0F || indices.Size() < 3U) return {};
    for (std::uint32_t index = 0U; index + 2U < indices.Size(); index += 3U) {
        const Point a = vertices[indices[index]];
        const Point b = vertices[indices[index + 1U]];
        const Point c = vertices[indices[index + 2U]];
        const Point quad[4] = {a, b, c, c};
        const Color colors[4] = {color, color, color, color};
        Result<void> drawn = builder.FillGradientQuad(quad, colors);
        if (!drawn) return drawn.GetStatus();
    }
    return {};
}

Result<void> StrokePenGeometry(
    ::Aero::Render::DisplayListBuilder& builder,
    const Pen& pen,
    const Geometry& geometry) noexcept {
    Base::Ref<Brush> brush = pen.GetBrush();
    if (!brush || pen.GetThickness() <= 0.0) return {};
    const bool spatial = IsSpatialGradientBrush(brush.Get());
    Color color{};
    if (!spatial) {
        color = SampleBrush(brush);
        if (color.alpha <= 0.0F) return {};
    } else if (brush->GetOpacity() <= 0.0) {
        return {};
    }
    Base::Vector<Point> points;
    Base::Vector<std::uint32_t> starts;
    Base::Vector<std::uint32_t> counts;
    Base::Vector<std::uint8_t> closed;
    StrokeContourSink sink(points, starts, counts, closed);
    Result<void> flattened = geometry.Flatten(sink);
    if (flattened) flattened = sink.Finish();
    if (!flattened) return flattened.GetStatus();
    Base::Vector<double> dashes;
    double dashOffset = 0.0;
    if (Ref<DashStyle> style = pen.GetDashStyle()) {
        const Base::Span<const double> values = style->GetDashes();
        for (std::uint32_t index = 0U; index < values.Size(); ++index) {
            Result<void> added = dashes.PushBack(values[index]);
            if (!added) return added.GetStatus();
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
        Rect bounds = geometry.GetBounds();
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

} // namespace

Base::Result<void> DrawingContext::PushClip(
    Base::Rect clip) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PushClip(clip);
}

Base::Result<void> DrawingContext::PopClip() noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PopClip();
}

Base::Result<void> DrawingContext::PushOpacity(
    double opacity) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PushOpacity(opacity);
}

Base::Result<void> DrawingContext::PopOpacity() noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PopOpacity();
}

Base::Result<void> DrawingContext::PushTransform(
    Base::Transform2D transform) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PushTransform(transform);
}

Base::Result<void> DrawingContext::PopTransform() noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PopTransform();
}

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    Base::Color color) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .FillRect(bounds, color);
}

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush) noexcept {
    return Media::PaintBrushRect(
        ::Aero::Render::DrawingPrivate::Builder(*this),
        brush,
        bounds);
}

Base::Result<void> DrawingContext::DrawRectangle(
    const Base::Ref<Media::Brush>& fill,
    const Base::Ref<Media::Brush>& stroke,
    Base::Rect bounds,
    double strokeThickness) noexcept {
    Base::Result<void> result = DrawRectangle(bounds, fill);
    if (!result || !stroke || strokeThickness <= 0.0) {
        return result;
    }
    return DrawRectangleOutline(
        bounds, stroke, strokeThickness);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    Base::Color color,
    double radius) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .FillRoundedRect(bounds, color, radius);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double radius) noexcept {
    return Media::PaintBrushRect(
        ::Aero::Render::DrawingPrivate::Builder(*this),
        brush,
        bounds,
        radius);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    Base::Color color,
    double thickness) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .StrokeRect(bounds, color, thickness);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double thickness) noexcept {
    const Base::Color color =
        Media::SampleBrush(brush);
    return color.alpha > 0.0F
        ? ::Aero::Render::DrawingPrivate::Builder(*this)
              .StrokeRect(bounds, color, thickness)
        : Base::Result<void>();
}

Result<void> DrawingContext::DrawLine(
    const Ref<Pen>& pen,
    Base::Point start,
    Base::Point end) noexcept {
    if (!pen) return {};
    LineGeometry line;
    line.SetStartPoint(start);
    line.SetEndPoint(end);
    return StrokePenGeometry(
        ::Aero::Render::DrawingPrivate::Builder(*this),
        *pen,
        line);
}

Result<void> DrawingContext::DrawGeometry(
    const Ref<Brush>& brush,
    const Ref<Pen>& pen,
    const Geometry& geometry) noexcept {
    auto& builder = ::Aero::Render::DrawingPrivate::Builder(*this);
    if (brush) {
        Base::Vector<Point> vertices;
        Base::Vector<std::uint32_t> indices;
        Result<void> filled = TessellateGeometryFill(
            geometry, vertices, indices);
        if (!filled) return filled.GetStatus();
        Result<void> drawn = PaintBrushGeometry(
            builder,
            brush,
            vertices.AsSpan(),
            indices.AsSpan(),
            geometry.GetBounds());
        if (!drawn) return drawn.GetStatus();
    }
    if (!pen) return {};
    return StrokePenGeometry(builder, *pen, geometry);
}

} // namespace Aero::Media
