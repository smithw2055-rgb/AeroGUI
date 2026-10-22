#pragma once

#include <Aero/TextFormatting.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Style.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/FrameworkTemplate.hpp>
#include <utility>
#include <cstddef>

namespace Aero { class VisualStateManager; }

namespace Aero::Controls {

namespace Primitives { class ToggleButton; }

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;

class ControlTemplate;
class ItemContainerGenerator;
class ContentPresenter;

// ---------------- Control Base ----------------

class AERO_GUI_API Control : public FrameworkElement {
    AERO_DECLARE_TYPE(Control, FrameworkElement)

public:
    bool ApplyTemplate() noexcept;
    virtual void OnApplyTemplate() noexcept { return; }
    void SetTemplateChildCore(UIElement* child) noexcept {
        if (child != nullptr && child->LayoutParent() != this) { return; }
        if (templateChild_ != nullptr && child != nullptr && templateChild_ != child) { return; }
        if (templateChild_ != nullptr && templateChild_ != child) {
            if (templateChild_->GetVisualParent() == this) {
                RemoveVisualChild(templateChild_);
            }
        }
        templateChild_ = child;
    }
    void NotifyTemplateApplied(std::uint64_t handleValue) noexcept {
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

    Ref<Aero::Media::Brush> GetBackground() const noexcept { return GetValue(BackgroundProperty); }
    void SetBackground(Ref<Aero::Media::Brush> value) noexcept { SetValue(BackgroundProperty, std::move(value)); }
    Ref<Aero::Media::Brush> GetBorderBrush() const noexcept { return GetValue(BorderBrushProperty); }
    void SetBorderBrush(Ref<Aero::Media::Brush> value) noexcept { SetValue(BorderBrushProperty, std::move(value)); }
    bool HitTestCore() const noexcept override {
        return (GetBackground() != nullptr || GetBorderBrush() != nullptr);
    }
    Aero::Base::Thickness GetBorderThickness() const noexcept { return GetValue(BorderThicknessProperty); }
    void SetBorderThickness(Aero::Base::Thickness value) noexcept { SetValue(BorderThicknessProperty, value); }
    void SetBorderThickness(double value) noexcept { SetBorderThickness({value, value, value, value}); }
    Aero::Base::Thickness GetPadding() const noexcept { return GetValue(PaddingProperty); }
    void SetPadding(Aero::Base::Thickness value) noexcept { SetValue(PaddingProperty, value); }
    Aero::HorizontalAlignment GetHorizontalContentAlignment() const noexcept { return GetValue(HorizontalContentAlignmentProperty); }
    Aero::VerticalAlignment GetVerticalContentAlignment() const noexcept { return GetValue(VerticalContentAlignmentProperty); }
    Ref<Aero::Media::Brush> GetForeground() const noexcept { return GetValue(ForegroundProperty); }
    void SetForeground(Ref<Aero::Media::Brush> value) noexcept { SetValue(ForegroundProperty, std::move(value)); }
    double GetFontSize() const noexcept { return GetValue(FontSizeProperty); }
    void SetFontSize(double value) noexcept { SetValue(FontSizeProperty, value); }
    FontWeight GetFontWeight() const noexcept { return GetValue(FontWeightProperty); }
    void SetFontWeight(FontWeight value) noexcept { SetValue(FontWeightProperty, value); }
    Ref<Aero::Style> GetFocusVisualStyle() const noexcept { return GetValue(FocusVisualStyleProperty); }
    void SetFocusVisualStyle(Ref<Aero::Style> value) noexcept { SetValue(FocusVisualStyleProperty, std::move(value)); }
    bool GetOverridesDefaultStyle() const noexcept { return GetValue(OverridesDefaultStyleProperty); }
    void SetOverridesDefaultStyle(bool value) noexcept { SetValue(OverridesDefaultStyleProperty, value); }
    UIElement* GetTemplateRoot() const noexcept { return templateChild_; }
    bool IsTemplateApplied() const noexcept { return templateHandleValue_ != 0U; }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseDoubleClickEvent{"PreviewMouseDoubleClick"};
    Event<MouseButtonEventArgs> PreviewMouseDoubleClick() noexcept { return GetEvent(PreviewMouseDoubleClickEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseDoubleClickEvent{"MouseDoubleClick"};
    Event<MouseButtonEventArgs> MouseDoubleClick() noexcept { return GetEvent(MouseDoubleClickEvent); }

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

protected:
    explicit Control(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Control() override {
        if (templateChild_ != nullptr && templateChild_->GetVisualParent() == this) {
            RemoveVisualChild(templateChild_);
        }
    }

    DependencyObject* GetTemplateChild(StringView name) const noexcept;
    DependencyObject* GetTemplateChild(TypeId type) const noexcept;
    virtual void OnTemplateDetached() noexcept {}
    virtual void OnTemplateChanged(ControlTemplate* oldTemplate, ControlTemplate* newTemplate) noexcept {
        static_cast<void>(oldTemplate);
        static_cast<void>(newTemplate);
    }
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        return templateChild_ != nullptr ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (index != 0U || templateChild_ == nullptr) {
            return nullptr;
        }
        return templateChild_;
    }
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    friend class Aero::VisualStateManager;
    UIElement* templateChild_ = nullptr;
    std::uint64_t templateHandleValue_ = 0U;
    std::uint32_t templateGeneration_ = 0U;
};

// ---------------- ControlTemplate ----------------

class AERO_GUI_API ControlTemplate : public FrameworkTemplate {
    AERO_DECLARE_TYPE(ControlTemplate, FrameworkTemplate)

public:
    ControlTemplate() noexcept = default;
    ~ControlTemplate() noexcept override = default;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
};

// ---------------- ContentControl ----------------

class AERO_GUI_API ContentControl : public Control {
    AERO_DECLARE_TYPE(ContentControl, Control)

public:
    static void OnContentPropertyChanged(::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& change) noexcept;

    Value GetContent() const noexcept {
        return GetValue(ContentProperty);
    }
    void SetContent(Ref<Base::Object> content) noexcept { SetContentValue(std::move(content)); }
    void SetContent(Value content) noexcept { SetContentValue(std::move(content)); }
    void SetContent(StringView text) noexcept;
    void SetContent(const char* text) noexcept;
    void SetContent(std::nullptr_t) noexcept { SetContent(static_cast<UIElement*>(nullptr)); }
    template<class T, class = std::enable_if_t< std::is_base_of_v<UIElement, T> && !std::is_same_v<T, Base::Object>>>
    void SetContent(Ref<T> element) noexcept { SetContent(element.Get()); }
    void SetContent(UIElement* content) noexcept;
    Ref<Base::Object> GetContentTemplate() const noexcept { return GetValue(ContentTemplateProperty); }
    void SetContentTemplate(Ref<Base::Object> value) noexcept { SetValue(ContentTemplateProperty, std::move(value)); }
    Ref<Base::Object> GetContentTemplateSelector() const noexcept { return GetValue(ContentTemplateSelectorProperty); }
    void SetContentTemplateSelector(Ref<Base::Object> value) noexcept {
        SetValue(ContentTemplateSelectorProperty, std::move(value));
    }
    void SetOwnedContent(const Ref<Base::Object>& contentObject, UIElement& content) noexcept;
    void SetContentValue(Ref<Base::Object> value) noexcept;
    void SetContentValue(Value value) noexcept;
    void SetGeneratedTextContent(const Ref<Base::Object>& contentObject, UIElement& content) noexcept;

    AERO_DEPENDENCY_PROPERTY(Value, Content);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ContentTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ContentTemplateSelector);

    virtual UIElement* GetContentElement() const noexcept;
    ContentPresenter* GetFallbackPresenter() const noexcept;
    void EnsureFallbackPresenter() noexcept;
    void EnsureHostedContent() noexcept { EnsureFallbackPresenter(); }
    const Ref<Base::Object>& OwnedContent() const noexcept { return ownedContent_; }
    const Ref<Base::Object>& ContentValue() const noexcept { return ownedContent_; }

protected:
    explicit ContentControl(TypeId runtimeType) noexcept;
    ~ContentControl() override;

    virtual void OnContentChanged(const Value& oldContent, const Value& newContent);
    virtual void OnContentTemplateChanged(const Ref<Base::Object>& oldContentTemplate,
        const Ref<Base::Object>& newContentTemplate);
    virtual void OnContentTemplateSelectorChanged(const Ref<Base::Object>& oldSelector,
        const Ref<Base::Object>& newSelector);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    std::uint32_t GetLogicalChildrenCount() const noexcept override;
    DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

    [[deprecated("Use GetContentElement() for WPF parity")]]
    UIElement* ContentElement() const noexcept { return GetContentElement(); }

private:
    void ClearFallbackPresenter() noexcept;

    Ref<ContentPresenter> fallbackPresenter_;
    Ref<Base::Object> ownedContent_;
};

// ---------------- Page ----------------

class AERO_GUI_API Page : public ContentControl {
    AERO_DECLARE_TYPE(Page, ContentControl)

public:
    Page() noexcept : ContentControl(StaticTypeId()) {}
    ~Page() override = default;

    UIElement* GetContent() const noexcept { return GetContentElement(); }
    Value GetContentValue() const noexcept { return GetValue(ContentProperty); }
};

// ---------------- ContentPresenter ----------------

class AERO_GUI_API ContentPresenter : public FrameworkElement {
    AERO_DECLARE_TYPE(ContentPresenter, FrameworkElement)

public:
    ContentPresenter() noexcept;

    void HostUiElement(const Ref<Base::Object>& owner, UIElement& element) noexcept;
    static void OnContentPropertyChanged(::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& change) noexcept;

    UIElement* GetContent() const noexcept { return content_; }
    void SetContent(UIElement* content) noexcept;
    void SetContent(std::nullptr_t) noexcept {
        Result<void> access = VerifyAccess();
        if (!access) return;
        if (content_ == nullptr) return;
        content_ = nullptr;
        ownedContent_.Reset();
        InvalidateMeasure();
    }
    const Ref<Base::Object>& GetOwnedContent() const noexcept { return ownedContent_; }
    void SetOwnedContent(const Ref<Base::Object>& contentObject, UIElement& content) noexcept;
    const Value& GetContentValue() const noexcept { return contentValue_; }
    void SetContentValue(Value value) noexcept { SetValue(ContentProperty, std::move(value)); }
    StringView GetContentSource() const noexcept { return GetValue(ContentSourceProperty); }
    void SetContentSource(StringView value) noexcept;
    void RefreshContentSource() noexcept;
    void ClearHostedContent() noexcept;

    Ref<Base::Object> GetContentTemplate() const noexcept { return GetValue(ContentTemplateProperty); }
    void SetContentTemplate(Ref<Base::Object> value) noexcept { SetValue(ContentTemplateProperty, std::move(value)); }
    Ref<Base::Object> GetContentTemplateSelector() const noexcept { return GetValue(ContentTemplateSelectorProperty); }
    void SetContentTemplateSelector(Ref<Base::Object> value) noexcept { SetValue(ContentTemplateSelectorProperty, std::move(value)); }

    AERO_DEPENDENCY_PROPERTY(String, ContentSource);
    AERO_DEPENDENCY_PROPERTY(Value, Content);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ContentTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ContentTemplateSelector);

protected:
    explicit ContentPresenter(TypeId runtimeType) noexcept;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        return (content_ != nullptr && content_ != this) ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (index != 0U || content_ == nullptr || content_ == this) { return nullptr; }
        return content_;
    }
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    Result<void> ValidateContent(UIElement* content) const noexcept;
    Result<void> UpdatePresentedText() noexcept;
    void ApplyContent() noexcept;
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept;

    UIElement* content_ = nullptr;
    Ref<Base::Object> ownedContent_;
    Ref<Base::Object> appliedTemplate_;
    Value contentValue_ = Value::NullObject(Meta::TypeOf<Base::Object>());
};

// ---------------- HeaderedContentControl ----------------

class AERO_GUI_API HeaderedContentControl : public ContentControl {
    AERO_DECLARE_TYPE(HeaderedContentControl, ContentControl)

public:
    Value GetHeader() const noexcept;
    void SetHeader(const Value& value) noexcept;
    void SetHeader(StringView value) noexcept;
    Ref<DataTemplate> GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Value, Header);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, HeaderTemplate);

protected:
    explicit HeaderedContentControl(TypeId runtimeType) noexcept;
    ~HeaderedContentControl() override;

    virtual void OnHeaderChanged(const Value& oldHeader, const Value& newHeader);
    virtual void OnHeaderTemplateChanged(const Ref<DataTemplate>& oldTemplate, const Ref<DataTemplate>& newTemplate);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
};

// ---------------- UserControl & Label & GroupBox ----------------

class AERO_GUI_API UserControl : public ContentControl {
    AERO_DECLARE_TYPE(UserControl, ContentControl)

public:
    UserControl() noexcept : ContentControl(StaticTypeId()) {}
    ~UserControl() override = default;

protected:
    explicit UserControl(TypeId runtimeType) noexcept : ContentControl(runtimeType) {}
};

class AERO_GUI_API Label : public ContentControl {
    AERO_DECLARE_TYPE(Label, ContentControl)

public:
    Label() noexcept : ContentControl(StaticTypeId()) {}
};

class AERO_GUI_API GroupBox : public HeaderedContentControl {
    AERO_DECLARE_TYPE(GroupBox, HeaderedContentControl)

public:
    GroupBox() noexcept : HeaderedContentControl(StaticTypeId()) {}
    ~GroupBox() override = default;
};

// ---------------- Expander ----------------

enum class ExpandDirection : std::uint8_t {
    Down = 0U,
    Up,
    Left,
    Right,
};

class AERO_GUI_API Expander : public HeaderedContentControl {
    AERO_DECLARE_TYPE(Expander, HeaderedContentControl)

public:
    Expander() noexcept;
    ~Expander() override;

    bool GetIsExpanded() const noexcept;
    void SetIsExpanded(bool value) noexcept;
    ExpandDirection GetDirection() const noexcept;
    void SetDirection(ExpandDirection value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    UIElement::Event<RoutedEventArgs> Expanded() noexcept { return GetEvent(ExpandedEvent); }
    UIElement::Event<RoutedEventArgs> Collapsed() noexcept { return GetEvent(CollapsedEvent); }
    AERO_DEPENDENCY_PROPERTY(bool, IsExpanded);
    AERO_DEPENDENCY_PROPERTY(ExpandDirection, ExpandDirection);

protected:
    virtual void OnExpanded();
    virtual void OnCollapsed();
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler headerCheckedHandler_;
    Primitives::ToggleButton* headerToggle_ = nullptr;
    bool synchronizingHeader_ = false;
    void OnHeaderCheckedChanged(DependencyObject& sender, const DependencyPropertyChangedEventArgs& args) noexcept;
    void UnbindHeaderToggle() noexcept;
    void BindHeaderToggle() noexcept;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::ExpandDirection)
