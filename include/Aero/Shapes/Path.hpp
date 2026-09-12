#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/DashStyle.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Shapes/Polygon.hpp>
#include <Aero/Shapes/Shape.hpp>

#include <cstdint>

namespace Aero { class AeroGuiInternal; }
namespace Aero::Shapes {

using ::Aero::Media::Geometry;
using ::Aero::Media::Brush;
using ::Aero::Media::PenLineJoin;
using ::Aero::Media::PenLineCap;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyHandle;

// WPF-shaped vector path. The textual Data value accepts the deterministic
// SVG/WPF subset used by the Gallery vector assets.
class AERO_GUI_API Path : public Shape {
    AERO_DECLARE_TYPE(Path, Shape)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Path() noexcept;
    ~Path() override;

    Ref<Geometry> GetData() const noexcept;
    FillRule GetFillRule() const noexcept;
    PenLineJoin GetStrokeLineJoin() const noexcept;
    PenLineCap GetStrokeStartLineCap() const noexcept;
    PenLineCap GetStrokeEndLineCap() const noexcept;
    double GetTrimStart() const noexcept;
    double GetTrimEnd() const noexcept;
    StringView GetStrokeDashArray() const noexcept;
    double GetStrokeDashOffset() const noexcept;
    Ref<Media::DashStyle> GetDashStyle() const noexcept;
    Rect GetGeometryBounds() const noexcept { return geometryBounds_; }

    void SetData(Ref<Geometry> value) noexcept;
    void SetFillRule(FillRule value) noexcept;
    void SetStrokeLineJoin(PenLineJoin value) noexcept;
    void SetStrokeStartLineCap(PenLineCap value) noexcept;
    void SetStrokeEndLineCap(PenLineCap value) noexcept;
    void SetTrimStart(double value) noexcept;
    void SetTrimEnd(double value) noexcept;
    void SetStrokeDashArray(StringView value) noexcept;
    void SetStrokeDashOffset(double value) noexcept;
    void SetDashStyle(Ref<Media::DashStyle> value) noexcept;

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
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    friend class ::Aero::AeroGuiInternal;
    Result<void> EnsureGeometry() noexcept;
    Result<void> EnsureMesh() noexcept;
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
