#include "render/DisplayList.hpp"
#include <Aero/Shapes.hpp>
#include <Aero/Media/StreamGeometry.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include <Aero/Media/Pen.hpp>
#include "gui/media/GeometryFlatten.hpp"
#include "gui/media/StrokeTessellate.hpp"

#include "render/RenderResources.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"
#include "gui/media/BrushRendering.hpp"

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
    if (Base::Ref<Media::Pen> pen = path.GetPen()) {
        if (Base::Ref<Media::DashStyle> style = pen->GetDashStyle()) {
            const Base::Span<const double> values = style->GetDashes();
            dashes.Clear();
            for (std::uint32_t index = 0U; index < values.Size(); ++index) {
                Base::Result<void> added = dashes.PushBack(values[index]);
                if (!added) return added.GetStatus();
            }
            offset = style->GetOffset();
            return {};
        }
    }
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

struct PathStrokeParams {
    double thickness = 1.0;
    PenLineJoin join = PenLineJoin::Miter;
    PenLineCap startCap = PenLineCap::Flat;
    PenLineCap endCap = PenLineCap::Flat;
    double miterLimit = 10.0;
    Base::Ref<Brush> brush;
};

PathStrokeParams ResolvePathStroke(const Path& path) noexcept {
    PathStrokeParams params;
    params.thickness = path.GetStrokeThickness();
    params.join = path.GetStrokeLineJoin();
    params.startCap = path.GetStrokeStartLineCap();
    params.endCap = path.GetStrokeEndLineCap();
    params.brush = path.GetStroke();
    if (Base::Ref<Media::Pen> pen = path.GetPen()) {
        if (pen->GetBrush()) params.brush = pen->GetBrush();
        params.thickness = pen->GetThickness();
        params.join = pen->GetLineJoin();
        params.startCap = pen->GetStartLineCap();
        params.endCap = pen->GetEndLineCap();
        params.miterLimit = pen->GetMiterLimit();
    }
    return params;
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
    if (!geometry) {
        geometryDirty_ = false;
        return {};
    }
    if (GetIsMeasuring()) {
        geometryBounds_ = geometry->GetBounds();
        if (geometryBounds_.width <= 0.0 && geometryBounds_.height <= 0.0) {
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
            if (flattened && hasBounds) {
                geometryBounds_ = bounds;
            }
        }
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
        if (GetFill()) {
            Base::Result<void> tessellated = TessellateFill(
                pathPoints_,
                contours,
                GetFillRule(),
                geometryVertices_,
                geometryIndices_);
            if (!tessellated) {
                geometryVertices_.Clear();
                geometryIndices_.Clear();
            }
        }
        const PathStrokeParams stroke = ResolvePathStroke(*this);
        if (stroke.brush) {
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
                stroke.thickness,
                GetTrimStart(),
                GetTrimEnd(),
                stroke.join,
                stroke.startCap,
                stroke.endCap,
                stroke.miterLimit,
                {dashes.Data(), dashes.Size()},
                dashOffset,
                strokeVertices_,
                strokeIndices_);
            if (!stroked) return stroked.GetStatus();
        }
    }
    geometryDirty_ = false;
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
        // WPF Path Stretch=None sizes to Bounds.Right / Bounds.Bottom so
        // sibling Paths in a Canvas keep their authored origin (QuestLog
        // gold frame vs inset CenterPanel).
        const Size native{
            std::max(
                0.0,
                geometryBounds_.x + geometryBounds_.width),
            std::max(
                0.0,
                geometryBounds_.y + geometryBounds_.height)};
        return Size{
            widthBounded
                ? std::min(native.width, availableSize.width)
                : native.width,
            heightBounded
                ? std::min(
                      native.height,
                      availableSize.height)
                : native.height};
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
         strokeMesh_ == InvalidRenderMeshId &&
         geometryIndices_.Empty()) ||
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
    // Stretch=None keeps authored coordinates. Subtracting origin here
    // stacked every Canvas Path at (0,0), so CenterPanel covered the
    // gold/coffee frames on the left and top.
    const double offsetX = GetStretch() == Stretch::None
        ? 0.0
        : (target.width - width) * 0.5 -
            geometryBounds_.x * scaleX;
    const double offsetY = GetStretch() == Stretch::None
        ? 0.0
        : (target.height - height) * 0.5 -
            geometryBounds_.y * scaleY;
    Base::Result<void> transform =
        builder.PushTransform(Transform2D{
            scaleX, 0.0, 0.0, scaleY,
            offsetX, offsetY});
    if (!transform) return;
    if (mesh_ != InvalidRenderMeshId || !geometryIndices_.Empty()) {
        Base::Ref<Brush> fill = GetFill();
        if (fill) {
            const bool spatial = IsSpatialGradientBrush(fill.Get());
            if (spatial && !geometryIndices_.Empty()) {
                Base::Result<void> drawn = PaintBrushGeometry(
                    builder,
                    fill,
                    geometryVertices_.AsSpan(),
                    geometryIndices_.AsSpan(),
                    geometryBounds_,
                    false,
                    mesh_);
                if (!drawn) return;
            } else if (!spatial && mesh_ != InvalidRenderMeshId) {
                Color fillColor = ::Aero::Media::SampleBrush(fill);
                if (fillColor.alpha > 0.0F) {
                    Base::Result<void> drawn =
                        builder.DrawMesh(mesh_, fillColor);
                    if (!drawn) return;
                }
            }
        }
    }
    if (strokeMesh_ != InvalidRenderMeshId) {
        Base::Ref<Brush> stroke = ResolvePathStroke(*this).brush;
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
