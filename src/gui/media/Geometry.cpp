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
    Result<void> AddPoint(Point point) noexcept override {
        return inner_.AddPoint(TransformPoint(matrix_, point));
    }
    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        return inner_.BeginFigure(TransformPoint(matrix_, start), isClosed);
    }
    Result<void> EndFigure(bool isClosed) noexcept override {
        return inner_.EndFigure(isClosed);
    }
private:
    FlattenSink& inner_;
    Base::Transform2D matrix_;
};

Result<void> FlattenRoundedRect(
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
        Result<void> started = sink.BeginFigure({left, top}, true);
        if (!started) return started.GetStatus();
        Result<void> added = sink.AddPoint({right, top});
        if (added) added = sink.AddPoint({right, bottom});
        if (added) added = sink.AddPoint({left, bottom});
        if (!added) return added.GetStatus();
        return sink.EndFigure(true);
    }
    Result<void> started =
        sink.BeginFigure({left + radiusX, top}, true);
    if (!started) return started.GetStatus();
    Result<void> added = sink.AddPoint({right - radiusX, top});
    if (!added) return added.GetStatus();
    added = FlattenCubicBezier(
        sink,
        {right - radiusX, top},
        {right - radiusX + Kappa * radiusX, top},
        {right, top + radiusY - Kappa * radiusY},
        {right, top + radiusY});
    if (!added) return added.GetStatus();
    added = sink.AddPoint({right, bottom - radiusY});
    if (!added) return added.GetStatus();
    added = FlattenCubicBezier(
        sink,
        {right, bottom - radiusY},
        {right, bottom - radiusY + Kappa * radiusY},
        {right - radiusX + Kappa * radiusX, bottom},
        {right - radiusX, bottom});
    if (!added) return added.GetStatus();
    added = sink.AddPoint({left + radiusX, bottom});
    if (!added) return added.GetStatus();
    added = FlattenCubicBezier(
        sink,
        {left + radiusX, bottom},
        {left + radiusX - Kappa * radiusX, bottom},
        {left, bottom - radiusY + Kappa * radiusY},
        {left, bottom - radiusY});
    if (!added) return added.GetStatus();
    added = sink.AddPoint({left, top + radiusY});
    if (!added) return added.GetStatus();
    added = FlattenCubicBezier(
        sink,
        {left, top + radiusY},
        {left, top + radiusY - Kappa * radiusY},
        {left + radiusX - Kappa * radiusX, top},
        {left + radiusX, top});
    if (!added) return added.GetStatus();
    return sink.EndFigure(true);
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
        Base::Result<void> subscribed =
            next->AddChangedHandler(transformChangedHandler_);
        if (!subscribed) return;
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

Result<void> Geometry::Flatten(FlattenSink& sink) const noexcept {
    if (!transform_) return FlattenCore(sink);
    TransformingSink wrapped(sink, transform_->GetMatrix());
    return FlattenCore(wrapped);
}

Result<void> Geometry::FlattenCore(FlattenSink&) const noexcept {
    return {};
}

Base::Result<void> PathFigure::AddSegment(
    Base::Ref<PathSegment> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PathFigure segment cannot be null");
    }
    Base::Result<void> added = segments_.Add(std::move(value));
    if (added) WritePostscript();
    return added;
}

Base::Result<void> PathGeometry::AddFigure(
    Base::Ref<PathFigure> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PathGeometry figure cannot be null");
    }
    Base::Result<void> added = figures_.Add(std::move(value));
    if (added) WritePostscript();
    return added;
}

Result<void> PathGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    for (const Ref<PathFigure>& figure : figures_) {
        if (!figure) continue;
        Point current = figure->GetStartPoint();
        Result<void> started =
            sink.BeginFigure(current, figure->GetIsClosed());
        if (!started) return started.GetStatus();
        for (const Ref<PathSegment>& segment : figure->GetSegments()) {
            if (!segment) continue;
            Result<void> flattened = segment->Flatten(sink, current);
            if (!flattened) return flattened.GetStatus();
        }
        Result<void> ended = sink.EndFigure(figure->GetIsClosed());
        if (!ended) return ended.GetStatus();
    }
    return {};
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

Result<void> LineSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point point = GetPoint();
    Result<void> added = sink.AddPoint(point);
    if (!added) return added.GetStatus();
    currentPoint = point;
    return {};
}

Result<void> BezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point end = GetPoint3();
    Result<void> flattened = FlattenCubicBezier(
        sink, currentPoint, GetPoint1(), GetPoint2(), end);
    if (!flattened) return flattened.GetStatus();
    currentPoint = end;
    return {};
}

Result<void> QuadraticBezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point end = GetPoint2();
    Result<void> flattened = FlattenQuadraticBezier(
        sink, currentPoint, GetPoint1(), end);
    if (!flattened) return flattened.GetStatus();
    currentPoint = end;
    return {};
}

Result<void> ArcSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    const Point end = GetPoint();
    Result<void> flattened = FlattenArc(
        sink,
        currentPoint,
        GetSize(),
        GetRotationAngle(),
        GetIsLargeArc(),
        GetSweepDirection() == SweepDirection::Clockwise,
        end);
    if (!flattened) return flattened.GetStatus();
    currentPoint = end;
    return {};
}

Result<void> PolyLineSegment::SetPoints(Span<const Point> points) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    points_.Clear();
    Result<void> stored = points_.Append(points);
    if (stored) WritePostscript();
    return stored;
}
Result<void> PolyLineSegment::AddPoint(Point point) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    Result<void> stored = points_.PushBack(point);
    if (stored) WritePostscript();
    return stored;
}
void PolyLineSegment::ClearPoints() noexcept {
    if (!WritePreamble()) return;
    points_.Clear();
    WritePostscript();
}
Result<void> PolyLineSegment::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePointList(text, parsed);
    if (!status) return status.GetStatus();
    return SetPoints(parsed.AsSpan());
}
Result<void> PolyLineSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    for (std::uint32_t index = 0U; index < points_.Size(); ++index) {
        Result<void> added = sink.AddPoint(points_[index]);
        if (!added) return added.GetStatus();
        currentPoint = points_[index];
    }
    return {};
}

Result<void> PolyBezierSegment::SetPoints(Span<const Point> points) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    points_.Clear();
    Result<void> stored = points_.Append(points);
    if (stored) WritePostscript();
    return stored;
}
Result<void> PolyBezierSegment::AddPoint(Point point) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    Result<void> stored = points_.PushBack(point);
    if (stored) WritePostscript();
    return stored;
}
void PolyBezierSegment::ClearPoints() noexcept {
    if (!WritePreamble()) return;
    points_.Clear();
    WritePostscript();
}
Result<void> PolyBezierSegment::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePointList(text, parsed);
    if (!status) return status.GetStatus();
    return SetPoints(parsed.AsSpan());
}
Result<void> PolyBezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    for (std::uint32_t index = 0U; index + 2U < points_.Size(); index += 3U) {
        const Point end = points_[index + 2U];
        Result<void> flattened = FlattenCubicBezier(
            sink,
            currentPoint,
            points_[index],
            points_[index + 1U],
            end);
        if (!flattened) return flattened.GetStatus();
        currentPoint = end;
    }
    return {};
}

Result<void> PolyQuadraticBezierSegment::SetPoints(
    Span<const Point> points) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    points_.Clear();
    Result<void> stored = points_.Append(points);
    if (stored) WritePostscript();
    return stored;
}
Result<void> PolyQuadraticBezierSegment::AddPoint(Point point) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    Result<void> stored = points_.PushBack(point);
    if (stored) WritePostscript();
    return stored;
}
void PolyQuadraticBezierSegment::ClearPoints() noexcept {
    if (!WritePreamble()) return;
    points_.Clear();
    WritePostscript();
}
Result<void> PolyQuadraticBezierSegment::SetPoints(StringView text) noexcept {
    Base::Vector<Point> parsed;
    Result<void> status = ParsePointList(text, parsed);
    if (!status) return status.GetStatus();
    return SetPoints(parsed.AsSpan());
}
Result<void> PolyQuadraticBezierSegment::Flatten(
    FlattenSink& sink,
    Point& currentPoint) const noexcept {
    for (std::uint32_t index = 0U; index + 1U < points_.Size(); index += 2U) {
        const Point end = points_[index + 1U];
        Result<void> flattened = FlattenQuadraticBezier(
            sink, currentPoint, points_[index], end);
        if (!flattened) return flattened.GetStatus();
        currentPoint = end;
    }
    return {};
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

Result<void> LineGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    const Point start = GetStartPoint();
    Result<void> started = sink.BeginFigure(start, false);
    if (!started) return started.GetStatus();
    Result<void> added = sink.AddPoint(GetEndPoint());
    if (!added) return added.GetStatus();
    return sink.EndFigure(false);
}

Result<void> RectangleGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    return FlattenRoundedRect(sink, GetRect(), GetRadiusX(), GetRadiusY());
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

Result<void> EllipseGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    const Point center = GetCenter();
    const double radiusX = GetRadiusX();
    const double radiusY = GetRadiusY();
    if (radiusX <= 0.0 || radiusY <= 0.0) return {};
    const Point start{center.x + radiusX, center.y};
    Result<void> started = sink.BeginFigure(start, true);
    if (!started) return started.GetStatus();
    Result<void> added = FlattenCubicBezier(
        sink,
        start,
        {center.x + radiusX, center.y + Kappa * radiusY},
        {center.x + Kappa * radiusX, center.y + radiusY},
        {center.x, center.y + radiusY});
    if (added) added = FlattenCubicBezier(
        sink,
        {center.x, center.y + radiusY},
        {center.x - Kappa * radiusX, center.y + radiusY},
        {center.x - radiusX, center.y + Kappa * radiusY},
        {center.x - radiusX, center.y});
    if (added) added = FlattenCubicBezier(
        sink,
        {center.x - radiusX, center.y},
        {center.x - radiusX, center.y - Kappa * radiusY},
        {center.x - Kappa * radiusX, center.y - radiusY},
        {center.x, center.y - radiusY});
    if (added) added = FlattenCubicBezier(
        sink,
        {center.x, center.y - radiusY},
        {center.x + Kappa * radiusX, center.y - radiusY},
        {center.x + radiusX, center.y - Kappa * radiusY},
        start);
    if (!added) return added.GetStatus();
    return sink.EndFigure(true);
}

Result<void> GeometryGroup::Add(Ref<Geometry> value) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GeometryGroup child cannot be null");
    }
    Result<void> added = children_.Add(std::move(value));
    if (added) WritePostscript();
    return added;
}

Result<void> GeometryGroup::FlattenCore(FlattenSink& sink) const noexcept {
    for (const Ref<Geometry>& child : children_) {
        if (!child) continue;
        Result<void> flattened = child->Flatten(sink);
        if (!flattened) return flattened.GetStatus();
    }
    return {};
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
        Result<void> subscribed =
            next->AddChangedHandler(childChangedHandler_);
        if (!subscribed) return;
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

Result<void> CombinedGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    // Boolean combine (Intersect/Xor/Exclude) needs a tessellator such as
    // libtess2; this pass concatenates both operands so Union still renders.
    if (geometry1_) {
        Result<void> flattened = geometry1_->Flatten(sink);
        if (!flattened) return flattened.GetStatus();
    }
    if (GetGeometryCombineMode() == GeometryCombineMode::Exclude) {
        return {};
    }
    if (geometry2_) {
        return geometry2_->Flatten(sink);
    }
    return {};
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

Result<void> DashStyle::SetDashes(Span<const double> value) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    dashes_.Clear();
    for (std::uint32_t index = 0U; index < value.Size(); ++index) {
        const double dash = value[index];
        if (!std::isfinite(dash) || dash < 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "DashStyle dashes must be finite and non-negative");
        }
        Result<void> added = dashes_.PushBack(dash);
        if (!added) return added.GetStatus();
    }
    WritePostscript();
    return {};
}

void DashStyle::SetOffset(double value) noexcept {
    if (!std::isfinite(value)) return;
    Result<void> writable = WritePreamble();
    if (!writable) return;
    offset_ = value;
    WritePostscript();
}

} // namespace Aero::Media
