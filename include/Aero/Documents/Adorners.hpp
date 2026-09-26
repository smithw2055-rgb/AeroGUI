#pragma once

// Adorner, AdornerLayer, and AdornerDecorator.
#include <Aero/FrameworkElement.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Decorator.hpp>

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

class AERO_GUI_API AdornerLayer : public FrameworkElement {
    AERO_DECLARE_TYPE(AdornerLayer, FrameworkElement)
public:
    AdornerLayer() noexcept : FrameworkElement(StaticTypeId()) {}

    void Add(Ref<Adorner> adorner) noexcept;
    void Remove(Adorner& adorner) noexcept;
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
