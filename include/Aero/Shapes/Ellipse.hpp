#pragma once

#include <Aero/Shapes/Shape.hpp>

namespace Aero::Shapes {

class AERO_GUI_API Ellipse : public Shape {
    AERO_DECLARE_TYPE(Ellipse, Shape)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Ellipse() noexcept : Shape(StaticTypeId()) {}
    ~Ellipse() override = default;

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;
};

} // namespace Aero::Shapes
