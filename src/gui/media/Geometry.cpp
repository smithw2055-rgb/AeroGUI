#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/StreamGeometry.hpp>
#include <Aero/Media/PathFigure.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include <Aero/Media/PathSegment.hpp>
#include <Aero/Media/LineSegment.hpp>
#include <Aero/Media/BezierSegment.hpp>
#include <Aero/Media/QuadraticBezierSegment.hpp>
#include <Aero/Media/ArcSegment.hpp>
#include <Aero/Media/PolyLineSegment.hpp>
#include <Aero/Media/PolyBezierSegment.hpp>
#include <Aero/Media/PolyQuadraticBezierSegment.hpp>
#include <Aero/Media/LineGeometry.hpp>
#include <Aero/Media/RectangleGeometry.hpp>
#include <Aero/Media/EllipseGeometry.hpp>
#include <Aero/Media/GeometryGroup.hpp>
#include <Aero/Media/CombinedGeometry.hpp>
#include "gui/media/GeometryFlatten.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace Aero::Media {
namespace {

constexpr double Kappa = 0.5522847498307936;

class TransformingSink final : public FlattenSink {
public:
    TransformingSink(
        FlattenSink& inner,
        const Base::Transform2D& matrix) noexcept
        : inner_(inner), matrix_(matrix) {}
    void AddPoint(Point point) noexcept override {
        inner_.AddPoint(TransformPoint(matrix_, point));
    }
    void BeginFigure(Point start, bool isClosed) noexcept override {
        inner_.BeginFigure(TransformPoint(matrix_, start), isClosed);
    }
    void EndFigure(bool isClosed) noexcept override {
        inner_.EndFigure(isClosed);
    }
private:
    FlattenSink& inner_;
    Base::Transform2D matrix_;
};

void FlattenRoundedRect(
    FlattenSink& sink,
    Rect rect,
    double radiusX,
    double radiusY) noexcept {
    const double left = rect.x;
    const double top = rect.y;
    const double right = rect.x + rect.width;
    const double bottom = rect.y + rect.height;
    radiusX = std::clamp(radiusX, 0.0, rect.width * 0.5);
    radiusY = std::clamp(radiusY, 0.0, rect.height * 0.5);
    if (radiusX <= 1.0e-9 || radiusY <= 1.0e-9) {
        sink.BeginFigure({left, top}, true);
        sink.AddPoint({right, top});
        sink.AddPoint({right, bottom});
        sink.AddPoint({left, bottom});
        sink.EndFigure(true);
        return;
    }
    sink.BeginFigure({left + radiusX, top}, true);
    sink.AddPoint({right - radiusX, top});
    FlattenCubicBezier(
        sink,
        {right - radiusX, top},
        {right - radiusX + Kappa * radiusX, top},
        {right, top + radiusY - Kappa * radiusY},
        {right, top + radiusY});
    sink.AddPoint({right, bottom - radiusY});
    FlattenCubicBezier(
        sink,
        {right, bottom - radiusY},
        {right, bottom - radiusY + Kappa * radiusY},
        {right - radiusX + Kappa * radiusX, bottom},
        {right - radiusX, bottom});
    sink.AddPoint({left + radiusX, bottom});
    FlattenCubicBezier(
        sink,
        {left + radiusX, bottom},
        {left + radiusX - Kappa * radiusX, bottom},
        {left, bottom - radiusY + Kappa * radiusY},
        {left, bottom - radiusY});
    sink.AddPoint({left, top + radiusY});
    FlattenCubicBezier(
        sink,
        {left, top + radiusY},
        {left, top + radiusY - Kappa * radiusY},
        {left + radiusX - Kappa * radiusX, top},
        {left + radiusX, top});
    sink.EndFigure(true);
}

} // namespace

Geometry::~Geometry() {
    if (transform_ && !transform_->IsFrozen() &&
        !transformChangedHandler_.Empty()) {
        static_cast<void>(transform_->RemoveChangedHandler(
            transformChangedHandler_));
    }
}

void Geometry::SetTransform(Base::Ref<Transform> value) noexcept {
    if (!WritePreamble() || transform_.Get() == value.Get()) return;
    if (transformChangedHandler_.Empty()) {
        transformChangedHandler_ = FreezableChangedHandler(
            this, &Geometry::OnTransformChanged);
    }
    Transform* next = value.Get();
    if (next != nullptr && !next->IsFrozen()) {
        next->AddChangedHandler(transformChangedHandler_);
    }
    Base::Ref<Transform> previous = std::move(transform_);
    transform_ = std::move(value);
    if (previous && !previous->IsFrozen()) {
        static_cast<void>(previous->RemoveChangedHandler(
            transformChangedHandler_));
    }
    WritePostscript();
}

void Geometry::OnTransformChanged(Freezable&) noexcept {
    WritePostscript();
}

bool Geometry::FreezeCore(bool isChecking) noexcept {
    if (transform_) {
        if (isChecking) {
            if (!transform_->CanFreeze()) return false;
        } else {
            static_cast<void>(transform_->Freeze());
        }
    }
    return Freezable::FreezeCore(isChecking);
}

void Geometry::Flatten(FlattenSink& sink) const noexcept {
    if (!transform_) return FlattenCore(sink);
    TransformingSink wrapped(sink, transform_->GetMatrix());
    return FlattenCore(wrapped);
}

void Geometry::FlattenCore(FlattenSink&) const noexcept {
    return;
}

void PathFigure::AddSegment(
    Base::Ref<PathSegment> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    if (!value) { AERO_ASSERT(false); return; }
    segments_.Add(std::move(value));
    WritePostscript();
}

void PathGeometry::AddFigure(
    Base::Ref<PathFigure> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    if (!value) { AERO_ASSERT(false); return; }
    figures_.Add(std::move(value));
    WritePostscript();
}

void PathGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    for (const Ref<PathFigure>& figure : figures_) {
        if (!figure) continue;
        Point current = figure->GetStartPoint();
        sink.BeginFigure(current, figure->GetIsClosed());
        for (const Ref<PathSegment>& segment : figure->GetSegments()) {
            if (!segment) continue;
            segment->Flatten(sink, current);
        }
        sink.EndFigure(figure->GetIsClosed());
    }
}

namespace {
Base::Result<void> AppendPoint(
    Base::String& output,
    char command,
    Base::Point point) noexcept {
    char text[96]{};
    const int length = std::snprintf(
        text, sizeof(text), "%c%.17g,%.17g", command, point.x, point.y);
    if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(text)) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "PathGeometry coordinate text is too large");
    }
    return output.Append(Base::StringView(
        text, static_cast<std::uint32_t>(length)));
}
} // namespace

Base::Result<Base::String> PathGeometry::ToStreamData() const noexcept {
    Base::String result;
    for (const Base::Ref<PathFigure>& figure : figures_) {
        if (!figure) continue;
        Base::Result<void> appended =
            AppendPoint(result, 'M', figure->GetStartPoint());
        if (!appended) return appended.GetStatus();
        for (const Base::Ref<PathSegment>& segment : figure->GetSegments()) {
            if (!segment || segment->RuntimeType() != LineSegment::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "PathGeometry currently supports LineSegment content");
            }
            appended = AppendPoint(
                result, 'L',
                static_cast<const LineSegment&>(*segment).GetPoint());
            if (!appended) return appended.GetStatus();
        }
        if (figure->GetIsClosed()) {
            appended = result.Append(Base::StringView("Z"));
            if (!appended) return appended.GetStatus();
        }
    }
    return result;
}

void LineSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point point = GetPoint();
    sink.AddPoint(point);
    currentPoint = point;
}

void BezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point end = GetPoint3();
    FlattenCubicBezier(
        sink, currentPoint, GetPoint1(), GetPoint2(), end);
    currentPoint = end;
}

void QuadraticBezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point end = GetPoint2();
    FlattenQuadraticBezier(
        sink, currentPoint, GetPoint1(), end);
    currentPoint = end;
}

void ArcSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point end = GetPoint();
    FlattenArc(
        sink,
        currentPoint,
        GetSize(),
        GetRotationAngle(),
        GetIsLargeArc(),
        GetSweepDirection() == SweepDirection::Clockwise,
        end);
    currentPoint = end;
}

void PolyLineSegment::SetPoints(Span<const Point> points) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    points_.Clear();
    points_.Append(points);
    WritePostscript();
}
void PolyLineSegment::AddPoint(Point point) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    points_.PushBack(point);
    WritePostscript();
}
void PolyLineSegment::ClearPoints() noexcept {
    if (!WritePreamble()) return;
    points_.Clear();
    WritePostscript();
}
void PolyLineSegment::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePointList(text, parsed);
    if (!status) { AERO_ASSERT(false); return; }
    SetPoints(parsed.AsSpan());
}
void PolyLineSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    for (std::uint32_t index = 0U; index < points_.Size(); ++index) {
        sink.AddPoint(points_[index]);
        currentPoint = points_[index];
    }
}

void PolyBezierSegment::SetPoints(Span<const Point> points) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    points_.Clear();
    points_.Append(points);
    WritePostscript();
}
void PolyBezierSegment::AddPoint(Point point) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    points_.PushBack(point);
    WritePostscript();
}
void PolyBezierSegment::ClearPoints() noexcept {
    if (!WritePreamble()) return;
    points_.Clear();
    WritePostscript();
}
void PolyBezierSegment::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePointList(text, parsed);
    if (!status) { AERO_ASSERT(false); return; }
    SetPoints(parsed.AsSpan());
}
void PolyBezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    for (std::uint32_t index = 0U; index + 2U < points_.Size(); index += 3U) {
        const Point end = points_[index + 2U];
        FlattenCubicBezier(
            sink,
            currentPoint,
            points_[index],
            points_[index + 1U],
            end);
        currentPoint = end;
    }
}

void PolyQuadraticBezierSegment::SetPoints(Span<const Point> points) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    points_.Clear();
    points_.Append(points);
    WritePostscript();
}
void PolyQuadraticBezierSegment::AddPoint(Point point) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    points_.PushBack(point);
    WritePostscript();
}
void PolyQuadraticBezierSegment::ClearPoints() noexcept {
    if (!WritePreamble()) return;
    points_.Clear();
    WritePostscript();
}
void PolyQuadraticBezierSegment::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePointList(text, parsed);
    if (!status) { AERO_ASSERT(false); return; }
    SetPoints(parsed.AsSpan());
}
void PolyQuadraticBezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    for (std::uint32_t index = 0U; index + 1U < points_.Size(); index += 2U) {
        const Point end = points_[index + 1U];
        FlattenQuadraticBezier(
            sink, currentPoint, points_[index], end);
        currentPoint = end;
    }
}

Rect LineGeometry::GetBounds() const noexcept {
    const Point start = GetStartPoint();
    const Point end = GetEndPoint();
    const double left = std::min(start.x, end.x);
    const double top = std::min(start.y, end.y);
    return {
        left,
        top,
        std::fabs(end.x - start.x),
        std::fabs(end.y - start.y)};
}

void LineGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    const Point start = GetStartPoint();
    sink.BeginFigure(start, false);
    sink.AddPoint(GetEndPoint());
    sink.EndFigure(false);
    return;
}

void RectangleGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    FlattenRoundedRect(sink, GetRect(), GetRadiusX(), GetRadiusY());
    return;
}

Rect EllipseGeometry::GetBounds() const noexcept {
    const Point center = GetCenter();
    const double radiusX = GetRadiusX();
    const double radiusY = GetRadiusY();
    return {
        center.x - radiusX,
        center.y - radiusY,
        radiusX * 2.0,
        radiusY * 2.0};
}

void EllipseGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    const Point center = GetCenter();
    const double radiusX = GetRadiusX();
    const double radiusY = GetRadiusY();
    if (radiusX <= 0.0 || radiusY <= 0.0) return;
    const Point start{center.x + radiusX, center.y};
    sink.BeginFigure(start, true);
    FlattenCubicBezier(
        sink,
        start,
        {center.x + radiusX, center.y + Kappa * radiusY},
        {center.x + Kappa * radiusX, center.y + radiusY},
        {center.x, center.y + radiusY});
    FlattenCubicBezier(
        sink,
        {center.x, center.y + radiusY},
        {center.x - Kappa * radiusX, center.y + radiusY},
        {center.x - radiusX, center.y + Kappa * radiusY},
        {center.x - radiusX, center.y});
    FlattenCubicBezier(
        sink,
        {center.x - radiusX, center.y},
        {center.x - radiusX, center.y - Kappa * radiusY},
        {center.x - Kappa * radiusX, center.y - radiusY},
        {center.x, center.y - radiusY});
    FlattenCubicBezier(
        sink,
        {center.x, center.y - radiusY},
        {center.x + Kappa * radiusX, center.y - radiusY},
        {center.x + radiusX, center.y - Kappa * radiusY},
        start);
    sink.EndFigure(true);
}

void GeometryGroup::Add(Ref<Geometry> value) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    if (!value) { AERO_ASSERT(false); return; }
    children_.Add(std::move(value));
    WritePostscript();
}

void GeometryGroup::FlattenCore(FlattenSink& sink) const noexcept {
    for (const Ref<Geometry>& child : children_) {
        if (!child) continue;
        child->Flatten(sink);
    }
    return;
}

void CombinedGeometry::OnChildChanged(Freezable&) noexcept {
    WritePostscript();
}

void CombinedGeometry::AttachChild(
    Ref<Geometry>& slot,
    Ref<Geometry> value) noexcept {
    if (!WritePreamble() || slot.Get() == value.Get()) return;
    if (childChangedHandler_.Empty()) {
        childChangedHandler_ = FreezableChangedHandler(
            this, &CombinedGeometry::OnChildChanged);
    }
    Geometry* next = value.Get();
    if (next != nullptr && !next->IsFrozen()) {
        next->AddChangedHandler(childChangedHandler_);
    }
    Ref<Geometry> previous = std::move(slot);
    slot = std::move(value);
    if (previous && !previous->IsFrozen()) {
        static_cast<void>(previous->RemoveChangedHandler(
            childChangedHandler_));
    }
    WritePostscript();
}

void CombinedGeometry::SetGeometry1(Ref<Geometry> value) noexcept {
    AttachChild(geometry1_, std::move(value));
}

void CombinedGeometry::SetGeometry2(Ref<Geometry> value) noexcept {
    AttachChild(geometry2_, std::move(value));
}

void CombinedGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    // Boolean combine (Intersect/Xor/Exclude) needs a tessellator such as
    // libtess2; this pass concatenates both operands so Union still renders.
    if (geometry1_) {
        geometry1_->Flatten(sink);
    }
    if (GetGeometryCombineMode() == GeometryCombineMode::Exclude) {
        return;
    }
    if (geometry2_) {
        geometry2_->Flatten(sink);
        return;
    }
    return;
}

bool CombinedGeometry::FreezeCore(bool isChecking) noexcept {
    auto freezeChild = [&](Geometry* child) noexcept {
        if (child == nullptr) return true;
        if (isChecking) return child->CanFreeze();
        static_cast<void>(child->Freeze());
        return true;
    };
    if (!freezeChild(geometry1_.Get()) || !freezeChild(geometry2_.Get())) {
        return false;
    }
    return Geometry::FreezeCore(isChecking);
}

} // namespace Aero::Media

#include <Aero/Media/DashStyle.hpp>

namespace Aero::Media {

void DashStyle::SetDashes(Span<const double> value) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    dashes_.Clear();
    for (std::uint32_t index = 0U; index < value.Size(); ++index) {
        const double dash = value[index];
        if (!std::isfinite(dash) || dash < 0.0) { AERO_ASSERT(false); return; }
        dashes_.PushBack(dash);
    }
    WritePostscript();
}

void DashStyle::SetOffset(double value) noexcept {
    if (!std::isfinite(value)) return;
    Result<void> writable = WritePreamble();
    if (!writable) return;
    offset_ = value;
    WritePostscript();
}

} // namespace Aero::Media
