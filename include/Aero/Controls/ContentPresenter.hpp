#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Value.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_GUI_API ContentPresenter : public FrameworkElement {
    AERO_DECLARE_TYPE(ContentPresenter, FrameworkElement)
public:
    ContentPresenter() noexcept;
    UIElement* GetContent() const noexcept { return content_; }
    const Ref<Base::Object>& GetOwnedContent() const noexcept { return ownedContent_; }
    const Value& GetContentValue() const noexcept {
        return contentValue_;
    }
    StringView GetContentSource() const noexcept {
        return GetValueOr(
            ContentSourceProperty,
            StringView{});
    }
    void SetContentSource(
        StringView value) noexcept;
    void SetContentValue(
        Value value) noexcept {
        SetValue(ContentProperty, std::move(value));
    }
    void SetContent(UIElement* content) noexcept;

    // Clear the presenter-owned reference. Callers that still have a visual
    // or layout edge must detach it through the element tree first; leftover
    // layout children must not block replacing a TextBlock host with a
    // UIElement header (gallery SampleSection Header StackPanel).
    void SetContent(std::nullptr_t) noexcept {
        Result<void> access = VerifyAccess();
        if (!access) return;
        if (content_ == nullptr) return;
        content_ = nullptr;
        ownedContent_.Reset();
        (void)InvalidateMeasure();
        return;
    }

    void SetOwnedContent(const Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;

    // Host a UIElement as Content, replacing any auto-created TextBlock from
    // ContentSource. Used by TemplateBinding ContentSource=Header and by
    // item generators that project DataTemplate visuals into PART_Header.
    void HostUiElement(
        const Ref<Base::Object>& owner,
        UIElement& element) noexcept;

    inline static constexpr DependencyProperty<String> ContentSourceProperty{"ContentSource"};
    inline static constexpr DependencyProperty<Value> ContentProperty{"Content"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ContentTemplateProperty{"ContentTemplate"};
    static void OnContentPropertyChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs&
            change) noexcept;
protected:
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        return content_ != nullptr && content_->GetVisualParent() == this ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (index != 0U || content_ == nullptr || content_->GetVisualParent() != this) {
            return nullptr;
        }
        return content_;
    }
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    UIElement* content_ = nullptr;
    Ref<Base::Object> ownedContent_;
    Value contentValue_ =
        Value::NullObject(
            Meta::TypeOf<Base::Object>());
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept;
    Result<void> ValidateContent(UIElement* content) const noexcept;
    Result<void> UpdatePresentedText() noexcept;
};
} // namespace Aero::Controls
