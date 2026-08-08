#pragma once

#include <Aero/TextFormatting.hpp>
#include <Aero/Controls/TextFormatting.hpp>
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
class AERO_API Control : public FrameworkElement {
    AERO_DECLARE_TYPE(Control, FrameworkElement)
public:
    struct Impl;

    Base::Ref<Aero::Media::Brush> GetBackground() const noexcept {
        return GetValueOr(
            BackgroundProperty,
            Base::Ref<Aero::Media::Brush>{});
    }
    void SetBackground(
        Base::Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    Base::Ref<Aero::Media::Brush> GetBorderBrush() const noexcept {
        return GetValueOr(
            BorderBrushProperty,
            Base::Ref<Aero::Media::Brush>{});
    }
    void SetBorderBrush(
        Base::Ref<Aero::Media::Brush> value) noexcept {
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
    Base::Ref<Aero::Media::Brush> GetForeground() const noexcept {
        return GetValueOr(
            ForegroundProperty,
            Base::Ref<Aero::Media::Brush>{});
    }
    void SetForeground(
        Base::Ref<Aero::Media::Brush> value) noexcept {
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
    Base::Ref<Aero::Style> GetFocusVisualStyle() const noexcept {
        return GetValueOr(
            FocusVisualStyleProperty,
            Base::Ref<Aero::Style>{});
    }
    void SetFocusVisualStyle(
        Base::Ref<Aero::Style> value) noexcept {
        SetValue(FocusVisualStyleProperty, std::move(value));
    }
    bool GetOverridesDefaultStyle() const noexcept {
        return GetValueOr(OverridesDefaultStyleProperty, false);
    }
    void SetOverridesDefaultStyle(
        bool value) noexcept {
        SetValue(OverridesDefaultStyleProperty, value);
    }
    inline static constexpr DependencyProperty<Base::Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr DependencyProperty<Base::Ref<Aero::Media::Brush>> BorderBrushProperty{"BorderBrush"};
    inline static constexpr DependencyProperty<Aero::Base::Thickness> BorderThicknessProperty{"BorderThickness"};
    inline static constexpr DependencyProperty<Aero::Base::Thickness> PaddingProperty{"Padding"};
    inline static constexpr DependencyProperty<Aero::HorizontalAlignment> HorizontalContentAlignmentProperty{"HorizontalContentAlignment"};
    inline static constexpr DependencyProperty<Aero::VerticalAlignment> VerticalContentAlignmentProperty{"VerticalContentAlignment"};
    inline static constexpr auto ForegroundProperty = Aero::Media::FrameworkElementForegroundProperty;
    inline static constexpr DependencyProperty<double> FontSizeProperty{"FontSize"};
    inline static constexpr DependencyProperty<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr DependencyProperty<Base::Ref<Aero::Style>> FocusVisualStyleProperty{"FocusVisualStyle"};
    inline static constexpr DependencyProperty<bool> OverridesDefaultStyleProperty{"OverridesDefaultStyle"};
    inline static constexpr DependencyProperty<Base::Ref<ControlTemplate>> TemplateProperty{"Template"};

    // Returns true only when this call materialized a new template instance.
    // Repeated calls are intentionally idempotent.
    bool ApplyTemplate() noexcept;

protected:
    DependencyObject* GetTemplateChild(Base::StringView name) const noexcept;
    DependencyObject* GetTemplateChild(TypeId type) const noexcept;
    UIElement* GetTemplateRoot() const noexcept { return templateChild_; }
    explicit Control(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Control() override = default;
    virtual void OnApplyTemplate() noexcept {
        return;
    }
    virtual void OnTemplateDetached() noexcept {}
    Size MeasureOverride(
        Size availableSize) noexcept override {
        if (templateChild_ == nullptr) return Size{};
        Base::Result<void> measured =
            MeasureChild(*templateChild_, availableSize);
        if (!measured) return Size{};
        return templateChild_->GetDesiredSize();
    }
    Size ArrangeOverride(
        Size finalSize) noexcept override {
        if (templateChild_ == nullptr) return finalSize;
        Base::Result<void> arranged = ArrangeChild(
            *templateChild_,
            {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
        return finalSize;
    }
    void OnRender(
        DrawingContext& context) noexcept override;
private:
    friend struct ::Aero::Controls::Control::Impl;
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
