#pragma once

#include <Aero/Controls/Decorator.hpp>
#include <Aero/Media/Brushes.hpp>

namespace Aero::Media { class DrawingContext; }

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;

class AERO_GUI_API Border : public Decorator {
    AERO_DECLARE_TYPE(Border, Decorator)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Border() noexcept;
    void SetBackground(Ref<Brush> value) noexcept;
    void SetBorderBrush(Ref<Brush> value) noexcept;
    void SetBorderThickness(Thickness value) noexcept;
    void SetBorderThickness(double value) noexcept;
    void SetCornerRadius(CornerRadius value) noexcept;
    void SetCornerRadius(double value) noexcept;
    void SetPadding(Thickness value) noexcept;
    Ref<Brush> GetBackground() const noexcept;
    Ref<Brush> GetBorderBrush() const noexcept;
    Thickness GetBorderThickness() const noexcept;
    CornerRadius GetCornerRadius() const noexcept;
    Thickness GetPadding() const noexcept;
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Background);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, BorderBrush);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::Thickness, BorderThickness);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::CornerRadius, CornerRadius);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::Thickness, Padding);
protected:
    explicit Border(TypeId runtimeType) noexcept : Decorator(runtimeType) {}
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
};

} // namespace Aero::Controls
