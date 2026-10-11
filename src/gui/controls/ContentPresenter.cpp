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
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "ControlPropertyValidators.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/TryCast.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include "gui/controls/ContentHosting.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Controls {

using namespace Primitives;

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;


ContentPresenter::ContentPresenter() noexcept
    : FrameworkElement(StaticTypeId()) {}



void AttachOwnedContentSubtree(
    ElementTree& tree,
    UIElement& parent) noexcept {
    const auto attachChild = [&](UIElement& child) noexcept {
        const Meta::TypeInfo* ct = DependencyObjectAccess::PropertyRegistry(child).Types().FindType(child.RuntimeType());
        std::fprintf(stderr, "[AttachChild] child=%s/%p visParent=%p tree=%p layoutAtt=%d logParent=%p\n",
            ct ? (const char*)ct->Name().Data() : "?",
            (void*)&child, (void*)child.GetVisualParent(), (void*)VisualTree(child),
            child.GetIsLayoutAttached(), (void*)child.GetLogicalParent());

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
            Base::Result<Aero::ElementAttachment> res = tree.AttachElement(parent, child);
            if (!res) {
                std::fprintf(stderr, "[AttachChild] tree.AttachElement failed: code=%d msg=%s\n",
                    static_cast<int>(res.GetStatus().code), res.GetStatus().message);
            } else {
                std::fprintf(stderr, "[AttachChild] tree.AttachElement SUCCESS for child=%p\n", (void*)&child);
            }
        } else if (child.GetVisualParent() != &parent ||
                   !child.GetIsLayoutAttached()) {
            Base::Result<Aero::VisualAttachment> res = tree.AttachVisualChild(parent, child);
            if (!res) {
                std::fprintf(stderr, "[AttachChild] tree.AttachVisualChild failed: code=%d msg=%s\n",
                    static_cast<int>(res.GetStatus().code), res.GetStatus().message);
            } else {
                std::fprintf(stderr, "[AttachChild] tree.AttachVisualChild SUCCESS for child=%p\n", (void*)&child);
            }
        }
        if (Aero::BindingEngine* bindings =
                Aero::ElementTree::BindingsOf(child)) {
            static_cast<void>(bindings->ActivateDeferredWhenReady(child));
        }
        AttachOwnedContentSubtree(tree, child);
    };

    if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
        auto& panel = static_cast<Controls::Panel&>(parent);
        const std::uint32_t count = (panel).ChildCountCore();
        for (std::uint32_t index = 0U; index < count; ++index) {
            const Base::Ref<Base::Object> owned =
                (panel).ChildAtCore( index);
            if (!owned ||
                !DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
                    owned->RuntimeType(), UIElement::StaticTypeId())) {
                continue;
            }
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Decorator::StaticTypeId())) {
        auto& decorator = static_cast<Controls::Decorator&>(parent);
        UIElement* child = decorator.GetChild();
        if (child != nullptr) {
            attachChild(*child);
        }
        return;
    }
    if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
            parent.RuntimeType(), ContentPresenter::StaticTypeId())) {
        auto& presenter = static_cast<ContentPresenter&>(parent);
        UIElement* child = presenter.GetContent();
        if (child != nullptr && child != &parent) {
            attachChild(*child);
        }
        return;
    }
    if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentControl::StaticTypeId())) {
        auto& control = static_cast<Controls::ContentControl&>(parent);
        if (control.GetTemplateRoot() == nullptr) {
            UIElement* child = control.GetContentElement();
            if (child != nullptr && child != &parent) {
                attachChild(*child);
            }
        }
        return;
    }
}


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
        ElementTree* detachTree = tree != nullptr ? tree : VisualTree(element);
        if (detachTree != nullptr) {
            ::Aero::VisualAttachment state;
            state.visualParent = element.GetVisualParent();
            state.child = &element;
            state.visualAttached = true;
            state.layoutAttached =
                element.GetIsLayoutAttached() &&
                element.LayoutParent() != nullptr;
            state.renderAttached = false;
            static_cast<void>(detachTree->DetachVisual(state));
        }
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

void ContentPresenter::ClearHostedContent() noexcept {
    if (content_ == nullptr) return;
    ElementTree* tree = VisualTree(this);
    if (tree != nullptr) {
        ::Aero::VisualAttachment state;
        state.visualParent =
            content_->GetVisualParent() != nullptr
            ? content_->GetVisualParent()
            : static_cast<::Aero::Media::Visual*>(this);
        state.child = content_;
        state.visualAttached = content_->GetVisualParent() != nullptr;
        state.layoutAttached =
            content_->GetIsLayoutAttached() &&
            content_->LayoutParent() != nullptr;
        state.renderAttached = false;
        if (state.IsAttached()) {
            static_cast<void>(tree->DetachVisual(state));
        }
    }
    content_ = nullptr;
    ownedContent_.Reset();
    InvalidateMeasure();
}

void ContentPresenter::ApplyContent() noexcept {
    contentValue_ = GetValue(ContentProperty);
    const Value& value = contentValue_;
    std::fprintf(stderr, "[CP %p src=%.*s] ApplyContent: valKind=%d isNull=%d obj=%p\n",
        (void*)this,
        static_cast<int>(GetContentSource().SizeBytes()), GetContentSource().Data(),
        static_cast<int>(value.Kind()), value.IsNullObject(),
        (value.Kind() == Meta::ValueKind::Object && !value.IsNullObject()) ? (void*)value.AsObject().Get() : nullptr);

    // 1. If value is a UIElement, host it directly.
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Object* obj = value.AsObject().Get();
        if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                obj->RuntimeType(), UIElement::StaticTypeId())) {
            auto* element = static_cast<UIElement*>(obj);
            HostUiElement(value.AsObject(), *element);
            return;
        }
    }

    // 2. If ContentTemplate is provided, instantiate the DataTemplate.
    Base::Ref<Base::Object> templateObj = GetValue(ContentTemplateProperty);
    if (templateObj &&
        templateObj->RuntimeType() == DataTemplate::StaticTypeId()) {
        auto* dataTemplate = static_cast<DataTemplate*>(templateObj.Get());
        Base::Ref<Base::Object> itemObj;
        if (value.Kind() == Meta::ValueKind::Object && !value.IsNullObject()) {
            itemObj = value.AsObject();
        }
        if (itemObj) {
            Base::Result<Base::Ref<Base::Object>> created =
                FrameworkTemplateState::Instantiate(
                    *dataTemplate, itemObj,
                    ElementTree::BindingsOf(*this));
            if (created && created.Value()) {
                Base::Ref<Base::Object> owned = std::move(created).Value();
                auto* element = ::Aero::TryCast<UIElement>(owned.Get());
                if (element != nullptr) {
                    if (auto* fe = ::Aero::TryCast<FrameworkElement>(element)) {
                        fe->SetDataContext(Value::FromObject(itemObj->RuntimeType(), itemObj));
                    }
                    HostUiElement(owned, *element);
                    return;
                }
            }
        }
    }

    // 3. Fallback: If contentValue is empty or null, clear content.
    if (value.Kind() == Meta::ValueKind::Unset ||
        (value.Kind() == Meta::ValueKind::Object && value.IsNullObject())) {
        ClearHostedContent();
        return;
    }

    // 4. Fallback: text presentation.
    if (content_ == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content_->RuntimeType(),
            TextBlock::StaticTypeId())) {
        Base::Result<Base::Ref<TextBlock>> made = Base::MakeRef<TextBlock>();
        if (made) {
            Base::Ref<Base::Object> owned = std::move(made).Value();
            auto* tb = static_cast<TextBlock*>(owned.Get());
            HostUiElement(owned, *tb);
        }
    }
    static_cast<void>(UpdatePresentedText());
}

void ContentPresenter::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    auto& presenter =
        static_cast<ContentPresenter&>(object);
    presenter.ApplyContent();
}

void ContentPresenter::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == ContentProperty.Handle() ||
        args.GetProperty() == ContentTemplateProperty.Handle()) {
        ApplyContent();
    }
    FrameworkElement::OnPropertyChanged(args);
}
Base::Result<void>
ContentPresenter::UpdatePresentedText() noexcept {
    if (content_ == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
            auto* obj = contentValue_.AsObject().Get();
            if (obj != nullptr) {
                const auto* typeInfo = DependencyObjectAccess::PropertyRegistry((*this)).Types().FindType(obj->RuntimeType());
                if (typeInfo != nullptr) {
                    (void)text.Assign(typeInfo->Name());
                }
            }
            break;
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
    if (content_ != nullptr && content_->GetVisualParent() != this) {
        if (ElementTree* tree = VisualTree(this)) {
            if (content_->GetVisualParent() != nullptr &&
                content_->GetVisualParent() != this) {
                ::Aero::VisualAttachment state;
                state.visualParent = content_->GetVisualParent();
                state.child = content_;
                state.visualAttached = true;
                state.layoutAttached =
                    content_->GetIsLayoutAttached() &&
                    content_->LayoutParent() != nullptr;
                state.renderAttached = false;
                static_cast<void>(tree->DetachVisual(state));
            }
            if (VisualTree(*content_) == nullptr &&
                content_->GetLogicalParent() == nullptr) {
                (void)tree->AttachElement(*this, *content_);
            } else if (content_->GetVisualParent() == nullptr ||
                       content_->GetVisualParent() == this) {
                (void)tree->AttachVisualChild(*this, *content_);
            }
            AttachOwnedContentSubtree(*tree, *content_);
        }
    }
    if (content_ == nullptr) {
        return Size{};
    }
    // WPF ContentPresenter measures its content regardless of whether the
    // layout-child table still lists it as the only child. Returning an empty
    // size here collapses UniformGrid rows whose cells bind Height to
    // ActualWidth (Inventory slots).
    MeasureChild(*content_, availableSize);
    return content_->GetDesiredSize();
}
Size ContentPresenter::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    ArrangeChild(*content_,
        {0.0, 0.0, finalSize.width, finalSize.height});
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
