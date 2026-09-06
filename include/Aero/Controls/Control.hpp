#pragma once

#include <Aero/TextFormatting.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brush.hpp>
#include <Aero/Style.hpp>
#include <utility>

namespace Aero { class VisualStateManager; }
namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;
class ControlTemplate;
class ItemContainerGenerator;
class AERO_GUI_API Control : public FrameworkElement {
    AERO_DECLARE_TYPE(Control, FrameworkElement)
public:

    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseDoubleClickEvent{"PreviewMouseDoubleClick"};
    Event<MouseButtonEventArgs> PreviewMouseDoubleClick() noexcept {
        return GetEvent(PreviewMouseDoubleClickEvent);
    }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseDoubleClickEvent{"MouseDoubleClick"};
    Event<MouseButtonEventArgs> MouseDoubleClick() noexcept {
        return GetEvent(MouseDoubleClickEvent);
    }

    Ref<Aero::Media::Brush> GetBackground() const noexcept {
        return GetValue(BackgroundProperty);
    }
    void SetBackground(Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    Ref<Aero::Media::Brush> GetBorderBrush() const noexcept {
        return GetValue(BorderBrushProperty);
    }
    void SetBorderBrush(Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BorderBrushProperty, std::move(value));
    }
    Aero::Base::Thickness GetBorderThickness() const noexcept {
        return GetValue(BorderThicknessProperty);
    }
    void SetBorderThickness(Aero::Base::Thickness value) noexcept {
        SetValue(BorderThicknessProperty, value);
    }
    void SetBorderThickness(double value) noexcept {
        SetBorderThickness({value, value, value, value});
    }
    Aero::Base::Thickness GetPadding() const noexcept {
        return GetValue(PaddingProperty);
    }
    void SetPadding(Aero::Base::Thickness value) noexcept {
        SetValue(PaddingProperty, value);
    }
    Aero::HorizontalAlignment
    GetHorizontalContentAlignment() const noexcept {
        return GetValue(HorizontalContentAlignmentProperty);
    }
    Aero::VerticalAlignment
    GetVerticalContentAlignment() const noexcept {
        return GetValue(VerticalContentAlignmentProperty);
    }
    Ref<Aero::Media::Brush> GetForeground() const noexcept {
        return GetValue(ForegroundProperty);
    }
    void SetForeground(Ref<Aero::Media::Brush> value) noexcept {
        SetValue(ForegroundProperty, std::move(value));
    }
    double GetFontSize() const noexcept {
        return GetValue(FontSizeProperty);
    }
    void SetFontSize(double value) noexcept {
        SetValue(FontSizeProperty, value);
    }
    FontWeight GetFontWeight() const noexcept {
        return GetValue(FontWeightProperty);
    }
    void SetFontWeight(FontWeight value) noexcept {
        SetValue(FontWeightProperty, value);
    }
    Ref<Aero::Style> GetFocusVisualStyle() const noexcept {
        return GetValue(FocusVisualStyleProperty);
    }
    void SetFocusVisualStyle(Ref<Aero::Style> value) noexcept {
        SetValue(FocusVisualStyleProperty, std::move(value));
    }
    bool GetOverridesDefaultStyle() const noexcept {
        return GetValue(OverridesDefaultStyleProperty);
    }
    void SetOverridesDefaultStyle(bool value) noexcept {
        SetValue(OverridesDefaultStyleProperty, value);
    }
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Background);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, BorderBrush);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::Thickness, BorderThickness);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::Thickness, Padding);
    AERO_DEPENDENCY_PROPERTY(Aero::HorizontalAlignment, HorizontalContentAlignment);
    AERO_DEPENDENCY_PROPERTY(Aero::VerticalAlignment, VerticalContentAlignment);
    inline static constexpr auto ForegroundProperty = Aero::Media::FrameworkElementForegroundProperty;
    AERO_DEPENDENCY_PROPERTY(double, FontSize);
    AERO_DEPENDENCY_PROPERTY(FontWeight, FontWeight);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Style>, FocusVisualStyle);
    AERO_DEPENDENCY_PROPERTY(bool, OverridesDefaultStyle);
    AERO_DEPENDENCY_PROPERTY(Ref<ControlTemplate>, Template);

    // Returns true only when this call materialized a new template instance.
    // Repeated calls are intentionally idempotent.
    bool ApplyTemplate() noexcept;

protected:
    DependencyObject* GetTemplateChild(StringView name) const noexcept;
    DependencyObject* GetTemplateChild(TypeId type) const noexcept;
    UIElement* GetTemplateRoot() const noexcept { return templateChild_; }
    explicit Control(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Control() override = default;
    virtual void OnApplyTemplate() noexcept {
        return;
    }
    virtual void OnTemplateDetached() noexcept {}
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        return templateChild_ != nullptr && templateChild_->GetVisualParent() == this
            ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (index != 0U || templateChild_ == nullptr ||
            templateChild_->GetVisualParent() != this) {
            return nullptr;
        }
        return templateChild_;
    }
    Size MeasureOverride(
        Size availableSize) noexcept override {
        if (templateChild_ == nullptr) return Size{};
        Result<void> measured =
            MeasureChild(*templateChild_, availableSize);
        if (!measured) return Size{};
        return templateChild_->GetDesiredSize();
    }
    Size ArrangeOverride(
        Size finalSize) noexcept override {
        if (templateChild_ == nullptr) return finalSize;
        Result<void> arranged = ArrangeChild(
            *templateChild_,
            {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
        return finalSize;
    }
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;
private:
    friend class ::Aero::VisualStateManager;
    void SetTemplateChildCore(UIElement* child) noexcept {
        if (child != nullptr && child->LayoutParent() != this) {
            return;
        }
        if (templateChild_ != nullptr && child != nullptr && templateChild_ != child) {
            return;
        }
        templateChild_ = child;
        return;
    }

    void NotifyTemplateApplied(
        std::uint64_t handleValue) noexcept {
        templateHandleValue_ = handleValue;
        ++templateGeneration_;
    }
    void NotifyTemplateDetached() noexcept {
        if (templateHandleValue_ != 0U) {
            OnTemplateDetached();
            templateHandleValue_ = 0U;
            ++templateGeneration_;
        }
    }
    UIElement* templateChild_ = nullptr;
    std::uint64_t templateHandleValue_ = 0U;
    std::uint64_t templateGeneration_ = 0U;
};
} // namespace Aero::Controls
