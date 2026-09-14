#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/PathSegment.hpp>

namespace Aero::Media {

class AERO_GUI_API PolyBezierSegment : public PathSegment {
    AERO_DECLARE_TYPE(PolyBezierSegment, PathSegment)
public:
    PolyBezierSegment() noexcept : PathSegment(StaticTypeId()) {}
    Span<const Point> GetPoints() const noexcept { return points_.AsSpan(); }
    void SetPoints(Span<const Point> points) noexcept;
    void AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    void SetPoints(StringView text) noexcept;
    void SetPointsText(Base::String text) noexcept {
        SetPoints(text.View());
    }
    void Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept override;
private:
    Base::Vector<Point> points_;
};
} // namespace Aero::Media
