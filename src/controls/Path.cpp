#include "../render/DisplayList.hpp"
#include <Aero/Shapes.hpp>
#include "../render/DrawingInternals.hpp"

#include "render/RenderResources.hpp"
#include "../media/BrushInternals.hpp"

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

struct ContourRecord final {
    std::uint32_t offset = 0U;
    std::uint32_t count = 0U;
    bool closed = false;
};

struct ScanIntersection final {
    double top = 0.0;
    double bottom = 0.0;
    double middle = 0.0;
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
    if (contour.Size() < 3U) {
        return InvalidPath(
            "Path contour must contain at least three distinct points");
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
        points.TryAppend(contour.AsSpan());
    if (!appended) return appended.GetStatus();
    appended = contours.TryPushBack(record);
    if (!appended) return appended.GetStatus();
    appended = contourStarts.TryPushBack(record.offset);
    if (!appended) return appended.GetStatus();
    appended = contourCounts.TryPushBack(record.count);
    if (!appended) return appended.GetStatus();
    return contourClosed.TryPushBack(
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

Base::Result<void> TessellateEvenOdd(
    const Base::Vector<Point>& points,
    const Base::Vector<ContourRecord>& contours,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept {
    Base::Vector<double> levels;
    Base::Result<void> reserved =
        levels.TryReserve(points.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Point point : points) {
        Base::Result<void> added =
            levels.TryPushBack(point.y);
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
                    intersections.TryPushBack({
                        EdgeXAt(start, end, topY),
                        EdgeXAt(start, end, bottomY),
                        EdgeXAt(start, end, middleY)});
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
        if (intersections.Size() % 2U != 0U) {
            return InvalidPath(
                "Path even-odd scan conversion found an unmatched edge");
        }
        for (std::uint32_t pair = 0U;
             pair < intersections.Size();
             pair += 2U) {
            const ScanIntersection left =
                intersections[pair];
            const ScanIntersection right =
                intersections[pair + 1U];
            if (right.middle - left.middle <=
                1.0e-9) {
                continue;
            }
            if (vertices.Size() >
                UINT32_MAX - 4U) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Path tessellation exceeds the 32-bit mesh limit");
            }
            const std::uint32_t base =
                vertices.Size();
            const Point quad[] = {
                {left.top, topY},
                {right.top, topY},
                {right.bottom, bottomY},
                {left.bottom, bottomY}};
            Base::Result<void> added =
                vertices.TryAppend({quad, 4U});
            if (!added) return added.GetStatus();
            const std::uint32_t triangles[] = {
                base, base + 1U, base + 2U,
                base, base + 2U, base + 3U};
            added = indices.TryAppend({
                triangles, 6U});
            if (!added) return added.GetStatus();
        }
    }
    if (vertices.Empty() || indices.Empty()) {
        return InvalidPath(
            "Path tessellation produced no filled area");
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
        const std::uint32_t segmentCount =
            count - 1U +
            (contourClosed[contour] != 0U ? 1U : 0U);
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
            const Point a{
                first.x + dx * from,
                first.y + dy * from};
            const Point b{
                first.x + dx * to,
                first.y + dy * to};
            const double nx = -dy / length * half;
            const double ny = dx / length * half;
            if (vertices.Size() > UINT32_MAX - 4U) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Path stroke tessellation exceeds the 32-bit vertex limit");
            }
            const std::uint32_t base = vertices.Size();
            const Point quad[] = {
                {a.x + nx, a.y + ny},
                {b.x + nx, b.y + ny},
                {b.x - nx, b.y - ny},
                {a.x - nx, a.y - ny}};
            Base::Result<void> added =
                vertices.TryAppend({quad, 4U});
            if (!added) return added.GetStatus();
            const std::uint32_t triangles[] = {
                base, base + 1U, base + 2U,
                base, base + 2U, base + 3U};
            added = indices.TryAppend({
                triangles, 6U});
            if (!added) return added.GetStatus();
        }
    }
    return {};
}

class PolygonPathParser final {
public:
    PolygonPathParser(
        Base::StringView source,
        Base::Vector<Point>& vertices,
        Base::Vector<std::uint32_t>& indices,
        Base::Vector<Point>& pathPoints,
        Base::Vector<std::uint32_t>& contourStarts,
        Base::Vector<std::uint32_t>& contourCounts,
        Base::Vector<std::uint8_t>& contourClosed,
        bool tessellateFill) noexcept
        : cursor_(source.Data()),
          end_(source.Data() + source.SizeBytes()),
          vertices_(&vertices),
          indices_(&indices),
          pathPoints_(&pathPoints),
          contourStarts_(&contourStarts),
          contourCounts_(&contourCounts),
          contourClosed_(&contourClosed),
          tessellateFill_(tessellateFill) {}

    Base::Result<Rect> Parse() noexcept {
        Base::Vector<Point> contour;
        Base::Vector<ContourRecord> contours;
        Point current;
        Point first;
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
                    contour.TryPushBack(point);
                if (!added) return added.GetStatus();
                current = point;
                hasCurrent = true;
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
                    contour.TryPushBack(point);
                if (!added) return added.GetStatus();
                current = point;
                include(point);
                continue;
            }
            if (absolute == 'C') {
                if (!hasCurrent) {
                    return InvalidPath(
                        "Path cubic command requires an active contour");
                }
                double x1 = 0.0;
                double y1 = 0.0;
                double x2 = 0.0;
                double y2 = 0.0;
                double x = 0.0;
                double y = 0.0;
                Base::Result<void> parsed =
                    ParsePair(x1, y1);
                if (parsed) {
                    parsed = ParsePair(x2, y2);
                }
                if (parsed) {
                    parsed = ParsePair(x, y);
                }
                if (!parsed) {
                    return parsed.GetStatus();
                }
                Point control1{x1, y1};
                Point control2{x2, y2};
                Point end{x, y};
                if (relative) {
                    control1.x += current.x;
                    control1.y += current.y;
                    control2.x += current.x;
                    control2.y += current.y;
                    end.x += current.x;
                    end.y += current.y;
                }
                const double controlLength =
                    std::hypot(
                        control1.x - current.x,
                        control1.y - current.y) +
                    std::hypot(
                        control2.x - control1.x,
                        control2.y - control1.y) +
                    std::hypot(
                        end.x - control2.x,
                        end.y - control2.y);
                const std::uint32_t segments =
                    static_cast<std::uint32_t>(
                        std::clamp(
                            std::ceil(
                                controlLength / 8.0),
                            4.0, 48.0));
                const Point start = current;
                for (std::uint32_t segment = 1U;
                     segment <= segments;
                     ++segment) {
                    const double t =
                        static_cast<double>(segment) /
                        static_cast<double>(segments);
                    const double inverse = 1.0 - t;
                    const Point point{
                        inverse * inverse * inverse *
                                start.x +
                            3.0 * inverse * inverse * t *
                                control1.x +
                            3.0 * inverse * t * t *
                                control2.x +
                            t * t * t * end.x,
                        inverse * inverse * inverse *
                                start.y +
                            3.0 * inverse * inverse * t *
                                control1.y +
                            3.0 * inverse * t * t *
                                control2.y +
                            t * t * t * end.y};
                    Base::Result<void> added =
                        contour.TryPushBack(point);
                    if (!added) {
                        return added.GetStatus();
                    }
                    include(point);
                }
                current = end;
                continue;
            }
            return InvalidPath(
                "Path supports commands M, L, H, V, C, and Z");
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
                TessellateEvenOdd(
                    *pathPoints_,
                    contours,
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
        return (value >= 'A' && value <= 'Z') ||
            (value >= 'a' && value <= 'z');
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
};

} // namespace

Path::Path() noexcept
    : FrameworkElement(StaticTypeId()) {}

Path::~Path() {
    ReleaseMesh();
}

Base::Ref<Geometry> Path::GetData() const noexcept {
    return GetValueOr(
        DataProperty,
        Base::Ref<Geometry>{});
}

Base::Ref<Brush> Path::GetFill() const noexcept {
    return GetValueOr(
        FillProperty, Base::Ref<Brush>{});
}

Base::Ref<Brush> Path::GetStroke() const noexcept {
    return GetValueOr(
        StrokeProperty, Base::Ref<Brush>{});
}

double Path::GetStrokeThickness() const noexcept {
    return GetValueOr(
        StrokeThicknessProperty, 1.0);
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

Stretch Path::GetStretch() const noexcept {
    return GetValueOr(
        StretchProperty, Stretch::Uniform);
}

void Path::SetData(
    Base::Ref<Geometry> value) noexcept {
    SetValue(
        DataProperty,
        std::move(value));
}

void Path::SetFill(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        FillProperty, std::move(value));
}

void Path::SetStroke(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        StrokeProperty, std::move(value));
}

void Path::SetStrokeThickness(
    double value) noexcept {
    SetValue(StrokeThicknessProperty, value);
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

void Path::SetStretch(
    Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

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
    const Base::StringView data =
        geometry &&
            geometry->RuntimeType() ==
                StreamGeometry::StaticTypeId()
        ? static_cast<StreamGeometry*>(
              geometry.Get())->GetData()
        : Base::StringView{};
    if (data.Empty()) {
        geometryDirty_ = false;
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
        // A storyboard can intentionally start a Path fill transparent and
        // reveal it later. Geometry is independent from the sampled alpha;
        // omitting it here leaves no mesh to draw when that animation reaches
        // an opaque key frame.
        static_cast<bool>(GetFill()));
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
    if (GetStroke()) {
        Base::Result<void> stroked =
            TessellateStroke(
                pathPoints_,
                pathContourStarts_,
                pathContourCounts_,
                pathContourClosed_,
                GetStrokeThickness(),
                GetTrimStart(),
                GetTrimEnd(),
                strokeVertices_,
                strokeIndices_);
        if (!stroked) return stroked.GetStatus();
    }
    geometryDirty_ = false;
    return {};
}

void Path::ReleaseMesh() noexcept {
    auto* services =
        static_cast<Aero::Internal::MeshResources*>(
            meshServices_);
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
        static_cast<Aero::Internal::MeshResources*>(
            rawServices);
    if (!force &&
        meshServices_ == rawServices &&
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
    meshServices_ = services;
    meshServiceGeneration_ =
        services != nullptr ? services->generation : 0U;
}

Base::Result<void> Path::EnsureMesh() noexcept {
    Base::Result<void> geometry =
        EnsureGeometry();
    if (!geometry) return geometry.GetStatus();
    auto* services =
        static_cast<Aero::Internal::MeshResources*>(
            meshServices_);
    if (services == nullptr ||
        services->create == nullptr) {
        return {};
    }
    if (mesh_ == InvalidRenderMeshId &&
        !geometryVertices_.Empty()) {
        Base::Result<RenderMeshId> created =
            services->create(
                services->context,
                geometryVertices_.AsSpan(),
                geometryIndices_.AsSpan());
        if (!created) return created.GetStatus();
        mesh_ = created.Value();
    }
    if (strokeMesh_ == InvalidRenderMeshId &&
        !strokeVertices_.Empty()) {
        Base::Result<RenderMeshId> created =
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
    DrawingContext& context) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    Base::Result<void> mesh =
        EnsureMesh();
    if (!mesh) return;
    if ((mesh_ == InvalidRenderMeshId &&
         strokeMesh_ == InvalidRenderMeshId) ||
        geometryBounds_.width <= 0.0 ||
        geometryBounds_.height <= 0.0) {
        return;
    }

    const Size target = GetRenderSize();
    double scaleX = 1.0;
    double scaleY = 1.0;
    switch (GetStretch()) {
    case Stretch::None:
        break;
    case Stretch::Fill:
        scaleX =
            target.width / geometryBounds_.width;
        scaleY =
            target.height / geometryBounds_.height;
        break;
    case Stretch::Uniform: {
        const double scale = std::min(
            target.width / geometryBounds_.width,
            target.height / geometryBounds_.height);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = std::max(
            target.width / geometryBounds_.width,
            target.height / geometryBounds_.height);
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
        builder.PushTransform({
            scaleX, 0.0, 0.0, scaleY,
            offsetX, offsetY});
    if (!transform) return;
    if (mesh_ != InvalidRenderMeshId) {
        Base::Result<void> drawn =
            builder.DrawMesh(
                mesh_, ::Aero::Internal::SampleBrush(GetFill()));
        if (!drawn) return;
    }
    if (strokeMesh_ != InvalidRenderMeshId) {
        Base::Result<void> drawn =
            builder.DrawMesh(
                strokeMesh_, ::Aero::Internal::SampleBrush(GetStroke()));
        if (!drawn) return;
    }
    static_cast<void>(builder.PopTransform());
}

} // namespace Aero::Shapes
