#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

class AERO_API Panel : public FrameworkElement {
    AERO_DECLARE_METADATA(Panel, FrameworkElement)
public:
    std::uint32_t OwnedChildCount() const noexcept {
        return ownedChildren_.Size();
    }
    Base::Result<void> AddOwnedChild(
        const Base::Ref<Base::Object>& childObject,
        UIElement& child) noexcept;
    Base::Result<void> ClearOwnedChildren() noexcept;

protected:
    explicit Panel(TypeId runtimeType) noexcept;
    ~Panel() override = default;

private:
    Base::Vector<Base::Ref<Base::Object>> ownedChildren_;
    bool IsOwnedChild(const UIElement& child) const noexcept;
};

class AERO_API Decorator : public FrameworkElement {
    AERO_DECLARE_METADATA(Decorator, FrameworkElement)
public:
    UIElement* Child() const noexcept { return child_; }
    const Base::Ref<Base::Object>& OwnedChild() const noexcept {
        return ownedChild_;
    }
    Base::Result<void> SetChild(UIElement* child) noexcept;
    Base::Result<void> SetOwnedChild(
        const Base::Ref<Base::Object>& childObject,
        UIElement& child) noexcept;

protected:
    explicit Decorator(TypeId runtimeType) noexcept;
    ~Decorator() override = default;
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    UIElement* child_ = nullptr;
    Base::Ref<Base::Object> ownedChild_;
    bool IsOnlyAttachedChild(const UIElement& child) const noexcept;
    Base::Result<void> ValidateChild(UIElement* child) const noexcept;
};

class AERO_API Control : public FrameworkElement {
    AERO_DECLARE_METADATA(Control, FrameworkElement)
protected:
    explicit Control(TypeId runtimeType) noexcept;
    ~Control() override = default;
};

class AERO_API ContentControl : public Control {
    AERO_DECLARE_METADATA(ContentControl, Control)
public:
    UIElement* Content() const noexcept { return content_; }
    const Base::Ref<Base::Object>& OwnedContent() const noexcept {
        return ownedContent_;
    }
    Base::Result<void> SetContent(UIElement* content) noexcept;
    Base::Result<void> SetOwnedContent(
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;

protected:
    explicit ContentControl(TypeId runtimeType) noexcept;
    ~ContentControl() override = default;
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept;
    Base::Result<void> ValidateContent(UIElement* content) const noexcept;
};

class AERO_API UserControl final : public ContentControl {
    AERO_DECLARE_METADATA(UserControl, ContentControl)
public:
    UserControl() noexcept;
    ~UserControl() override = default;
};

AERO_API Base::Result<void> TryRegisterControlPrimitiveMetadata(
    MetaRegistrationContext& context) noexcept;

} // namespace Aero::Core
