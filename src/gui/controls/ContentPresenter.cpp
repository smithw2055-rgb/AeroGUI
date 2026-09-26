#include "gui/core/ElementTree.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/BrushRendering.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Documents.hpp>
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/ValueConversion.hpp"
#include "ControlsMetadata.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/TryCast.hpp>
#include <Aero/VisualTreeHelper.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace Aero::Controls {

using namespace Primitives;

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;


ContentPresenter::ContentPresenter() noexcept
    : FrameworkElement(StaticTypeId()) {}


namespace {

void AttachOwnedContentSubtree(
    ElementTree& tree,
    UIElement& parent) noexcept {
    const auto attachChild = [&](UIElement& child) noexcept {
        if (child.GetVisualParent() == &parent &&
            VisualTree(child) == &tree &&
            child.GetIsLayoutAttached()) {
            AttachOwnedContentSubtree(tree, child);
            return;
        }
        if (child.GetVisualParent() != nullptr &&
            child.GetVisualParent() != &parent) {
            static_cast<void>(tree.DetachVisual(
                *child.GetVisualParent(),
                static_cast<::Aero::Media::Visual&>(child)));
        }
        if (VisualTree(child) == nullptr &&
            child.GetLogicalParent() == nullptr) {
            static_cast<void>(tree.AttachElement(parent, child));
        } else if (child.GetVisualParent() != &parent ||
                   !child.GetIsLayoutAttached()) {
            static_cast<void>(tree.AttachVisualChild(parent, child));
        }
        if (Aero::BindingEngine* bindings =
                Aero::ElementTree::BindingsOf(child)) {
            static_cast<void>(bindings->ActivateDeferredWhenReady(child));
        }
        AttachOwnedContentSubtree(tree, child);
    };

    if ((parent).PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
        auto& panel = static_cast<Controls::Panel&>(parent);
        const std::uint32_t count = (panel).ChildCountCore();
        for (std::uint32_t index = 0U; index < count; ++index) {
            const Base::Ref<Base::Object> owned =
                (panel).ChildAtCore( index);
            if (!owned ||
                !(parent).PropertyRegistry().Types().IsDerivedFrom(
                    owned->RuntimeType(), UIElement::StaticTypeId())) {
                continue;
            }
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if ((parent).PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Decorator::StaticTypeId())) {
        const Base::Ref<Base::Object>& owned =
            (static_cast<Controls::Decorator&>(parent)).OwnedChild();
        if (owned &&
            (parent).PropertyRegistry().Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if ((parent).PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), ContentPresenter::StaticTypeId())) {
        auto& presenter = static_cast<ContentPresenter&>(parent);
        const Base::Ref<Base::Object>& owned = presenter.GetOwnedContent();
        if (owned &&
            (parent).PropertyRegistry().Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if ((parent).PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentControl::StaticTypeId())) {
        const Base::Ref<Base::Object>& owned =
            (static_cast<Controls::ContentControl&>(parent)).OwnedContent();
        if (owned &&
            (parent).PropertyRegistry().Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
    }
}

} // namespace

void ContentPresenter::HostUiElement(
    const Base::Ref<Base::Object>& owner,
    UIElement& element) noexcept {
    if (!owner || owner.Get() != &element) {
        return;
    }
    ElementTree* tree = VisualTree(this);
    const auto detachHosted = [&](UIElement& hosted) noexcept {
        if (tree == nullptr) {
            return;
        }
        ::Aero::VisualAttachment state;
        state.visualParent =
            hosted.GetVisualParent() != nullptr
            ? hosted.GetVisualParent()
            : static_cast<::Aero::Media::Visual*>(this);
        state.child = &hosted;
        state.visualAttached = hosted.GetVisualParent() != nullptr;
        state.layoutAttached =
            hosted.GetIsLayoutAttached() &&
            hosted.LayoutParent() != nullptr;
        state.renderAttached = false;
        if (state.IsAttached()) {
            static_cast<void>(tree->DetachVisual(state));
        }
    };

    UIElement* existing = content_;
    if (existing != nullptr && existing != &element) {
        detachHosted(*existing);
        SetContent(nullptr);
        if (content_ == existing) {
            content_ = nullptr;
            ownedContent_.Reset();
        }
    }
    if (tree != nullptr) {
        const UIElementChildRange children = LayoutChildren();
        for (std::uint32_t index = children.Size(); index > 0U; --index) {
            UIElement* child = children[index - 1U];
            if (child == nullptr || child == &element) {
                continue;
            }
            detachHosted(*child);
        }
    }
    if (element.GetVisualParent() != nullptr &&
        element.GetVisualParent() != this) {
        detachHosted(element);
    }
    if (tree != nullptr &&
        (element.GetVisualParent() != this ||
         !element.GetIsLayoutAttached())) {
        // AttachVisual requires the child to already be a tree member.
        // Authored Header visuals and DataTemplate roots often are not;
        // AttachElement joins them first. LoadComponent can also leave the
        // visual parent set while layout is still detached.
        if (VisualTree(element) == nullptr &&
            element.GetLogicalParent() == nullptr) {
            static_cast<void>(tree->AttachElement(*this, element));
        } else if (element.GetVisualParent() == nullptr ||
                   element.GetVisualParent() == this) {
            static_cast<void>(tree->AttachVisualChild(*this, element));
        }
    }
    SetOwnedContent(owner, element);
    if (content_ != &element) {
        content_ = &element;
        ownedContent_ = owner;
        InvalidateMeasure();
    }
    if (tree != nullptr) {
        AttachOwnedContentSubtree(*tree, element);
    }
}

void ContentPresenter::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& presenter =
        static_cast<ContentPresenter&>(object);
    presenter.contentValue_ = change.GetNewValue();
    const Value& value = presenter.contentValue_;
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Object* obj = value.AsObject().Get();
        if ((presenter).PropertyRegistry().Types().IsDerivedFrom(
                obj->RuntimeType(), UIElement::StaticTypeId())) {
            auto* element = static_cast<UIElement*>(obj);
            presenter.HostUiElement(value.AsObject(), *element);
            return;
        }
    }
    static_cast<void>(
        presenter.UpdatePresentedText());
}

void ContentPresenter::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == ContentProperty.Handle()) {
        OnContentPropertyChanged(*this, args);
    }
    FrameworkElement::OnPropertyChanged(args);
}
Base::Result<void>
ContentPresenter::UpdatePresentedText() noexcept {
    if (content_ == nullptr ||
        !(*this).PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(),
            TextBlock::StaticTypeId())) {
        return {};
    }
    Base::String text;
    switch (contentValue_.Kind()) {
    case Meta::ValueKind::String:
        {
            Base::Result<void> assigned =
                text.Assign(
                    contentValue_.AsString());
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::Boolean:
        {
            Base::Result<void> assigned =
                text.Assign(
                    contentValue_.AsBoolean()
                    ? Base::StringView("True")
                    : Base::StringView("False"));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double:
        {
            char raw[64]{};
            if (contentValue_.Kind() ==
                Meta::ValueKind::SignedInteger) {
                std::snprintf(
                    raw, sizeof(raw), "%lld",
                    static_cast<long long>(
                        contentValue_.
                            AsSignedInteger()));
            } else if (contentValue_.Kind() ==
                       Meta::ValueKind::
                           UnsignedInteger) {
                std::snprintf(
                    raw, sizeof(raw), "%llu",
                    static_cast<
                        unsigned long long>(
                            contentValue_.
                                AsUnsignedInteger()));
            } else {
                std::snprintf(
                    raw, sizeof(raw), "%.15g",
                    contentValue_.AsDouble());
            }
            Base::Result<void> assigned =
                text.Assign(raw);
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::Object:
        if (!contentValue_.IsNullObject()) {
            return {};
        }
        break;
    default:
        return {};
    }
    auto* textBlock = static_cast<TextBlock*>(content_);
    textBlock->SetValue(RichText::TextProperty, text.View());
    textBlock->SetText(text.View());
    return {};
}
void ContentPresenter::SetContentSource(
    Base::StringView value) noexcept {
    SetValue(
        ContentSourceProperty, value);
}
bool ContentPresenter::IsOnlyAttachedContent(
    const UIElement& content) const noexcept {
    const UIElementChildRange children = LayoutChildren();
    return children.Size() == 1U && children[0] == &content;
}
void ContentPresenter::SetContent(UIElement* content) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> validated = ValidateContent(content);
    if (!validated) return;
    if (content == content_) return;
    content_ = content;
    if (content == nullptr) ownedContent_.Reset();
    InvalidateMeasure();
}
void ContentPresenter::SetOwnedContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) {
        return;
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> validated = ValidateContent(&content);
    if (!validated) return;
    content_ = &content;
    ownedContent_ = contentObject;
    (void)UpdatePresentedText();
    InvalidateMeasure();
    if (ElementTree* tree = VisualTree(this)) {
        AttachOwnedContentSubtree(*tree, content);
    }
}
Base::Result<void> ContentPresenter::ValidateContent(
    UIElement* content) const noexcept {
    if (content == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentPresenter content must be detached before clearing it");
        }
    } else if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "ContentPresenter content must be its only attached layout child");
    }
    return {};
}
Size ContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    if (content_ == nullptr) {
        return Size{};
    }
    // WPF ContentPresenter measures its content regardless of whether the
    // layout-child table still lists it as the only child. Returning an empty
    // size here collapses UniformGrid rows whose cells bind Height to
    // ActualWidth (Inventory slots).
    Base::Result<void> measured = MeasureChild(*content_, availableSize);
    if (!measured) return Size{};
    return content_->GetDesiredSize();
}
Size ContentPresenter::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    Base::Result<void> arranged = ArrangeChild(*content_,
        {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}

namespace {

void SetContentPresenterContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    static_cast<ContentPresenter&>(owner).SetOwnedContent(
        child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearContentPresenterContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ContentPresenter&>(owner).SetContent(nullptr);
}

} // namespace

AERO_DESCRIBE(ContentPresenter) {
    using namespace Aero::Meta;
    Base::String defaultContentSource;
    (void)defaultContentSource.Assign(Base::StringView("Content"));

    Register<ContentPresenter>(context)
        .Property(ContentPresenter::ContentProperty, FrameworkPropertyMetadata(Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure).Structural())
        .Property(ContentPresenter::ContentTemplateProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .Property(ContentPresenter::ContentSourceProperty, std::move(defaultContentSource))
        .ContentAccessor(MakeMemberId(ContentPresenter::StaticTypeId(), MemberKind::Property, "Content"), ContentKind::Single, &SetContentPresenterContent, &ClearContentPresenterContent, ContentFlags::Visual)
        .Factory();
}

} // namespace Aero::Controls
