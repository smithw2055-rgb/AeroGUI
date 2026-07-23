#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

class AERO_API Panel : public FrameworkElement {
    AERO_DECLARE_METADATA(Panel, FrameworkElement)
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
    AERO_DECLARE_METADATA(Decorator, FrameworkElement)
public:
    UIElement* Child() const noexcept { return child_; }
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
        if (child_ == nullptr) {
            if (!LayoutChildren().Empty()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState,
                    "Decorator has attached children without a configured child");
            }
            return Size{};
        }
        if (!IsOnlyAttachedChild(*child_)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Decorator child attachment is invalid");
        }
        Base::Result<void> measured = MeasureChild(*child_, availableSize);
        if (!measured) return measured.GetStatus();
        return child_->DesiredSize();
    }
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        if (child_ == nullptr) return finalSize;
        if (!IsOnlyAttachedChild(*child_)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Decorator child attachment is invalid");
        }
        Base::Result<void> arranged = ArrangeChild(
            *child_, {0.0, 0.0, finalSize.width, finalSize.height});
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
    AERO_DECLARE_METADATA(Control, FrameworkElement)
protected:
    explicit Control(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Control() override = default;
};

class AERO_API ContentControl : public Control {
    AERO_DECLARE_METADATA(ContentControl, Control)
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
        } else if (!LayoutChildren().Empty() &&
            !IsOnlyAttachedContent(*content)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentControl content must be its only attached UIElement");
        }
        return {};
    }
};

class AERO_API UserControl final : public ContentControl {
    AERO_DECLARE_METADATA(UserControl, ContentControl)
public:
    UserControl() noexcept : ContentControl(StaticTypeId()) {}
    ~UserControl() override = default;
};

#define AERO_DETAIL_IMPLEMENT_CONTROL_METADATA(classType, typeFlags, body) \
    inline Base::Result<void> classType::TryRegisterMetadata( \
        MetaRegistrationContext& context) noexcept { \
        MetaRegistrationBuilder helper(context, StaticTypeId(), \
            StaticMetadataNamespace(), StaticMetadataName(), \
            ParentClass::StaticTypeId(), typeFlags); \
        Base::Result<void> begun = helper.Begin(); \
        if (!begun) return begun.GetStatus(); \
        StaticFillMetadata(helper); \
        return helper.Finish(); \
    } \
    inline void classType::StaticFillMetadata( \
        MetaRegistrationBuilder& helper) noexcept body

AERO_DETAIL_IMPLEMENT_CONTROL_METADATA(Panel, TypeFlags::Abstract, {
    AeroContent("Children", ContentKind::Collection);
})
AERO_DETAIL_IMPLEMENT_CONTROL_METADATA(Decorator, TypeFlags::Abstract, {
    AeroContent("Content", ContentKind::Single);
})
AERO_DETAIL_IMPLEMENT_CONTROL_METADATA(Control, TypeFlags::Abstract, {
    (void)helper;
})
AERO_DETAIL_IMPLEMENT_CONTROL_METADATA(ContentControl, TypeFlags::Abstract, {
    AeroContent("Content", ContentKind::Single);
})
AERO_DETAIL_IMPLEMENT_CONTROL_METADATA(UserControl, TypeFlags::None, {
    (void)helper;
})

#undef AERO_DETAIL_IMPLEMENT_CONTROL_METADATA

inline Base::Result<void> TryRegisterControlPrimitiveMetadata(
    MetaRegistrationContext& context) noexcept {
    using Registrar = Base::Result<void> (*)(MetaRegistrationContext&) noexcept;
    const Registrar registrars[] = {
        &Panel::TryRegisterMetadata, &Decorator::TryRegisterMetadata,
        &Control::TryRegisterMetadata, &ContentControl::TryRegisterMetadata,
        &UserControl::TryRegisterMetadata};
    for (Registrar registrar : registrars) {
        Base::Result<void> registered = registrar(context);
        if (!registered) return registered.GetStatus();
    }
    return {};
}

} // namespace Aero::Core
