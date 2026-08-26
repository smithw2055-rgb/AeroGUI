#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/DrawingContext.hpp>
#include <Aero/Controls/BulletDecorator.hpp>

#include <algorithm>


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
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        return child_ != nullptr && child_->GetVisualParent() == this ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (index != 0U || child_ == nullptr || child_->GetVisualParent() != this) {
            return nullptr;
        }
        return child_;
    }
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
    friend class ::Aero::AeroGuiInternal;
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

} // namespace Aero::Controls
