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

    // Template teardown is a two-step transaction: clear the presenter-owned
    // reference first, then detach the visual/layout/render edge through the Gui context. A nullptr literal selects this overload without weakening
    // the ordinary UIElement* validation path.
    void SetContent(std::nullptr_t) noexcept {
        Result<void> access = VerifyAccess();
        if (!access) return;
        if (content_ == nullptr) return;
        if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content_)) {
            return;
        }
        content_ = nullptr;
        ownedContent_.Reset();
        return;
    }

    void SetOwnedContent(const Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;

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
