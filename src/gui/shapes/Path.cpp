#include "render/DisplayList.hpp"
#include <Aero/Shapes.hpp>
#include <Aero/Media/StreamGeometry.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include "gui/media/GeometryFlatten.hpp"

#include "render/RenderResources.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace Aero::Shapes {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Media;
using namespace Aero::Render;

namespace {

Base::Status InvalidPath(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed, message);
}

bool SamePoint(Point left, Point right) noexcept {
    constexpr double Epsilon = 1.0e-9;
    return std::abs(left.x - right.x) <= Epsilon &&
        std::abs(left.y - right.y) <= Epsilon;
}

struct ContourRecord {
    std::uint32_t offset = 0U;
    std::uint32_t count = 0U;
    bool closed = false;
};

struct ScanIntersection {
    double top = 0.0;
    double bottom = 0.0;
    double middle = 0.0;
    int winding = 1;
};

Base::Result<void> StoreContour(
    Base::Vector<Point>& contour,
    Base::Vector<Point>& points,
    Base::Vector<ContourRecord>& contours,
    Base::Vector<std::uint32_t>& contourStarts,
    Base::Vector<std::uint32_t>& contourCounts,
    Base::Vector<std::uint8_t>& contourClosed,
    bool closed) noexcept {
    if (contour.Size() > 1U &&
        SamePoint(contour[0], contour.Back())) {
        contour.PopBack();
    }
    for (std::uint32_t index = 1U;
         index < contour.Size();) {
        if (!SamePoint(
                contour[index - 1U],
                contour[index])) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < contour.Size(); ++next) {
            contour[next - 1U] =
                contour[next];
        }
        contour.PopBack();
    }
    if (contour.Size() < 2U) {
        return {};
    }
    if (points.Size() >
        UINT32_MAX - contour.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Path vertex count exceeds the 32-bit mesh limit");
    }
    const ContourRecord record{
        points.Size(), contour.Size(), closed};
    Base::Result<void> appended =
        points.Append(contour.AsSpan());
    if (!appended) return appended.GetStatus();
    appended = contours.PushBack(record);
    if (!appended) return appended.GetStatus();
    appended = contourStarts.PushBack(record.offset);
    if (!appended) return appended.GetStatus();
    appended = contourCounts.PushBack(record.count);
    if (!appended) return appended.GetStatus();
    return contourClosed.PushBack(
        closed ? std::uint8_t{1U} : std::uint8_t{0U});
}

double EdgeXAt(
    Point start,
    Point end,
    double y) noexcept {
    const double height = end.y - start.y;
    if (std::abs(height) <= 1.0e-12) {
        return start.x;
    }
    const double amount =
        (y - start.y) / height;
    return start.x +
        (end.x - start.x) * amount;
}

Base::Result<void> TessellateFill(
    const Base::Vector<Point>& points,
    const Base::Vector<ContourRecord>& contours,
    FillRule fillRule,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    Base::Vector<double> levels;
    Base::Result<void> reserved =
        levels.Reserve(points.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Point point : points) {
        Base::Result<void> added =
            levels.PushBack(point.y);
        if (!added) return added.GetStatus();
    }
    std::sort(
        levels.Data(),
        levels.Data() + levels.Size());
    std::uint32_t uniqueCount = 0U;
    for (std::uint32_t index = 0U;
         index < levels.Size(); ++index) {
        if (uniqueCount != 0U &&
            std::abs(
                levels[index] -
                levels[uniqueCount - 1U]) <=
                1.0e-9) {
            continue;
        }
        levels[uniqueCount++] = levels[index];
    }
    while (levels.Size() > uniqueCount) {
        levels.PopBack();
    }

    Base::Vector<ScanIntersection> intersections;
    for (std::uint32_t band = 1U;
         band < levels.Size(); ++band) {
        const double topY = levels[band - 1U];
        const double bottomY = levels[band];
        if (bottomY - topY <= 1.0e-9) {
            continue;
        }
        const double middleY =
            (topY + bottomY) * 0.5;
        intersections.Clear();
        for (const ContourRecord contour :
             contours) {
            if (contour.count < 3U) continue;
            if (contour.offset > points.Size() ||
                contour.count >
                    points.Size() - contour.offset) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Path contour exceeds flattened point buffer");
            }
            for (std::uint32_t edge = 0U;
                 edge < contour.count; ++edge) {
                const Point start =
                    points[contour.offset + edge];
                const Point end =
                    points[
                        contour.offset +
                        (edge + 1U) %
                            contour.count];
                const double minimum =
                    std::min(start.y, end.y);
                const double maximum =
                    std::max(start.y, end.y);
                if (middleY <= minimum ||
                    middleY >= maximum) {
                    continue;
                }
                Base::Result<void> added =
                    intersections.PushBack({
                        EdgeXAt(start, end, topY),
                        EdgeXAt(start, end, bottomY),
                        EdgeXAt(start, end, middleY),
                        end.y > start.y ? 1 : -1});
                if (!added) {
                    return added.GetStatus();
                }
            }
        }
        if (intersections.Empty()) continue;
        std::sort(
            intersections.Data(),
            intersections.Data() +
                intersections.Size(),
            [](const ScanIntersection& left,
               const ScanIntersection& right) noexcept {
                return left.middle < right.middle;
            });
        auto emitBand = [&](
            const ScanIntersection& left,
            const ScanIntersection& right) noexcept -> Base::Result<void> {
            if (right.middle - left.middle <= 1.0e-9) {
                return {};
            }
            if (vertices.Size() > 262144U - 4U) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Path tessellation exceeds the fill mesh budget");
            }
            const std::uint32_t base = vertices.Size();
            const Point quad[] = {
                {left.top, topY},
                {right.top, topY},
                {right.bottom, bottomY},
                {left.bottom, bottomY}};
            Base::Result<void> added =
                vertices.Append({quad, 4U});
            if (!added) return added.GetStatus();
            const std::uint32_t triangles[] = {
                base, base + 1U, base + 2U,
                base, base + 2U, base + 3U};
            return indices.Append({triangles, 6U});
        };
        if (fillRule == FillRule::EvenOdd) {
            const std::uint32_t pairCount =
                (intersections.Size() / 2U) * 2U;
            for (std::uint32_t pair = 0U;
                 pair < pairCount;
                 pair += 2U) {
                Base::Result<void> emitted = emitBand(
                    intersections[pair],
                    intersections[pair + 1U]);
                if (!emitted) return emitted.GetStatus();
            }
        } else {
            int winding = 0;
            for (std::uint32_t index = 0U;
                 index + 1U < intersections.Size();
                 ++index) {
                winding += intersections[index].winding;
                if (winding == 0) continue;
                Base::Result<void> emitted = emitBand(
                    intersections[index],
                    intersections[index + 1U]);
                if (!emitted) return emitted.GetStatus();
            }
        }
    }
    return {};
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kMiterLimit = 10.0;

Base::Result<void> AppendStrokeTriangle(
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
    Base::Result<void> added = vertices.Append({triangle, 3U});
    if (!added) return added.GetStatus();
    const std::uint32_t indices3[] = {base, base + 1U, base + 2U};
    return indices.Append({indices3, 3U});
}

Base::Result<void> AppendStrokeQuad(
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
    Base::Result<void> added = vertices.Append({quad, 4U});
    if (!added) return added.GetStatus();
    const std::uint32_t triangles[] = {
        base, base + 1U, base + 2U,
        base, base + 2U, base + 3U};
    return indices.Append({triangles, 6U});
}

Base::Result<void> AppendStrokeFan(
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
        Base::Result<void> added =
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

Base::Result<void> EmitStrokeJoin(
    const StrokeSegment& incoming,
    const StrokeSegment& outgoing,
    PenLineJoin join,
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
    if (half > 1.0e-12 && miterLength / half > kMiterLimit) {
        return AppendStrokeTriangle(
            vertices, indices, joint, outer0, outer1);
    }
    Base::Result<void> added = AppendStrokeTriangle(
        vertices, indices, joint, outer0, miter);
    if (!added) return added.GetStatus();
    return AppendStrokeTriangle(
        vertices, indices, joint, miter, outer1);
}

Base::Result<void> EmitStrokeCap(
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

Base::Result<void> EmitStrokeRun(
    const Base::Vector<StrokeSegment>& segments,
    std::uint32_t begin,
    std::uint32_t end,
    bool closedLoop,
    PenLineJoin join,
    PenLineCap startCap,
    PenLineCap endCap,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    if (end <= begin) return {};
    for (std::uint32_t index = begin; index < end; ++index) {
        const StrokeSegment& segment = segments[index];
        Base::Result<void> added = AppendStrokeQuad(
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
                segment, segments[index + 1U], join, vertices, indices);
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
            vertices,
            indices);
    }
    if (closedLoop) return {};
    Base::Result<void> capped = EmitStrokeCap(
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

Base::Result<void> ExpandDashPattern(
    Base::Span<const double> dashes,
    Base::Vector<double>& pattern) noexcept {
    pattern.Clear();
    for (std::uint32_t index = 0U; index < dashes.Size(); ++index) {
        const double value = dashes[index];
        if (!std::isfinite(value) || value <= 0.0) continue;
        Base::Result<void> added = pattern.PushBack(value);
        if (!added) return added.GetStatus();
    }
    if (pattern.Size() % 2U == 1U) {
        const std::uint32_t count = pattern.Size();
        for (std::uint32_t index = 0U; index < count; ++index) {
            Base::Result<void> added = pattern.PushBack(pattern[index]);
            if (!added) return added.GetStatus();
        }
    }
    return {};
}

Base::Result<void> SplitDashedSegments(
    const Base::Vector<StrokeSegment>& visible,
    Base::Span<const double> dashes,
    double dashOffset,
    Base::Vector<StrokeSegment>& dashed) noexcept {
    dashed.Clear();
    Base::Vector<double> pattern;
    Base::Result<void> expanded = ExpandDashPattern(dashes, pattern);
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
                Base::Result<void> added = dashed.PushBack(piece);
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

Base::Result<void> TessellateStroke(
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
            Base::Result<void> added = visible.PushBack(record);
            if (!added) return added.GetStatus();
        }
        if (visible.Empty()) continue;
        const Base::Vector<StrokeSegment>* runs = &visible;
        Base::Vector<StrokeSegment> dashed;
        const bool dashing = dashes.Size() >= 1U;
        if (dashing) {
            Base::Result<void> split = SplitDashedSegments(
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
            Base::Result<void> emitted = EmitStrokeRun(
                *runs,
                runBegin,
                index,
                closedLoop,
                join,
                dashing ? startCap : startCap,
                dashing ? endCap : endCap,
                vertices,
                indices);
            if (!emitted) return emitted.GetStatus();
            runBegin = index;
        }
    }
    return {};
}

class ContourPointSink final : public FlattenSink {
public:
    ContourPointSink(
        Base::Vector<Point>& contour,
        Rect& bounds,
        bool& hasBounds) noexcept
        : contour_(&contour),
          bounds_(&bounds),
          hasBounds_(&hasBounds) {}
    Result<void> AddPoint(Point point) noexcept override {
        Include(point);
        return contour_->PushBack(point);
    }
private:
    void Include(Point point) noexcept {
        if (!*hasBounds_) {
            *bounds_ = {point.x, point.y, 0.0, 0.0};
            *hasBounds_ = true;
            return;
        }
        const double right = std::max(bounds_->x + bounds_->width, point.x);
        const double bottom = std::max(bounds_->y + bounds_->height, point.y);
        bounds_->x = std::min(bounds_->x, point.x);
        bounds_->y = std::min(bounds_->y, point.y);
        bounds_->width = right - bounds_->x;
        bounds_->height = bottom - bounds_->y;
    }
    Base::Vector<Point>* contour_ = nullptr;
    Rect* bounds_ = nullptr;
    bool* hasBounds_ = nullptr;
};

class GeometryContourSink final : public FlattenSink {
public:
    GeometryContourSink(
        Base::Vector<Point>& points,
        Base::Vector<ContourRecord>& contours,
        Base::Vector<std::uint32_t>& contourStarts,
        Base::Vector<std::uint32_t>& contourCounts,
        Base::Vector<std::uint8_t>& contourClosed,
        Rect& bounds,
        bool& hasBounds) noexcept
        : points_(&points),
          contours_(&contours),
          contourStarts_(&contourStarts),
          contourCounts_(&contourCounts),
          contourClosed_(&contourClosed),
          bounds_(&bounds),
          hasBounds_(&hasBounds) {}
    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        Result<void> finished = Flush(closed_);
        if (!finished) return finished.GetStatus();
        closed_ = isClosed;
        contour_.Clear();
        Include(start);
        return contour_.PushBack(start);
    }
    Result<void> AddPoint(Point point) noexcept override {
        if (contour_.Empty()) {
            Result<void> started = BeginFigure(point, closed_);
            return started;
        }
        Include(point);
        return contour_.PushBack(point);
    }
    Result<void> EndFigure(bool isClosed) noexcept override {
        closed_ = isClosed;
        return Flush(isClosed);
    }
    Result<void> Finish() noexcept { return Flush(closed_); }
private:
    void Include(Point point) noexcept {
        if (!*hasBounds_) {
            *bounds_ = {point.x, point.y, 0.0, 0.0};
            *hasBounds_ = true;
            return;
        }
        const double right = std::max(bounds_->x + bounds_->width, point.x);
        const double bottom = std::max(bounds_->y + bounds_->height, point.y);
        bounds_->x = std::min(bounds_->x, point.x);
        bounds_->y = std::min(bounds_->y, point.y);
        bounds_->width = right - bounds_->x;
        bounds_->height = bottom - bounds_->y;
    }
    Result<void> Flush(bool closed) noexcept {
        Result<void> stored = StoreContour(
            contour_,
            *points_,
            *contours_,
            *contourStarts_,
            *contourCounts_,
            *contourClosed_,
            closed);
        contour_.Clear();
        return stored;
    }
    Base::Vector<Point> contour_;
    Base::Vector<Point>* points_ = nullptr;
    Base::Vector<ContourRecord>* contours_ = nullptr;
    Base::Vector<std::uint32_t>* contourStarts_ = nullptr;
    Base::Vector<std::uint32_t>* contourCounts_ = nullptr;
    Base::Vector<std::uint8_t>* contourClosed_ = nullptr;
    Rect* bounds_ = nullptr;
    bool* hasBounds_ = nullptr;
    bool closed_ = false;
};

class PolygonPathParser {
public:
    PolygonPathParser(
        Base::StringView source,
        Base::Vector<Point>& vertices,
        Base::Vector<std::uint32_t>& indices,
        Base::Vector<Point>& pathPoints,
        Base::Vector<std::uint32_t>& contourStarts,
        Base::Vector<std::uint32_t>& contourCounts,
        Base::Vector<std::uint8_t>& contourClosed,
        bool tessellateFill,
        FillRule fillRule) noexcept
        : cursor_(source.Data()),
          end_(source.Data() + source.SizeBytes()),
          vertices_(&vertices),
          indices_(&indices),
          pathPoints_(&pathPoints),
          contourStarts_(&contourStarts),
          contourCounts_(&contourCounts),
          contourClosed_(&contourClosed),
          tessellateFill_(tessellateFill),
          fillRule_(fillRule) {}

    Base::Result<Rect> Parse() noexcept {
        Base::Vector<Point> contour;
        Base::Vector<ContourRecord> contours;
        Point current;
        Point first;
        Point lastControl;
        char lastCommand = '\0';
        char command = '\0';
        bool hasCurrent = false;
        bool hasBounds = false;
        Rect bounds;

        auto include = [&](Point point) noexcept {
            if (!hasBounds) {
                bounds = {point.x, point.y, 0.0, 0.0};
                hasBounds = true;
                return;
            }
            const double right =
                std::max(bounds.x + bounds.width, point.x);
            const double bottom =
                std::max(bounds.y + bounds.height, point.y);
            bounds.x = std::min(bounds.x, point.x);
            bounds.y = std::min(bounds.y, point.y);
            bounds.width = right - bounds.x;
            bounds.height = bottom - bounds.y;
        };

        auto finishContour = [&](bool closed = false) noexcept
            -> Base::Result<void> {
            if (contour.Empty()) return {};
            Base::Result<void> tessellated =
                StoreContour(
                    contour,
                    *pathPoints_,
                    contours,
                    *contourStarts_,
                    *contourCounts_,
                    *contourClosed_,
                    closed);
            contour.Clear();
            return tessellated;
        };

        while (true) {
            SkipSeparators();
            if (cursor_ == end_) break;
            if (IsCommand(*cursor_)) {
                command = *cursor_++;
                if (command == 'z' || command == 'Z') {
                    if (!hasCurrent || contour.Empty()) {
                        return InvalidPath(
                            "Path close command has no active contour");
                    }
                    current = first;
                    Base::Result<void> finished =
                        finishContour(true);
                    if (!finished) {
                        return finished.GetStatus();
                    }
                    hasCurrent = false;
                    command = '\0';
                    lastCommand = 'Z';
                    continue;
                }
            } else if (command == '\0') {
                return InvalidPath(
                    "Path data must begin with a move command");
            }

            const bool relative =
                command >= 'a' && command <= 'z';
            const char absolute = relative
                ? static_cast<char>(command - 'a' + 'A')
                : command;

            if (absolute == 'M' || absolute == 'L') {
                double x = 0.0;
                double y = 0.0;
                Base::Result<void> pair =
                    ParsePair(x, y);
                if (!pair) return pair.GetStatus();
                Point point{x, y};
                if (relative && hasCurrent) {
                    point.x += current.x;
                    point.y += current.y;
                }
                if (absolute == 'M') {
                    Base::Result<void> finished =
                        finishContour();
                    if (!finished) {
                        return finished.GetStatus();
                    }
                    first = point;
                    command = relative ? 'l' : 'L';
                } else if (!hasCurrent) {
                    return InvalidPath(
                        "Path line command requires an active contour");
                }
                Base::Result<void> added =
                    contour.PushBack(point);
                if (!added) return added.GetStatus();
                current = point;
                hasCurrent = true;
                lastControl = point;
                lastCommand = absolute;
                include(point);
                continue;
            }

            if (absolute == 'H' || absolute == 'V') {
                if (!hasCurrent) {
                    return InvalidPath(
                        "Path axis line command requires an active contour");
                }
                double value = 0.0;
                Base::Result<void> parsed =
                    ParseNumber(value);
                if (!parsed) return parsed.GetStatus();
                Point point = current;
                if (absolute == 'H') {
                    point.x = relative
                        ? current.x + value : value;
                } else {
                    point.y = relative
                        ? current.y + value : value;
                }
                Base::Result<void> added =
                    contour.PushBack(point);
                if (!added) return added.GetStatus();
                current = point;
                lastControl = point;
                lastCommand = absolute;
                include(point);
                continue;
            }

            if (absolute == 'C' || absolute == 'S') {
                if (!hasCurrent) {
                    return InvalidPath(
                        "Path cubic command requires an active contour");
                }
                Point control1 = current;
                Point control2;
                Point endPoint;

                if (absolute == 'C') {
                    double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, x = 0.0, y = 0.0;
                    Base::Result<void> parsed = ParsePair(x1, y1);
                    if (parsed) parsed = ParsePair(x2, y2);
                    if (parsed) parsed = ParsePair(x, y);
                    if (!parsed) return parsed.GetStatus();
                    control1 = {x1, y1};
                    control2 = {x2, y2};
                    endPoint = {x, y};
                } else {
                    // 'S' / 's' - Smooth cubic bezier
                    if (lastCommand == 'C' || lastCommand == 'S') {
                        control1 = {
                            2.0 * current.x - lastControl.x,
                            2.0 * current.y - lastControl.y
                        };
                    } else {
                        control1 = current;
                    }
                    double x2 = 0.0, y2 = 0.0, x = 0.0, y = 0.0;
                    Base::Result<void> parsed = ParsePair(x2, y2);
                    if (parsed) parsed = ParsePair(x, y);
                    if (!parsed) return parsed.GetStatus();
                    control2 = {x2, y2};
                    endPoint = {x, y};
                }

                if (relative) {
                    if (absolute == 'C') {
                        control1.x += current.x;
                        control1.y += current.y;
                    }
                    control2.x += current.x;
                    control2.y += current.y;
                    endPoint.x += current.x;
                    endPoint.y += current.y;
                }

                lastControl = control2;
                lastCommand = absolute;

                ContourPointSink sink(contour, bounds, hasBounds);
                Base::Result<void> flattened = FlattenCubicBezier(
                    sink, current, control1, control2, endPoint);
                if (!flattened) return flattened.GetStatus();
                current = endPoint;
                continue;
            }

            if (absolute == 'Q' || absolute == 'T') {
                if (!hasCurrent) {
                    return InvalidPath(
                        "Path quadratic command requires an active contour");
                }
                Point control = current;
                Point endPoint;

                if (absolute == 'Q') {
                    double x1 = 0.0, y1 = 0.0, x = 0.0, y = 0.0;
                    Base::Result<void> parsed = ParsePair(x1, y1);
                    if (parsed) parsed = ParsePair(x, y);
                    if (!parsed) return parsed.GetStatus();
                    control = {x1, y1};
                    endPoint = {x, y};
                } else {
                    if (lastCommand == 'Q' || lastCommand == 'T') {
                        control = {
                            2.0 * current.x - lastControl.x,
                            2.0 * current.y - lastControl.y
                        };
                    } else {
                        control = current;
                    }
                    double x = 0.0, y = 0.0;
                    Base::Result<void> parsed = ParsePair(x, y);
                    if (!parsed) return parsed.GetStatus();
                    endPoint = {x, y};
                }

                if (relative) {
                    if (absolute == 'Q') {
                        control.x += current.x;
                        control.y += current.y;
                    }
                    endPoint.x += current.x;
                    endPoint.y += current.y;
                }

                lastControl = control;
                lastCommand = absolute;

                ContourPointSink sink(contour, bounds, hasBounds);
                Base::Result<void> flattened = FlattenQuadraticBezier(
                    sink, current, control, endPoint);
                if (!flattened) return flattened.GetStatus();
                current = endPoint;
                continue;
            }

            if (absolute == 'A') {
                if (!hasCurrent) {
                    return InvalidPath(
                        "Path arc command requires an active contour");
                }
                double rx = 0.0, ry = 0.0, rotation = 0.0;
                double large = 0.0, sweep = 0.0, x = 0.0, y = 0.0;
                Base::Result<void> parsed = ParsePair(rx, ry);
                if (parsed) parsed = ParseNumber(rotation);
                if (parsed) parsed = ParseNumber(large);
                if (parsed) parsed = ParseNumber(sweep);
                if (parsed) parsed = ParsePair(x, y);
                if (!parsed) return parsed.GetStatus();
                Point endPoint{x, y};
                if (relative) {
                    endPoint.x += current.x;
                    endPoint.y += current.y;
                }
                lastControl = endPoint;
                lastCommand = absolute;
                ContourPointSink sink(contour, bounds, hasBounds);
                Base::Result<void> flattened = FlattenArc(
                    sink,
                    current,
                    Size{std::fabs(rx), std::fabs(ry)},
                    rotation,
                    large != 0.0,
                    sweep != 0.0,
                    endPoint);
                if (!flattened) return flattened.GetStatus();
                current = endPoint;
                continue;
            }

            return InvalidPath(
                "Path supports commands M, L, H, V, C, S, Q, T, A, and Z");
        }

        Base::Result<void> finished =
            finishContour();
        if (!finished) return finished.GetStatus();
        if (!hasBounds || pathPoints_->Empty() ||
            contours.Empty()) {
            return InvalidPath(
                "Path data does not contain renderable geometry");
        }
        if (tessellateFill_) {
            Base::Result<void> tessellated =
                TessellateFill(
                    *pathPoints_,
                    contours,
                    fillRule_,
                    *vertices_,
                    *indices_);
            if (!tessellated) {
                return tessellated.GetStatus();
            }
        }
        return bounds;
    }

private:
    static bool IsCommand(char value) noexcept {
        switch (value) {
        case 'M': case 'm':
        case 'L': case 'l':
        case 'H': case 'h':
        case 'V': case 'v':
        case 'C': case 'c':
        case 'S': case 's':
        case 'Q': case 'q':
        case 'T': case 't':
        case 'A': case 'a':
        case 'Z': case 'z':
            return true;
        default:
            return false;
        }
    }

    void SkipSeparators() noexcept {
        while (cursor_ != end_) {
            const char value = *cursor_;
            if (value != ' ' && value != '\t' &&
                value != '\r' && value != '\n' &&
                value != ',') {
                break;
            }
            ++cursor_;
        }
    }

    Base::Result<void> ParseNumber(
        double& output) noexcept {
        SkipSeparators();
        if (cursor_ == end_) {
            return InvalidPath(
                "Path command is missing a coordinate");
        }
        errno = 0;
        char* parsedEnd = nullptr;
        output = std::strtod(cursor_, &parsedEnd);
        if (parsedEnd == cursor_ ||
            parsedEnd > end_ ||
            errno == ERANGE ||
            !std::isfinite(output)) {
            return InvalidPath(
                "Path coordinate must be a finite number");
        }
        cursor_ = parsedEnd;
        return {};
    }

    Base::Result<void> ParsePair(
        double& x,
        double& y) noexcept {
        Base::Result<void> first =
            ParseNumber(x);
        return first
            ? ParseNumber(y)
            : first;
    }

    const char* cursor_ = nullptr;
    const char* end_ = nullptr;
    Base::Vector<Point>* vertices_ = nullptr;
    Base::Vector<std::uint32_t>* indices_ = nullptr;
    Base::Vector<Point>* pathPoints_ = nullptr;
    Base::Vector<std::uint32_t>* contourStarts_ = nullptr;
    Base::Vector<std::uint32_t>* contourCounts_ = nullptr;
    Base::Vector<std::uint8_t>* contourClosed_ = nullptr;
    bool tessellateFill_ = true;
    FillRule fillRule_ = FillRule::EvenOdd;
};

Base::Result<Rect> ScanPathDataBounds(Base::StringView source) noexcept {
    const char* cursor = source.Data();
    const char* const end = source.Data() + source.SizeBytes();
    auto skipSeparators = [&]() noexcept {
        while (cursor != end) {
            const char value = *cursor;
            if (value != ' ' && value != '\t' &&
                value != '\r' && value != '\n' &&
                value != ',') {
                break;
            }
            ++cursor;
        }
    };
    auto parseNumber = [&](double& output) noexcept -> Base::Result<void> {
        skipSeparators();
        if (cursor == end) {
            return InvalidPath("Path command is missing a coordinate");
        }
        errno = 0;
        char* parsedEnd = nullptr;
        output = std::strtod(cursor, &parsedEnd);
        if (parsedEnd == cursor ||
            parsedEnd > end ||
            errno == ERANGE ||
            !std::isfinite(output)) {
            return InvalidPath("Path coordinate must be a finite number");
        }
        cursor = parsedEnd;
        return {};
    };

    bool hasBounds = false;
    Rect bounds{};
    Point current{};
    bool hasCurrent = false;
    char command = '\0';
    auto include = [&](Point point) noexcept {
        if (!hasBounds) {
            bounds = {point.x, point.y, 0.0, 0.0};
            hasBounds = true;
            return;
        }
        const double right = std::max(bounds.x + bounds.width, point.x);
        const double bottom = std::max(bounds.y + bounds.height, point.y);
        bounds.x = std::min(bounds.x, point.x);
        bounds.y = std::min(bounds.y, point.y);
        bounds.width = right - bounds.x;
        bounds.height = bottom - bounds.y;
    };

    while (cursor != end) {
        skipSeparators();
        if (cursor == end) break;
        const char next = *cursor;
        if ((next >= 'A' && next <= 'Z') ||
            (next >= 'a' && next <= 'z')) {
            command = next;
            ++cursor;
        } else if (command == '\0') {
            return InvalidPath("Path data must start with a command");
        }
        const char absolute =
            (command >= 'a' && command <= 'z')
                ? static_cast<char>(command - ('a' - 'A'))
                : command;
        const bool relative = command != absolute;
        if (absolute == 'Z') {
            continue;
        }
        if (absolute == 'H' || absolute == 'V') {
            if (!hasCurrent) {
                return InvalidPath(
                    "Path axis line command requires an active contour");
            }
            double value = 0.0;
            Base::Result<void> parsed = parseNumber(value);
            if (!parsed) return parsed.GetStatus();
            Point point = current;
            if (absolute == 'H') {
                point.x = relative ? current.x + value : value;
            } else {
                point.y = relative ? current.y + value : value;
            }
            include(point);
            current = point;
            hasCurrent = true;
            continue;
        }
        if (absolute == 'A') {
            if (!hasCurrent) {
                return InvalidPath(
                    "Path arc command requires an active contour");
            }
            double rx = 0.0, ry = 0.0, rotation = 0.0, large = 0.0,
                   sweep = 0.0, x = 0.0, y = 0.0;
            Base::Result<void> parsed = parseNumber(rx);
            if (parsed) parsed = parseNumber(ry);
            if (parsed) parsed = parseNumber(rotation);
            if (parsed) parsed = parseNumber(large);
            if (parsed) parsed = parseNumber(sweep);
            if (parsed) parsed = parseNumber(x);
            if (parsed) parsed = parseNumber(y);
            if (!parsed) return parsed.GetStatus();
            Point endPoint = relative
                ? Point{current.x + x, current.y + y}
                : Point{x, y};
            include(endPoint);
            include({
                current.x + (relative ? 0.0 : 0.0) + rx,
                current.y + ry});
            current = endPoint;
            hasCurrent = true;
            continue;
        }

        std::uint32_t pairs = 1U;
        if (absolute == 'C') pairs = 3U;
        else if (absolute == 'S' || absolute == 'Q') pairs = 2U;
        for (std::uint32_t pair = 0U; pair < pairs; ++pair) {
            double x = 0.0, y = 0.0;
            Base::Result<void> parsed = parseNumber(x);
            if (parsed) parsed = parseNumber(y);
            if (!parsed) return parsed.GetStatus();
            Point point = relative
                ? Point{current.x + x, current.y + y}
                : Point{x, y};
            include(point);
            current = point;
            hasCurrent = true;
        }
    }
    if (!hasBounds) {
        return InvalidPath("Path data does not contain renderable geometry");
    }
    return bounds;
}

} // namespace

Path::Path() noexcept
    : Shape(StaticTypeId()) {}

Path::~Path() {
    ReleaseMesh();
}

Base::Ref<Geometry> Path::GetData() const noexcept {
    return GetValueOr(
        DataProperty,
        Base::Ref<Geometry>{});
}

FillRule Path::GetFillRule() const noexcept {
    return GetValueOr(FillRuleProperty, FillRule::EvenOdd);
}

PenLineJoin Path::GetStrokeLineJoin() const noexcept {
    return GetValueOr(
        StrokeLineJoinProperty,
        PenLineJoin::Miter);
}

PenLineCap Path::GetStrokeStartLineCap() const noexcept {
    return GetValueOr(
        StrokeStartLineCapProperty,
        PenLineCap::Flat);
}

PenLineCap Path::GetStrokeEndLineCap() const noexcept {
    return GetValueOr(
        StrokeEndLineCapProperty,
        PenLineCap::Flat);
}

double Path::GetTrimStart() const noexcept {
    return GetValueOr(TrimStartProperty, 0.0);
}

double Path::GetTrimEnd() const noexcept {
    return GetValueOr(TrimEndProperty, 1.0);
}

void Path::SetData(
    Base::Ref<Geometry> value) noexcept {
    SetValue(
        DataProperty,
        std::move(value));
}

void Path::SetFillRule(FillRule value) noexcept {
    SetValue(FillRuleProperty, value);
}

void Path::SetStrokeLineJoin(
    PenLineJoin value) noexcept {
    SetValue(StrokeLineJoinProperty, value);
}

void Path::SetStrokeStartLineCap(
    PenLineCap value) noexcept {
    SetValue(StrokeStartLineCapProperty, value);
}

void Path::SetStrokeEndLineCap(
    PenLineCap value) noexcept {
    SetValue(StrokeEndLineCapProperty, value);
}

void Path::SetTrimStart(
    double value) noexcept {
    SetValue(TrimStartProperty, value);
}

void Path::SetTrimEnd(
    double value) noexcept {
    SetValue(TrimEndProperty, value);
}

Base::StringView Path::GetStrokeDashArray() const noexcept {
    return GetValueOr(StrokeDashArrayProperty, Base::StringView{});
}

double Path::GetStrokeDashOffset() const noexcept {
    return GetValueOr(StrokeDashOffsetProperty, 0.0);
}

Base::Ref<Media::DashStyle> Path::GetDashStyle() const noexcept {
    return GetValueOr(
        DashStyleProperty,
        Base::Ref<Media::DashStyle>{});
}

void Path::SetStrokeDashArray(Base::StringView value) noexcept {
    SetValue(StrokeDashArrayProperty, value);
}

void Path::SetStrokeDashOffset(double value) noexcept {
    SetValue(StrokeDashOffsetProperty, value);
}

void Path::SetDashStyle(Base::Ref<Media::DashStyle> value) noexcept {
    SetValue(DashStyleProperty, std::move(value));
}

namespace {

Base::Result<void> ParseStrokeDashArray(
    Base::StringView text,
    Base::Vector<double>& dashes) noexcept {
    dashes.Clear();
    const char* cursor = text.Data();
    const char* end = cursor + text.SizeBytes();
    while (cursor != end) {
        while (cursor != end) {
            const char value = *cursor;
            if (value != ' ' && value != '\t' && value != '\r' &&
                value != '\n' && value != ',' && value != ';') {
                break;
            }
            ++cursor;
        }
        if (cursor == end) break;
        char* parsedEnd = nullptr;
        errno = 0;
        const double dash = std::strtod(cursor, &parsedEnd);
        if (parsedEnd == cursor || parsedEnd > end || errno != 0 ||
            !std::isfinite(dash) || dash < 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "StrokeDashArray value is invalid");
        }
        Base::Result<void> added = dashes.PushBack(dash);
        if (!added) return added.GetStatus();
        cursor = parsedEnd;
    }
    return {};
}

Base::Result<void> ResolvePathDashes(
    const Path& path,
    Base::Vector<double>& dashes,
    double& offset) noexcept {
    offset = path.GetStrokeDashOffset();
    if (Base::Ref<Media::DashStyle> style = path.GetDashStyle()) {
        const Base::Span<const double> values = style->GetDashes();
        dashes.Clear();
        for (std::uint32_t index = 0U; index < values.Size(); ++index) {
            Base::Result<void> added = dashes.PushBack(values[index]);
            if (!added) return added.GetStatus();
        }
        offset = style->GetOffset();
        return {};
    }
    return ParseStrokeDashArray(path.GetStrokeDashArray(), dashes);
}

} // namespace

void Path::ResetGeometry() noexcept {
    ReleaseMesh();
    geometryVertices_.Clear();
    geometryIndices_.Clear();
    pathPoints_.Clear();
    pathContourStarts_.Clear();
    pathContourCounts_.Clear();
    pathContourClosed_.Clear();
    strokeVertices_.Clear();
    strokeIndices_.Clear();
    geometryBounds_ = {};
    geometryDirty_ = true;
}

Base::Result<void> Path::EnsureGeometry() noexcept {
    if (!geometryDirty_) return {};
    geometryVertices_.Clear();
    geometryIndices_.Clear();
    pathPoints_.Clear();
    pathContourStarts_.Clear();
    pathContourCounts_.Clear();
    pathContourClosed_.Clear();
    strokeVertices_.Clear();
    strokeIndices_.Clear();
    geometryBounds_ = {};
    Base::Ref<Geometry> geometry = GetData();
    if (geometry &&
        geometry->RuntimeType() != StreamGeometry::StaticTypeId()) {
        if (GetIsMeasuring()) {
            geometryBounds_ = geometry->GetBounds();
            return {};
        }
        bool hasBounds = false;
        Rect bounds{};
        Base::Vector<ContourRecord> contours;
        GeometryContourSink sink(
            pathPoints_,
            contours,
            pathContourStarts_,
            pathContourCounts_,
            pathContourClosed_,
            bounds,
            hasBounds);
        Base::Result<void> flattened = geometry->Flatten(sink);
        if (flattened) flattened = sink.Finish();
        if (!flattened) {
            geometryVertices_.Clear();
            geometryIndices_.Clear();
            pathPoints_.Clear();
            pathContourStarts_.Clear();
            pathContourCounts_.Clear();
            pathContourClosed_.Clear();
            return flattened.GetStatus();
        }
        if (hasBounds && !pathPoints_.Empty() && !contours.Empty()) {
            geometryBounds_ = bounds;
            if (GetIsMeasuring()) {
                return {};
            }
            if (GetFill()) {
                Base::Result<void> tessellated = TessellateFill(
                    pathPoints_,
                    contours,
                    GetFillRule(),
                    geometryVertices_,
                    geometryIndices_);
                if (!tessellated) return tessellated.GetStatus();
            }
            if (GetStroke() && !GetIsMeasuring()) {
                Base::Vector<double> dashes;
                double dashOffset = 0.0;
                Base::Result<void> resolved =
                    ResolvePathDashes(*this, dashes, dashOffset);
                if (!resolved) return resolved.GetStatus();
                Base::Result<void> stroked = TessellateStroke(
                    pathPoints_,
                    pathContourStarts_,
                    pathContourCounts_,
                    pathContourClosed_,
                    GetStrokeThickness(),
                    GetTrimStart(),
                    GetTrimEnd(),
                    GetStrokeLineJoin(),
                    GetStrokeStartLineCap(),
                    GetStrokeEndLineCap(),
                    {dashes.Data(), dashes.Size()},
                    dashOffset,
                    strokeVertices_,
                    strokeIndices_);
                if (!stroked) return stroked.GetStatus();
            }
        }
        if (!GetIsMeasuring()) {
            geometryDirty_ = false;
        }
        return {};
    }
    Base::StringView data;
    if (geometry && geometry->RuntimeType() ==
            StreamGeometry::StaticTypeId()) {
        data = static_cast<StreamGeometry*>(geometry.Get())->GetData();
    }
    if (data.Empty()) {
        geometryDirty_ = false;
        return {};
    }
    // Measure only needs an AABB. Large emblem paths (Scoreboard) have
    // crashed the scanline tessellator; skip fill meshes for those and
    // keep geometryDirty_ so modest paths still tessellate on render.
    constexpr std::uint32_t kMaxTessellatedStreamBytes = 2048U;
    if (GetIsMeasuring() || data.SizeBytes() > kMaxTessellatedStreamBytes) {
        Base::Result<Rect> bounds = ScanPathDataBounds(data);
        if (!bounds) return bounds.GetStatus();
        geometryBounds_ = bounds.Value();
        if (!GetIsMeasuring()) {
            geometryDirty_ = false;
        }
        return {};
    }
    PolygonPathParser parser(
        data,
        geometryVertices_,
        geometryIndices_,
        pathPoints_,
        pathContourStarts_,
        pathContourCounts_,
        pathContourClosed_,
        // Measure only needs bounds. Tessellating Scoreboard-scale emblem
        // paths during layout has crashed the sample host; fill meshes are
        // built on the render path instead.
        static_cast<bool>(GetFill()) && !GetIsMeasuring(),
        GetFillRule());
    Base::Result<Rect> parsed =
        parser.Parse();
    if (!parsed) {
        geometryVertices_.Clear();
        geometryIndices_.Clear();
        pathPoints_.Clear();
        pathContourStarts_.Clear();
        pathContourCounts_.Clear();
        pathContourClosed_.Clear();
        return parsed.GetStatus();
    }
    geometryBounds_ = parsed.Value();
    if (geometry) {
        if (Ref<Transform> transform = geometry->GetTransform()) {
            const Base::Transform2D matrix = transform->GetMatrix();
            auto mapPoints = [&](Base::Vector<Point>& points) noexcept {
                for (std::uint32_t index = 0U; index < points.Size(); ++index) {
                    points[index] = TransformPoint(matrix, points[index]);
                }
            };
            mapPoints(pathPoints_);
            mapPoints(geometryVertices_);
            if (!pathPoints_.Empty()) {
                geometryBounds_ = {
                    pathPoints_[0].x, pathPoints_[0].y, 0.0, 0.0};
                for (std::uint32_t index = 1U;
                     index < pathPoints_.Size();
                     ++index) {
                    const Point point = pathPoints_[index];
                    const double right = std::max(
                        geometryBounds_.x + geometryBounds_.width, point.x);
                    const double bottom = std::max(
                        geometryBounds_.y + geometryBounds_.height, point.y);
                    geometryBounds_.x = std::min(geometryBounds_.x, point.x);
                    geometryBounds_.y = std::min(geometryBounds_.y, point.y);
                    geometryBounds_.width = right - geometryBounds_.x;
                    geometryBounds_.height = bottom - geometryBounds_.y;
                }
            }
        }
    }
    if (GetStroke() && !GetIsMeasuring()) {
        Base::Vector<double> dashes;
        double dashOffset = 0.0;
        Base::Result<void> resolved =
            ResolvePathDashes(*this, dashes, dashOffset);
        if (!resolved) return resolved.GetStatus();
        Base::Result<void> stroked =
            TessellateStroke(
                pathPoints_,
                pathContourStarts_,
                pathContourCounts_,
                pathContourClosed_,
                GetStrokeThickness(),
                GetTrimStart(),
                GetTrimEnd(),
                GetStrokeLineJoin(),
                GetStrokeStartLineCap(),
                GetStrokeEndLineCap(),
                {dashes.Data(), dashes.Size()},
                dashOffset,
                strokeVertices_,
                strokeIndices_);
        if (!stroked) return stroked.GetStatus();
    }
    if (!GetIsMeasuring()) {
        geometryDirty_ = false;
    }
    return {};
}

void Path::ReleaseMesh() noexcept {
    auto* services =
        static_cast<Aero::Render::MeshResources*>(
            AeroGuiInternal::MeshResourcesRuntime(*this));
    if (mesh_ != InvalidRenderMeshId &&
        services != nullptr &&
        services->release != nullptr &&
        services->generation ==
            meshServiceGeneration_) {
        services->release(
            services->context, mesh_);
    }
    if (strokeMesh_ != InvalidRenderMeshId &&
        services != nullptr &&
        services->release != nullptr &&
        services->generation ==
            meshServiceGeneration_) {
        services->release(
            services->context, strokeMesh_);
    }
    mesh_ = InvalidRenderMeshId;
    strokeMesh_ = InvalidRenderMeshId;
}

void Path::AttachMeshResources(
    void* rawServices,
    bool force) noexcept {
    auto* services =
        static_cast<Aero::Render::MeshResources*>(
            rawServices);
    auto* currentServices =
        static_cast<Aero::Render::MeshResources*>(
            AeroGuiInternal::MeshResourcesRuntime(*this));
    if (!force &&
        currentServices == services &&
        (services == nullptr ||
         meshServiceGeneration_ ==
             services->generation)) {
        return;
    }
    if (services == nullptr && !force) {
        ReleaseMesh();
    } else {
        // A changed or explicitly invalidated render device already destroyed its
        // resources. Never call through that stale lease, even when the
        // allocator reused both the service address and generation value.
        mesh_ = InvalidRenderMeshId;
        strokeMesh_ = InvalidRenderMeshId;
    }
    meshServiceGeneration_ =
        services != nullptr ? services->generation : 0U;
}

Base::Result<void> Path::EnsureMesh() noexcept {
    Base::Result<void> geometry =
        EnsureGeometry();
    if (!geometry) {
        return geometry.GetStatus();
    }
    auto* tree = this->GetTree();
    auto* services = tree != nullptr ? tree->MeshResources() : nullptr;
    if (services == nullptr ||
        services->create == nullptr) {
        return {};
    }
    if (mesh_ == InvalidRenderMeshId &&
        !geometryVertices_.Empty()) {
        auto created =
            services->create(
                services->context,
                geometryVertices_.AsSpan(),
                geometryIndices_.AsSpan());
        if (!created) {
            return created.GetStatus();
        }
        mesh_ = created.Value();
    }
    if (strokeMesh_ == InvalidRenderMeshId &&
        !strokeVertices_.Empty()) {
        auto created =
            services->create(
                services->context,
                strokeVertices_.AsSpan(),
                strokeIndices_.AsSpan());
        if (!created) return created.GetStatus();
        strokeMesh_ = created.Value();
    }
    return {};
}

Size Path::MeasureOverride(
    Size availableSize) noexcept {
    Base::Result<void> geometry =
        EnsureGeometry();
    if (!geometry) return Size{};
    const Size natural{
        geometryBounds_.width,
        geometryBounds_.height};
    if (natural.width <= 0.0 ||
        natural.height <= 0.0) {
        return Size{};
    }

    // Layout uses a large finite value for an unconstrained dimension.
    // Treat it as unbounded here so Uniform does not preserve the natural
    // height while scaling only the constrained width (or vice versa).
    constexpr double UnboundedConstraint = 5.0e11;
    const bool widthBounded =
        std::isfinite(availableSize.width) &&
        availableSize.width < UnboundedConstraint;
    const bool heightBounded =
        std::isfinite(availableSize.height) &&
        availableSize.height < UnboundedConstraint;
    if (GetStretch() == Stretch::None) {
        return Size{
            widthBounded
                ? std::min(
                      natural.width,
                      availableSize.width)
                : natural.width,
            heightBounded
                ? std::min(
                      natural.height,
                      availableSize.height)
                : natural.height};
    }

    if (GetStretch() == Stretch::Fill) {
        return Size{
            widthBounded
                ? availableSize.width
                : natural.width,
            heightBounded
                ? availableSize.height
                : natural.height};
    }

    if (!widthBounded && !heightBounded) {
        return natural;
    }

    const double scaleX = widthBounded
        ? availableSize.width / natural.width
        : 0.0;
    const double scaleY = heightBounded
        ? availableSize.height / natural.height
        : 0.0;
    double scale = widthBounded
        ? scaleX
        : scaleY;
    if (widthBounded && heightBounded) {
        scale = GetStretch() ==
                Stretch::UniformToFill
            ? std::max(scaleX, scaleY)
            : std::min(scaleX, scaleY);
    }
    return Size{
        natural.width * scale,
        natural.height * scale};
}

void Path::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    Base::Ref<Geometry> authoredGeometry = GetData();
    if (authoredGeometry && authoredGeometry->RuntimeType() ==
            PathGeometry::StaticTypeId()) {
        // Segment points can be animation targets. Rebuild the lightweight
        // mesh when rendering object-model geometry so those changes are
        // visible without coupling Geometry to the visual tree.
        ResetGeometry();
    }
    Base::Result<void> mesh =
        EnsureMesh();
    if (!mesh) return;
    const Size target = GetRenderSize();
    if ((mesh_ == InvalidRenderMeshId &&
         strokeMesh_ == InvalidRenderMeshId) ||
        (geometryBounds_.width <= 0.0 &&
         geometryBounds_.height <= 0.0)) {
        return;
    }
    const double geomW = std::max(geometryBounds_.width, 1.0e-6);
    const double geomH = std::max(geometryBounds_.height, 1.0e-6);
    double scaleX = 1.0;
    double scaleY = 1.0;
    switch (GetStretch()) {
    case Stretch::None:
        break;
    case Stretch::Fill:
        scaleX = geometryBounds_.width > 0.0 ? target.width / geomW : 1.0;
        scaleY = geometryBounds_.height > 0.0 ? target.height / geomH : 1.0;
        break;
    case Stretch::Uniform: {
        const double scale = (geometryBounds_.width > 0.0 && geometryBounds_.height > 0.0)
            ? std::min(target.width / geomW, target.height / geomH)
            : (geometryBounds_.width > 0.0 ? target.width / geomW : (geometryBounds_.height > 0.0 ? target.height / geomH : 1.0));
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = (geometryBounds_.width > 0.0 && geometryBounds_.height > 0.0)
            ? std::max(target.width / geomW, target.height / geomH)
            : (geometryBounds_.width > 0.0 ? target.width / geomW : (geometryBounds_.height > 0.0 ? target.height / geomH : 1.0));
        scaleX = scale;
        scaleY = scale;
        break;
    }
    }
    const double width =
        geometryBounds_.width * scaleX;
    const double height =
        geometryBounds_.height * scaleY;
    const double offsetX =
        (target.width - width) * 0.5 -
        geometryBounds_.x * scaleX;
    const double offsetY =
        (target.height - height) * 0.5 -
        geometryBounds_.y * scaleY;
    Base::Result<void> transform =
        builder.PushTransform(Transform2D{
            scaleX, 0.0, 0.0, scaleY,
            offsetX, offsetY});
    if (!transform) return;
    if (mesh_ != InvalidRenderMeshId) {
        Base::Ref<Brush> fill = GetFill();
        if (fill) {
            Color fillColor = ::Aero::Media::SampleBrush(fill);
            if (fillColor.alpha > 0.0F) {
                Base::Result<void> drawn =
                    builder.DrawMesh(mesh_, fillColor);
                if (!drawn) return;
            }
        }
    }
    if (strokeMesh_ != InvalidRenderMeshId) {
        Base::Ref<Brush> stroke = GetStroke();
        if (stroke) {
            Color strokeColor = ::Aero::Media::SampleBrush(stroke);
            if (strokeColor.alpha > 0.0F) {
                Base::Result<void> drawn =
                    builder.DrawMesh(strokeMesh_, strokeColor);
                if (!drawn) return;
            }
        }
    }
    static_cast<void>(builder.PopTransform());
}

} // namespace Aero::Shapes
