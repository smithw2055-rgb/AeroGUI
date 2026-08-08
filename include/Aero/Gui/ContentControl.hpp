#pragma once

#include <Aero/Gui/Control.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
class AERO_GUI_API ContentControl : public Control {
    AERO_DECLARE_TYPE(ContentControl, Control)
public:
    inline static constexpr DependencyProperty<Value> ContentProperty{"Content"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ContentTemplateSelectorProperty{"ContentTemplateSelector"};

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
    friend struct ::Aero::Controls::Control::Access;
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
} // namespace Aero::Controls
