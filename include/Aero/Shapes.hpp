#pragma once

#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/FrameworkElement.hpp>

namespace Aero::Shapes {

using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;
using ::Aero::Media::Geometry;
using ::Aero::Media::Stretch;

enum class PenLineJoin : std::uint8_t { Miter = 0U, Bevel, Round };
enum class PenLineCap : std::uint8_t { Flat = 0U, Square, Round, Triangle };

class AERO_API Shape : public FrameworkElement {
    AERO_DECLARE_TYPE(Shape, FrameworkElement)
public:
    Base::Ref<Brush> GetFill() const noexcept;
    Base::Ref<Brush> GetStroke() const noexcept;
    double GetStrokeThickness() const noexcept;
    Stretch GetStretch() const noexcept;

    void SetFill(
        Base::Ref<Brush> value) noexcept;
    void SetStroke(
        Base::Ref<Brush> value) noexcept;
    void SetStrokeThickness(double value) noexcept;
    void SetStretch(Stretch value) noexcept;

    inline static constexpr DependencyProperty<Base::Ref<Brush>> FillProperty{"Fill"};
    inline static constexpr DependencyProperty<Base::Ref<Brush>> StrokeProperty{"Stroke"};
    inline static constexpr DependencyProperty<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};

protected:
    explicit Shape(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType) {}
    ~Shape() override = default;
};

class AERO_API Rectangle : public Shape {
    AERO_DECLARE_TYPE(Rectangle, Shape)
public:
    Rectangle() noexcept : Shape(StaticTypeId()) {}
    ~Rectangle() override = default;

    double GetRadiusX() const noexcept;
    double GetRadiusY() const noexcept;
    void SetRadiusX(double value) noexcept;
    void SetRadiusY(double value) noexcept;

    inline static constexpr DependencyProperty<double> RadiusXProperty{"RadiusX"};
    inline static constexpr DependencyProperty<double> RadiusYProperty{"RadiusY"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnRender(
        DrawingContext& context) noexcept override;
};

class AERO_API Ellipse : public Shape {
    AERO_DECLARE_TYPE(Ellipse, Shape)
public:
    Ellipse() noexcept : Shape(StaticTypeId()) {}
    ~Ellipse() override = default;

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnRender(
        DrawingContext& context) noexcept override;
};

// WPF-shaped vector path. The textual Data value accepts the deterministic
// SVG/WPF subset used by the Gallery vector assets.
class AERO_API Path : public FrameworkElement {
    AERO_DECLARE_TYPE(Path, FrameworkElement)
public:
    Path() noexcept;
    ~Path() override;

    Base::Ref<Geometry> GetData() const noexcept;
    Base::Ref<Brush> GetFill() const noexcept;
    Base::Ref<Brush> GetStroke() const noexcept;
    double GetStrokeThickness() const noexcept;
    PenLineJoin GetStrokeLineJoin() const noexcept;
    PenLineCap GetStrokeStartLineCap() const noexcept;
    PenLineCap GetStrokeEndLineCap() const noexcept;
    double GetTrimStart() const noexcept;
    double GetTrimEnd() const noexcept;
    Stretch GetStretch() const noexcept;
    Rect GetGeometryBounds() const noexcept { return geometryBounds_; }

    void SetData(Base::Ref<Geometry> value) noexcept;
    void SetFill(Base::Ref<Brush> value) noexcept;
    void SetStroke(Base::Ref<Brush> value) noexcept;
    void SetStrokeThickness(double value) noexcept;
    void SetStrokeLineJoin(PenLineJoin value) noexcept;
    void SetStrokeStartLineCap(PenLineCap value) noexcept;
    void SetStrokeEndLineCap(PenLineCap value) noexcept;
    void SetTrimStart(double value) noexcept;
    void SetTrimEnd(double value) noexcept;
    void SetStretch(Stretch value) noexcept;

    inline static constexpr DependencyProperty<Base::Ref<Geometry>> DataProperty{"Data"};
    inline static constexpr DependencyProperty<Base::Ref<Brush>> FillProperty{"Fill"};
    inline static constexpr DependencyProperty<Base::Ref<Brush>> StrokeProperty{"Stroke"};
    inline static constexpr DependencyProperty<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr DependencyProperty<PenLineJoin> StrokeLineJoinProperty{"StrokeLineJoin"};
    inline static constexpr DependencyProperty<PenLineCap> StrokeStartLineCapProperty{"StrokeStartLineCap"};
    inline static constexpr DependencyProperty<PenLineCap> StrokeEndLineCapProperty{"StrokeEndLineCap"};
    inline static constexpr AttachedProperty<double> TrimStartProperty{"TrimStart"};
    inline static constexpr AttachedProperty<double> TrimEndProperty{"TrimEnd"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(DrawingContext& context) noexcept override;

private:
    friend struct ::Aero::Visual::Impl;

    Base::Result<void> EnsureGeometry() noexcept;
    Base::Result<void> EnsureMesh() noexcept;
    void ResetGeometry() noexcept;
    void AttachMeshResources(void* services, bool force = false) noexcept;
    void ReleaseMesh() noexcept;

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

AERO_DECLARE_TYPE_ENUM(Aero::Shapes::PenLineJoin)

AERO_DECLARE_TYPE_ENUM(Aero::Shapes::PenLineCap)
