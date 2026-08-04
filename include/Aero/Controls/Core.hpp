#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <utility>
#include <Aero/Style.hpp>

#include <cstdint>

namespace Aero {

class VisualStateManager;

// WPF-facing text values are shared by TextBlock, TextBox and document
// elements.  Provider and shaping contracts remain private to src/text.
enum class FontStyle : std::uint8_t {
    Normal = 0U,
    Italic,
    Oblique
};

enum class FontStretch : std::uint8_t {
    UltraCondensed = 0U,
    ExtraCondensed,
    Condensed,
    SemiCondensed,
    Normal,
    SemiExpanded,
    Expanded,
    ExtraExpanded,
    UltraExpanded
};

enum class TextWrapping : std::uint8_t {
    NoWrap = 0U,
    Wrap,
    WrapWithOverflow
};

enum class TextTrimming : std::uint8_t {
    None = 0U,
    CharacterEllipsis,
    WordEllipsis
};

enum class TextAlignment : std::uint8_t {
    Start = 0U,
    Center,
    End,
    Justify
};

struct TextRange {
    std::uint32_t start = 0U;
    std::uint32_t length = 0U;

    std::uint32_t GetEnd() const noexcept { return start + length; }
    bool GetIsEmpty() const noexcept { return length == 0U; }
};

struct TextSelection {
    std::uint32_t anchor = 0U;
    std::uint32_t caret = 0U;

    std::uint32_t GetStart() const noexcept {
        return anchor < caret ? anchor : caret;
    }
    std::uint32_t GetEnd() const noexcept {
        return anchor < caret ? caret : anchor;
    }
    std::uint32_t GetLength() const noexcept { return GetEnd() - GetStart(); }
    bool GetIsEmpty() const noexcept { return anchor == caret; }
};

struct TextHitRegion {
    std::uint32_t textOffset = 0U;
    std::uint32_t textLength = 0U;
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

} // namespace Aero

namespace Aero::Controls {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;
using ::Aero::Input::ICommand;


// Inline formatting value shared by retained text controls and the
// Aero::Documents inline hierarchy.
enum class TextDecorations : std::uint8_t {
    None = 0U,
    Underline
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::FontWeight)

AERO_DECLARE_TYPE_ENUM(Aero::Controls::TextDecorations)

namespace Aero::Controls {

class ControlTemplate;
class ItemContainerGenerator;
class Panel;

class AERO_API UIElementCollection {
public:
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    UIElement* GetItem(std::uint32_t index) const noexcept;
    Base::Result<void> Add(Base::Ref<UIElement> child) noexcept;
    Base::Result<void> Remove(UIElement& child) noexcept;
    void Clear() noexcept;

private:
    friend class Panel;
    explicit UIElementCollection(Panel& owner) noexcept : owner_(&owner) {}
    Panel* owner_ = nullptr;
};

class AERO_API Panel : public FrameworkElement {
    AERO_DECLARE_TYPE(Panel, FrameworkElement)
public:
    Base::Ref<Aero::Media::Brush> GetBackground() const noexcept {
        return GetValueOr(
            BackgroundProperty,
            Base::Ref<Aero::Media::Brush>{});
    }
    void SetBackground(
        Base::Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr Members::Property<bool> IsItemsHostProperty{"IsItemsHost"};
    inline static constexpr Members::AttachedProperty<std::int32_t> ZIndexProperty{"ZIndex"};
    UIElementCollection& GetChildren() noexcept { return children_; }
    const UIElementCollection& GetChildren() const noexcept { return children_; }
protected:
    explicit Panel(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType), children_(*this), ownedChildren_() {}
    ~Panel() override = default;
    void OnRender(
        DrawingContext& context) noexcept override;
private:
    friend class UIElementCollection;
    friend struct ::Aero::Visual::Impl;
    std::uint32_t ChildCountCore() const noexcept { return ownedChildren_.Size(); }
    Base::Ref<Base::Object> ChildAtCore(std::uint32_t index) const noexcept {
        return index < ownedChildren_.Size() ? ownedChildren_[index] : Base::Ref<Base::Object>{};
    }
    Base::Result<void> AddChildCore(const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept;
    Base::Result<bool> RemoveChildCore(UIElement& child) noexcept;
    void ClearChildrenCore() noexcept;
    UIElementCollection children_;
    Base::Vector<Base::Ref<Base::Object>> ownedChildren_;
};

class AERO_API Decorator : public FrameworkElement {
    AERO_DECLARE_TYPE(Decorator, FrameworkElement)
public:
    // Decorator is constructible in the reference XAML surface and is used as
    // a lightweight single-child layout node in control templates.
    Decorator() noexcept : Decorator(StaticTypeId()) {}
    ~Decorator() override = default;
    UIElement* GetChild() const noexcept {
        if (child_ != nullptr) return child_;
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U ? children[0] : nullptr;
    }
    void SetChild(UIElement* child) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        Base::Result<void> valid = ValidateChild(child);
        if (!valid) return;
        if (child_ == child) return;
        child_ = child;
        if (child == nullptr) ownedChild_.Reset();
        return;
    }
protected:
    explicit Decorator(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    Size MeasureOverride(Size availableSize) noexcept override {
        UIElement* child = GetChild();
        if (child == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Size{};
            }
            return Size{};
        }
        Base::Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return Size{};
        return child->GetDesiredSize();
    }
    Size ArrangeOverride(Size finalSize) noexcept override {
        UIElement* child = GetChild();
        if (child == nullptr) return finalSize;
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
        return finalSize;
    }
private:
    friend struct ::Aero::Visual::Impl;
    void SetOwnedChild(
        const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) {
            return;
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        Base::Result<void> valid = ValidateChild(&child);
        if (!valid) return;
        child_ = &child;
        ownedChild_ = childObject;
        return;
    }
    UIElement* child_ = nullptr;
    Base::Ref<Base::Object> ownedChild_;
    bool IsOnlyAttachedChild(const UIElement& child) const noexcept {
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U && children[0] == &child;
    }
    Base::Result<void> ValidateChild(UIElement* child) const noexcept {
        if (child == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState,
                    "Decorator child must be detached before clearing it");
            }
        } else if (!LayoutChildren().Empty() && !IsOnlyAttachedChild(*child)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Decorator child must be its only attached UIElement");
        }
        return {};
    }
};

class AERO_API Control : public FrameworkElement {
    AERO_DECLARE_TYPE(Control, FrameworkElement)
public:
    struct Impl;

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
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BorderBrushProperty{"BorderBrush"};
    inline static constexpr Members::Property<Aero::Base::Thickness> BorderThicknessProperty{"BorderThickness"};
    inline static constexpr Members::Property<Aero::Base::Thickness> PaddingProperty{"Padding"};
    inline static constexpr Members::Property<Aero::HorizontalAlignment> HorizontalContentAlignmentProperty{"HorizontalContentAlignment"};
    inline static constexpr Members::Property<Aero::VerticalAlignment> VerticalContentAlignmentProperty{"VerticalContentAlignment"};
    inline static constexpr auto ForegroundProperty = Aero::Media::FrameworkElementForegroundProperty;
    inline static constexpr Members::Property<double> FontSizeProperty{"FontSize"};
    inline static constexpr Members::Property<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr Members::Property<Base::Ref<Aero::Style>> FocusVisualStyleProperty{"FocusVisualStyle"};
    inline static constexpr Members::Property<bool> OverridesDefaultStyleProperty{"OverridesDefaultStyle"};
    inline static constexpr Members::Property<Base::Ref<ControlTemplate>> TemplateProperty{"Template"};

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

class AERO_API ContentControl : public Control {
    AERO_DECLARE_TYPE(ContentControl, Control)
public:
    inline static constexpr Members::Property<Value> ContentProperty{"Content"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ContentTemplateSelectorProperty{"ContentTemplateSelector"};

    Value GetContent() const noexcept {
        return GetValueOr(ContentProperty, Value::NullObject(Meta::TypeOf<Base::Object>()));
    }
    Base::Ref<Base::Object> GetContentTemplate() const noexcept {
        return GetValueOr(
            ContentTemplateProperty,
            Base::Ref<Base::Object>{});
    }
    void SetContentTemplate(
        Base::Ref<Base::Object> value) noexcept {
        SetValue(ContentTemplateProperty, std::move(value));
    }
    Base::Ref<Base::Object>
    GetContentTemplateSelector() const noexcept {
        return GetValueOr(
            ContentTemplateSelectorProperty,
            Base::Ref<Base::Object>{});
    }
    void SetContentTemplateSelector(
        Base::Ref<Base::Object> value) noexcept {
        SetValue(ContentTemplateSelectorProperty, std::move(value));
    }
    void SetContent(Base::Ref<Base::Object> content) noexcept {
        SetContentValue(std::move(content));
    }
    void SetContent(Value content) noexcept {
        SetContentValue(std::move(content));
    }
    void SetContent(UIElement* content) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        Base::Result<void> valid = ValidateContent(content);
        if (!valid) return;
        Value propertyValue =
            content != nullptr
            ? Value::FromObject(
                  content->RuntimeType(),
                  Base::Ref<Base::Object>::FromBorrowed(
                      *content))
            : Value::NullObject(
                  Meta::TypeOf<Base::Object>());
        Base::Result<void> stored =
            StoreContentProperty(
                std::move(propertyValue));
        if (!stored) return;
        if (content_ == content) return;
        content_ = content;
        literalTextContent_ = false;
        if (content == nullptr) {
            ownedContent_.Reset();
            contentValue_.Reset();
        }
        return;
    }
protected:
    UIElement* ContentElement() const noexcept { return content_; }
    explicit ContentControl(TypeId runtimeType) noexcept;
    ~ContentControl() override;
    Size MeasureOverride(Size availableSize) noexcept override {
        if (GetTemplateRoot() != nullptr) {
            return Control::MeasureOverride(availableSize);
        }
        if (content_ == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Size{};
            }
            return Size{};
        }
        if (!IsOnlyAttachedContent(*content_)) {
            return Size{};
        }
        Base::Result<void> measured = MeasureChild(*content_, availableSize);
        if (!measured) return Size{};
        return content_->GetDesiredSize();
    }
    Size ArrangeOverride(Size finalSize) noexcept override {
        if (GetTemplateRoot() != nullptr) {
            return Control::ArrangeOverride(finalSize);
        }
        if (content_ == nullptr) return finalSize;
        if (!IsOnlyAttachedContent(*content_)) {
            return finalSize;
        }
        Base::Result<void> arranged = ArrangeChild(
            *content_, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
        return finalSize;
    }
private:
    friend struct ::Aero::Controls::Control::Impl;
    void SetOwnedContent(
        const Base::Ref<Base::Object>& contentObject, UIElement& content) noexcept {
        if (!contentObject || contentObject.Get() != &content) {
            return;
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        Base::Result<void> valid = ValidateContent(&content);
        if (!valid) return;
        Base::Result<void> stored =
            StoreContentProperty(
                Value::FromObject(
                    contentObject->RuntimeType(),
                    contentObject));
        if (!stored) return;
        content_ = &content;
        ownedContent_ = contentObject;
        contentValue_ = contentObject;
        literalTextContent_ = false;
        return;
    }
    // Stores arbitrary business content without exposing it as a visual.
    // A matching ContentTemplate can materialize it through
    // CreateTemplatedContent(); the UIElement overloads remain the
    // source-compatible direct-content path.
    void SetContentValue(
        Base::Ref<Base::Object> value) noexcept;
    void SetContentValue(
        Value value) noexcept;
    static void OnContentPropertyChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs&
            change) noexcept;
    Base::Result<Base::Ref<Base::Object>>
        CreateTemplatedContent() const noexcept;
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
    Base::Ref<Base::Object> contentValue_;
    Value authoredContent_;
    DependencyPropertyChangedEventHandler
        foregroundChangedHandler_;
    bool literalTextContent_ = false;
    bool synchronizingContentProperty_ = false;
    Base::Result<void> StoreContentProperty(
        Value value) noexcept;
    void SetGeneratedTextContent(
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;
    void OnForegroundChanged(
        DependencyObject&,
        const DependencyPropertyChangedEventArgs&) noexcept;
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept {
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U && children[0] == &content;
    }
    Base::Result<void> ValidateContent(UIElement* content) const noexcept {
        if (content == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState,
                    "ContentControl content must be detached before clearing it");
            }
        } else if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentControl content must be its only attached UIElement");
        }
        return {};
    }
};

class AERO_API UserControl : public ContentControl {
    AERO_DECLARE_TYPE(UserControl, ContentControl)
public:
    UserControl() noexcept : ContentControl(StaticTypeId()) {}
    ~UserControl() override = default;
protected:
    explicit UserControl(TypeId runtimeType) noexcept : ContentControl(runtimeType) {}
};

// Navigable content surface. It shares UserControl's single-child layout but
// remains a distinct XAML/runtime type so Page-targeted WPF styles resolve.
class AERO_API Page : public UserControl {
    AERO_DECLARE_TYPE(Page, UserControl)
public:
    Page() noexcept : UserControl(StaticTypeId()) {}
    ~Page() override = default;
};

} // namespace Aero::Controls
