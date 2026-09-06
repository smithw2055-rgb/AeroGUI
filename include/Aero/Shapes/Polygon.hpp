#pragma once

#include <cstdint>

#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Shapes/Shape.hpp>

namespace Aero::Shapes {

enum class FillRule : std::uint8_t { EvenOdd = 0U, Nonzero };

class AERO_GUI_API Polygon : public Shape {
    AERO_DECLARE_TYPE(Polygon, Shape)
public:
    Polygon() noexcept : Shape(StaticTypeId()) {}
    ~Polygon() override = default;

    FillRule GetFillRule() const noexcept;
    void SetFillRule(FillRule value) noexcept;
    Span<const Point> GetPoints() const noexcept;
    void SetPoints(Span<const Point> points) noexcept;
    void AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    void SetPoints(StringView text) noexcept;
    void SetPointsText(Base::String text) noexcept {
        SetPoints(text.View());
    }

    AERO_DEPENDENCY_PROPERTY(FillRule, FillRule);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    Base::Vector<Point> points_;
};

} // namespace Aero::Shapes

AERO_DECLARE_TYPE_ENUM(Aero::Shapes::FillRule)
