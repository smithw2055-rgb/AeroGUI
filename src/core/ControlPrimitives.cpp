#include <Aero/Core/ControlPrimitives.hpp>

namespace Aero::Core {
namespace {

Base::Status InvalidContent(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidContentState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

Panel::Panel(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType), ownedChildren_() {}

bool Panel::IsOwnedChild(const UIElement& child) const noexcept {
    for (const Base::Ref<Base::Object>& owned : ownedChildren_) {
        if (owned.Get() == &child) return true;
    }
    return false;
}

Base::Result<void> Panel::AddOwnedChild(
    const Base::Ref<Base::Object>& childObject,
    UIElement& child) noexcept {
    if (!childObject || childObject.Get() != &child) {
        return InvalidContent("Panel owned child does not match its UIElement");
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (IsOwnedChild(child)) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Panel already owns the child");
    }
    Base::Result<void> appended = ownedChildren_.TryPushBack(childObject);
    if (!appended) return appended.GetStatus();
    return InvalidateMeasure();
}

Base::Result<void> Panel::ClearOwnedChildren() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!LayoutChildren().Empty()) {
        return InvalidContentState(
            "Panel children must be detached before releasing ownership");
    }
    ownedChildren_.Clear();
    return InvalidateMeasure();
}

Decorator::Decorator(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType) {}

bool Decorator::IsOnlyAttachedChild(const UIElement& child) const noexcept {
    const UIElementChildRange children = LayoutChildren();
    return children.Size() == 1U && children[0] == &child;
}

Base::Result<void> Decorator::ValidateChild(UIElement* child) const noexcept {
    if (child == nullptr) {
        if (!LayoutChildren().Empty()) {
            return InvalidContentState(
                "Decorator child must be detached before clearing it");
        }
    } else if (!LayoutChildren().Empty() && !IsOnlyAttachedChild(*child)) {
        return InvalidContentState(
            "Decorator child must be its only attached UIElement");
    }
    return {};
}

Base::Result<void> Decorator::SetChild(UIElement* child) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    Base::Result<void> valid = ValidateChild(child);
    if (!valid) return valid.GetStatus();
    if (child_ == child) return {};
    child_ = child;
    if (child == nullptr) ownedChild_.Reset();
    return InvalidateMeasure();
}

Base::Result<void> Decorator::SetOwnedChild(
    const Base::Ref<Base::Object>& childObject,
    UIElement& child) noexcept {
    if (!childObject || childObject.Get() != &child) {
        return InvalidContent(
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

Base::Result<Size> Decorator::MeasureOverride(Size availableSize) noexcept {
    if (child_ == nullptr) {
        if (!LayoutChildren().Empty()) {
            return InvalidContentState(
                "Decorator has attached children without a configured child");
        }
        return Size{};
    }
    if (!IsOnlyAttachedChild(*child_)) {
        return InvalidContentState("Decorator child attachment is invalid");
    }
    Base::Result<void> measured = MeasureChild(*child_, availableSize);
    if (!measured) return measured.GetStatus();
    return child_->DesiredSize();
}

Base::Result<Size> Decorator::ArrangeOverride(Size finalSize) noexcept {
    if (child_ == nullptr) return finalSize;
    if (!IsOnlyAttachedChild(*child_)) {
        return InvalidContentState("Decorator child attachment is invalid");
    }
    Base::Result<void> arranged = ArrangeChild(
        *child_, {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return arranged.GetStatus();
    return finalSize;
}

Control::Control(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType) {}

ContentControl::ContentControl(TypeId runtimeType) noexcept
    : Control(runtimeType) {}

bool ContentControl::IsOnlyAttachedContent(
    const UIElement& content) const noexcept {
    const UIElementChildRange children = LayoutChildren();
    return children.Size() == 1U && children[0] == &content;
}

Base::Result<void> ContentControl::ValidateContent(
    UIElement* content) const noexcept {
    if (content == nullptr) {
        if (!LayoutChildren().Empty()) {
            return InvalidContentState(
                "ContentControl content must be detached before clearing it");
        }
    } else if (!LayoutChildren().Empty() &&
        !IsOnlyAttachedContent(*content)) {
        return InvalidContentState(
            "ContentControl content must be its only attached UIElement");
    }
    return {};
}

Base::Result<void> ContentControl::SetContent(UIElement* content) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    Base::Result<void> valid = ValidateContent(content);
    if (!valid) return valid.GetStatus();
    if (content_ == content) return {};
    content_ = content;
    if (content == nullptr) ownedContent_.Reset();
    return InvalidateMeasure();
}

Base::Result<void> ContentControl::SetOwnedContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) {
        return InvalidContent(
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

Base::Result<Size> ContentControl::MeasureOverride(
    Size availableSize) noexcept {
    if (content_ == nullptr) {
        if (!LayoutChildren().Empty()) {
            return InvalidContentState(
                "ContentControl has attached children without content");
        }
        return Size{};
    }
    if (!IsOnlyAttachedContent(*content_)) {
        return InvalidContentState(
            "ContentControl content attachment is invalid");
    }
    Base::Result<void> measured = MeasureChild(*content_, availableSize);
    if (!measured) return measured.GetStatus();
    return content_->DesiredSize();
}

Base::Result<Size> ContentControl::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    if (!IsOnlyAttachedContent(*content_)) {
        return InvalidContentState(
            "ContentControl content attachment is invalid");
    }
    Base::Result<void> arranged = ArrangeChild(
        *content_, {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return arranged.GetStatus();
    return finalSize;
}

UserControl::UserControl() noexcept
    : ContentControl(StaticTypeId()) {}

AERO_IMPLEMENT_METADATA(Panel, TypeFlags::Abstract) {
    AeroContent("Children", ContentKind::Collection);
}

AERO_IMPLEMENT_METADATA(Decorator, TypeFlags::Abstract) {
    AeroContent("Content", ContentKind::Single);
}

AERO_IMPLEMENT_EMPTY_METADATA(Control, TypeFlags::Abstract)

AERO_IMPLEMENT_METADATA(ContentControl, TypeFlags::Abstract) {
    AeroContent("Content", ContentKind::Single);
}

AERO_IMPLEMENT_EMPTY_METADATA(UserControl, TypeFlags::None)

Base::Result<void> TryRegisterControlPrimitiveMetadata(
    MetaRegistrationContext& context) noexcept {
    using Registrar = Base::Result<void> (*)(MetaRegistrationContext&) noexcept;
    const Registrar registrars[] = {
        &Panel::TryRegisterMetadata,
        &Decorator::TryRegisterMetadata,
        &Control::TryRegisterMetadata,
        &ContentControl::TryRegisterMetadata,
        &UserControl::TryRegisterMetadata};
    for (Registrar registrar : registrars) {
        Base::Result<void> registered = registrar(context);
        if (!registered) return registered.GetStatus();
    }
    return {};
}

} // namespace Aero::Core
