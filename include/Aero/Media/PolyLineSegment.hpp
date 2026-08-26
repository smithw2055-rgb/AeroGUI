#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/PathSegment.hpp>

namespace Aero::Media {

class AERO_GUI_API PolyLineSegment : public PathSegment {
    AERO_DECLARE_TYPE(PolyLineSegment, PathSegment)
public:
    PolyLineSegment() noexcept : PathSegment(StaticTypeId()) {}
    Span<const Point> GetPoints() const noexcept { return points_.AsSpan(); }
    Result<void> SetPoints(Span<const Point> points) noexcept;
    Result<void> AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    Result<void> SetPoints(StringView text) noexcept;
    Result<void> SetPointsText(Base::String text) noexcept {
        return SetPoints(text.View());
    }
    Result<void> Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept override;
private:
    Base::Vector<Point> points_;
};
} // namespace Aero::Media
