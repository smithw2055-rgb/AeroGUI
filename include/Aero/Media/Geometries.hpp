#pragma once

// Geometry family: Geometry, path segments, figures, stream context, and concretes.
#include <Aero/Animatable.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/FreezableCollection.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Media {

using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;

struct FlattenSink {
    virtual ~FlattenSink() = default;
    virtual void AddPoint(Point point) noexcept = 0;
    virtual void BeginFigure(Point start, bool isClosed) noexcept {
        (void)isClosed;
        AddPoint(start);
    }
    virtual void EndFigure(bool isClosed) noexcept { (void)isClosed; }
};

class AERO_GUI_API Geometry : public Animatable {
    AERO_DECLARE_TYPE(Geometry, Animatable)

public:
    Geometry() noexcept : Animatable(StaticTypeId()) {}
    ~Geometry() override;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    virtual Rect GetBounds() const noexcept { return {}; }
    // Applies Geometry.Transform, then FlattenCore. Rendering must Flatten
    // rather than round-trip PathGeometry through ToStreamData.
    void Flatten(FlattenSink& sink) const noexcept;
    Ref<Transform> GetTransform() const noexcept { return transform_; }
    void SetTransform(Ref<Transform> value) noexcept;
    AERO_DEPENDENCY_PROPERTY(Ref<Transform>, Transform);
    std::uint64_t GetRevision() const noexcept { return Revision(); }

protected:
    explicit Geometry(Meta::TypeId runtimeType) noexcept : Animatable(runtimeType) {}
    virtual void FlattenCore(FlattenSink& sink) const noexcept;
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnTransformChanged(Freezable&) noexcept;
    Ref<Transform> transform_;
    FreezableChangedHandler transformChangedHandler_;
};

class AERO_GUI_API PathSegment : public Animatable {
    AERO_DECLARE_TYPE(PathSegment, Animatable)

public:
    virtual void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept {
        (void)sink;
        (void)currentPoint;
    }

protected:
    explicit PathSegment(Meta::TypeId runtimeType) noexcept : Animatable(runtimeType) {}
    ~PathSegment() override = default;
};

class AERO_GUI_API LineSegment : public PathSegment {
    AERO_DECLARE_TYPE(LineSegment, PathSegment)

public:
    LineSegment() noexcept : PathSegment(StaticTypeId()) {}
    AERO_DEPENDENCY_PROPERTY2(Point, Point);
    void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept override;
};

class AERO_GUI_API BezierSegment : public PathSegment {
    AERO_DECLARE_TYPE(BezierSegment, PathSegment)

public:
    BezierSegment() noexcept : PathSegment(StaticTypeId()) {}
    AERO_DEPENDENCY_PROPERTY2(Point, Point1);
    AERO_DEPENDENCY_PROPERTY2(Point, Point2);
    AERO_DEPENDENCY_PROPERTY2(Point, Point3);
    void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept override;
};

class AERO_GUI_API QuadraticBezierSegment : public PathSegment {
    AERO_DECLARE_TYPE(QuadraticBezierSegment, PathSegment)

public:
    QuadraticBezierSegment() noexcept : PathSegment(StaticTypeId()) {}
    AERO_DEPENDENCY_PROPERTY2(Point, Point1);
    AERO_DEPENDENCY_PROPERTY2(Point, Point2);
    void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept override;
};

class AERO_GUI_API PolyLineSegment : public PathSegment {
    AERO_DECLARE_TYPE(PolyLineSegment, PathSegment)

public:
    PolyLineSegment() noexcept : PathSegment(StaticTypeId()) {}
    Span<const Point> GetPoints() const noexcept { return points_.AsSpan(); }
    void SetPoints(Span<const Point> points) noexcept;
    void AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    void SetPoints(StringView text) noexcept;
    void SetPointsText(Base::String text) noexcept { SetPoints(text.View()); }
    void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept override;

private:
    Base::Vector<Point> points_;
};

class AERO_GUI_API PolyBezierSegment : public PathSegment {
    AERO_DECLARE_TYPE(PolyBezierSegment, PathSegment)

public:
    PolyBezierSegment() noexcept : PathSegment(StaticTypeId()) {}
    Span<const Point> GetPoints() const noexcept { return points_.AsSpan(); }
    void SetPoints(Span<const Point> points) noexcept;
    void AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    void SetPoints(StringView text) noexcept;
    void SetPointsText(Base::String text) noexcept { SetPoints(text.View()); }
    void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept override;

private:
    Base::Vector<Point> points_;
};

class AERO_GUI_API PolyQuadraticBezierSegment : public PathSegment {
    AERO_DECLARE_TYPE(PolyQuadraticBezierSegment, PathSegment)

public:
    PolyQuadraticBezierSegment() noexcept : PathSegment(StaticTypeId()) {}
    Span<const Point> GetPoints() const noexcept { return points_.AsSpan(); }
    void SetPoints(Span<const Point> points) noexcept;
    void AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    void SetPoints(StringView text) noexcept;
    void SetPointsText(Base::String text) noexcept { SetPoints(text.View()); }
    void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept override;

private:
    Base::Vector<Point> points_;
};

enum class SweepDirection : std::uint8_t {
    Counterclockwise = 0U,
    Clockwise
};

class AERO_GUI_API ArcSegment : public PathSegment {
    AERO_DECLARE_TYPE(ArcSegment, PathSegment)

public:
    ArcSegment() noexcept : PathSegment(StaticTypeId()) {}
    AERO_DEPENDENCY_PROPERTY2(Point, Point);
    AERO_DEPENDENCY_PROPERTY2(Size, Size);
    AERO_DEPENDENCY_PROPERTY2(double, RotationAngle);
    AERO_DEPENDENCY_PROPERTY2(bool, IsLargeArc);
    AERO_DEPENDENCY_PROPERTY2(SweepDirection, SweepDirection);
    void Flatten(FlattenSink& sink, Point& currentPoint) const noexcept override;
};

class AERO_GUI_API PathFigure : public Animatable {
    AERO_DECLARE_TYPE(PathFigure, Animatable)

public:
    PathFigure() noexcept : Animatable(StaticTypeId()) {}
    void AddSegment(Ref<PathSegment> value) noexcept;
    void ClearSegments() noexcept {
        if (!WritePreamble()) return;
        segments_.Clear();
        WritePostscript();
    }
    Span<const Ref<PathSegment>> GetSegments() const noexcept { return segments_.AsSpan(); }
    AERO_DEPENDENCY_PROPERTY2(Point, StartPoint);
    AERO_DEPENDENCY_PROPERTY2(bool, IsClosed);

private:
    FreezableCollection<PathSegment> segments_;
};

class StreamGeometry;

// Programmatic StreamGeometry writer. Records figure commands without
// allocating PathSegment objects or parsing mini-language text.
class AERO_GUI_API StreamGeometryContext {
public:
    StreamGeometryContext() noexcept = default;
    StreamGeometryContext(const StreamGeometryContext&) = delete;
    StreamGeometryContext& operator=(const StreamGeometryContext&) = delete;
    StreamGeometryContext(StreamGeometryContext&& other) noexcept;
    StreamGeometryContext& operator=(StreamGeometryContext&& other) noexcept;
    ~StreamGeometryContext();

    void BeginFigure(Point startPoint, bool isFilled, bool isClosed) noexcept;
    void LineTo(Point point, bool isStroked, bool isSmoothJoin) noexcept;
    void BezierTo(Point controlPoint1, Point controlPoint2, Point endPoint, bool isStroked,
        bool isSmoothJoin) noexcept;
    void QuadraticBezierTo(Point controlPoint, Point endPoint, bool isStroked, bool isSmoothJoin) noexcept;
    void ArcTo(Point point, Size size, double rotationAngle, bool isLargeArc, SweepDirection sweepDirection,
        bool isStroked,
        bool isSmoothJoin) noexcept;
    void PolyLineTo(const Point* points, std::uint32_t count, bool isStroked, bool isSmoothJoin) noexcept;
    void Close() noexcept;

private:
    friend class StreamGeometry;
    explicit StreamGeometryContext(StreamGeometry* owner) noexcept;
    StreamGeometry* owner_ = nullptr;
    bool closed_ = true;
};

class AERO_GUI_API RectangleGeometry : public Geometry {
    AERO_DECLARE_TYPE(RectangleGeometry, Geometry)

public:
    RectangleGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Rect GetBounds() const noexcept override { return GetRect(); }
    AERO_DEPENDENCY_PROPERTY2(Rect, Rect);
    AERO_DEPENDENCY_PROPERTY2(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY2(double, RadiusY);

protected:
    void FlattenCore(FlattenSink& sink) const noexcept override;
};

class AERO_GUI_API EllipseGeometry : public Geometry {
    AERO_DECLARE_TYPE(EllipseGeometry, Geometry)

public:
    EllipseGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Rect GetBounds() const noexcept override;
    AERO_DEPENDENCY_PROPERTY2(Point, Center);
    AERO_DEPENDENCY_PROPERTY2(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY2(double, RadiusY);

protected:
    void FlattenCore(FlattenSink& sink) const noexcept override;
};

class AERO_GUI_API LineGeometry : public Geometry {
    AERO_DECLARE_TYPE(LineGeometry, Geometry)

public:
    LineGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Rect GetBounds() const noexcept override;
    AERO_DEPENDENCY_PROPERTY2(Point, StartPoint);
    AERO_DEPENDENCY_PROPERTY2(Point, EndPoint);

protected:
    void FlattenCore(FlattenSink& sink) const noexcept override;
};

class AERO_GUI_API GeometryGroup : public Geometry {
    AERO_DECLARE_TYPE(GeometryGroup, Geometry)

public:
    GeometryGroup() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    void Add(Ref<Geometry> value) noexcept;
    void Clear() noexcept {
        if (!WritePreamble()) return;
        children_.Clear();
        WritePostscript();
    }
    Span<const Ref<Geometry>> GetChildren() const noexcept { return children_.AsSpan(); }

protected:
    void FlattenCore(FlattenSink& sink) const noexcept override;

private:
    FreezableCollection<Geometry> children_;
};

enum class GeometryCombineMode : std::uint8_t {
    Union = 0U,
    Intersect,
    Xor,
    Exclude
};

class AERO_GUI_API CombinedGeometry : public Geometry {
    AERO_DECLARE_TYPE(CombinedGeometry, Geometry)

public:
    CombinedGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Ref<Geometry> GetGeometry1() const noexcept { return geometry1_; }
    Ref<Geometry> GetGeometry2() const noexcept { return geometry2_; }
    void SetGeometry1(Ref<Geometry> value) noexcept;
    void SetGeometry2(Ref<Geometry> value) noexcept;
    AERO_DEPENDENCY_PROPERTY2(GeometryCombineMode, GeometryCombineMode);

protected:
    void FlattenCore(FlattenSink& sink) const noexcept override;
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnChildChanged(Freezable&) noexcept;
    void AttachChild(Ref<Geometry>& slot, Ref<Geometry> value) noexcept;
    Ref<Geometry> geometry1_;
    Ref<Geometry> geometry2_;
    FreezableChangedHandler childChangedHandler_;
};

class AERO_GUI_API PathGeometry : public Geometry {
    AERO_DECLARE_TYPE(PathGeometry, Geometry)

public:
    PathGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    void AddFigure(Ref<PathFigure> value) noexcept;
    void ClearFigures() noexcept {
        if (!WritePreamble()) return;
        figures_.Clear();
        WritePostscript();
    }
    Span<const Ref<PathFigure>> GetFigures() const noexcept { return figures_.AsSpan(); }
    Result<String> ToStreamData() const noexcept;

protected:
    void FlattenCore(FlattenSink& sink) const noexcept override;

private:
    FreezableCollection<PathFigure> figures_;
};

class AERO_GUI_API StreamGeometry : public Geometry {
    AERO_DECLARE_TYPE(StreamGeometry, Geometry)

public:
    StreamGeometry() noexcept : Geometry(StaticTypeId()) {}
    ~StreamGeometry() override = default;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    StringView GetData() const noexcept { return data_.View(); }
    void SetData(StringView value) noexcept;
    Rect GetBounds() const noexcept override;
    void SetBounds(Rect value) noexcept {
        if (!WritePreamble() || (bounds_.x == value.x && bounds_.y == value.y &&
             bounds_.width == value.width && bounds_.height == value.height)) return;
        bounds_ = value;
        boundsValid_ = true;
        WritePostscript();
    }
    StreamGeometryContext Open() noexcept;

protected:
    void FlattenCore(FlattenSink& sink) const noexcept override;

private:
    friend class StreamGeometryContext;

    enum class CommandKind : std::uint8_t {
        BeginFigure = 0U,
        LineTo,
        BezierTo,
        QuadraticBezierTo,
        ArcTo,
        Close
    };

    struct Command {
        CommandKind kind = CommandKind::LineTo;
        bool filled = true;
        bool closed = false;
        bool isStroked = true;
        bool isSmoothJoin = false;
        bool largeArc = false;
        bool sweepClockwise = false;
        Point p0{};
        Point p1{};
        Point p2{};
        Size size{};
        double rotation = 0.0;
    };

    void AppendCommand(const Command& command) noexcept;
    void ReplayCommands(FlattenSink& sink) const noexcept;
    void InvalidateBounds() noexcept {
        bounds_ = {};
        boundsValid_ = false;
    }

    String data_;
    Base::Vector<Command> commands_;
    mutable Rect bounds_{};
    mutable bool boundsValid_ = false;
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::GeometryCombineMode)
AERO_DECLARE_TYPE_ENUM(Aero::Media::SweepDirection)
