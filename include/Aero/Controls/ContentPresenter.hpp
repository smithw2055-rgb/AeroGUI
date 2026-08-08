#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Value.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_API ContentPresenter : public FrameworkElement {
    AERO_DECLARE_TYPE(ContentPresenter, FrameworkElement)
public:
    ContentPresenter() noexcept;
    UIElement* GetContent() const noexcept { return content_; }
    const Base::Ref<Base::Object>& GetOwnedContent() const noexcept { return ownedContent_; }
    const Value& GetContentValue() const noexcept {
        return contentValue_;
    }
    Base::StringView GetContentSource() const noexcept {
        return GetValueOr(
            ContentSourceProperty,
            Base::StringView{});
    }
    void SetContentSource(
        Base::StringView value) noexcept;
    void SetContentValue(
        Value value) noexcept {
        SetValue(ContentProperty, std::move(value));
    }
    void SetContent(UIElement* content) noexcept;

    // Template teardown is a two-step transaction: clear the presenter-owned
    // reference first, then detach the visual/layout/render edge through the Gui context. A nullptr literal selects this overload without weakening
    // the ordinary UIElement* validation path.
    void SetContent(std::nullptr_t) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        if (content_ == nullptr) return;
        if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content_)) {
            return;
        }
        content_ = nullptr;
        ownedContent_.Reset();
        return;
    }

    void SetOwnedContent(const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;

    inline static constexpr DependencyProperty<Base::String> ContentSourceProperty{"ContentSource"};
    inline static constexpr DependencyProperty<Value> ContentProperty{"Content"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ContentTemplateProperty{"ContentTemplate"};
    static void OnContentPropertyChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs&
            change) noexcept;
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
    Value contentValue_ =
        Value::NullObject(
            Meta::TypeOf<Base::Object>());
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept;
    Base::Result<void> ValidateContent(UIElement* content) const noexcept;
    Base::Result<void> UpdatePresentedText() noexcept;
};
} // namespace Aero::Controls
