#include "gui/media/StrokeTessellate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <climits>
#include <limits>

namespace Aero::Media {
namespace {

constexpr double kPi = 3.14159265358979323846;

bool SamePoint(Point left, Point right) noexcept {
    constexpr double Epsilon = 1.0e-9;
    return std::abs(left.x - right.x) <= Epsilon &&
        std::abs(left.y - right.y) <= Epsilon;
}

Result<void> AppendStrokeTriangle(
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices,
    Point a,
    Point b,
    Point c) noexcept {
    if (vertices.Size() > UINT32_MAX - 3U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Path stroke tessellation exceeds the 32-bit vertex limit");
    }
    const std::uint32_t base = vertices.Size();
    const Point triangle[] = {a, b, c};
    Result<void> added = vertices.Append({triangle, 3U});
    if (!added) return added.GetStatus();
    const std::uint32_t indices3[] = {base, base + 1U, base + 2U};
    return indices.Append({indices3, 3U});
}

Result<void> AppendStrokeQuad(
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices,
    Point a,
    Point b,
    Point c,
    Point d) noexcept {
    if (vertices.Size() > UINT32_MAX - 4U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Path stroke tessellation exceeds the 32-bit vertex limit");
    }
    const std::uint32_t base = vertices.Size();
    const Point quad[] = {a, b, c, d};
    Result<void> added = vertices.Append({quad, 4U});
    if (!added) return added.GetStatus();
    const std::uint32_t triangles[] = {
        base, base + 1U, base + 2U,
        base, base + 2U, base + 3U};
    return indices.Append({triangles, 6U});
}

Result<void> AppendStrokeFan(
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices,
    Point center,
    Point start,
    Point end,
    double radius,
    bool ccw) noexcept {
    if (radius <= 1.0e-9) return {};
    const double a0 = std::atan2(start.y - center.y, start.x - center.x);
    const double a1 = std::atan2(end.y - center.y, end.x - center.x);
    double delta = a1 - a0;
    if (ccw) {
        while (delta <= 0.0) delta += kPi * 2.0;
    } else {
        while (delta >= 0.0) delta -= kPi * 2.0;
    }
    const double absDelta = std::abs(delta);
    if (absDelta <= 1.0e-9) return {};
    const std::uint32_t steps = std::max(
        1U,
        static_cast<std::uint32_t>(std::ceil(absDelta / (kPi / 8.0))));
    Point previous = start;
    for (std::uint32_t step = 1U; step <= steps; ++step) {
        const double angle = a0 + delta * static_cast<double>(step) /
            static_cast<double>(steps);
        Point next = step == steps
            ? end
            : Point{
                center.x + radius * std::cos(angle),
                center.y + radius * std::sin(angle)};
        Result<void> added =
            AppendStrokeTriangle(vertices, indices, center, previous, next);
        if (!added) return added.GetStatus();
        previous = next;
    }
    return {};
}

struct StrokeSegment {
    Point a;
    Point b;
    double nx = 0.0;
    double ny = 0.0;
    double ux = 0.0;
    double uy = 0.0;
    std::uint32_t index = 0U;
    bool startVertex = false;
    bool endVertex = false;
};

Result<void> EmitStrokeJoin(
    const StrokeSegment& incoming,
    const StrokeSegment& outgoing,
    PenLineJoin join,
    double miterLimit,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    const Point joint = incoming.b;
    const double cross =
        incoming.ux * outgoing.uy - incoming.uy * outgoing.ux;
    const double dot =
        incoming.ux * outgoing.ux + incoming.uy * outgoing.uy;
    if (std::abs(cross) <= 1.0e-12 && dot > 0.0) {
        return {};
    }
    const double side = cross > 0.0 ? -1.0 : 1.0;
    const Point outer0{
        joint.x + side * incoming.nx,
        joint.y + side * incoming.ny};
    const Point outer1{
        joint.x + side * outgoing.nx,
        joint.y + side * outgoing.ny};
    const double half = std::hypot(incoming.nx, incoming.ny);
    if (join == PenLineJoin::Round) {
        return AppendStrokeFan(
            vertices,
            indices,
            joint,
            outer0,
            outer1,
            half,
            cross < 0.0);
    }
    if (join == PenLineJoin::Bevel) {
        return AppendStrokeTriangle(
            vertices, indices, joint, outer0, outer1);
    }
    const double det =
        incoming.ux * outgoing.uy - incoming.uy * outgoing.ux;
    if (std::abs(det) <= 1.0e-12) {
        return AppendStrokeTriangle(
            vertices, indices, joint, outer0, outer1);
    }
    const double t =
        ((outer1.x - outer0.x) * outgoing.uy -
         (outer1.y - outer0.y) * outgoing.ux) / det;
    const Point miter{
        outer0.x + t * incoming.ux,
        outer0.y + t * incoming.uy};
    const double miterLength =
        std::hypot(miter.x - joint.x, miter.y - joint.y);
    if (half > 1.0e-12 && miterLimit > 0.0 && miterLength / half > miterLimit) {
        return AppendStrokeTriangle(
            vertices, indices, joint, outer0, outer1);
    }
    Result<void> added = AppendStrokeTriangle(
        vertices, indices, joint, outer0, miter);
    if (!added) return added.GetStatus();
    return AppendStrokeTriangle(
        vertices, indices, joint, miter, outer1);
}

Result<void> EmitStrokeCap(
    Point center,
    double nx,
    double ny,
    double ux,
    double uy,
    PenLineCap cap,
    bool start,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    if (cap == PenLineCap::Flat) return {};
    const double sign = start ? -1.0 : 1.0;
    const Point left{center.x + nx, center.y + ny};
    const Point right{center.x - nx, center.y - ny};
    const Point outward{
        center.x + sign * ux * std::hypot(nx, ny),
        center.y + sign * uy * std::hypot(nx, ny)};
    if (cap == PenLineCap::Square) {
        const Point leftOut{
            left.x + sign * ux * std::hypot(nx, ny),
            left.y + sign * uy * std::hypot(nx, ny)};
        const Point rightOut{
            right.x + sign * ux * std::hypot(nx, ny),
            right.y + sign * uy * std::hypot(nx, ny)};
        return start
            ? AppendStrokeQuad(
                  vertices, indices, leftOut, left, right, rightOut)
            : AppendStrokeQuad(
                  vertices, indices, left, leftOut, rightOut, right);
    }
    if (cap == PenLineCap::Round) {
        return AppendStrokeFan(
            vertices,
            indices,
            center,
            start ? right : left,
            start ? left : right,
            std::hypot(nx, ny),
            true);
    }
    return AppendStrokeTriangle(
        vertices, indices, left, right, outward);
}

Result<void> EmitStrokeRun(
    const Base::Vector<StrokeSegment>& segments,
    std::uint32_t begin,
    std::uint32_t end,
    bool closedLoop,
    PenLineJoin join,
    PenLineCap startCap,
    PenLineCap endCap,
    double miterLimit,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    if (end <= begin) return {};
    for (std::uint32_t index = begin; index < end; ++index) {
        const StrokeSegment& segment = segments[index];
        Result<void> added = AppendStrokeQuad(
            vertices,
            indices,
            {segment.a.x + segment.nx, segment.a.y + segment.ny},
            {segment.b.x + segment.nx, segment.b.y + segment.ny},
            {segment.b.x - segment.nx, segment.b.y - segment.ny},
            {segment.a.x - segment.nx, segment.a.y - segment.ny});
        if (!added) return added.GetStatus();
        if (index + 1U < end &&
            segment.endVertex &&
            segments[index + 1U].startVertex) {
            added = EmitStrokeJoin(
                segment, segments[index + 1U], join, miterLimit, vertices, indices);
            if (!added) return added.GetStatus();
        }
    }
    if (closedLoop &&
        end - begin >= 2U &&
        segments[begin].startVertex &&
        segments[end - 1U].endVertex) {
        return EmitStrokeJoin(
            segments[end - 1U],
            segments[begin],
            join,
            miterLimit,
            vertices,
            indices);
    }
    if (closedLoop) return {};
    Result<void> capped = EmitStrokeCap(
        segments[begin].a,
        segments[begin].nx,
        segments[begin].ny,
        segments[begin].ux,
        segments[begin].uy,
        startCap,
        true,
        vertices,
        indices);
    if (!capped) return capped.GetStatus();
    return EmitStrokeCap(
        segments[end - 1U].b,
        segments[end - 1U].nx,
        segments[end - 1U].ny,
        segments[end - 1U].ux,
        segments[end - 1U].uy,
        endCap,
        false,
        vertices,
        indices);
}

} // namespace

Result<void> ExpandDashPattern(
    Base::Span<const double> dashes,
    Base::Vector<double>& pattern) noexcept {
    pattern.Clear();
    for (std::uint32_t index = 0U; index < dashes.Size(); ++index) {
        const double value = dashes[index];
        if (!std::isfinite(value) || value <= 0.0) continue;
        Result<void> added = pattern.PushBack(value);
        if (!added) return added.GetStatus();
    }
    if (pattern.Size() % 2U == 1U) {
        const std::uint32_t count = pattern.Size();
        for (std::uint32_t index = 0U; index < count; ++index) {
            Result<void> added = pattern.PushBack(pattern[index]);
            if (!added) return added.GetStatus();
        }
    }
    return {};
}

Result<void> SplitDashedSegments(
    const Base::Vector<StrokeSegment>& visible,
    Base::Span<const double> dashes,
    double dashOffset,
    Base::Vector<StrokeSegment>& dashed) noexcept {
    dashed.Clear();
    Base::Vector<double> pattern;
    Result<void> expanded = ExpandDashPattern(dashes, pattern);
    if (!expanded) return expanded.GetStatus();
    if (pattern.Size() < 2U) {
        return dashed.Append(visible.AsSpan());
    }
    double period = 0.0;
    for (std::uint32_t index = 0U; index < pattern.Size(); ++index) {
        period += pattern[index];
    }
    if (!(period > 1.0e-9) || !std::isfinite(period)) {
        return dashed.Append(visible.AsSpan());
    }
    double patternPos = std::fmod(dashOffset, period);
    if (patternPos < 0.0) patternPos += period;
    std::uint32_t runIndex = 0U;
    bool havePrevious = false;
    Point previousEnd{};
    for (std::uint32_t visibleIndex = 0U;
         visibleIndex < visible.Size(); ++visibleIndex) {
        const StrokeSegment& segment = visible[visibleIndex];
        const double dx = segment.b.x - segment.a.x;
        const double dy = segment.b.y - segment.a.y;
        const double length = std::hypot(dx, dy);
        if (length <= 1.0e-9) continue;
        double remaining = length;
        double consumed = 0.0;
        while (remaining > 1.0e-9) {
            double accumulated = 0.0;
            std::uint32_t window = 0U;
            for (; window < pattern.Size(); ++window) {
                if (patternPos < accumulated + pattern[window] - 1.0e-12) {
                    break;
                }
                accumulated += pattern[window];
            }
            if (window >= pattern.Size()) {
                patternPos = 0.0;
                continue;
            }
            const bool on = (window % 2U) == 0U;
            const double windowLeft =
                pattern[window] - (patternPos - accumulated);
            const double take = std::min(remaining, windowLeft);
            if (on && take > 1.0e-9) {
                const double from = consumed / length;
                const double to = (consumed + take) / length;
                StrokeSegment piece = segment;
                piece.a = {
                    segment.a.x + dx * from,
                    segment.a.y + dy * from};
                piece.b = {
                    segment.a.x + dx * to,
                    segment.a.y + dy * to};
                const bool connected =
                    havePrevious &&
                    SamePoint(previousEnd, piece.a);
                if (!connected) {
                    runIndex += 1U;
                }
                piece.index = runIndex;
                piece.startVertex = connected;
                piece.endVertex = false;
                if (!dashed.Empty() &&
                    dashed.Back().index == runIndex) {
                    dashed.Back().endVertex = true;
                    piece.startVertex = true;
                }
                Result<void> added = dashed.PushBack(piece);
                if (!added) return added.GetStatus();
                previousEnd = piece.b;
                havePrevious = true;
                runIndex += 1U;
            } else {
                havePrevious = false;
            }
            consumed += take;
            remaining -= take;
            patternPos += take;
            if (patternPos >= period - 1.0e-12) {
                patternPos = 0.0;
            }
        }
    }
    return {};
}

Result<void> TessellateStroke(
    const Base::Vector<Point>& points,
    const Base::Vector<std::uint32_t>& contourStarts,
    const Base::Vector<std::uint32_t>& contourCounts,
    const Base::Vector<std::uint8_t>& contourClosed,
    double thickness,
    double trimStart,
    double trimEnd,
    PenLineJoin join,
    PenLineCap startCap,
    PenLineCap endCap,
    double miterLimit,
    Base::Span<const double> dashes,
    double dashOffset,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    if (thickness <= 0.0 || trimEnd <= trimStart) {
        return {};
    }
    double totalLength = 0.0;
    for (std::uint32_t contour = 0U;
         contour < contourStarts.Size(); ++contour) {
        const std::uint32_t start = contourStarts[contour];
        const std::uint32_t count = contourCounts[contour];
        if (count < 2U) continue;
        const std::uint32_t segmentCount =
            count - 1U +
            (contourClosed[contour] != 0U ? 1U : 0U);
        for (std::uint32_t segment = 0U;
             segment < segmentCount; ++segment) {
            const Point a = points[start + segment % count];
            const Point b =
                points[start + (segment + 1U) % count];
            totalLength += std::hypot(
                b.x - a.x, b.y - a.y);
        }
    }
    if (!std::isfinite(totalLength) ||
        totalLength <= 1.0e-9) {
        return {};
    }

    const double visibleStart =
        std::clamp(trimStart, 0.0, 1.0) *
        totalLength;
    const double visibleEnd =
        std::clamp(trimEnd, 0.0, 1.0) *
        totalLength;
    const double half = thickness * 0.5;
    double traversed = 0.0;
    for (std::uint32_t contour = 0U;
         contour < contourStarts.Size(); ++contour) {
        const std::uint32_t start = contourStarts[contour];
        const std::uint32_t count = contourCounts[contour];
        if (count < 2U) continue;
        const bool closed = contourClosed[contour] != 0U;
        const std::uint32_t segmentCount =
            count - 1U + (closed ? 1U : 0U);
        Base::Vector<StrokeSegment> visible;
        for (std::uint32_t segment = 0U;
             segment < segmentCount; ++segment) {
            const Point first =
                points[start + segment % count];
            const Point second =
                points[start + (segment + 1U) % count];
            const double dx = second.x - first.x;
            const double dy = second.y - first.y;
            const double length = std::hypot(dx, dy);
            if (length <= 1.0e-9) continue;
            const double segmentStart = traversed;
            const double segmentEnd = traversed + length;
            traversed = segmentEnd;
            const double clippedStart =
                std::max(segmentStart, visibleStart);
            const double clippedEnd =
                std::min(segmentEnd, visibleEnd);
            if (clippedEnd <= clippedStart) continue;

            const double from =
                (clippedStart - segmentStart) / length;
            const double to =
                (clippedEnd - segmentStart) / length;
            StrokeSegment record;
            record.a = {
                first.x + dx * from,
                first.y + dy * from};
            record.b = {
                first.x + dx * to,
                first.y + dy * to};
            record.nx = -dy / length * half;
            record.ny = dx / length * half;
            record.ux = dx / length;
            record.uy = dy / length;
            record.index = segment;
            record.startVertex = from <= 1.0e-9;
            record.endVertex = to >= 1.0 - 1.0e-9;
            Result<void> added = visible.PushBack(record);
            if (!added) return added.GetStatus();
        }
        if (visible.Empty()) continue;
        const Base::Vector<StrokeSegment>* runs = &visible;
        Base::Vector<StrokeSegment> dashed;
        const bool dashing = dashes.Size() >= 1U;
        if (dashing) {
            Result<void> split = SplitDashedSegments(
                visible, dashes, dashOffset, dashed);
            if (!split) return split.GetStatus();
            runs = &dashed;
        }
        if (runs->Empty()) continue;
        std::uint32_t runBegin = 0U;
        for (std::uint32_t index = 1U; index <= runs->Size(); ++index) {
            const bool split =
                index == runs->Size() ||
                (*runs)[index].index !=
                    (*runs)[index - 1U].index + 1U;
            if (!split) continue;
            const bool closedLoop =
                !dashing &&
                closed &&
                runBegin == 0U &&
                index == runs->Size() &&
                runs->Front().startVertex &&
                runs->Back().endVertex &&
                runs->Front().index == 0U &&
                runs->Back().index + 1U == segmentCount;
            Result<void> emitted = EmitStrokeRun(
                *runs,
                runBegin,
                index,
                closedLoop,
                join,
                dashing ? startCap : startCap,
                dashing ? endCap : endCap,
                miterLimit,
                vertices,
                indices);
            if (!emitted) return emitted.GetStatus();
            runBegin = index;
        }
    }
    return {};
}

} // namespace Aero::Media
