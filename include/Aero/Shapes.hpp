#pragma once

// Shape family: Shape plus the concrete shapes.
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Geometries.hpp>

#include <cstdint>

namespace Aero::Shapes {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::PropertyValue;
using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;
using ::Aero::Media::Stretch;

class AERO_GUI_API Shape : public FrameworkElement {
    AERO_DECLARE_TYPE(Shape, FrameworkElement)

public:
    Ref<Brush> GetFill() const noexcept;
    void SetFill(Ref<Brush> value) noexcept;
    Ref<Brush> GetStroke() const noexcept;
    void SetStroke(Ref<Brush> value) noexcept;
    Ref<Media::Pen> GetPen() const noexcept;
    void SetPen(Ref<Media::Pen> value) noexcept;
    double GetStrokeThickness() const noexcept;
    void SetStrokeThickness(double value) noexcept;
    Stretch GetStretch() const noexcept;
    void SetStretch(Stretch value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<Brush>, Fill);
    AERO_DEPENDENCY_PROPERTY(Ref<Brush>, Stroke);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Pen>, Pen);
    AERO_DEPENDENCY_PROPERTY(double, StrokeThickness);
    AERO_DEPENDENCY_PROPERTY(Stretch, Stretch);

protected:
    explicit Shape(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Shape() override = default;

    // Replaces OnShapePenChanged (Fill/Stroke had a no-op and were dropped).
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
};

class AERO_GUI_API Rectangle : public Shape {
    AERO_DECLARE_TYPE(Rectangle, Shape)

public:
    Rectangle() noexcept : Shape(StaticTypeId()) {}
    ~Rectangle() override = default;

    double GetRadiusX() const noexcept;
    void SetRadiusX(double value) noexcept;
    double GetRadiusY() const noexcept;
    void SetRadiusY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY(double, RadiusY);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
};

class AERO_GUI_API Ellipse : public Shape {
    AERO_DECLARE_TYPE(Ellipse, Shape)

public:
    Ellipse() noexcept : Shape(StaticTypeId()) {}
    ~Ellipse() override = default;

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
};

class AERO_GUI_API Line : public Shape {
    AERO_DECLARE_TYPE(Line, Shape)

public:
    Line() noexcept : Shape(StaticTypeId()) {}
    ~Line() override = default;

    double GetX1() const noexcept;
    void SetX1(double value) noexcept;
    double GetY1() const noexcept;
    void SetY1(double value) noexcept;
    double GetX2() const noexcept;
    void SetX2(double value) noexcept;
    double GetY2() const noexcept;
    void SetY2(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, X1);
    AERO_DEPENDENCY_PROPERTY(double, Y1);
    AERO_DEPENDENCY_PROPERTY(double, X2);
    AERO_DEPENDENCY_PROPERTY(double, Y2);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
};

enum class FillRule : std::uint8_t { EvenOdd = 0U, Nonzero };

class AERO_GUI_API Polygon : public Shape {
    AERO_DECLARE_TYPE(Polygon, Shape)

public:
    Polygon() noexcept : Shape(StaticTypeId()) {}
    ~Polygon() override = default;

    void AddPoint(Point point) noexcept;

    FillRule GetFillRule() const noexcept;
    void SetFillRule(FillRule value) noexcept;
    Span<const Point> GetPoints() const noexcept;
    void SetPoints(Span<const Point> points) noexcept;
    void SetPoints(StringView text) noexcept;
    void ClearPoints() noexcept;
    void SetPointsText(Base::String text) noexcept { SetPoints(text.View()); }

    AERO_DEPENDENCY_PROPERTY(FillRule, FillRule);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    Base::Vector<Point> points_;
};

class AERO_GUI_API Polyline : public Shape {
    AERO_DECLARE_TYPE(Polyline, Shape)

public:
    Polyline() noexcept : Shape(StaticTypeId()) {}
    ~Polyline() override = default;

    void AddPoint(Point point) noexcept;

    Span<const Point> GetPoints() const noexcept;
    void SetPoints(Span<const Point> points) noexcept;
    void SetPoints(StringView text) noexcept;
    void ClearPoints() noexcept;
    void SetPointsText(Base::String text) noexcept { SetPoints(text.View()); }

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    Base::Vector<Point> points_;
};

using ::Aero::Media::Geometry;
using ::Aero::Media::PenLineJoin;
using ::Aero::Media::PenLineCap;

// WPF-shaped vector path. The textual Data value accepts the deterministic
// SVG/WPF subset used by the Gallery vector assets.
class AERO_GUI_API Path : public Shape {
    AERO_DECLARE_TYPE(Path, Shape)

public:
    Path() noexcept;
    ~Path() override;

    Result<void> EnsureGeometry() noexcept;
    Result<void> EnsureMesh() noexcept;
    void ResetGeometry() noexcept;
    void AttachMeshResources(void* services, bool force = false) noexcept;
    void ReleaseMesh() noexcept;

    Ref<Geometry> GetData() const noexcept;
    void SetData(Ref<Geometry> value) noexcept;
    FillRule GetFillRule() const noexcept;
    void SetFillRule(FillRule value) noexcept;
    PenLineJoin GetStrokeLineJoin() const noexcept;
    void SetStrokeLineJoin(PenLineJoin value) noexcept;
    PenLineCap GetStrokeStartLineCap() const noexcept;
    void SetStrokeStartLineCap(PenLineCap value) noexcept;
    PenLineCap GetStrokeEndLineCap() const noexcept;
    void SetStrokeEndLineCap(PenLineCap value) noexcept;
    double GetTrimStart() const noexcept;
    void SetTrimStart(double value) noexcept;
    double GetTrimEnd() const noexcept;
    void SetTrimEnd(double value) noexcept;
    StringView GetStrokeDashArray() const noexcept;
    void SetStrokeDashArray(StringView value) noexcept;
    double GetStrokeDashOffset() const noexcept;
    void SetStrokeDashOffset(double value) noexcept;
    Ref<Media::DashStyle> GetDashStyle() const noexcept;
    void SetDashStyle(Ref<Media::DashStyle> value) noexcept;
    Rect GetGeometryBounds() const noexcept { return geometryBounds_; }

    AERO_DEPENDENCY_PROPERTY(Ref<Geometry>, Data);
    AERO_DEPENDENCY_PROPERTY(FillRule, FillRule);
    AERO_DEPENDENCY_PROPERTY(PenLineJoin, StrokeLineJoin);
    AERO_DEPENDENCY_PROPERTY(PenLineCap, StrokeStartLineCap);
    AERO_DEPENDENCY_PROPERTY(PenLineCap, StrokeEndLineCap);
    AERO_ATTACHED_PROPERTY(double, TrimStart);
    AERO_ATTACHED_PROPERTY(double, TrimEnd);
    AERO_DEPENDENCY_PROPERTY(String, StrokeDashArray);
    AERO_DEPENDENCY_PROPERTY(double, StrokeDashOffset);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::DashStyle>, DashStyle);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
    // Replaces the OnPath* metadata delegates (all funnel to geometry reset).
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    Base::Vector<Point> geometryVertices_;
    Base::Vector<std::uint32_t> geometryIndices_;
    Base::Vector<Point> pathPoints_;
    Base::Vector<std::uint32_t> pathContourStarts_;
    Base::Vector<std::uint32_t> pathContourCounts_;
    Base::Vector<std::uint8_t> pathContourClosed_;
    Base::Vector<Point> strokeVertices_;
    Base::Vector<std::uint32_t> strokeIndices_;
    Rect geometryBounds_;
    std::uint64_t meshServiceGeneration_ = 0U;
    std::uint64_t mesh_ = 0U;
    std::uint64_t strokeMesh_ = 0U;
    bool geometryDirty_ = true;
};

} // namespace Aero::Shapes

AERO_DECLARE_TYPE_ENUM(Aero::Shapes::FillRule)
