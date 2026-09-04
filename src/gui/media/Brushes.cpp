#include <Aero/Media/Brushes.hpp>
#include "BrushRendering.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"

#include <Aero/TryCast.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Aero::Media {

double Brush::GetOpacity() const noexcept {
    return GetValue(OpacityProperty);
}

void Brush::SetOpacity(
    double value) noexcept {
    SetValue(OpacityProperty, value);
}

Color SolidColorBrush::GetColor() const noexcept {
    return GetValue(ColorProperty);
}

void SolidColorBrush::SetColor(
    Color value) noexcept {
    SetValue(ColorProperty, value);
}

double GradientStop::GetOffset() const noexcept {
    return GetValue(OffsetProperty);
}

Color GradientStop::GetColor() const noexcept {
    return GetValue(ColorProperty);
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
            retained->AddChangedHandler(stopChangedHandler_);
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
            retained->AddChangedHandler(stopChangedHandler_);
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

std::uint64_t Brush::GetRevision() const noexcept {
    return AeroGuiInternal::FreezableRevision(*this);
}

Base::Result<void> ImageBrush::SetRuntimeImage(
    std::uint64_t image, std::uint32_t width, std::uint32_t height) noexcept {
    const bool changed =
        renderImage_ != image || pixelWidth_ != width || pixelHeight_ != height;
    renderImage_ = image;
    pixelWidth_ = width;
    pixelHeight_ = height;
    if (changed) WritePostscript();
    return {};
}

BrushMappingMode GradientBrush::GetMappingMode() const noexcept {
    return GetValue(MappingModeProperty);
}

void GradientBrush::SetMappingMode(
    BrushMappingMode value) noexcept {
    SetValue(MappingModeProperty, value);
}

GradientSpreadMethod GradientBrush::GetSpreadMethod() const noexcept {
    return GetValue(SpreadMethodProperty);
}

void GradientBrush::SetSpreadMethod(
    GradientSpreadMethod value) noexcept {
    SetValue(SpreadMethodProperty, value);
}

Point LinearGradientBrush::GetStartPoint() const noexcept {
    return GetValue(StartPointProperty);
}

Point LinearGradientBrush::GetEndPoint() const noexcept {
    return GetValue(EndPointProperty);
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
    return GetValue(CenterProperty);
}

Point RadialGradientBrush::GetGradientOrigin() const noexcept {
    return GetValue(GradientOriginProperty);
}

double RadialGradientBrush::GetRadiusX() const noexcept {
    return GetValue(RadiusXProperty);
}

double RadialGradientBrush::GetRadiusY() const noexcept {
    return GetValue(RadiusYProperty);
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

Stretch TileBrush::GetStretch() const noexcept {
    return GetValue(StretchProperty);
}

Rect TileBrush::GetViewbox() const noexcept {
    return GetValue(ViewboxProperty);
}

Rect TileBrush::GetViewport() const noexcept {
    return GetValue(ViewportProperty);
}

BrushMappingMode TileBrush::GetViewboxUnits() const noexcept {
    return GetValue(ViewboxUnitsProperty);
}

BrushMappingMode TileBrush::GetViewportUnits() const noexcept {
    return GetValue(ViewportUnitsProperty);
}

TileMode TileBrush::GetTileMode() const noexcept {
    return GetValue(TileModeProperty);
}

HorizontalAlignment TileBrush::GetAlignmentX() const noexcept {
    return GetValue(AlignmentXProperty);
}

VerticalAlignment TileBrush::GetAlignmentY() const noexcept {
    return GetValue(AlignmentYProperty);
}

void TileBrush::SetStretch(Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

void TileBrush::SetViewbox(Rect value) noexcept {
    SetValue(ViewboxProperty, value);
}

void TileBrush::SetViewport(Rect value) noexcept {
    SetValue(ViewportProperty, value);
}

void TileBrush::SetViewboxUnits(BrushMappingMode value) noexcept {
    SetValue(ViewboxUnitsProperty, value);
}

void TileBrush::SetViewportUnits(BrushMappingMode value) noexcept {
    SetValue(ViewportUnitsProperty, value);
}

void TileBrush::SetTileMode(TileMode value) noexcept {
    SetValue(TileModeProperty, value);
}

void TileBrush::SetAlignmentX(HorizontalAlignment value) noexcept {
    SetValue(AlignmentXProperty, value);
}

void TileBrush::SetAlignmentY(VerticalAlignment value) noexcept {
    SetValue(AlignmentYProperty, value);
}

Base::Ref<ImageSource>
ImageBrush::GetSource() const noexcept {
    return GetValue(ImageSourceProperty);
}

void ImageBrush::SetSource(
    Base::Ref<ImageSource> value) noexcept {
    SetValue(
        ImageSourceProperty, std::move(value));
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

namespace {

constexpr double kGeomEps = 1.0e-9;
constexpr std::uint32_t kRoundedCornerSegments = 12U;
constexpr std::uint32_t kRoundedContourPoints = kRoundedCornerSegments * 4U;

bool InsideClipEdge(Point point, Point start, Point end) noexcept {
    return (end.x - start.x) * (point.y - start.y) -
               (end.y - start.y) * (point.x - start.x) >= -1.0e-9;
}

Point ClipEdgeIntersection(
    Point from, Point to, Point edgeStart, Point edgeEnd) noexcept {
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const double ex = edgeEnd.x - edgeStart.x;
    const double ey = edgeEnd.y - edgeStart.y;
    const double denom = dx * ey - dy * ex;
    if (std::fabs(denom) < 1.0e-18) return to;
    const double t =
        ((edgeStart.x - from.x) * ey - (edgeStart.y - from.y) * ex) / denom;
    return {from.x + t * dx, from.y + t * dy};
}

void BuildRoundedRectContour(
    Rect bounds,
    double radius,
    Point* out) noexcept {
    const double pi = 3.14159265358979323846;
    const double centers[4][2] = {
        {bounds.x + radius, bounds.y + radius},
        {bounds.x + bounds.width - radius, bounds.y + radius},
        {bounds.x + bounds.width - radius, bounds.y + bounds.height - radius},
        {bounds.x + radius, bounds.y + bounds.height - radius}};
    const double start[4] = {pi, pi * 1.5, 0.0, pi * 0.5};
    std::uint32_t index = 0U;
    for (int corner = 0; corner < 4; ++corner) {
        for (std::uint32_t i = 0U; i < kRoundedCornerSegments; ++i) {
            const double angle = start[corner] +
                pi * 0.5 *
                    (static_cast<double>(i) /
                     static_cast<double>(kRoundedCornerSegments));
            out[index].x = centers[corner][0] + radius * std::cos(angle);
            out[index].y = centers[corner][1] + radius * std::sin(angle);
            ++index;
        }
    }
}

int ClipPolygonToConvex(
    const Point* subject,
    int subjectCount,
    const Point* clip,
    int clipCount,
    Point* output) noexcept {
    Point bufferA[64];
    Point bufferB[64];
    int count = std::min(subjectCount, 63);
    for (int i = 0; i < count; ++i) bufferA[i] = subject[i];
    Point* input = bufferA;
    Point* next = bufferB;
    for (int edge = 0; edge < clipCount && count > 0; ++edge) {
        const Point start = clip[edge];
        const Point end = clip[(edge + 1) % clipCount];
        int outCount = 0;
        Point previous = input[count - 1];
        bool previousInside = InsideClipEdge(previous, start, end);
        for (int i = 0; i < count && outCount < 63; ++i) {
            const Point current = input[i];
            const bool currentInside = InsideClipEdge(current, start, end);
            if (currentInside) {
                if (!previousInside) {
                    next[outCount++] =
                        ClipEdgeIntersection(previous, current, start, end);
                    if (outCount >= 63) break;
                }
                next[outCount++] = current;
            } else if (previousInside) {
                next[outCount++] =
                    ClipEdgeIntersection(previous, current, start, end);
            }
            previous = current;
            previousInside = currentInside;
        }
        Point* swap = input;
        input = next;
        next = swap;
        count = outCount;
    }
    for (int i = 0; i < count; ++i) output[i] = input[i];
    return count;
}

int IntersectLineWithRect(
    Rect bounds,
    double nx,
    double ny,
    double d,
    Point hits[2]) noexcept {
    Point points[4];
    int count = 0;
    const auto consider = [&](double x, double y) noexcept {
        if (x < bounds.x - 1.0e-6 ||
            x > bounds.x + bounds.width + 1.0e-6 ||
            y < bounds.y - 1.0e-6 ||
            y > bounds.y + bounds.height + 1.0e-6) {
            return;
        }
        x = std::clamp(x, bounds.x, bounds.x + bounds.width);
        y = std::clamp(y, bounds.y, bounds.y + bounds.height);
        for (int i = 0; i < count; ++i) {
            if (std::hypot(points[i].x - x, points[i].y - y) < 1.0e-5) {
                return;
            }
        }
        if (count < 4) points[count++] = {x, y};
    };
    if (std::fabs(nx) > 1.0e-12) {
        consider((d - ny * bounds.y) / nx, bounds.y);
        consider(
            (d - ny * (bounds.y + bounds.height)) / nx,
            bounds.y + bounds.height);
    }
    if (std::fabs(ny) > 1.0e-12) {
        consider(bounds.x, (d - nx * bounds.x) / ny);
        consider(
            bounds.x + bounds.width,
            (d - nx * (bounds.x + bounds.width)) / ny);
    }
    if (count < 2) {
        if (count == 1) hits[0] = points[0];
        return count;
    }
    int first = 0;
    int second = 1;
    double best = -1.0;
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            const double dist = std::hypot(
                points[i].x - points[j].x, points[i].y - points[j].y);
            if (dist > best) {
                best = dist;
                first = i;
                second = j;
            }
        }
    }
    hits[0] = points[first];
    hits[1] = points[second];
    return 2;
}

void AddUniquePoint(Point* points, int& count, Point point, int limit) noexcept {
    for (int i = 0; i < count; ++i) {
        if (std::hypot(points[i].x - point.x, points[i].y - point.y) < 1.0e-4) {
            return;
        }
    }
    if (count < limit) points[count++] = point;
}

int SortConvexCcw(Point* points, int count) noexcept {
    if (count < 3) return count;
    Point centroid{};
    for (int i = 0; i < count; ++i) {
        centroid.x += points[i].x;
        centroid.y += points[i].y;
    }
    centroid.x /= static_cast<double>(count);
    centroid.y /= static_cast<double>(count);
    std::sort(
        points,
        points + count,
        [centroid](Point left, Point right) noexcept {
            return std::atan2(left.y - centroid.y, left.x - centroid.x) <
                std::atan2(right.y - centroid.y, right.x - centroid.x);
        });
    return count;
}

int IntersectSlabWithRect(
    Rect bounds,
    Point start,
    double axisX,
    double axisY,
    double axisLengthSquared,
    double t0,
    double t1,
    Point* output) noexcept {
    Point points[12];
    int count = 0;
    const Point corners[4] = {
        {bounds.x, bounds.y},
        {bounds.x + bounds.width, bounds.y},
        {bounds.x + bounds.width, bounds.y + bounds.height},
        {bounds.x, bounds.y + bounds.height}};
    const auto tOf = [&](Point point) noexcept {
        return ((point.x - start.x) * axisX + (point.y - start.y) * axisY) /
            axisLengthSquared;
    };
    for (Point corner : corners) {
        const double t = tOf(corner);
        if (t + 1.0e-8 >= t0 && t - 1.0e-8 <= t1) {
            AddUniquePoint(points, count, corner, 12);
        }
    }
    const double startDot = start.x * axisX + start.y * axisY;
    Point hits[2];
    const int count0 = IntersectLineWithRect(
        bounds, axisX, axisY, startDot + t0 * axisLengthSquared, hits);
    for (int i = 0; i < count0; ++i) {
        AddUniquePoint(points, count, hits[i], 12);
    }
    const int count1 = IntersectLineWithRect(
        bounds, axisX, axisY, startDot + t1 * axisLengthSquared, hits);
    for (int i = 0; i < count1; ++i) {
        AddUniquePoint(points, count, hits[i], 12);
    }
    if (count < 3) return 0;
    SortConvexCcw(points, count);
    std::memcpy(
        output,
        points,
        static_cast<std::size_t>(count) * sizeof(Point));
    return count;
}

Base::Result<void> FillSolidPolygon(
    Render::DisplayListBuilder& builder,
    const Point* points,
    int count,
    Color color) noexcept {
    if (count < 3 || color.alpha <= 0.0F) return {};
    const Color colors[4] = {color, color, color, color};
    for (int i = 1; i + 1 < count; ++i) {
        const Point quad[4] = {
            points[0], points[i], points[i + 1], points[i + 1]};
        Base::Result<void> painted =
            builder.FillGradientQuad(quad, colors);
        if (!painted) return painted.GetStatus();
    }
    return {};
}

constexpr std::uint8_t kGpuPaintLinear = 1U;
constexpr std::uint8_t kGpuPaintRadial = 2U;

struct GpuGradientPaint {
    std::uint8_t kind = 0U;
    float uniforms[8]{};
    std::uintptr_t brush = 0U;
    Point start{};
    Point delta{};
    double len2 = 1.0;
    Point origin{};
    double radiusX = 1.0;
    double radiusY = 1.0;
    Rect bounds{};
    Base::Transform2D inverse{};
    bool hasInverse = false;
};

Point GpuSamplePoint(const GpuGradientPaint& gpu, Point point) noexcept {
    if (!gpu.hasInverse) return point;
    const Point uv{
        gpu.bounds.width > 1.0e-12
            ? (point.x - gpu.bounds.x) / gpu.bounds.width : 0.5,
        gpu.bounds.height > 1.0e-12
            ? (point.y - gpu.bounds.y) / gpu.bounds.height : 0.5};
    const Point mapped = TransformPoint(gpu.inverse, uv);
    return {
        gpu.bounds.x + mapped.x * gpu.bounds.width,
        gpu.bounds.y + mapped.y * gpu.bounds.height};
}

Point GpuGradientUv(const GpuGradientPaint& gpu, Point point) noexcept {
    const Point sample = GpuSamplePoint(gpu, point);
    if (gpu.kind == kGpuPaintLinear) {
        const double t =
            ((sample.x - gpu.start.x) * gpu.delta.x +
             (sample.y - gpu.start.y) * gpu.delta.y) /
            gpu.len2;
        return {t, 0.5};
    }
    return {
        (sample.x - gpu.origin.x) / gpu.radiusX,
        (sample.y - gpu.origin.y) / gpu.radiusY};
}

bool TryPrepareGpuGradient(
    const Base::Ref<Brush>& brush,
    Rect bounds,
    bool isRtl,
    GpuGradientPaint& gpu) noexcept {
    gpu = {};
    if (!brush || bounds.width <= 0.0 || bounds.height <= 0.0) {
        return false;
    }
    const auto* linear = ::Aero::TryCast<LinearGradientBrush>(brush.Get());
    const auto* radial = ::Aero::TryCast<RadialGradientBrush>(brush.Get());
    const GradientBrush* gradient = linear != nullptr
        ? static_cast<const GradientBrush*>(linear)
        : static_cast<const GradientBrush*>(radial);
    if (gradient == nullptr) return false;
    if (gradient->GetSpreadMethod() != GradientSpreadMethod::Pad) {
        return false;
    }
    gpu.bounds = bounds;
    gpu.brush = reinterpret_cast<std::uintptr_t>(gradient);
    const float opacity = static_cast<float>(
        std::clamp(brush->GetOpacity(), 0.0, 1.0));
    if (gradient->GetRelativeTransform()) {
        gpu.hasInverse = InvertTransform(
            gradient->GetRelativeTransform()->GetMatrix(), gpu.inverse);
    }
    if (linear != nullptr) {
        Point start = linear->GetStartPoint();
        Point end = linear->GetEndPoint();
        if (isRtl && linear->GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            start.x = 1.0 - start.x;
            end.x = 1.0 - end.x;
        }
        if (linear->GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            start = {
                bounds.x + start.x * bounds.width,
                bounds.y + start.y * bounds.height};
            end = {
                bounds.x + end.x * bounds.width,
                bounds.y + end.y * bounds.height};
        }
        gpu.start = start;
        gpu.delta = {end.x - start.x, end.y - start.y};
        gpu.len2 = gpu.delta.x * gpu.delta.x + gpu.delta.y * gpu.delta.y;
        if (gpu.len2 <= 1.0e-12) return false;
        gpu.kind = kGpuPaintLinear;
        gpu.uniforms[0] = opacity;
        return true;
    }

    Point center = radial->GetCenter();
    Point origin = radial->GetGradientOrigin();
    if (isRtl && radial->GetMappingMode() ==
        BrushMappingMode::RelativeToBoundingBox) {
        center.x = 1.0 - center.x;
        origin.x = 1.0 - origin.x;
    }
    double radiusX = std::max(std::fabs(radial->GetRadiusX()), 1.0e-6);
    double radiusY = std::max(std::fabs(radial->GetRadiusY()), 1.0e-6);
    if (radial->GetMappingMode() ==
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
    if (radiusX <= 1.0e-6 || radiusY <= 1.0e-6) return false;
    const Point c{
        (center.x - origin.x) / radiusX,
        (center.y - origin.y) / radiusY};
    const double d = 1.0 - c.x * c.x - c.y * c.y;
    if (d <= 1.0e-5 || !std::isfinite(d)) return false;
    gpu.origin = origin;
    gpu.radiusX = radiusX;
    gpu.radiusY = radiusY;
    gpu.kind = kGpuPaintRadial;
    gpu.uniforms[0] = static_cast<float>(c.x / d);
    gpu.uniforms[1] = static_cast<float>(c.y / d);
    gpu.uniforms[2] = static_cast<float>(1.0 / d);
    gpu.uniforms[3] = opacity;
    gpu.uniforms[4] = static_cast<float>(c.y);
    gpu.uniforms[5] = static_cast<float>(c.x);
    gpu.uniforms[6] = 0.5F;
    gpu.uniforms[7] = 0.0F;
    return true;
}

Base::Result<void> FillGpuPolygon(
    Render::DisplayListBuilder& builder,
    const GpuGradientPaint& gpu,
    const Point* points,
    int count) noexcept {
    if (count < 3 || gpu.kind == 0U) return {};
    for (int i = 1; i + 1 < count; ++i) {
        const Point quad[4] = {
            points[0], points[i], points[i + 1], points[i + 1]};
        const Point uvs[4] = {
            GpuGradientUv(gpu, quad[0]),
            GpuGradientUv(gpu, quad[1]),
            GpuGradientUv(gpu, quad[2]),
            GpuGradientUv(gpu, quad[3])};
        Base::Result<void> painted = builder.FillGradientQuad(
            quad, uvs, gpu.kind, gpu.uniforms, gpu.brush);
        if (!painted) return painted.GetStatus();
    }
    return {};
}

} // namespace

Color SampleBrushAt(
    const Base::Ref<Brush>& brush,
    Point point,
    Rect bounds,
    bool isRtl) noexcept;

Base::Result<void> PaintBrushRect(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double cornerRadius,
    bool isRtl) noexcept {
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
            imageBrush.GetRenderImageId();
        const std::uint32_t pixelWidth =
            imageBrush.GetPixelWidth();
        const std::uint32_t pixelHeight =
            imageBrush.GetPixelHeight();
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
                if (flipX ^ isRtl) {
                    uv.x += uv.width * (1.0 - relativeX);
                    uv.width = -uv.width * (tile.width / viewport.width);
                } else {
                    uv.x += uv.width * relativeX;
                    uv.width *= tile.width / viewport.width;
                }
                if (flipY) {
                    uv.y += uv.height * (1.0 - relativeY);
                    uv.height = -uv.height * (tile.height / viewport.height);
                } else {
                    uv.y += uv.height * relativeY;
                    uv.height *= tile.height / viewport.height;
                }
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
    GpuGradientPaint gpuPaint;
    if (TryPrepareGpuGradient(brush, bounds, isRtl, gpuPaint)) {
        const double clipRadius = std::min(
            std::max(0.0, cornerRadius),
            std::min(bounds.width, bounds.height) * 0.5);
        if (clipRadius > kGeomEps) {
            Point contour[kRoundedContourPoints];
            BuildRoundedRectContour(bounds, clipRadius, contour);
            return FillGpuPolygon(
                builder,
                gpuPaint,
                contour,
                static_cast<int>(kRoundedContourPoints));
        }
        const Point quad[4] = {
            {bounds.x, bounds.y},
            {bounds.x + bounds.width, bounds.y},
            {bounds.x + bounds.width, bounds.y + bounds.height},
            {bounds.x, bounds.y + bounds.height}};
        return FillGpuPolygon(builder, gpuPaint, quad, 4);
    }
    if (::Aero::TryCast<LinearGradientBrush>(brush.Get()) != nullptr) {
        const auto& gradient =
            *static_cast<LinearGradientBrush*>(
                brush.Get());
        Point start = gradient.GetStartPoint();
        Point end = gradient.GetEndPoint();
        if (isRtl && gradient.GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            start.x = 1.0 - start.x;
            end.x = 1.0 - end.x;
        }
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

        double tMin = 0.0;
        double tMax = 0.0;
        for (int i = 0; i < 4; ++i) {
            Point samplePoint = quadPoints[i];
            if (hasInverse) {
                samplePoint = TransformPoint(inverse, samplePoint);
            }
            const double position =
                ((samplePoint.x - start.x) * axisX +
                 (samplePoint.y - start.y) * axisY) /
                axisLengthSquared;
            if (i == 0) {
                tMin = position;
                tMax = position;
            } else {
                tMin = std::min(tMin, position);
                tMax = std::max(tMax, position);
            }
        }

        const double clipRadius = std::min(
            std::max(0.0, cornerRadius),
            std::min(bounds.width, bounds.height) * 0.5);
        Point clipContour[kRoundedContourPoints];
        if (clipRadius > kGeomEps) {
            BuildRoundedRectContour(bounds, clipRadius, clipContour);
        }

        const auto emitSolid = [&](const Point* points, int count, Color color)
            noexcept -> Base::Result<void> {
            Point clipped[64];
            const Point* emit = points;
            int emitCount = count;
            if (clipRadius > kGeomEps) {
                emitCount = ClipPolygonToConvex(
                    points, count, clipContour,
                    static_cast<int>(kRoundedContourPoints), clipped);
                emit = clipped;
            }
            return FillSolidPolygon(builder, emit, emitCount, color);
        };

        const auto emitSampled = [&](const Point* points, int count)
            noexcept -> Base::Result<void> {
            Point clipped[64];
            const Point* emit = points;
            int emitCount = count;
            if (clipRadius > kGeomEps) {
                emitCount = ClipPolygonToConvex(
                    points, count, clipContour,
                    static_cast<int>(kRoundedContourPoints), clipped);
                emit = clipped;
            }
            if (emitCount < 3) return {};
            Color colors[4];
            for (int i = 1; i + 1 < emitCount; ++i) {
                const Point quad[4] = {
                    emit[0], emit[i], emit[i + 1], emit[i + 1]};
                colors[0] = SampleBrushAt(brush, quad[0], bounds, isRtl);
                colors[1] = SampleBrushAt(brush, quad[1], bounds, isRtl);
                colors[2] = SampleBrushAt(brush, quad[2], bounds, isRtl);
                colors[3] = colors[2];
                Base::Result<void> painted =
                    builder.FillGradientQuad(quad, colors);
                if (!painted) return painted.GetStatus();
            }
            return {};
        };

        if (!hasInverse) {
            Base::Vector<double> boundaries;
            const GradientSpreadMethod spread = gradient.GetSpreadMethod();
            const auto stops = gradient.GetGradientStops();
            const int periodStart = static_cast<int>(std::floor(tMin)) - 1;
            const int periodEnd = static_cast<int>(std::ceil(tMax)) + 1;
            for (int period = periodStart; period <= periodEnd; ++period) {
                for (const Base::Ref<GradientStop>& stop : stops) {
                    if (!stop) continue;
                    if (spread == GradientSpreadMethod::Reflect) {
                        static_cast<void>(boundaries.PushBack(
                            static_cast<double>(period) + stop->GetOffset()));
                        static_cast<void>(boundaries.PushBack(
                            static_cast<double>(period) + 1.0 -
                            stop->GetOffset()));
                    } else if (spread == GradientSpreadMethod::Repeat) {
                        static_cast<void>(boundaries.PushBack(
                            static_cast<double>(period) + stop->GetOffset()));
                    } else {
                        static_cast<void>(boundaries.PushBack(
                            stop->GetOffset()));
                    }
                }
            }
            static_cast<void>(boundaries.PushBack(tMin));
            static_cast<void>(boundaries.PushBack(tMax));
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
            if (boundaryCount > 1U && boundaryCount <= 512U) {
                std::uint32_t paintedCount = 0U;
                for (std::uint32_t i = 0U; i + 1U < boundaryCount; ++i) {
                    const double a = std::clamp(
                        boundaries[i], tMin, tMax);
                    const double b = std::clamp(
                        boundaries[i + 1U], tMin, tMax);
                    if (b - a < 1.0e-9) continue;
                    Point slab[12];
                    const int slabCount = IntersectSlabWithRect(
                        bounds, start, axisX, axisY, axisLengthSquared,
                        a, b, slab);
                    if (slabCount < 3) continue;
                    const Color fill = ::Aero::Media::SampleBrush(
                        brush, (a + b) * 0.5);
                    Base::Result<void> painted =
                        emitSolid(slab, slabCount, fill);
                    if (!painted) return painted.GetStatus();
                    ++paintedCount;
                }
                if (paintedCount > 0U) return {};
            }
        }

        const GradientSpreadMethod spread = gradient.GetSpreadMethod();
        const double tRange = tMax - tMin;
        const bool needsGrid =
            hasInverse ||
            spread == GradientSpreadMethod::Repeat ||
            spread == GradientSpreadMethod::Reflect ||
            tRange > 1.25;
        if (!needsGrid) {
            return emitSampled(quadPoints, 4);
        }
        const int grid = std::clamp(
            static_cast<int>(std::ceil(std::max(8.0, tRange * 8.0))),
            8, 32);
        for (int r = 0; r < grid; ++r) {
            const double y0 = bounds.y + bounds.height *
                (static_cast<double>(r) / grid);
            const double y1 = bounds.y + bounds.height *
                (static_cast<double>(r + 1) / grid);
            for (int c = 0; c < grid; ++c) {
                const double x0 = bounds.x + bounds.width *
                    (static_cast<double>(c) / grid);
                const double x1 = bounds.x + bounds.width *
                    (static_cast<double>(c + 1) / grid);
                const Point cell[4] = {
                    {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
                Base::Result<void> painted = emitSampled(cell, 4);
                if (!painted) return painted.GetStatus();
            }
        }
        return {};
    }
    if (::Aero::TryCast<RadialGradientBrush>(brush.Get()) != nullptr) {
        const double clipRadius = std::min(
            std::max(0.0, cornerRadius),
            std::min(bounds.width, bounds.height) * 0.5);
        Point clipContour[kRoundedContourPoints];
        if (clipRadius > kGeomEps) {
            BuildRoundedRectContour(bounds, clipRadius, clipContour);
        }
        constexpr int gridCols = 16;
        constexpr int gridRows = 16;
        for (int r = 0; r < gridRows; ++r) {
            const double y0 = bounds.y + bounds.height *
                (static_cast<double>(r) / gridRows);
            const double y1 = bounds.y + bounds.height *
                (static_cast<double>(r + 1) / gridRows);
            for (int c = 0; c < gridCols; ++c) {
                const double x0 = bounds.x + bounds.width *
                    (static_cast<double>(c) / gridCols);
                const double x1 = bounds.x + bounds.width *
                    (static_cast<double>(c + 1) / gridCols);
                Point cell[4] = {
                    {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
                const Point* emit = cell;
                int emitCount = 4;
                Point clipped[64];
                if (clipRadius > kGeomEps) {
                    emitCount = ClipPolygonToConvex(
                        cell, 4, clipContour,
                        static_cast<int>(kRoundedContourPoints), clipped);
                    emit = clipped;
                }
                if (emitCount < 3) continue;
                for (int i = 1; i + 1 < emitCount; ++i) {
                    const Point quad[4] = {
                        emit[0], emit[i], emit[i + 1], emit[i + 1]};
                    const Color colors[4] = {
                        SampleBrushAt(brush, quad[0], bounds, isRtl),
                        SampleBrushAt(brush, quad[1], bounds, isRtl),
                        SampleBrushAt(brush, quad[2], bounds, isRtl),
                        SampleBrushAt(brush, quad[2], bounds, isRtl)};
                    Base::Result<void> drawn =
                        builder.FillGradientQuad(quad, colors);
                    if (!drawn) return drawn.GetStatus();
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

Base::Result<void> PaintBrushRoundedStroke(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double thickness,
    double cornerRadius,
    bool isRtl) noexcept {
    if (!brush || bounds.width <= 0.0 || bounds.height <= 0.0 ||
        thickness <= 0.0 || brush->GetOpacity() <= 0.0) {
        return {};
    }
    const double half = thickness * 0.5;
    const double maxRadius = std::min(bounds.width, bounds.height) * 0.5 - half;
    if (maxRadius <= kGeomEps) {
        return PaintBrushRect(builder, brush, bounds, cornerRadius, isRtl);
    }
    const double radius = std::clamp(cornerRadius, 0.0, maxRadius);
    GpuGradientPaint gpuPaint;
    if (TryPrepareGpuGradient(brush, bounds, isRtl, gpuPaint)) {
        const Rect outer{
            bounds.x - half,
            bounds.y - half,
            bounds.width + thickness,
            bounds.height + thickness};
        const Rect inner{
            bounds.x + half,
            bounds.y + half,
            std::max(0.0, bounds.width - thickness),
            std::max(0.0, bounds.height - thickness)};
        Point outerPts[kRoundedContourPoints];
        Point innerPts[kRoundedContourPoints];
        BuildRoundedRectContour(outer, radius + half, outerPts);
        BuildRoundedRectContour(
            inner, std::max(0.0, radius - half), innerPts);
        for (std::uint32_t i = 0U; i < kRoundedContourPoints; ++i) {
            const std::uint32_t next = (i + 1U) % kRoundedContourPoints;
            const Point quad[4] = {
                outerPts[i], outerPts[next], innerPts[next], innerPts[i]};
            const Point uvs[4] = {
                GpuGradientUv(gpuPaint, quad[0]),
                GpuGradientUv(gpuPaint, quad[1]),
                GpuGradientUv(gpuPaint, quad[2]),
                GpuGradientUv(gpuPaint, quad[3])};
            Base::Result<void> painted = builder.FillGradientQuad(
                quad, uvs, gpuPaint.kind, gpuPaint.uniforms, gpuPaint.brush);
            if (!painted) return painted.GetStatus();
        }
        return {};
    }
    const Color color = ::Aero::Media::SampleBrush(brush);
    if (color.alpha <= 0.0F) return {};
    return builder.StrokeRect(bounds, color, thickness, radius);
}

Color SampleBrushAt(
    const Base::Ref<Brush>& brush,
    Point point,
    Rect bounds,
    bool isRtl) noexcept {
    if (!brush) return {};
    const Point uv{
        bounds.width > 1.0e-12
            ? (point.x - bounds.x) / bounds.width : 0.5,
        bounds.height > 1.0e-12
            ? (point.y - bounds.y) / bounds.height : 0.5};
    const Size size{bounds.width, bounds.height};
    if (::Aero::TryCast<LinearGradientBrush>(brush.Get()) != nullptr) {
        const auto& gradient =
            *static_cast<LinearGradientBrush*>(brush.Get());
        Point start = gradient.GetStartPoint();
        Point end = gradient.GetEndPoint();
        if (isRtl && gradient.GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            start.x = 1.0 - start.x;
            end.x = 1.0 - end.x;
        }
        if (gradient.GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            start = {
                bounds.x + start.x * bounds.width,
                bounds.y + start.y * bounds.height};
            end = {
                bounds.x + end.x * bounds.width,
                bounds.y + end.y * bounds.height};
        }
        Point samplePoint = point;
        if (gradient.GetRelativeTransform()) {
            Base::Transform2D inverse;
            if (InvertTransform(
                    gradient.GetRelativeTransform()->GetMatrix(), inverse)) {
                const Point mapped = TransformPoint(inverse, uv);
                samplePoint = {
                    bounds.x + mapped.x * bounds.width,
                    bounds.y + mapped.y * bounds.height};
            }
        }
        const double axisX = end.x - start.x;
        const double axisY = end.y - start.y;
        const double axisLengthSquared = std::max(
            axisX * axisX + axisY * axisY, 1.0e-12);
        const double position =
            ((samplePoint.x - start.x) * axisX +
             (samplePoint.y - start.y) * axisY) /
            axisLengthSquared;
        return ::Aero::Media::SampleBrush(
            brush, position, {}, uv, size);
    }
    if (::Aero::TryCast<RadialGradientBrush>(brush.Get()) != nullptr) {
        const auto& gradient =
            *static_cast<RadialGradientBrush*>(brush.Get());
        Point center = gradient.GetCenter();
        Point origin = gradient.GetGradientOrigin();
        if (isRtl && gradient.GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            center.x = 1.0 - center.x;
            origin.x = 1.0 - origin.x;
        }
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
        Point samplePoint = point;
        if (gradient.GetRelativeTransform()) {
            Base::Transform2D inverse;
            if (InvertTransform(
                    gradient.GetRelativeTransform()->GetMatrix(), inverse)) {
                const Point mapped = TransformPoint(inverse, uv);
                samplePoint = {
                    bounds.x + mapped.x * bounds.width,
                    bounds.y + mapped.y * bounds.height};
            }
        }
        const double fx = (origin.x - center.x) / radiusX;
        const double fy = (origin.y - center.y) / radiusY;
        const bool hasFocal = (fx * fx + fy * fy) > 1.0e-6;
        const double u = (samplePoint.x - center.x) / radiusX;
        const double v = (samplePoint.y - center.y) / radiusY;
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
        return ::Aero::Media::SampleBrush(brush, t, {}, uv, size);
    }
    return ::Aero::Media::SampleBrush(brush, 0.5, {}, uv, size);
}

Base::Result<void> PaintBrushGeometry(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Base::Span<const Point> vertices,
    Base::Span<const std::uint32_t> indices,
    Rect bounds,
    bool isRtl,
    Render::RenderMeshId mesh) noexcept {
    if (!brush || indices.Size() < 3U || vertices.Empty()) {
        return {};
    }
    if (brush->GetOpacity() <= 0.0) return {};
    GpuGradientPaint gpuPaint;
    if (TryPrepareGpuGradient(brush, bounds, isRtl, gpuPaint)) {
        if (mesh != Render::InvalidRenderMeshId) {
            return builder.DrawMesh(
                mesh,
                gpuPaint.kind,
                gpuPaint.uniforms,
                gpuPaint.brush,
                gpuPaint.bounds,
                gpuPaint.kind == kGpuPaintLinear
                    ? gpuPaint.start
                    : gpuPaint.origin,
                gpuPaint.delta,
                Point{gpuPaint.radiusX, gpuPaint.radiusY},
                gpuPaint.len2,
                gpuPaint.hasInverse,
                gpuPaint.inverse);
        }
        for (std::uint32_t index = 0U; index + 2U < indices.Size(); index += 3U) {
            const std::uint32_t ia = indices[index];
            const std::uint32_t ib = indices[index + 1U];
            const std::uint32_t ic = indices[index + 2U];
            if (ia >= vertices.Size() || ib >= vertices.Size() ||
                ic >= vertices.Size()) {
                continue;
            }
            const Point tri[3] = {
                vertices[ia], vertices[ib], vertices[ic]};
            Base::Result<void> drawn =
                FillGpuPolygon(builder, gpuPaint, tri, 3);
            if (!drawn) return drawn.GetStatus();
        }
        return {};
    }
    const bool spatial = IsSpatialGradientBrush(brush.Get());
    Color solid{};
    if (!spatial) {
        solid = ::Aero::Media::SampleBrush(brush);
        if (solid.alpha <= 0.0F) return {};
    }
    const auto emit = [&](Point pa, Point pb, Point pc)
        noexcept -> Base::Result<void> {
        const Color ca =
            spatial ? SampleBrushAt(brush, pa, bounds, isRtl) : solid;
        const Color cb =
            spatial ? SampleBrushAt(brush, pb, bounds, isRtl) : solid;
        const Color cc =
            spatial ? SampleBrushAt(brush, pc, bounds, isRtl) : solid;
        if (ca.alpha <= 0.0F && cb.alpha <= 0.0F && cc.alpha <= 0.0F) {
            return {};
        }
        const Point quad[4] = {pa, pb, pc, pc};
        const Color colors[4] = {ca, cb, cc, cc};
        return builder.FillGradientQuad(quad, colors);
    };
    const bool radial =
        ::Aero::TryCast<RadialGradientBrush>(brush.Get()) != nullptr;
    for (std::uint32_t index = 0U; index + 2U < indices.Size(); index += 3U) {
        const std::uint32_t ia = indices[index];
        const std::uint32_t ib = indices[index + 1U];
        const std::uint32_t ic = indices[index + 2U];
        if (ia >= vertices.Size() || ib >= vertices.Size() ||
            ic >= vertices.Size()) {
            continue;
        }
        const Point a = vertices[ia];
        const Point b = vertices[ib];
        const Point c = vertices[ic];
        // Scanline tessellation already emits thin bands. Recursive midpoint
        // splits turn each band into 64 quads and freeze the first frame on
        // vector-heavy scenes (TicTacToe hills/bushes). Radial still needs one
        // interior sample so the highlight is not flattened to the outline.
        if (radial) {
            const Point mid{
                (a.x + b.x + c.x) / 3.0,
                (a.y + b.y + c.y) / 3.0};
            Base::Result<void> drawn = emit(a, b, mid);
            if (drawn) drawn = emit(b, c, mid);
            if (drawn) drawn = emit(c, a, mid);
            if (!drawn) return drawn.GetStatus();
            continue;
        }
        Base::Result<void> drawn = emit(a, b, c);
        if (!drawn) return drawn.GetStatus();
    }
    return {};
}

} // namespace Aero::Media
