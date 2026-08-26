#include "gui/media/GeometryFlatten.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace Aero::Media {
namespace {

constexpr double Pi = 3.14159265358979323846;

bool SamePoint(Point left, Point right) noexcept {
    constexpr double Epsilon = 1.0e-9;
    return std::abs(left.x - right.x) <= Epsilon &&
        std::abs(left.y - right.y) <= Epsilon;
}

std::uint32_t SegmentCount(double controlLength) noexcept {
    return static_cast<std::uint32_t>(
        std::clamp(std::ceil(controlLength / 8.0), 4.0, 48.0));
}

} // namespace

Result<void> FlattenCubicBezier(
    FlattenSink& sink,
    Point start,
    Point control1,
    Point control2,
    Point end) noexcept {
    const double controlLength =
        std::hypot(control1.x - start.x, control1.y - start.y) +
        std::hypot(control2.x - control1.x, control2.y - control1.y) +
        std::hypot(end.x - control2.x, end.y - control2.y);
    const std::uint32_t segments = SegmentCount(controlLength);
    for (std::uint32_t segment = 1U; segment <= segments; ++segment) {
        const double t =
            static_cast<double>(segment) / static_cast<double>(segments);
        const double inverse = 1.0 - t;
        const Point point{
            inverse * inverse * inverse * start.x +
                3.0 * inverse * inverse * t * control1.x +
                3.0 * inverse * t * t * control2.x +
                t * t * t * end.x,
            inverse * inverse * inverse * start.y +
                3.0 * inverse * inverse * t * control1.y +
                3.0 * inverse * t * t * control2.y +
                t * t * t * end.y};
        Result<void> added = sink.AddPoint(point);
        if (!added) return added.GetStatus();
    }
    return {};
}

Result<void> FlattenQuadraticBezier(
    FlattenSink& sink,
    Point start,
    Point control,
    Point end) noexcept {
    const double controlLength =
        std::hypot(control.x - start.x, control.y - start.y) +
        std::hypot(end.x - control.x, end.y - control.y);
    const std::uint32_t segments = SegmentCount(controlLength);
    for (std::uint32_t segment = 1U; segment <= segments; ++segment) {
        const double t =
            static_cast<double>(segment) / static_cast<double>(segments);
        const double inverse = 1.0 - t;
        const Point point{
            inverse * inverse * start.x +
                2.0 * inverse * t * control.x +
                t * t * end.x,
            inverse * inverse * start.y +
                2.0 * inverse * t * control.y +
                t * t * end.y};
        Result<void> added = sink.AddPoint(point);
        if (!added) return added.GetStatus();
    }
    return {};
}

Result<void> FlattenArc(
    FlattenSink& sink,
    Point start,
    Size radii,
    double rotationDegrees,
    bool isLargeArc,
    bool sweepClockwise,
    Point end) noexcept {
    if (SamePoint(start, end)) return {};
    double rx = std::fabs(radii.width);
    double ry = std::fabs(radii.height);
    if (rx < 1.0e-12 || ry < 1.0e-12) {
        return sink.AddPoint(end);
    }
    const double phi = rotationDegrees * Pi / 180.0;
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);
    const double dx = (start.x - end.x) / 2.0;
    const double dy = (start.y - end.y) / 2.0;
    const double x1p = cosPhi * dx + sinPhi * dy;
    const double y1p = -sinPhi * dx + cosPhi * dy;
    double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1.0) {
        const double scale = std::sqrt(lambda);
        rx *= scale;
        ry *= scale;
    }
    const double rxSq = rx * rx;
    const double rySq = ry * ry;
    const double x1pSq = x1p * x1p;
    const double y1pSq = y1p * y1p;
    double numerator = rxSq * rySq - rxSq * y1pSq - rySq * x1pSq;
    if (numerator < 0.0) numerator = 0.0;
    const double denom = rxSq * y1pSq + rySq * x1pSq;
    double coefficient = denom > 0.0 ? std::sqrt(numerator / denom) : 0.0;
    if (isLargeArc == sweepClockwise) coefficient = -coefficient;
    const double cxp = coefficient * (rx * y1p) / ry;
    const double cyp = coefficient * -(ry * x1p) / rx;
    const double cx = cosPhi * cxp - sinPhi * cyp + (start.x + end.x) / 2.0;
    const double cy = sinPhi * cxp + cosPhi * cyp + (start.y + end.y) / 2.0;

    auto unitAngle = [](double ux, double uy, double vx, double vy) noexcept {
        const double dot = ux * vx + uy * vy;
        const double len =
            std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
        double angle = len > 0.0
            ? std::acos(std::clamp(dot / len, -1.0, 1.0))
            : 0.0;
        if (ux * vy - uy * vx < 0.0) angle = -angle;
        return angle;
    };
    const double theta1 = unitAngle(
        1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    double delta = unitAngle(
        (x1p - cxp) / rx,
        (y1p - cyp) / ry,
        (-x1p - cxp) / rx,
        (-y1p - cyp) / ry);
    if (!sweepClockwise && delta > 0.0) delta -= 2.0 * Pi;
    if (sweepClockwise && delta < 0.0) delta += 2.0 * Pi;

    const double absDelta = std::fabs(delta);
    const std::uint32_t pieces = std::max(
        1U, static_cast<std::uint32_t>(std::ceil(absDelta / (Pi / 2.0))));
    const double step = delta / static_cast<double>(pieces);
    Point previous = start;
    for (std::uint32_t piece = 0U; piece < pieces; ++piece) {
        const double t0 = theta1 + static_cast<double>(piece) * step;
        const double t1 = t0 + step;
        const double alpha =
            std::sin(step) * (std::sqrt(4.0 + 3.0 * std::tan(step / 2.0) *
                std::tan(step / 2.0)) - 1.0) / 3.0;
        auto pointAt = [&](double theta) noexcept {
            const double cosT = std::cos(theta);
            const double sinT = std::sin(theta);
            return Point{
                cx + cosPhi * rx * cosT - sinPhi * ry * sinT,
                cy + sinPhi * rx * cosT + cosPhi * ry * sinT};
        };
        auto tangentAt = [&](double theta) noexcept {
            const double cosT = std::cos(theta);
            const double sinT = std::sin(theta);
            return Point{
                -cosPhi * rx * sinT - sinPhi * ry * cosT,
                -sinPhi * rx * sinT + cosPhi * ry * cosT};
        };
        const Point p0 = pointAt(t0);
        const Point p1 = pointAt(t1);
        const Point d0 = tangentAt(t0);
        const Point d1 = tangentAt(t1);
        const Point c1{
            p0.x + alpha * d0.x,
            p0.y + alpha * d0.y};
        const Point c2{
            p1.x - alpha * d1.x,
            p1.y - alpha * d1.y};
        if (piece == 0U) {
            previous = start;
        } else {
            previous = p0;
        }
        Result<void> flattened = FlattenCubicBezier(
            sink, previous, c1, c2, p1);
        if (!flattened) return flattened.GetStatus();
        previous = p1;
    }
    if (!SamePoint(previous, end)) {
        return sink.AddPoint(end);
    }
    return {};
}

Result<void> ParsePointList(
    StringView text,
    Base::Vector<Point>& points) noexcept {
    points.Clear();
    const char* cursor = text.Data();
    const char* end = cursor + text.SizeBytes();
    auto skip = [&]() noexcept {
        while (cursor != end) {
            const char value = *cursor;
            if (value != ' ' && value != '\t' && value != '\r' &&
                value != '\n' && value != ',') {
                break;
            }
            ++cursor;
        }
    };
    while (true) {
        skip();
        if (cursor == end) break;
        char* parsedEnd = nullptr;
        errno = 0;
        const double x = std::strtod(cursor, &parsedEnd);
        if (parsedEnd == cursor || parsedEnd > end || errno != 0 ||
            !std::isfinite(x)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Point list coordinate is invalid");
        }
        cursor = parsedEnd;
        skip();
        parsedEnd = nullptr;
        errno = 0;
        const double y = std::strtod(cursor, &parsedEnd);
        if (parsedEnd == cursor || parsedEnd > end || errno != 0 ||
            !std::isfinite(y)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Point list coordinate is invalid");
        }
        cursor = parsedEnd;
        Result<void> added = points.PushBack(Point{x, y});
        if (!added) return added.GetStatus();
    }
    return {};
}

namespace {

struct FillContour {
    std::uint32_t offset = 0U;
    std::uint32_t count = 0U;
};

class GeometryFillSink final : public FlattenSink {
public:
    GeometryFillSink(
        Base::Vector<Point>& points,
        Base::Vector<FillContour>& contours) noexcept
        : points_(&points), contours_(&contours) {}

    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        Result<void> finished = Flush();
        if (!finished) return finished.GetStatus();
        closed_ = isClosed;
        contour_.Clear();
        return contour_.PushBack(start);
    }
    Result<void> AddPoint(Point point) noexcept override {
        if (contour_.Empty()) {
            return BeginFigure(point, closed_);
        }
        return contour_.PushBack(point);
    }
    Result<void> EndFigure(bool isClosed) noexcept override {
        closed_ = isClosed;
        return Flush();
    }
    Result<void> Finish() noexcept { return Flush(); }

private:
    Result<void> Flush() noexcept {
        if (contour_.Size() > 1U &&
            SamePoint(contour_.Front(), contour_.Back())) {
            contour_.PopBack();
        }
        if (contour_.Size() < 3U) {
            contour_.Clear();
            return {};
        }
        FillContour record{points_->Size(), contour_.Size()};
        Result<void> appended = points_->Append(contour_.AsSpan());
        if (!appended) return appended.GetStatus();
        appended = contours_->PushBack(record);
        contour_.Clear();
        return appended;
    }

    Base::Vector<Point> contour_;
    Base::Vector<Point>* points_ = nullptr;
    Base::Vector<FillContour>* contours_ = nullptr;
    bool closed_ = true;
};

double FillEdgeX(Point start, Point end, double y) noexcept {
    const double height = end.y - start.y;
    if (std::abs(height) <= 1.0e-12) {
        return start.x;
    }
    return start.x + (end.x - start.x) * ((y - start.y) / height);
}

} // namespace

Result<void> TessellateGeometryFill(
    const Geometry& geometry,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    vertices.Clear();
    indices.Clear();
    Base::Vector<Point> points;
    Base::Vector<FillContour> contours;
    GeometryFillSink sink(points, contours);
    Result<void> flattened = geometry.Flatten(sink);
    if (!flattened) return flattened.GetStatus();
    flattened = sink.Finish();
    if (!flattened) return flattened.GetStatus();
    if (points.Empty() || contours.Empty()) return {};

    Base::Vector<double> levels;
    Result<void> reserved = levels.Reserve(points.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Point point : points) {
        Result<void> added = levels.PushBack(point.y);
        if (!added) return added.GetStatus();
    }
    std::sort(levels.Data(), levels.Data() + levels.Size());
    std::uint32_t uniqueCount = 0U;
    for (std::uint32_t index = 0U; index < levels.Size(); ++index) {
        if (uniqueCount != 0U &&
            std::abs(levels[index] - levels[uniqueCount - 1U]) <= 1.0e-9) {
            continue;
        }
        levels[uniqueCount++] = levels[index];
    }
    while (levels.Size() > uniqueCount) {
        levels.PopBack();
    }

    struct ScanHit {
        double top = 0.0;
        double bottom = 0.0;
        double middle = 0.0;
    };
    Base::Vector<ScanHit> hits;
    for (std::uint32_t band = 1U; band < levels.Size(); ++band) {
        const double topY = levels[band - 1U];
        const double bottomY = levels[band];
        if (bottomY - topY <= 1.0e-9) continue;
        const double middleY = (topY + bottomY) * 0.5;
        hits.Clear();
        for (const FillContour contour : contours) {
            if (contour.count < 3U) continue;
            for (std::uint32_t edge = 0U; edge < contour.count; ++edge) {
                const Point start = points[contour.offset + edge];
                const Point end = points[
                    contour.offset + (edge + 1U) % contour.count];
                const double minimum = std::min(start.y, end.y);
                const double maximum = std::max(start.y, end.y);
                if (middleY <= minimum || middleY >= maximum) continue;
                Result<void> added = hits.PushBack({
                    FillEdgeX(start, end, topY),
                    FillEdgeX(start, end, bottomY),
                    FillEdgeX(start, end, middleY)});
                if (!added) return added.GetStatus();
            }
        }
        if (hits.Empty()) continue;
        std::sort(
            hits.Data(),
            hits.Data() + hits.Size(),
            [](const ScanHit& left, const ScanHit& right) noexcept {
                return left.middle < right.middle;
            });
        const std::uint32_t pairCount = (hits.Size() / 2U) * 2U;
        for (std::uint32_t pair = 0U; pair < pairCount; pair += 2U) {
            const ScanHit& left = hits[pair];
            const ScanHit& right = hits[pair + 1U];
            if (right.middle - left.middle <= 1.0e-9) continue;
            if (vertices.Size() > UINT32_MAX - 4U) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Geometry tessellation exceeds the 32-bit mesh limit");
            }
            const std::uint32_t base = vertices.Size();
            const Point quad[] = {
                {left.top, topY},
                {right.top, topY},
                {right.bottom, bottomY},
                {left.bottom, bottomY}};
            Result<void> added = vertices.Append({quad, 4U});
            if (!added) return added.GetStatus();
            const std::uint32_t triangles[] = {
                base, base + 1U, base + 2U,
                base, base + 2U, base + 3U};
            added = indices.Append({triangles, 6U});
            if (!added) return added.GetStatus();
        }
    }
    return {};
}

bool GeometryContainsLocalPoint(
    const Geometry& geometry,
    Point point) noexcept {
    Base::Vector<Point> points;
    Base::Vector<FillContour> contours;
    GeometryFillSink sink(points, contours);
    if (!geometry.Flatten(sink) || !sink.Finish()) return false;
    int crossings = 0;
    for (const FillContour contour : contours) {
        if (contour.count < 3U) continue;
        for (std::uint32_t edge = 0U; edge < contour.count; ++edge) {
            const Point start = points[contour.offset + edge];
            const Point end = points[
                contour.offset + (edge + 1U) % contour.count];
            if (((start.y > point.y) == (end.y > point.y)) ||
                std::abs(end.y - start.y) <= 1.0e-12) {
                continue;
            }
            const double x = FillEdgeX(start, end, point.y);
            if (x >= point.x) continue;
            ++crossings;
        }
    }
    return (crossings % 2) != 0;
}

} // namespace Aero::Media
