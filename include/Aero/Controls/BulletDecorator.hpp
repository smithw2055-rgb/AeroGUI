#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/DrawingContext.hpp>

#include <algorithm>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

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
        return GetValue(BackgroundProperty);
    }
    void SetBackground(Ref<Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    void SetBullet(Ref<UIElement> value) noexcept {
        UIElement* previous = bullet_.Get();
        if (previous == value.Get()) {
            bullet_ = std::move(value);
            return;
        }
        if (previous != nullptr) RemoveVisualChild(previous);
        bullet_ = std::move(value);
        if (bullet_) AddVisualChild(bullet_.Get());
    }
    void SetChild(Ref<UIElement> value) noexcept {
        UIElement* previous = child_.Get();
        if (previous == value.Get()) {
            child_ = std::move(value);
            return;
        }
        if (previous != nullptr) RemoveVisualChild(previous);
        child_ = std::move(value);
        if (child_) AddVisualChild(child_.Get());
    }

    inline static constexpr DependencyProperty<Ref<Media::Brush>> BackgroundProperty{"Background"};

protected:
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        std::uint32_t count = 0U;
        if (bullet_ && bullet_->GetVisualParent() == this) ++count;
        if (child_ && child_->GetVisualParent() == this) ++count;
        return count;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (bullet_ && bullet_->GetVisualParent() == this) {
            if (index == 0U) return bullet_.Get();
            --index;
        }
        if (child_ && child_->GetVisualParent() == this && index == 0U) {
            return child_.Get();
        }
        return nullptr;
    }
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
