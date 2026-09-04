#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Shapes/Shape.hpp>

namespace Aero::Shapes {

class AERO_GUI_API Polyline : public Shape {
    AERO_DECLARE_TYPE(Polyline, Shape)
public:
    Polyline() noexcept : Shape(StaticTypeId()) {}
    ~Polyline() override = default;

    Span<const Point> GetPoints() const noexcept;
    Result<void> SetPoints(Span<const Point> points) noexcept;
    Result<void> AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    Result<void> SetPoints(StringView text) noexcept;
    Result<void> SetPointsText(Base::String text) noexcept {
        return SetPoints(text.View());
    }

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    Base::Vector<Point> points_;
};

} // namespace Aero::Shapes
