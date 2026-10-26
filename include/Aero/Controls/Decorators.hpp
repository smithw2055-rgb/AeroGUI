#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/DrawingContext.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Controls/Control.hpp>
#include <algorithm>
#include <utility>

namespace Aero::Controls {

using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;
using ::Aero::Media::ImageSource;
using ::Aero::Media::MatrixTransform;

// ---------------- Decorator Base & BulletDecorator ----------------

class AERO_GUI_API Decorator : public FrameworkElement {
    AERO_DECLARE_TYPE(Decorator, FrameworkElement)

public:
    Decorator() noexcept : Decorator(StaticTypeId()) {}
    ~Decorator() override {
        if (child_ != nullptr && child_->GetVisualParent() == this) { RemoveVisualChild(child_); }
    }

    UIElement* GetChild() const noexcept {
        if (child_ != nullptr) return child_;
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U ? children[0] : nullptr;
    }
    void SetChild(UIElement* child) noexcept {
        if (!CheckAccess() || !ValidateChild(child)) return;
        if (child_ == child) return;
        if (child_ != nullptr && child_->GetVisualParent() == this) { RemoveVisualChild(child_); }
        child_ = child;
        if (child_ != nullptr && child_->GetVisualParent() == nullptr) { AddVisualChild(child_); }
        if (child == nullptr) ownedChild_.Reset();
    }
    void SetOwnedChild(const Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) { return; }
        if (!CheckAccess() || !ValidateChild(&child)) return;
        if (child_ == &child) {
            ownedChild_ = childObject;
            return;
        }
        if (child_ != nullptr && child_->GetVisualParent() == this) { RemoveVisualChild(child_); }
        child_ = &child;
        ownedChild_ = childObject;
        if (child_->GetVisualParent() == nullptr) { AddVisualChild(child_); }
    }

    const Ref<Base::Object>& OwnedChild() const noexcept { return ownedChild_; }

protected:
    explicit Decorator(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}

    std::uint32_t GetVisualChildrenCount() const noexcept override {
        return child_ != nullptr && child_->GetVisualParent() == this ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (index != 0U || child_ == nullptr || child_->GetVisualParent() != this) { return nullptr; }
        return child_;
    }
    Size MeasureOverride(Size availableSize) noexcept override {
        UIElement* child = GetChild();
        if (child == nullptr) return Size{};
        MeasureChild(*child, availableSize);
        return child->GetDesiredSize();
    }
    Size ArrangeOverride(Size finalSize) noexcept override {
        UIElement* child = GetChild();
        if (child == nullptr) return finalSize;
        ArrangeChild(*child, {0.0, 0.0, finalSize.width, finalSize.height});
        return finalSize;
    }

private:
    bool ValidateChild(UIElement* child) const noexcept {
        if (child == nullptr) {
            return LayoutChildren().Empty();
        }
        return LayoutChildren().Empty() || IsOnlyAttachedChild(*child);
    }

    bool IsOnlyAttachedChild(const UIElement& child) const noexcept {
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U && children[0] == &child;
    }

    UIElement* child_ = nullptr;
    Ref<Base::Object> ownedChild_;
};

class AERO_GUI_API BulletDecorator : public FrameworkElement {
    AERO_DECLARE_TYPE(BulletDecorator, FrameworkElement)

public:
    BulletDecorator() noexcept : FrameworkElement(StaticTypeId()) {}

    UIElement* GetBullet() const noexcept { return bullet_.Get(); }
    UIElement* GetChild() const noexcept { return child_.Get(); }
    Ref<Media::Brush> GetBackground() const noexcept { return GetValue(BackgroundProperty); }
    void SetBackground(Ref<Media::Brush> value) noexcept { SetValue(BackgroundProperty, std::move(value)); }
    bool HitTestCore() const noexcept override { return GetBackground() != nullptr; }
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

    AERO_DEPENDENCY_PROPERTY(Ref<Media::Brush>, Background);

protected:
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        std::uint32_t count = 0U;
        if (bullet_) ++count;
        if (child_) ++count;
        return count;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (bullet_) {
            if (index == 0U) return bullet_.Get();
            --index;
        }
        if (child_ && index == 0U) { return child_.Get(); }
        return nullptr;
    }
    Size MeasureOverride(Size availableSize) noexcept override {
        Size bulletSize{};
        if (bullet_) {
            MeasureChild(*bullet_, availableSize);
            bulletSize = bullet_->GetDesiredSize();
        }
        Size childSize{};
        if (child_) {
            const Size childAvailable{
                std::max(0.0f, availableSize.width - bulletSize.width),
                availableSize.height};
            MeasureChild(*child_, childAvailable);
            childSize = child_->GetDesiredSize();
        }
        return {
            bulletSize.width + childSize.width,
            std::max(bulletSize.height, childSize.height)};
    }
    Size ArrangeOverride(Size finalSize) noexcept override {
        float bulletWidth = 0.0f;
        if (bullet_) {
            const Size desired = bullet_->GetDesiredSize();
            bulletWidth = std::min(finalSize.width, desired.width);
            const float y = std::max(0.0f, (finalSize.height - desired.height) * 0.5f);
            ArrangeChild(*bullet_, {
                0.0f, y, bulletWidth,
                std::min(finalSize.height, desired.height)});
        }
        if (child_) {
            ArrangeChild(*child_, {
                bulletWidth, 0.0f,
                std::max(0.0f, finalSize.width - bulletWidth), finalSize.height});
        }
        return finalSize;
    }
    void OnRender(Media::DrawingContext& context) noexcept override {
        const Ref<Media::Brush> background = GetBackground();
        if (background) {
            context.DrawRectangle({0.0, 0.0, GetRenderSize().width, GetRenderSize().height}, background);
        }
    }

private:
    Ref<UIElement> bullet_;
    Ref<UIElement> child_;
};

// ---------------- Border ----------------

class AERO_GUI_API Border : public Decorator {
    AERO_DECLARE_TYPE(Border, Decorator)

public:
    Border() noexcept;

    bool HitTestCore() const noexcept override {
        return GetBackground() != nullptr || GetBorderBrush() != nullptr;
    }

    Ref<Brush> GetBackground() const noexcept;
    void SetBackground(Ref<Brush> value) noexcept;
    Ref<Brush> GetBorderBrush() const noexcept;
    void SetBorderBrush(Ref<Brush> value) noexcept;
    Thickness GetBorderThickness() const noexcept;
    void SetBorderThickness(Thickness value) noexcept;
    void SetBorderThickness(double value) noexcept;
    CornerRadius GetCornerRadius() const noexcept;
    void SetCornerRadius(CornerRadius value) noexcept;
    void SetCornerRadius(double value) noexcept;
    Thickness GetPadding() const noexcept;
    void SetPadding(Thickness value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Background);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, BorderBrush);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::Thickness, BorderThickness);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::CornerRadius, CornerRadius);
    AERO_DEPENDENCY_PROPERTY(Aero::Base::Thickness, Padding);

protected:
    explicit Border(TypeId runtimeType) noexcept : Decorator(runtimeType) {}

    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
};

// ---------------- Viewbox ----------------

class AERO_GUI_API Viewbox : public Decorator {
    AERO_DECLARE_TYPE(Viewbox, Decorator)

public:
    Viewbox() noexcept : Decorator(StaticTypeId()) {}

    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    void SetStretch(Stretch value) noexcept;
    void SetStretchDirection(StretchDirection value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Stretch, Stretch);
    AERO_DEPENDENCY_PROPERTY(StretchDirection, StretchDirection);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    Ref<MatrixTransform> viewTransform_;
    Ref<FrameworkElement> projectedChild_;
    void ApplyViewTransform(double scaleX, double scaleY, double offsetX, double offsetY) noexcept;
};

// ---------------- Separator ----------------

class AERO_GUI_API Separator : public Control {
    AERO_DECLARE_TYPE(Separator, Control)

public:
    Separator() noexcept : Control(StaticTypeId()) {}
    ~Separator() override = default;
};

// ---------------- Image ----------------

class AERO_GUI_API Image : public FrameworkElement {
    AERO_DECLARE_TYPE(Image, FrameworkElement)

public:
    Image() noexcept : FrameworkElement(StaticTypeId()) {}
    ~Image() override = default;

    bool HitTestCore() const noexcept override { return GetSource() != nullptr; }

    Ref<ImageSource> GetSource() const noexcept;
    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    void SetSource(Ref<ImageSource> value) noexcept;
    void SetStretch(Stretch value) noexcept;
    void SetStretchDirection(StretchDirection value) noexcept;
    void SetRuntimeData(
        std::uint64_t renderImage,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<ImageSource>, Source);
    AERO_DEPENDENCY_PROPERTY(Stretch, Stretch);
    AERO_DEPENDENCY_PROPERTY(StretchDirection, StretchDirection);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};

} // namespace Aero::Controls
