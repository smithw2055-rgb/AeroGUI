#include <Aero/Media/Geometries.hpp>
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
            transform_->Freeze();
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
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
    if (!value) { AERO_ASSERT(false); return; }
    segments_.Add(std::move(value));
    WritePostscript();
}

void PathGeometry::AddFigure(
    Base::Ref<PathFigure> value) noexcept {
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
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
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
    points_.Clear();
    points_.Append(points);
    WritePostscript();
}
void PolyLineSegment::AddPoint(Point point) noexcept {
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
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
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
    points_.Clear();
    points_.Append(points);
    WritePostscript();
}
void PolyBezierSegment::AddPoint(Point point) noexcept {
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
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
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
    points_.Clear();
    points_.Append(points);
    WritePostscript();
}
void PolyQuadraticBezierSegment::AddPoint(Point point) noexcept {
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
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
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
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
        child->Freeze();
        return true;
    };
    if (!freezeChild(geometry1_.Get()) || !freezeChild(geometry2_.Get())) {
        return false;
    }
    return Geometry::FreezeCore(isChecking);
}

} // namespace Aero::Media

#include <Aero/Media/Pen.hpp>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"

#include <cctype>
#include <cstdlib>

namespace Aero::Media {

void DashStyle::SetDashes(Span<const double> value) noexcept {
    if (!WritePreamble()) { AERO_ASSERT(false); return; }
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
    if (!WritePreamble()) return;
    offset_ = value;
    WritePostscript();
}

} // namespace Aero::Media

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
using Media::Geometry;
using Media::GeometryGroup;
using Media::PathFigure;
using Media::PathGeometry;
using Media::PathSegment;
using Media::StreamGeometry;
namespace {

void AddPathFigureSegment(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<PathSegment> retained =
        Base::Ref<PathSegment>::TryFromBorrowed(
            static_cast<PathSegment&>(*value));
    if (retained) {
        static_cast<PathFigure&>(owner)
            .AddSegment(std::move(retained));
    }
}

void ClearPathFigureSegments(
    Base::Object& owner,
    void*) noexcept {
    static_cast<PathFigure&>(owner).ClearSegments();
}

void AddPathGeometryFigure(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<PathFigure> retained =
        Base::Ref<PathFigure>::TryFromBorrowed(
            static_cast<PathFigure&>(*value));
    if (retained) {
        static_cast<PathGeometry&>(owner)
            .AddFigure(std::move(retained));
    }
}

void ClearPathGeometryFigures(
    Base::Object& owner,
    void*) noexcept {
    static_cast<PathGeometry&>(owner).ClearFigures();
}

void AddGeometryGroupChild(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Geometry> retained =
        Base::Ref<Geometry>::TryFromBorrowed(
            static_cast<Geometry&>(*value));
    if (retained) {
        static_cast<GeometryGroup&>(owner)
            .Add(std::move(retained));
    }
}

void ClearGeometryGroupChildren(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GeometryGroup&>(owner).Clear();
}

Base::Result<Value> ConvertGeometryText(
    TypeId targetType,
    Base::StringView text,
    void*) noexcept {
    const bool streamGeometry =
        targetType == StreamGeometry::StaticTypeId();
    if (targetType != Geometry::StaticTypeId() &&
        !streamGeometry) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Geometry text conversion received an invalid target");
    }
    Base::Ref<StreamGeometry> made =
        Base::MakeRef<StreamGeometry>();
    if (!made) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "StreamGeometry allocation failed");
    }
    made->SetData(text);
    return Value::FromObject(
        targetType,
        Base::Ref<Base::Object>(std::move(made)));
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero::Media {

AERO_DESCRIBE(Geometry) {
    using namespace Aero::Meta;
    Register<Geometry>(context)
            .Property("Transform", &Geometry::GetTransform, &Geometry::SetTransform, PropertyFlags::Structural)
            .Content(MakeMemberId(Geometry::StaticTypeId(), MemberKind::Property, "Transform"))
            .TextConverter(&::Aero::MetadataSupport::ConvertGeometryText)
            .Factory();
}

AERO_DESCRIBE(PathSegment) {
    using namespace Aero::Meta;
    Register<PathSegment>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(LineSegment) {
    using namespace Aero::Meta;
    Register<LineSegment>(context)
            .Property(LineSegment::PointProperty, Point{}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(PathFigure) {
    using namespace Aero::Meta;
    Register<PathFigure>(context)
            .Property(PathFigure::StartPointProperty, Point{}, AffectsRender)
            .Property(PathFigure::IsClosedProperty, false, AffectsRender)
            .Content<PathSegment>("Segments", ContentKind::Collection, &::Aero::MetadataSupport::AddPathFigureSegment, &::Aero::MetadataSupport::ClearPathFigureSegments)
            .Factory();
}

AERO_DESCRIBE(PathGeometry) {
    using namespace Aero::Meta;
    Register<PathGeometry>(context)
            .Content<PathFigure>("Figures", ContentKind::Collection, &::Aero::MetadataSupport::AddPathGeometryFigure, &::Aero::MetadataSupport::ClearPathGeometryFigures)
            .Factory();
}

AERO_DESCRIBE(BezierSegment) {
    using namespace Aero::Meta;
    Register<BezierSegment>(context)
            .Property(BezierSegment::Point1Property, Point{}, AffectsRender)
            .Property(BezierSegment::Point2Property, Point{}, AffectsRender)
            .Property(BezierSegment::Point3Property, Point{}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(QuadraticBezierSegment) {
    using namespace Aero::Meta;
    Register<QuadraticBezierSegment>(context)
            .Property(QuadraticBezierSegment::Point1Property, Point{}, AffectsRender)
            .Property(QuadraticBezierSegment::Point2Property, Point{}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(ArcSegment) {
    using namespace Aero::Meta;
    Register<ArcSegment>(context)
            .Property(ArcSegment::PointProperty, Point{}, AffectsRender)
            .Property(ArcSegment::SizeProperty, Size{}, AffectsRender)
            .Property(ArcSegment::RotationAngleProperty, 0.0, AffectsRender)
            .Property(ArcSegment::IsLargeArcProperty, false, AffectsRender)
            .Property(ArcSegment::SweepDirectionProperty, SweepDirection::Counterclockwise, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(PolyLineSegment) {
    using namespace Aero::Meta;
    Register<PolyLineSegment>(context)
            .Property<Base::String, &PolyLineSegment::SetPointsText>("Points", PropertyFlags::None)
            .Factory();
}

AERO_DESCRIBE(PolyBezierSegment) {
    using namespace Aero::Meta;
    Register<PolyBezierSegment>(context)
            .Property<Base::String, &PolyBezierSegment::SetPointsText>("Points", PropertyFlags::None)
            .Factory();
}

AERO_DESCRIBE(PolyQuadraticBezierSegment) {
    using namespace Aero::Meta;
    Register<PolyQuadraticBezierSegment>(context)
            .Property<Base::String, &PolyQuadraticBezierSegment::SetPointsText>("Points", PropertyFlags::None)
            .Factory();
}

AERO_DESCRIBE(LineGeometry) {
    using namespace Aero::Meta;
    Register<LineGeometry>(context)
            .Property(LineGeometry::StartPointProperty, Point{}, AffectsRender)
            .Property(LineGeometry::EndPointProperty, Point{}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(RectangleGeometry) {
    using namespace Aero::Meta;
    Register<RectangleGeometry>(context)
            .Property(RectangleGeometry::RectProperty, Rect{}, AffectsRender)
            .Property(RectangleGeometry::RadiusXProperty, 0.0, AffectsRender)
            .Property(RectangleGeometry::RadiusYProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(EllipseGeometry) {
    using namespace Aero::Meta;
    Register<EllipseGeometry>(context)
            .Property(EllipseGeometry::CenterProperty, Point{}, AffectsRender)
            .Property(EllipseGeometry::RadiusXProperty, 0.0, AffectsRender)
            .Property(EllipseGeometry::RadiusYProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(GeometryGroup) {
    using namespace Aero::Meta;
    Register<GeometryGroup>(context)
            .Content<Geometry>("Children", ContentKind::Collection, &::Aero::MetadataSupport::AddGeometryGroupChild, &::Aero::MetadataSupport::ClearGeometryGroupChildren)
            .Factory();
}

AERO_DESCRIBE(CombinedGeometry) {
    using namespace Aero::Meta;
    Register<CombinedGeometry>(context)
            .Property("Geometry1", &CombinedGeometry::GetGeometry1, &CombinedGeometry::SetGeometry1, PropertyFlags::Structural)
            .Property("Geometry2", &CombinedGeometry::GetGeometry2, &CombinedGeometry::SetGeometry2, PropertyFlags::Structural)
            .Property(CombinedGeometry::GeometryCombineModeProperty, GeometryCombineMode::Union, AffectsRender)
            .Factory();
}

} // namespace Aero::Media



// ===== Media POD value types (Thickness/Color/Point/…) — who-defines-registers =====
namespace Aero::MetadataSupport {
using namespace ::Aero::Media;
using ::Aero::Thickness;
using ::Aero::CornerRadius;
namespace {

Base::Result<Thickness> ConvertThickness(
    Base::StringView text) noexcept {
    return ::Aero::Base::ValueConversion::ConvertThickness(text);
}

Base::Result<CornerRadius> ConvertCornerRadius(
    Base::StringView text) noexcept {
    return ::Aero::Base::ValueConversion::ConvertCornerRadius(text);
}

Base::Result<Point> ConvertPoint(
    Base::StringView input) noexcept {
    return ::Aero::Base::ValueConversion::ConvertPoint(input);
}

Base::Result<Base::Size> ConvertSize(
    Base::StringView input) noexcept {
    Base::Result<Point> parsed = ConvertPoint(input);
    if (!parsed) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Size requires two finite values");
    }
    return Base::Size{parsed.Value().x, parsed.Value().y};
}

Base::Result<Rect> ConvertRect(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned =
        text.Assign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[4]{};
    for (std::uint32_t index = 0U;
         index < 4U; ++index) {
        while (*cursor == ' ' ||
               *cursor == ',') {
            ++cursor;
        }
        char* end = nullptr;
        values[index] =
            std::strtod(cursor, &end);
        if (end == cursor ||
            !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Rect requires four finite values");
        }
        cursor = end;
    }
    while (std::isspace(
        static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor != '\0' ||
        values[2] < 0.0 ||
        values[3] < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Rect requires x,y,width,height with non-negative size");
    }
    return Rect{
        values[0], values[1],
        values[2], values[3]};
}

Base::Result<Base::Transform2D> ConvertMatrix(
    Base::StringView input) noexcept {
    return ::Aero::Base::ValueConversion::ConvertMatrix(input);
}

Base::Result<Base::Transform3> ConvertTransform3(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[12]{};
    for (std::uint32_t index = 0U; index < 12U; ++index) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        char* end = nullptr;
        values[index] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Transform3 requires twelve finite values");
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Transform3 contains trailing text");
    }
    return Base::Transform3{
        values[0], values[1], values[2],
        values[3], values[4], values[5],
        values[6], values[7], values[8],
        values[9], values[10], values[11]};
}

Base::Result<Color> ConvertColor(
    Base::StringView text) noexcept {
    return ::Aero::Base::ValueConversion::ConvertColor(text);
}


bool EqualThickness(const void* left, const void* right, void*) noexcept {
    const Thickness& a = *static_cast<const Thickness*>(left);
    const Thickness& b = *static_cast<const Thickness*>(right);
    return a.left == b.left && a.top == b.top &&
        a.right == b.right && a.bottom == b.bottom;
}

bool EqualColor(const void* left, const void* right, void*) noexcept {
    const Color& a = *static_cast<const Color*>(left);
    const Color& b = *static_cast<const Color*>(right);
    return a.red == b.red && a.green == b.green &&
        a.blue == b.blue && a.alpha == b.alpha;
}

bool EqualCornerRadius(
    const void* left,
    const void* right,
    void*) noexcept {
    const CornerRadius& a =
        *static_cast<const CornerRadius*>(left);
    const CornerRadius& b =
        *static_cast<const CornerRadius*>(right);
    return a.topLeft == b.topLeft &&
        a.topRight == b.topRight &&
        a.bottomRight == b.bottomRight &&
        a.bottomLeft == b.bottomLeft;
}

} // namespace
} // namespace Aero::MetadataSupport


namespace Aero {

Base::Result<void> PopulateMediaValueTypes(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace ::Aero::Meta;
    using namespace ::Aero::Media;
    Register<Thickness>(context)
        .Field<&Thickness::left>("Left")
        .Field<&Thickness::top>("Top")
        .Field<&Thickness::right>("Right")
        .Field<&Thickness::bottom>("Bottom")
        .ValueSemantics({sizeof(Thickness), alignof(Thickness), nullptr, nullptr, &::Aero::MetadataSupport::EqualThickness, nullptr, true})
        .TextConverter<&::Aero::MetadataSupport::ConvertThickness>();
    Register<CornerRadius>(context)
        .Field<&CornerRadius::topLeft>("TopLeft")
        .Field<&CornerRadius::topRight>("TopRight")
        .Field<&CornerRadius::bottomRight>("BottomRight")
        .Field<&CornerRadius::bottomLeft>("BottomLeft")
        .ValueSemantics({ sizeof(CornerRadius), alignof(CornerRadius), nullptr, nullptr, &::Aero::MetadataSupport::EqualCornerRadius, nullptr, true})
        .TextConverter<&::Aero::MetadataSupport::ConvertCornerRadius>();
    Register<Color>(context)
        .Field<&Color::red>("Red")
        .Field<&Color::green>("Green")
        .Field<&Color::blue>("Blue")
        .Field<&Color::alpha>("Alpha")
        .ValueSemantics({sizeof(Color), alignof(Color), nullptr, nullptr, &::Aero::MetadataSupport::EqualColor, nullptr, true})
        .TextConverter<&::Aero::MetadataSupport::ConvertColor>();
    Register<Point>(context)
        .Field<&Point::x>("X")
        .Field<&Point::y>("Y")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertPoint>();
    Register<Rect>(context)
        .Field<&Rect::x>("X")
        .Field<&Rect::y>("Y")
        .Field<&Rect::width>("Width")
        .Field<&Rect::height>("Height")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertRect>();
    Register<Base::Size>(context)
        .Field<&Base::Size::width>("Width")
        .Field<&Base::Size::height>("Height")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertSize>();
    Register<Base::Transform2D>(context)
        .Field<&Base::Transform2D::m11>("M11")
        .Field<&Base::Transform2D::m12>("M12")
        .Field<&Base::Transform2D::m21>("M21")
        .Field<&Base::Transform2D::m22>("M22")
        .Field<&Base::Transform2D::dx>("OffsetX")
        .Field<&Base::Transform2D::dy>("OffsetY")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertMatrix>();
    Register<Base::Transform3>(context)
        .Field<&Base::Transform3::m11>("M11")
        .Field<&Base::Transform3::m12>("M12")
        .Field<&Base::Transform3::m13>("M13")
        .Field<&Base::Transform3::m21>("M21")
        .Field<&Base::Transform3::m22>("M22")
        .Field<&Base::Transform3::m23>("M23")
        .Field<&Base::Transform3::m31>("M31")
        .Field<&Base::Transform3::m32>("M32")
        .Field<&Base::Transform3::m33>("M33")
        .Field<&Base::Transform3::dx>("OffsetX")
        .Field<&Base::Transform3::dy>("OffsetY")
        .Field<&Base::Transform3::dz>("OffsetZ")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertTransform3>();
    return {};
}

} // namespace Aero
