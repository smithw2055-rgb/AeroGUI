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
    bool HitTestCore() const noexcept override {
        return GetFill() != nullptr || GetStroke() != nullptr;
    }

    AERO_DEPENDENCY_PROPERTY2(Ref<Brush>, Fill);
    AERO_DEPENDENCY_PROPERTY2(Ref<Brush>, Stroke);
    AERO_DEPENDENCY_PROPERTY2(Ref<Media::Pen>, Pen);
    AERO_DEPENDENCY_PROPERTY2(double, StrokeThickness);
    AERO_DEPENDENCY_PROPERTY2(Stretch, Stretch);

protected:
    explicit Shape(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Shape() override = default;

    // Replaces OnShapePenChanged (Fill/Stroke had a no-op and were dropped).
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnPropertyInvalidated(Meta::PropertyInvalidationFlags flags) noexcept override;
};

class AERO_GUI_API Rectangle : public Shape {
    AERO_DECLARE_TYPE(Rectangle, Shape)

public:
    Rectangle() noexcept : Shape(StaticTypeId()) {}
    ~Rectangle() override = default;

    AERO_DEPENDENCY_PROPERTY2(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY2(double, RadiusY);

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

    AERO_DEPENDENCY_PROPERTY2(double, X1);
    AERO_DEPENDENCY_PROPERTY2(double, Y1);
    AERO_DEPENDENCY_PROPERTY2(double, X2);
    AERO_DEPENDENCY_PROPERTY2(double, Y2);

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

    AERO_DEPENDENCY_PROPERTY2(FillRule, FillRule);

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

    bool EnsureGeometry() noexcept;
    bool EnsureMesh() noexcept;
    void ResetGeometry() noexcept;
    void AttachMeshResources(void* services, bool force = false) noexcept;
    void ReleaseMesh() noexcept;

    double GetTrimStart() const noexcept;
    void SetTrimStart(double value) noexcept;
    double GetTrimEnd() const noexcept;
    void SetTrimEnd(double value) noexcept;
    StringView GetStrokeDashArray() const noexcept;
    void SetStrokeDashArray(StringView value) noexcept;
    Rect GetGeometryBounds() const noexcept { return geometryBounds_; }

    AERO_DEPENDENCY_PROPERTY2(Ref<Geometry>, Data);
    AERO_DEPENDENCY_PROPERTY2(FillRule, FillRule);
    AERO_DEPENDENCY_PROPERTY2(PenLineJoin, StrokeLineJoin);
    AERO_DEPENDENCY_PROPERTY2(PenLineCap, StrokeStartLineCap);
    AERO_DEPENDENCY_PROPERTY2(PenLineCap, StrokeEndLineCap);
    AERO_ATTACHED_PROPERTY(double, TrimStart);
    AERO_ATTACHED_PROPERTY(double, TrimEnd);
    AERO_DEPENDENCY_PROPERTY(String, StrokeDashArray);
    AERO_DEPENDENCY_PROPERTY2(double, StrokeDashOffset);
    AERO_DEPENDENCY_PROPERTY2(Ref<Media::DashStyle>, DashStyle);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
    // Replaces the OnPath* metadata delegates (all funnel to geometry reset).
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnPropertyInvalidated(Meta::PropertyInvalidationFlags flags) noexcept override;

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
    double lastRenderWidth_ = 0.0;
    double lastRenderHeight_ = 0.0;
    bool geometryDirty_ = true;
};

} // namespace Aero::Shapes

AERO_DECLARE_TYPE_ENUM(Aero::Shapes::FillRule)
