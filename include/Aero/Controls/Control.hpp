#pragma once

#include <Aero/TextFormatting.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
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
        return GetValueOr(
            BackgroundProperty,
            Ref<Aero::Media::Brush>{});
    }
    void SetBackground(
        Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    Ref<Aero::Media::Brush> GetBorderBrush() const noexcept {
        return GetValueOr(
            BorderBrushProperty,
            Ref<Aero::Media::Brush>{});
    }
    void SetBorderBrush(
        Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BorderBrushProperty, std::move(value));
    }
    Aero::Base::Thickness GetBorderThickness() const noexcept {
        return GetValueOr(
            BorderThicknessProperty,
            Aero::Base::Thickness{});
    }
    void SetBorderThickness(
        Aero::Base::Thickness value) noexcept {
        SetValue(BorderThicknessProperty, value);
    }
    void SetBorderThickness(
        double value) noexcept {
        SetBorderThickness({value, value, value, value});
    }
    Aero::Base::Thickness GetPadding() const noexcept {
        return GetValueOr(
            PaddingProperty,
            Aero::Base::Thickness{});
    }
    void SetPadding(
        Aero::Base::Thickness value) noexcept {
        SetValue(PaddingProperty, value);
    }
    Aero::HorizontalAlignment
    GetHorizontalContentAlignment() const noexcept {
        return GetValueOr(
            HorizontalContentAlignmentProperty,
            Aero::HorizontalAlignment::Left);
    }
    Aero::VerticalAlignment
    GetVerticalContentAlignment() const noexcept {
        return GetValueOr(
            VerticalContentAlignmentProperty,
            Aero::VerticalAlignment::Top);
    }
    Ref<Aero::Media::Brush> GetForeground() const noexcept {
        return GetValueOr(
            ForegroundProperty,
            Ref<Aero::Media::Brush>{});
    }
    void SetForeground(
        Ref<Aero::Media::Brush> value) noexcept {
        SetValue(ForegroundProperty, std::move(value));
    }
    double GetFontSize() const noexcept {
        return GetValueOr(FontSizeProperty, 16.0);
    }
    void SetFontSize(
        double value) noexcept {
        SetValue(FontSizeProperty, value);
    }
    FontWeight GetFontWeight() const noexcept {
        return GetValueOr(FontWeightProperty, FontWeight::Normal);
    }
    void SetFontWeight(
        FontWeight value) noexcept {
        SetValue(FontWeightProperty, value);
    }
    Ref<Aero::Style> GetFocusVisualStyle() const noexcept {
        return GetValueOr(
            FocusVisualStyleProperty,
            Ref<Aero::Style>{});
    }
    void SetFocusVisualStyle(
        Ref<Aero::Style> value) noexcept {
        SetValue(FocusVisualStyleProperty, std::move(value));
    }
    bool GetOverridesDefaultStyle() const noexcept {
        return GetValueOr(OverridesDefaultStyleProperty, false);
    }
    void SetOverridesDefaultStyle(
        bool value) noexcept {
        SetValue(OverridesDefaultStyleProperty, value);
    }
    inline static constexpr DependencyProperty<Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr DependencyProperty<Ref<Aero::Media::Brush>> BorderBrushProperty{"BorderBrush"};
    inline static constexpr DependencyProperty<Aero::Base::Thickness> BorderThicknessProperty{"BorderThickness"};
    inline static constexpr DependencyProperty<Aero::Base::Thickness> PaddingProperty{"Padding"};
    inline static constexpr DependencyProperty<Aero::HorizontalAlignment> HorizontalContentAlignmentProperty{"HorizontalContentAlignment"};
    inline static constexpr DependencyProperty<Aero::VerticalAlignment> VerticalContentAlignmentProperty{"VerticalContentAlignment"};
    inline static constexpr auto ForegroundProperty = Aero::Media::FrameworkElementForegroundProperty;
    inline static constexpr DependencyProperty<double> FontSizeProperty{"FontSize"};
    inline static constexpr DependencyProperty<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr DependencyProperty<Ref<Aero::Style>> FocusVisualStyleProperty{"FocusVisualStyle"};
    inline static constexpr DependencyProperty<bool> OverridesDefaultStyleProperty{"OverridesDefaultStyle"};
    inline static constexpr DependencyProperty<Ref<ControlTemplate>> TemplateProperty{"Template"};

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
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
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
