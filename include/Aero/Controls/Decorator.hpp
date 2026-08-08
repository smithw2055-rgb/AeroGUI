#pragma once

#include <Aero/FrameworkElement.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_API Decorator : public FrameworkElement {
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
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        Base::Result<void> valid = ValidateChild(child);
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
        Base::Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return Size{};
        return child->GetDesiredSize();
    }
    Size ArrangeOverride(Size finalSize) noexcept override {
        UIElement* child = GetChild();
        if (child == nullptr) return finalSize;
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
        return finalSize;
    }
private:
    friend struct ::Aero::Visual::Impl;
    void SetOwnedChild(
        const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) {
            return;
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        Base::Result<void> valid = ValidateChild(&child);
        if (!valid) return;
        child_ = &child;
        ownedChild_ = childObject;
        return;
    }
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
} // namespace Aero::Controls
