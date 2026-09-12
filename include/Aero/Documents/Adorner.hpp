#pragma once

#include <Aero/FrameworkElement.hpp>

namespace Aero::Documents {

class AERO_GUI_API Adorner : public FrameworkElement {
    AERO_DECLARE_TYPE(Adorner, FrameworkElement)
public:
    Adorner() noexcept : FrameworkElement(StaticTypeId()) {}
    explicit Adorner(UIElement* adorned) noexcept
        : FrameworkElement(StaticTypeId()), adorned_(adorned) {}

    UIElement* GetAdornedElement() const noexcept { return adorned_; }
    void SetAdornedElement(UIElement* value) noexcept { adorned_ = value; }

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    UIElement* adorned_ = nullptr;
};

} // namespace Aero::Documents
