#pragma once

#include <Aero/Controls/Decorator.hpp>
#include <Aero/Documents/AdornerLayer.hpp>

namespace Aero::Documents {

class AERO_GUI_API AdornerDecorator : public Controls::Decorator {
    AERO_DECLARE_TYPE(AdornerDecorator, Controls::Decorator)
public:
    AdornerDecorator() noexcept;

    AdornerLayer* GetAdornerLayer() const noexcept { return layer_.Get(); }

protected:
    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    Ref<AdornerLayer> layer_;
};

} // namespace Aero::Documents
