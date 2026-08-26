#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Shapes/Shape.hpp>

#include <cstdint>

namespace Aero::Shapes {

using ::Aero::Media::Geometry;
using ::Aero::Media::Brush;

enum class PenLineJoin : std::uint8_t { Miter = 0U, Bevel, Round };
enum class PenLineCap : std::uint8_t { Flat = 0U, Square, Round, Triangle };

// WPF-shaped vector path. The textual Data value accepts the deterministic
// SVG/WPF subset used by the Gallery vector assets.
class AERO_GUI_API Path : public Shape {
    AERO_DECLARE_TYPE(Path, Shape)
public:
    Path() noexcept;
    ~Path() override;

    Ref<Geometry> GetData() const noexcept;
    PenLineJoin GetStrokeLineJoin() const noexcept;
    PenLineCap GetStrokeStartLineCap() const noexcept;
    PenLineCap GetStrokeEndLineCap() const noexcept;
    double GetTrimStart() const noexcept;
    double GetTrimEnd() const noexcept;
    Rect GetGeometryBounds() const noexcept { return geometryBounds_; }

    void SetData(Ref<Geometry> value) noexcept;
    void SetStrokeLineJoin(PenLineJoin value) noexcept;
    void SetStrokeStartLineCap(PenLineCap value) noexcept;
    void SetStrokeEndLineCap(PenLineCap value) noexcept;
    void SetTrimStart(double value) noexcept;
    void SetTrimEnd(double value) noexcept;

    inline static constexpr DependencyProperty<Ref<Geometry>> DataProperty{"Data"};
    inline static constexpr DependencyProperty<PenLineJoin> StrokeLineJoinProperty{"StrokeLineJoin"};
    inline static constexpr DependencyProperty<PenLineCap> StrokeStartLineCapProperty{"StrokeStartLineCap"};
    inline static constexpr DependencyProperty<PenLineCap> StrokeEndLineCapProperty{"StrokeEndLineCap"};
    inline static constexpr AttachedProperty<double> TrimStartProperty{"TrimStart"};
    inline static constexpr AttachedProperty<double> TrimEndProperty{"TrimEnd"};

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif

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

AERO_DECLARE_TYPE_ENUM(Aero::Shapes::PenLineJoin)
AERO_DECLARE_TYPE_ENUM(Aero::Shapes::PenLineCap)
