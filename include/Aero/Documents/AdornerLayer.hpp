#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Documents/Adorner.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Documents {

class AERO_GUI_API AdornerLayer : public FrameworkElement {
    AERO_DECLARE_TYPE(AdornerLayer, FrameworkElement)
public:
    AdornerLayer() noexcept : FrameworkElement(StaticTypeId()) {}

    Result<void> Add(Ref<Adorner> adorner) noexcept;
    Result<void> Remove(Adorner& adorner) noexcept;
    void Clear() noexcept { adorners_.Clear(); }
    Base::Span<const Ref<Adorner>> GetAdorners() const noexcept {
        return {adorners_.Data(), adorners_.Size()};
    }

    static AdornerLayer* GetAdornerLayer(UIElement* element) noexcept;

protected:
    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    Base::Vector<Ref<Adorner>> adorners_;
};

} // namespace Aero::Documents
