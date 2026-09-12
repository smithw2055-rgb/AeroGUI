#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Shapes/Shape.hpp>

namespace Aero::Shapes {

class AERO_GUI_API Polyline : public Shape {
    AERO_DECLARE_TYPE(Polyline, Shape)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Polyline() noexcept : Shape(StaticTypeId()) {}
    ~Polyline() override = default;

    Span<const Point> GetPoints() const noexcept;
    void SetPoints(Span<const Point> points) noexcept;
    void AddPoint(Point point) noexcept;
    void ClearPoints() noexcept;
    void SetPoints(StringView text) noexcept;
    void SetPointsText(Base::String text) noexcept {
        SetPoints(text.View());
    }

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    Base::Vector<Point> points_;
};

} // namespace Aero::Shapes
