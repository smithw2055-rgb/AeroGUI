#pragma once

#include <Aero/Controls/Control.hpp>


namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
class AERO_GUI_API ContentControl : public Control {
    AERO_DECLARE_TYPE(ContentControl, Control)
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
public:
    inline static constexpr DependencyProperty<Value> ContentProperty{"Content"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ContentTemplateSelectorProperty{"ContentTemplateSelector"};

    Value GetContent() const noexcept {
        return GetValue(ContentProperty);
    }
    Ref<Base::Object> GetContentTemplate() const noexcept {
        return GetValue(ContentTemplateProperty);
    }
    void SetContentTemplate(
        Ref<Base::Object> value) noexcept {
        SetValue(ContentTemplateProperty, std::move(value));
    }
    Ref<Base::Object>
    GetContentTemplateSelector() const noexcept {
        return GetValue(ContentTemplateSelectorProperty);
    }
    void SetContentTemplateSelector(
        Ref<Base::Object> value) noexcept {
        SetValue(ContentTemplateSelectorProperty, std::move(value));
    }
    void SetContent(Ref<Base::Object> content) noexcept {
        SetContentValue(std::move(content));
    }
    void SetContent(Value content) noexcept {
        SetContentValue(std::move(content));
    }
    void SetContent(StringView text) noexcept;
    void SetContent(const char* text) noexcept;
    template<class T,
        class = std::enable_if_t<
            std::is_base_of_v<UIElement, T> &&
            !std::is_same_v<T, Base::Object>>>
    void SetContent(Ref<T> element) noexcept {
        SetContent(element.Get());
    }
    void SetContent(UIElement* content) noexcept {
        Result<void> access = VerifyAccess();
        if (!access) return;
        Result<void> valid = ValidateContent(content);
        if (!valid) return;
        Value propertyValue =
            content != nullptr
            ? Value::FromObject(
                  content->RuntimeType(),
                  Ref<Base::Object>::FromBorrowed(
                      *content))
            : Value::NullObject(
                  Meta::TypeOf<Base::Object>());
        Result<void> stored =
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
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        if (GetTemplateRoot() != nullptr) {
            return Control::GetVisualChildrenCount();
        }
        return content_ != nullptr && content_->GetVisualParent() == this ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (GetTemplateRoot() != nullptr) {
            return Control::GetVisualChild(index);
        }
        if (index != 0U || content_ == nullptr || content_->GetVisualParent() != this) {
            return nullptr;
        }
        return content_;
    }
    std::uint32_t GetLogicalChildrenCount() const noexcept override {
        return content_ != nullptr ? 1U : Control::GetLogicalChildrenCount();
    }
    DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept override {
        if (content_ != nullptr) {
            return index == 0U ? content_ : nullptr;
        }
        return Control::GetLogicalChild(index);
    }
    void EnsureHostedContent() noexcept;
    Size MeasureOverride(Size availableSize) noexcept override {
        if (GetTemplateRoot() != nullptr) {
            return Control::MeasureOverride(availableSize);
        }
        EnsureHostedContent();
        if (content_ == nullptr) {
            return Size{};
        }
        (void)MeasureChild(*content_, availableSize);
        return content_->GetDesiredSize();
    }
    Size ArrangeOverride(Size finalSize) noexcept override {
        if (GetTemplateRoot() != nullptr) {
            return Control::ArrangeOverride(finalSize);
        }
        EnsureHostedContent();
        if (content_ == nullptr) return finalSize;
        Result<void> arranged = ArrangeChild(
            *content_, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
        return finalSize;
    }
private:
    void SetOwnedContent(
        const Ref<Base::Object>& contentObject, UIElement& content) noexcept {
        if (!contentObject || contentObject.Get() != &content) {
            return;
        }
        Result<void> access = VerifyAccess();
        if (!access) return;
        Result<void> valid = ValidateContent(&content);
        if (!valid) return;
        Result<void> stored =
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
        Ref<Base::Object> value) noexcept;
    void SetContentValue(
        Value value) noexcept;
    static void OnContentPropertyChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs&
            change) noexcept;
    Result<Ref<Base::Object>>
        CreateTemplatedContent() const noexcept;
    UIElement* content_ = nullptr;
    Ref<Base::Object> ownedContent_;
    Ref<Base::Object> contentValue_;
    Value authoredContent_;
    DependencyPropertyChangedEventHandler
        foregroundChangedHandler_;
    DependencyPropertyChangedEventHandler
        fontSizeChangedHandler_;
    bool literalTextContent_ = false;
    bool synchronizingContentProperty_ = false;
    Result<void> StoreContentProperty(
        Value value) noexcept;
    void SetGeneratedTextContent(
        const Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;
    void SyncGeneratedTextFormatting() noexcept;
    void OnForegroundChanged(
        DependencyObject&,
        const DependencyPropertyChangedEventArgs&) noexcept;
    void OnFontSizeChanged(
        DependencyObject&,
        const DependencyPropertyChangedEventArgs&) noexcept;
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept {
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U && children[0] == &content;
    }
    Result<void> ValidateContent(UIElement* content) const noexcept {
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
} // namespace Aero::Controls
