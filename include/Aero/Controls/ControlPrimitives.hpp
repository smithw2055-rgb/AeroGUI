#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/Brushes.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Style.hpp>

#include <utility>

namespace Aero::Detail {
class ControlRuntimeAccess;
class RuntimePresentationServices;
}

namespace Aero::Presentation {
}

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero::Presentation;

enum class FontWeight : std::uint8_t {
    Normal = 0U,
    SemiBold,
    Bold
};

// Inline formatting value shared by retained text controls and the
// Aero::Documents inline hierarchy.
enum class TextDecorations : std::uint8_t {
    None = 0U,
    Underline
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::FontWeight> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("FontWeight");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "FontWeight";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::TextDecorations> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("TextDecorations");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "TextDecorations";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core

namespace Aero::Controls {

class ControlTemplate;
class ItemContainerGenerator;
class DataTemplate;

class AERO_API Panel : public FrameworkElement {
    AERO_DECLARE_TYPE(Panel, FrameworkElement)
public:
    Presentation::Color Background() const noexcept {
        return Presentation::SampleBrush(
            BackgroundBrush());
    }
    Base::Ref<Presentation::Brush>
    BackgroundBrush() const noexcept {
        return GetValueOr(
            BackgroundProperty,
            Base::Ref<Presentation::Brush>{});
    }
    Base::Result<void> SetBackground(
        Presentation::Color value) noexcept {
        Base::Result<Base::Ref<Presentation::Brush>> brush =
            Presentation::MakeSolidColorBrush(value);
        return brush
            ? SetBackgroundBrush(
                std::move(brush).Value())
            : brush.GetStatus();
    }
    Base::Result<void> SetBackgroundBrush(
        Base::Ref<Presentation::Brush> value) noexcept {
        return SetValue(
            BackgroundProperty, std::move(value));
    }
    inline static constexpr Members::Property<
        Base::Ref<Presentation::Brush>>
        BackgroundProperty{"Background"};
    inline static constexpr Members::Property<bool>
        IsItemsHostProperty{"IsItemsHost"};
    inline static constexpr Members::AttachedProperty<std::int32_t>
        ZIndexProperty{"ZIndex"};
    std::uint32_t OwnedChildCount() const noexcept { return ownedChildren_.Size(); }
    Base::Ref<Base::Object> OwnedChildAt(
        std::uint32_t index) const noexcept {
        return index < ownedChildren_.Size()
            ? ownedChildren_[index]
            : Base::Ref<Base::Object>{};
    }
    Base::Result<void> AddOwnedChild(
        const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Panel owned child does not match its UIElement");
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        for (const Base::Ref<Base::Object>& owned : ownedChildren_) {
            if (owned.Get() == &child) {
                return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                    "Panel already owns the child");
            }
        }
        Base::Result<void> appended = ownedChildren_.TryPushBack(childObject);
        if (!appended) return appended.GetStatus();
        return InvalidateMeasure();
    }
    Base::Result<void> ClearOwnedChildren() noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        if (!LayoutChildren().Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Panel children must be detached before releasing ownership");
        }
        ownedChildren_.Clear();
        return InvalidateMeasure();
    }
protected:
    explicit Panel(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType), ownedChildren_() {}
    ~Panel() override = default;
    Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;
private:
    Base::Vector<Base::Ref<Base::Object>> ownedChildren_;
};

class AERO_API Decorator : public FrameworkElement {
    AERO_DECLARE_TYPE(Decorator, FrameworkElement)
public:
    // Decorator is constructible in the reference XAML surface and is used as
    // a lightweight single-child layout node in control templates.
    Decorator() noexcept : Decorator(StaticTypeId()) {}
    ~Decorator() override = default;
    UIElement* Child() const noexcept {
        if (child_ != nullptr) return child_;
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U ? children[0] : nullptr;
    }
    const Base::Ref<Base::Object>& OwnedChild() const noexcept { return ownedChild_; }
    Base::Result<void> SetChild(UIElement* child) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateChild(child);
        if (!valid) return valid.GetStatus();
        if (child_ == child) return {};
        child_ = child;
        if (child == nullptr) ownedChild_.Reset();
        return InvalidateMeasure();
    }
    Base::Result<void> SetOwnedChild(
        const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Decorator owned child does not match its UIElement");
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateChild(&child);
        if (!valid) return valid.GetStatus();
        child_ = &child;
        ownedChild_ = childObject;
        return InvalidateMeasure();
    }
protected:
    explicit Decorator(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override {
        UIElement* child = Child();
        if (child == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState,
                    "Decorator has multiple attached children");
            }
            return Size{};
        }
        Base::Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return measured.GetStatus();
        return child->DesiredSize();
    }
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        UIElement* child = Child();
        if (child == nullptr) return finalSize;
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return arranged.GetStatus();
        return finalSize;
    }
private:
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
    Presentation::Color Background() const noexcept {
        return Presentation::SampleBrush(
            BackgroundBrush());
    }
    Base::Ref<Presentation::Brush>
    BackgroundBrush() const noexcept {
        return GetValueOr(
            BackgroundProperty,
            Base::Ref<Presentation::Brush>{});
    }
    Base::Result<void> SetBackground(
        Presentation::Color value) noexcept {
        Base::Result<Base::Ref<Presentation::Brush>> brush =
            Presentation::MakeSolidColorBrush(value);
        return brush
            ? SetBackgroundBrush(
                std::move(brush).Value())
            : brush.GetStatus();
    }
    Base::Result<void> SetBackgroundBrush(
        Base::Ref<Presentation::Brush> value) noexcept {
        return SetValue(
            BackgroundProperty, std::move(value));
    }
    Presentation::Color BorderBrush() const noexcept {
        return Presentation::SampleBrush(
            BorderBrushObject());
    }
    Base::Ref<Presentation::Brush>
    BorderBrushObject() const noexcept {
        return GetValueOr(
            BorderBrushProperty,
            Base::Ref<Presentation::Brush>{});
    }
    Base::Result<void> SetBorderBrush(
        Presentation::Color value) noexcept {
        Base::Result<Base::Ref<Presentation::Brush>> brush =
            Presentation::MakeSolidColorBrush(value);
        return brush
            ? SetBorderBrushObject(
                std::move(brush).Value())
            : brush.GetStatus();
    }
    Base::Result<void> SetBorderBrushObject(
        Base::Ref<Presentation::Brush> value) noexcept {
        return SetValue(
            BorderBrushProperty, std::move(value));
    }
    Presentation::Thickness BorderThickness() const noexcept {
        return GetValueOr(
            BorderThicknessProperty,
            Presentation::Thickness{});
    }
    Base::Result<void> SetBorderThickness(
        Presentation::Thickness value) noexcept {
        return SetValue(BorderThicknessProperty, value);
    }
    Base::Result<void> SetBorderThickness(
        double value) noexcept {
        return SetBorderThickness(
            {value, value, value, value});
    }
    Presentation::Thickness Padding() const noexcept {
        return GetValueOr(
            PaddingProperty,
            Presentation::Thickness{});
    }
    Base::Result<void> SetPadding(
        Presentation::Thickness value) noexcept {
        return SetValue(PaddingProperty, value);
    }
    Presentation::HorizontalAlignment
    GetHorizontalContentAlignment() const noexcept {
        return GetValueOr(
            HorizontalContentAlignmentProperty,
            Presentation::HorizontalAlignment::Left);
    }
    Presentation::VerticalAlignment
    GetVerticalContentAlignment() const noexcept {
        return GetValueOr(
            VerticalContentAlignmentProperty,
            Presentation::VerticalAlignment::Top);
    }
    Presentation::Color Foreground() const noexcept {
        return Presentation::SampleBrush(
            ForegroundBrush(),
            0.5,
            Presentation::Color{
                0.0F, 0.0F, 0.0F, 1.0F});
    }
    Base::Ref<Presentation::Brush>
    ForegroundBrush() const noexcept {
        Base::Ref<Presentation::Brush> brush =
            GetValueOr(
            ForegroundProperty,
            Base::Ref<Presentation::Brush>{});
        const FrameworkElement* parent =
            RenderParent();
        while (!brush && parent != nullptr) {
            brush = parent->GetValueOr(
                ForegroundProperty,
                Base::Ref<Presentation::Brush>{});
            parent = parent->RenderParent();
        }
        return brush;
    }
    Base::Result<void> SetForeground(
        Presentation::Color value) noexcept {
        Base::Result<Base::Ref<Presentation::Brush>> brush =
            Presentation::MakeSolidColorBrush(value);
        return brush
            ? SetForegroundBrush(
                std::move(brush).Value())
            : brush.GetStatus();
    }
    Base::Result<void> SetForegroundBrush(
        Base::Ref<Presentation::Brush> value) noexcept {
        return SetValue(
            ForegroundProperty, std::move(value));
    }
    double FontSize() const noexcept {
        return GetValueOr(FontSizeProperty, 16.0);
    }
    Base::Result<void> SetFontSize(
        double value) noexcept {
        return SetValue(FontSizeProperty, value);
    }
    FontWeight GetFontWeight() const noexcept {
        return GetValueOr(FontWeightProperty, FontWeight::Normal);
    }
    Base::Result<void> SetFontWeight(
        FontWeight value) noexcept {
        return SetValue(FontWeightProperty, value);
    }
    Base::Ref<Presentation::Style>
    FocusVisualStyle() const noexcept {
        return GetValueOr(
            FocusVisualStyleProperty,
            Base::Ref<Presentation::Style>{});
    }
    Base::Result<void> SetFocusVisualStyle(
        Base::Ref<Presentation::Style> value) noexcept {
        return SetValue(
            FocusVisualStyleProperty,
            std::move(value));
    }
    bool OverridesDefaultStyle() const noexcept {
        return GetValueOr(OverridesDefaultStyleProperty, false);
    }
    Base::Result<void> SetOverridesDefaultStyle(
        bool value) noexcept {
        return SetValue(OverridesDefaultStyleProperty, value);
    }
    inline static constexpr Members::Property<
        Base::Ref<Presentation::Brush>>
        BackgroundProperty{"Background"};
    inline static constexpr Members::Property<
        Base::Ref<Presentation::Brush>>
        BorderBrushProperty{"BorderBrush"};
    inline static constexpr Members::Property<
        Presentation::Thickness>
        BorderThicknessProperty{"BorderThickness"};
    inline static constexpr Members::Property<
        Presentation::Thickness>
        PaddingProperty{"Padding"};
    inline static constexpr Members::Property<
        Presentation::HorizontalAlignment>
        HorizontalContentAlignmentProperty{
            "HorizontalContentAlignment"};
    inline static constexpr Members::Property<
        Presentation::VerticalAlignment>
        VerticalContentAlignmentProperty{
            "VerticalContentAlignment"};
    inline static constexpr auto ForegroundProperty =
        Presentation::
            FrameworkElementForegroundProperty;
    inline static constexpr Members::Property<double>
        FontSizeProperty{"FontSize"};
    inline static constexpr Members::Property<FontWeight>
        FontWeightProperty{"FontWeight"};
    inline static constexpr Members::Property<
        Base::Ref<Presentation::Style>>
        FocusVisualStyleProperty{"FocusVisualStyle"};
    inline static constexpr Members::Property<bool>
        OverridesDefaultStyleProperty{"OverridesDefaultStyle"};
    inline static constexpr Members::Property<
        Base::Ref<ControlTemplate>>
        TemplateProperty{"Template"};

    // Aero resolves implicit styles by the runtime type, which is the
    // equivalent of WPF's default style key for built-in controls.
    TypeId DefaultStyleKey() const noexcept {
        return RuntimeType();
    }
    bool IsTemplateApplied() const noexcept {
        return templateHandleValue_ != 0U;
    }
    std::uint64_t TemplateGeneration() const noexcept {
        return templateGeneration_;
    }
    // Returns true only when this call materialized a new template instance.
    // Repeated calls are intentionally idempotent.
    Base::Result<bool> ApplyTemplate() noexcept;
    DependencyObject* GetTemplateChild(
        Base::StringView name) const noexcept;
    DependencyObject* GetTemplateChild(
        TypeId type) const noexcept;
    UIElement* TemplateChild() const noexcept {
        return templateChild_;
    }
    Base::Result<void> SetTemplateChild(
        UIElement* child) noexcept {
        if (child != nullptr &&
            child->LayoutParent() != this) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Control template child must be visually attached");
        }
        if (templateChild_ != nullptr &&
            child != nullptr &&
            templateChild_ != child) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Control already has a template child");
        }
        templateChild_ = child;
        return InvalidateMeasure();
    }
protected:
    explicit Control(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Control() override = default;
    virtual Base::Result<void> OnApplyTemplate() noexcept {
        return {};
    }
    virtual void OnTemplateDetached() noexcept {}
    Base::Result<void> RaiseRoutedEvent(
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override {
        if (templateChild_ == nullptr) return Size{};
        Base::Result<void> measured =
            MeasureChild(*templateChild_, availableSize);
        if (!measured) return measured.GetStatus();
        return templateChild_->DesiredSize();
    }
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override {
        if (templateChild_ == nullptr) return finalSize;
        Base::Result<void> arranged = ArrangeChild(
            *templateChild_,
            {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return arranged.GetStatus();
        return finalSize;
    }
    Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;
private:
    friend class Aero::Detail::ControlRuntimeAccess;
    friend class Aero::Detail::RuntimePresentationServices;
    void AttachTemplateManager(
        TemplateManager& manager) noexcept {
        templateManager_ = &manager;
    }
    Base::Result<void> NotifyTemplateApplied(
        std::uint64_t handleValue) noexcept {
        templateHandleValue_ = handleValue;
        ++templateGeneration_;
        return OnApplyTemplate();
    }
    void NotifyTemplateDetached() noexcept {
        if (templateHandleValue_ != 0U) {
            OnTemplateDetached();
            templateHandleValue_ = 0U;
            ++templateGeneration_;
        }
    }
    TemplateManager* templateManager_ = nullptr;
    RoutedEventManager* routedEvents_ = nullptr;
    UIElement* templateChild_ = nullptr;
    std::uint64_t templateHandleValue_ = 0U;
    std::uint64_t templateGeneration_ = 0U;
};

class AERO_API ContentControl : public Control {
    AERO_DECLARE_TYPE(ContentControl, Control)
public:
    inline static constexpr Members::Property<Core::Value>
        ContentProperty{"Content"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ContentTemplateSelectorProperty{
            "ContentTemplateSelector"};

    UIElement* Content() const noexcept { return content_; }
    const Base::Ref<Base::Object>& OwnedContent() const noexcept { return ownedContent_; }
    const Base::Ref<Base::Object>& ContentValue() const noexcept {
        return contentValue_;
    }
    Core::Value MetadataContent() const noexcept {
        return GetValueOr(
            ContentProperty,
            Core::Value::NullObject(
                Core::TypeOf<Base::Object>()));
    }
    Base::Ref<Base::Object> ContentTemplate() const noexcept {
        return GetValueOr(
            ContentTemplateProperty,
            Base::Ref<Base::Object>{});
    }
    Base::Result<void> SetContentTemplate(
        Base::Ref<Base::Object> value) noexcept {
        return SetValue(
            ContentTemplateProperty,
            std::move(value));
    }
    Base::Ref<Base::Object>
    ContentTemplateSelector() const noexcept {
        return GetValueOr(
            ContentTemplateSelectorProperty,
            Base::Ref<Base::Object>{});
    }
    Base::Result<void> SetContentTemplateSelector(
        Base::Ref<Base::Object> value) noexcept {
        return SetValue(
            ContentTemplateSelectorProperty,
            std::move(value));
    }
    Base::Result<void> SetContent(UIElement* content) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateContent(content);
        if (!valid) return valid.GetStatus();
        Core::Value propertyValue =
            content != nullptr
            ? Core::Value::FromObject(
                  content->RuntimeType(),
                  Base::Ref<Base::Object>::FromBorrowed(
                      *content))
            : Core::Value::NullObject(
                  Core::TypeOf<Base::Object>());
        Base::Result<void> stored =
            StoreContentProperty(
                std::move(propertyValue));
        if (!stored) return stored.GetStatus();
        if (content_ == content) return {};
        content_ = content;
        literalTextContent_ = false;
        if (content == nullptr) {
            ownedContent_.Reset();
            contentValue_.Reset();
        }
        return InvalidateMeasure();
    }
    Base::Result<void> SetOwnedContent(
        const Base::Ref<Base::Object>& contentObject, UIElement& content) noexcept {
        if (!contentObject || contentObject.Get() != &content) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "ContentControl owned content does not match its UIElement");
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateContent(&content);
        if (!valid) return valid.GetStatus();
        Base::Result<void> stored =
            StoreContentProperty(
                Core::Value::FromObject(
                    contentObject->RuntimeType(),
                    contentObject));
        if (!stored) return stored.GetStatus();
        content_ = &content;
        ownedContent_ = contentObject;
        contentValue_ = contentObject;
        literalTextContent_ = false;
        return InvalidateMeasure();
    }
    // Stores arbitrary business content without exposing it as a visual.
    // A matching ContentTemplate can materialize it through
    // TryCreateTemplatedContent(); the UIElement overloads remain the
    // source-compatible direct-content path.
    Base::Result<void> SetContentValue(
        Base::Ref<Base::Object> value) noexcept;
    Base::Result<void> SetContentValue(
        Core::Value value) noexcept;
    static void OnContentPropertyChanged(
        Core::DependencyObject& object,
        const Core::DependencyPropertyChangedEventArgs&
            change) noexcept;
    Base::Result<Base::Ref<Base::Object>>
        TryCreateTemplatedContent() const noexcept;
protected:
    explicit ContentControl(TypeId runtimeType) noexcept;
    ~ContentControl() override;
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override {
        if (TemplateChild() != nullptr) {
            return Control::MeasureOverride(availableSize);
        }
        if (content_ == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState,
                    "ContentControl has attached children without content");
            }
            return Size{};
        }
        if (!IsOnlyAttachedContent(*content_)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentControl content attachment is invalid");
        }
        Base::Result<void> measured = MeasureChild(*content_, availableSize);
        if (!measured) return measured.GetStatus();
        return content_->DesiredSize();
    }
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        if (TemplateChild() != nullptr) {
            return Control::ArrangeOverride(finalSize);
        }
        if (content_ == nullptr) return finalSize;
        if (!IsOnlyAttachedContent(*content_)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentControl content attachment is invalid");
        }
        Base::Result<void> arranged = ArrangeChild(
            *content_, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return arranged.GetStatus();
        return finalSize;
    }
private:
    friend class ItemContainerGenerator;
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
    Base::Ref<Base::Object> contentValue_;
    Core::Value authoredContent_;
    DependencyPropertyChangedEventHandler
        foregroundChangedHandler_;
    bool literalTextContent_ = false;
    bool synchronizingContentProperty_ = false;
    Base::Result<void> StoreContentProperty(
        Core::Value value) noexcept;
    Base::Result<void> SetGeneratedTextContent(
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
class AERO_API Page final : public UserControl {
    AERO_DECLARE_TYPE(Page, UserControl)
public:
    Page() noexcept : UserControl(StaticTypeId()) {}
    ~Page() override = default;
};

} // namespace Aero::Controls
