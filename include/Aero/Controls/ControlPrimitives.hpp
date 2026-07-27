#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Type.hpp>

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero::Presentation;

class ControlTemplate;

class AERO_API Panel : public FrameworkElement {
    AERO_DECLARE_TYPE(Panel, FrameworkElement)
public:
    std::uint32_t OwnedChildCount() const noexcept { return ownedChildren_.Size(); }
    Base::Result<void> AddOwnedChild(
        const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Panel owned child does not match its UIElement");
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        for (const Base::Ref<Base::Object>& owned : ownedChildren_) {
            if (owned.Get() == &child) {
                return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                    "Panel already owns the child");
            }
        }
        Base::Result<void> appended = ownedChildren_.TryPushBack(childObject);
        if (!appended) return appended.GetStatus();
        return InvalidateMeasure();
    }
    Base::Result<void> ClearOwnedChildren() noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        if (!LayoutChildren().Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Panel children must be detached before releasing ownership");
        }
        ownedChildren_.Clear();
        return InvalidateMeasure();
    }
protected:
    explicit Panel(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType), ownedChildren_() {}
    ~Panel() override = default;
private:
    Base::Vector<Base::Ref<Base::Object>> ownedChildren_;
};

class AERO_API Decorator : public FrameworkElement {
    AERO_DECLARE_TYPE(Decorator, FrameworkElement)
public:
    UIElement* Child() const noexcept {
        if (child_ != nullptr) return child_;
        const UIElementChildRange children = LayoutChildren();
        return children.Size() == 1U ? children[0] : nullptr;
    }
    const Base::Ref<Base::Object>& OwnedChild() const noexcept { return ownedChild_; }
    Base::Result<void> SetChild(UIElement* child) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateChild(child);
        if (!valid) return valid.GetStatus();
        if (child_ == child) return {};
        child_ = child;
        if (child == nullptr) ownedChild_.Reset();
        return InvalidateMeasure();
    }
    Base::Result<void> SetOwnedChild(
        const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
        if (!childObject || childObject.Get() != &child) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Decorator owned child does not match its UIElement");
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateChild(&child);
        if (!valid) return valid.GetStatus();
        child_ = &child;
        ownedChild_ = childObject;
        return InvalidateMeasure();
    }
protected:
    explicit Decorator(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Decorator() override = default;
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override {
        UIElement* child = Child();
        if (child == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState,
                    "Decorator has multiple attached children");
            }
            return Size{};
        }
        Base::Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return measured.GetStatus();
        return child->DesiredSize();
    }
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        UIElement* child = Child();
        if (child == nullptr) return finalSize;
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return arranged.GetStatus();
        return finalSize;
    }
private:
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

class AERO_API Control : public FrameworkElement {
    AERO_DECLARE_TYPE(Control, FrameworkElement)
public:
    inline static constexpr auto TemplateProperty =
        Members::Property<ControlTemplate>{"Template"};

    UIElement* TemplateChild() const noexcept {
        return templateChild_;
    }
    Base::Result<void> SetTemplateChild(
        UIElement* child) noexcept {
        if (child != nullptr &&
            child->LayoutParent() != this) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Control template child must be visually attached");
        }
        if (templateChild_ != nullptr &&
            child != nullptr &&
            templateChild_ != child) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Control already has a template child");
        }
        templateChild_ = child;
        return InvalidateMeasure();
    }
protected:
    explicit Control(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Control() override = default;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override {
        if (templateChild_ == nullptr) return Size{};
        Base::Result<void> measured =
            MeasureChild(*templateChild_, availableSize);
        if (!measured) return measured.GetStatus();
        return templateChild_->DesiredSize();
    }
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override {
        if (templateChild_ == nullptr) return finalSize;
        Base::Result<void> arranged = ArrangeChild(
            *templateChild_,
            {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return arranged.GetStatus();
        return finalSize;
    }
private:
    UIElement* templateChild_ = nullptr;
};

class AERO_API ContentControl : public Control {
    AERO_DECLARE_TYPE(ContentControl, Control)
public:
    UIElement* Content() const noexcept { return content_; }
    const Base::Ref<Base::Object>& OwnedContent() const noexcept { return ownedContent_; }
    Base::Result<void> SetContent(UIElement* content) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateContent(content);
        if (!valid) return valid.GetStatus();
        if (content_ == content) return {};
        content_ = content;
        if (content == nullptr) ownedContent_.Reset();
        return InvalidateMeasure();
    }
    Base::Result<void> SetOwnedContent(
        const Base::Ref<Base::Object>& contentObject, UIElement& content) noexcept {
        if (!contentObject || contentObject.Get() != &content) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "ContentControl owned content does not match its UIElement");
        }
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        Base::Result<void> valid = ValidateContent(&content);
        if (!valid) return valid.GetStatus();
        content_ = &content;
        ownedContent_ = contentObject;
        return InvalidateMeasure();
    }
protected:
    explicit ContentControl(TypeId runtimeType) noexcept : Control(runtimeType) {}
    ~ContentControl() override = default;
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override {
        if (TemplateChild() != nullptr) {
            return Control::MeasureOverride(availableSize);
        }
        if (content_ == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState,
                    "ContentControl has attached children without content");
            }
            return Size{};
        }
        if (!IsOnlyAttachedContent(*content_)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentControl content attachment is invalid");
        }
        Base::Result<void> measured = MeasureChild(*content_, availableSize);
        if (!measured) return measured.GetStatus();
        return content_->DesiredSize();
    }
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        if (TemplateChild() != nullptr) {
            return Control::ArrangeOverride(finalSize);
        }
        if (content_ == nullptr) return finalSize;
        if (!IsOnlyAttachedContent(*content_)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentControl content attachment is invalid");
        }
        Base::Result<void> arranged = ArrangeChild(
            *content_, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return arranged.GetStatus();
        return finalSize;
    }
private:
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
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

class AERO_API UserControl : public ContentControl {
    AERO_DECLARE_TYPE(UserControl, ContentControl)
public:
    UserControl() noexcept : ContentControl(StaticTypeId()) {}
    ~UserControl() override = default;
protected:
    explicit UserControl(TypeId runtimeType) noexcept : ContentControl(runtimeType) {}
};

} // namespace Aero::Controls
