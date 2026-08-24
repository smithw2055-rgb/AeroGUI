#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>

#include <algorithm>

namespace Aero::Core { class VisualFacet; }

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_GUI_API Decorator : public FrameworkElement {
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
        Result<void> access = VerifyAccess();
        if (!access) return;
        Result<void> valid = ValidateChild(child);
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
        Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return Size{};
        return child->GetDesiredSize();
    }
    Size ArrangeOverride(Size finalSize) noexcept override {
        UIElement* child = GetChild();
        if (child == nullptr) return finalSize;
        Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
        return finalSize;
    }
private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::Core::VisualFacet;
#endif
    void SetOwnedChild(
        const Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) {
            return;
        }
        Result<void> access = VerifyAccess();
        if (!access) return;
        Result<void> valid = ValidateChild(&child);
        if (!valid) return;
        child_ = &child;
        ownedChild_ = childObject;
        return;
    }
    UIElement* child_ = nullptr;
    Ref<Base::Object> ownedChild_;
    bool IsOnlyAttachedChild(const UIElement& child) const noexcept {
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U && children[0] == &child;
    }
    Result<void> ValidateChild(UIElement* child) const noexcept {
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

// WPF-compatible two-part decorator used by radio-button and check-box
// templates. The bullet occupies its desired width on the leading edge while
// the regular content receives the remaining slot.
class AERO_GUI_API BulletDecorator : public FrameworkElement {
    AERO_DECLARE_TYPE(BulletDecorator, FrameworkElement)
public:
    BulletDecorator() noexcept : FrameworkElement(StaticTypeId()) {}

    UIElement* GetBullet() const noexcept { return bullet_.Get(); }
    UIElement* GetChild() const noexcept { return child_.Get(); }
    Ref<Media::Brush> GetBackground() const noexcept {
        return GetValueOr(BackgroundProperty, Ref<Media::Brush>{});
    }
    void SetBackground(Ref<Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    void SetBullet(Ref<UIElement> value) noexcept {
        bullet_ = std::move(value);
    }
    void SetChild(Ref<UIElement> value) noexcept {
        child_ = std::move(value);
    }

    inline static constexpr DependencyProperty<Ref<Media::Brush>> BackgroundProperty{"Background"};

protected:
    Size MeasureOverride(Size availableSize) noexcept override {
        Size bulletSize{};
        if (bullet_) {
            if (MeasureChild(*bullet_, availableSize)) {
                bulletSize = bullet_->GetDesiredSize();
            }
        }
        Size childSize{};
        if (child_) {
            const Size childAvailable{
                std::max(0.0, availableSize.width - bulletSize.width),
                availableSize.height};
            if (MeasureChild(*child_, childAvailable)) {
                childSize = child_->GetDesiredSize();
            }
        }
        return {
            bulletSize.width + childSize.width,
            std::max(bulletSize.height, childSize.height)};
    }

    Size ArrangeOverride(Size finalSize) noexcept override {
        double bulletWidth = 0.0;
        if (bullet_) {
            const Size desired = bullet_->GetDesiredSize();
            bulletWidth = std::min(finalSize.width, desired.width);
            const double y = std::max(
                0.0, (finalSize.height - desired.height) * 0.5);
            (void)ArrangeChild(*bullet_, {
                0.0, y, bulletWidth,
                std::min(finalSize.height, desired.height)});
        }
        if (child_) {
            (void)ArrangeChild(*child_, {
                bulletWidth, 0.0,
                std::max(0.0, finalSize.width - bulletWidth),
                finalSize.height});
        }
        return finalSize;
    }

    void OnRender(Media::DrawingContext& context) noexcept override {
        const Ref<Media::Brush> background = GetBackground();
        if (background) {
            (void)context.DrawRectangle(
                {0.0, 0.0, GetRenderSize().width, GetRenderSize().height},
                background);
        }
    }

private:
    Ref<UIElement> bullet_;
    Ref<UIElement> child_;
};
} // namespace Aero::Controls
