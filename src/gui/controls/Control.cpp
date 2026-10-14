#include "gui/core/ElementTree.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/BrushRendering.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Buttons.hpp>
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
#include <Aero/LogicalTreeHelper.hpp>
#include "gui/controls/ContentHosting.hpp"
#include "gui/core/DependencyObjectAccess.hpp"

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

// ============================================================================
// Control
// ============================================================================

Size Control::MeasureOverride(Size availableSize) noexcept {
    if (templateChild_ == nullptr) return Size{};
    MeasureChild(*templateChild_, availableSize);
    return templateChild_->GetDesiredSize();
}

Size Control::ArrangeOverride(Size finalSize) noexcept {
    if (templateChild_ == nullptr) return finalSize;
    ArrangeChild(
        *templateChild_,
        {0.0, 0.0, finalSize.width, finalSize.height});
    return finalSize;
}

void Control::OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept {
    FrameworkElement::OnPropertyChanged(args);
}

void Control::OnRender(::Aero::Media::DrawingContext& context) noexcept {
    if (GetTemplateRoot() != nullptr) return;
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    static_cast<void>(PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height}));
}

// ============================================================================
// AttachOwnedContentSubtree helper
// ============================================================================

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

    if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
        auto& panel = static_cast<Controls::Panel&>(parent);
        const std::uint32_t count = (panel).ChildCountCore();
        for (std::uint32_t index = 0U; index < count; ++index) {
            const Base::Ref<Base::Object> owned =
                (panel).ChildAtCore(index);
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

// ============================================================================
// ContentControl
// ============================================================================

ContentControl::ContentControl(TypeId runtimeType) noexcept
    : Control(runtimeType) {}

ContentControl::~ContentControl() = default;

void ContentControl::OnContentChanged(const Value&, const Value&) {}

void ContentControl::OnContentTemplateChanged(
    const Ref<Base::Object>&,
    const Ref<Base::Object>&) {}

void ContentControl::OnContentTemplateSelectorChanged(
    const Ref<Base::Object>&,
    const Ref<Base::Object>&) {}

void ContentControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == ContentProperty.Handle()) {
        OnContentPropertyChanged(*this, args);
    }
    Control::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == Control::ForegroundProperty ||
        prop == Control::FontSizeProperty) {
        SyncGeneratedTextFormatting();
    } else if (prop == ContentProperty) {
        OnContentChanged(args.GetOldValue(), args.GetNewValue());
    } else if (prop == ContentTemplateProperty) {
        OnContentTemplateChanged(
            args.GetOldValue().Kind() == Meta::ValueKind::Object ? args.GetOldValue().AsObject() : Ref<Base::Object>{},
            args.GetNewValue().Kind() == Meta::ValueKind::Object ? args.GetNewValue().AsObject() : Ref<Base::Object>{});
    } else if (prop == ContentTemplateSelectorProperty) {
        OnContentTemplateSelectorChanged(
            args.GetOldValue().Kind() == Meta::ValueKind::Object ? args.GetOldValue().AsObject() : Ref<Base::Object>{},
            args.GetNewValue().Kind() == Meta::ValueKind::Object ? args.GetNewValue().AsObject() : Ref<Base::Object>{});
    }
}

void ContentControl::SyncGeneratedTextFormatting() noexcept {
    if (!literalTextContent_ || content_ == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
        return;
    }
    auto* text = static_cast<TextBlock*>(content_);
    text->SetValue(TextBlock::FontSizeProperty, GetFontSize());
}

void ContentControl::SetGeneratedTextContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content.RuntimeType(), TextBlock::StaticTypeId())) {
        return;
    }
    SetOwnedContent(contentObject, content);
    literalTextContent_ = true;
    SyncGeneratedTextFormatting();
}

void ContentControl::StoreContentProperty(Meta::Value value) noexcept {
    if (synchronizingContentProperty_) return;
    synchronizingContentProperty_ = true;
    SetValue(ContentProperty, std::move(value));
    synchronizingContentProperty_ = false;
}

void ContentControl::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& change) noexcept {
    auto& control = static_cast<ContentControl&>(object);
    if (control.synchronizingContentProperty_) return;
    control.synchronizingContentProperty_ = true;
    static_cast<void>(control.SetContentValue(change.GetNewValue()));
    control.synchronizingContentProperty_ = false;
}

void ContentControl::SetContentValue(Base::Ref<Base::Object> value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    const bool isUIE = value &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            value->RuntimeType(), UIElement::StaticTypeId());
    if (isUIE) {
        authoredContent_ = Meta::Value::FromObject(value->RuntimeType(), value);
        (void)StoreContentProperty(authoredContent_);
        auto& content = *static_cast<UIElement*>(value.Get());
        SetOwnedContent(value, content);
        if (GetTemplateRoot() == nullptr &&
            content.GetVisualParent() != this) {
            if (ElementTree* tree = VisualTree(this)) {
                if (VisualTree(content) == nullptr &&
                    content.GetLogicalParent() == nullptr) {
                    (void)tree->AttachElement(*this, content);
                } else if (content.GetVisualParent() != this) {
                    (void)tree->AttachVisualChild(*this, content);
                }
            }
        }
        if (GetTemplateRoot() == nullptr) {
            if (ElementTree* tree = VisualTree(this)) {
                AttachOwnedContentSubtree(*tree, content);
            }
        }
        InvalidateMeasure();
        return;
    }
    literalTextContent_ = false;
    if (content_ != nullptr) {
        SetContent(nullptr);
    }
    contentValue_ = std::move(value);
    ownedContent_.Reset();
    authoredContent_ = contentValue_
        ? Meta::Value::FromObject(contentValue_->RuntimeType(), contentValue_)
        : Meta::Value::NullObject(Meta::TypeOf<Base::Object>());
    (void)StoreContentProperty(authoredContent_);
    InvalidateMeasure();
}

void ContentControl::EnsureHostedContent() noexcept {
    if (GetTemplateRoot() != nullptr || content_ == nullptr) {
        return;
    }
    if (content_->GetVisualParent() == this &&
        content_->GetIsLayoutAttached()) {
        return;
    }
    ElementTree* tree = VisualTree(this);
    if (tree == nullptr) {
        if (content_->GetVisualParent() == nullptr) {
            AddVisualChild(content_);
        }
        return;
    }
    if (VisualTree(content_) == nullptr &&
        content_->GetLogicalParent() == nullptr) {
        (void)tree->AttachElement(*this, *content_);
    } else if (content_->GetVisualParent() != this || !content_->GetIsLayoutAttached()) {
        (void)tree->AttachVisualChild(*this, *content_);
    }
    AttachOwnedContentSubtree(*tree, *content_);
}

void ContentControl::SetContentValue(Meta::Value value) noexcept {
    if (value.IsUnset()) {
        return;
    }
    if (value.Kind() == Meta::ValueKind::Object) {
        authoredContent_ = value;
        SetContentValue(value.AsObject());
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        (void)StoreContentProperty(value);
        authoredContent_ = std::move(value);
        contentValue_.Reset();
        ownedContent_.Reset();
        InvalidateMeasure();
        return;
    }

    if (literalTextContent_ && content_ != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
        auto* textBlock = static_cast<TextBlock*>(content_);
        textBlock->SetValue(RichText::TextProperty, value.AsString());
        textBlock->SetText(value.AsString());
        (void)StoreContentProperty(value);
        authoredContent_ = std::move(value);
        contentValue_.Reset();
        InvalidateMeasure();
        InvalidateVisual();
        return;
    }

    Base::Result<Base::Ref<TextBlock>> created = Base::MakeRef<TextBlock>();
    if (!created) return;
    created.Value()->SetValue(RichText::TextProperty, value.AsString());
    created.Value()->SetText(value.AsString());
    Base::Ref<Base::Object> retained(created.Value());
    SetOwnedContent(retained, *created.Value());
    if (GetTemplateRoot() == nullptr &&
        created.Value()->GetVisualParent() != this) {
        if (ElementTree* tree = VisualTree(this)) {
            (void)tree->AttachElement(*this, *created.Value());
        }
    }
    (void)StoreContentProperty(value);
    authoredContent_ = std::move(value);
    contentValue_.Reset();
    literalTextContent_ = true;
}

void ContentControl::SetContent(StringView text) noexcept {
    auto boxed = Value::TryFromString(TypeOf<Base::String>(), text);
    if (boxed) {
        SetContentValue(std::move(boxed).Value());
    }
}

void ContentControl::SetContent(const char* text) noexcept {
    SetContent(text != nullptr
        ? StringView(text, static_cast<std::uint32_t>(std::strlen(text)))
        : StringView());
}

void ContentControl::SetContent(UIElement* content) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    Result<void> valid = ValidateContent(content);
    if (!valid) return;
    Value propertyValue = content != nullptr
        ? Value::FromObject(content->RuntimeType(), Ref<Base::Object>::FromBorrowed(*content))
        : Value::NullObject(Meta::TypeOf<Base::Object>());
    StoreContentProperty(std::move(propertyValue));
    if (content_ == content) return;
    if (content_ != nullptr && content == nullptr) {
        if (content_->GetVisualParent() == this) { RemoveVisualChild(content_); }
    }
    content_ = content;
    literalTextContent_ = false;
    if (content == nullptr) {
        ownedContent_.Reset();
        contentValue_.Reset();
    }
    InvalidateMeasure();
}

void ContentControl::SetOwnedContent(const Ref<Base::Object>& contentObject, UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) { return; }
    Result<void> access = VerifyAccess();
    if (!access) return;
    Result<void> valid = ValidateContent(&content);
    if (!valid) return;
    StoreContentProperty(Value::FromObject(contentObject->RuntimeType(), contentObject));
    content_ = &content;
    ownedContent_ = contentObject;
    contentValue_ = contentObject;
    literalTextContent_ = false;
}

std::uint32_t ContentControl::GetVisualChildrenCount() const noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::GetVisualChildrenCount(); }
    return content_ != nullptr && content_->GetVisualParent() == this ? 1U : 0U;
}

::Aero::Media::Visual* ContentControl::GetVisualChild(std::uint32_t index) const noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::GetVisualChild(index); }
    if (index != 0U || content_ == nullptr || content_->GetVisualParent() != this) { return nullptr; }
    return content_;
}

std::uint32_t ContentControl::GetLogicalChildrenCount() const noexcept {
    return content_ != nullptr ? 1U : Control::GetLogicalChildrenCount();
}

DependencyObject* ContentControl::GetLogicalChild(std::uint32_t index) const noexcept {
    if (content_ != nullptr) { return index == 0U ? content_ : nullptr; }
    return Control::GetLogicalChild(index);
}

Size ContentControl::MeasureOverride(Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::MeasureOverride(availableSize); }
    EnsureHostedContent();
    if (content_ == nullptr) { return Size{}; }
    MeasureChild(*content_, availableSize);
    return content_->GetDesiredSize();
}

Size ContentControl::ArrangeOverride(Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::ArrangeOverride(finalSize); }
    EnsureHostedContent();
    if (content_ == nullptr) return finalSize;
    ArrangeChild(*content_, {0.0, 0.0, finalSize.width, finalSize.height});
    return finalSize;
}

Result<void> ContentControl::ValidateContent(UIElement* content) const noexcept {
    if (content == nullptr) return {};
    if (GetTemplateRoot() != nullptr) return {};
    if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "ContentControl content must be its only attached UIElement");
    }
    return {};
}

bool ContentControl::IsOnlyAttachedContent(const UIElement& content) const noexcept {
    const UIElementChildRange children = LayoutChildren();
    return children.Size() == 1U && children[0] == &content;
}

Base::Result<Base::Ref<Base::Object>> ContentControl::CreateTemplatedContent() const noexcept {
    if (content_ != nullptr) {
        return ownedContent_;
    }
    if (!contentValue_) {
        return Base::Ref<Base::Object>{};
    }
    Base::Ref<Base::Object> contentTemplate = GetContentTemplate();
    if (!contentTemplate) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ContentControl business content requires a ContentTemplate");
    }
    if (contentTemplate->RuntimeType() != DataTemplate::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentControl ContentTemplate is not a DataTemplate");
    }
    Base::Result<Base::Ref<Base::Object>> created =
        FrameworkTemplateState::Instantiate(
            *static_cast<DataTemplate*>(contentTemplate.Get()),
            contentValue_,
            ElementTree::BindingsOf(*this));
    if (!created) return created.GetStatus();
    if (!created.Value() ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            created.Value()->RuntimeType(),
            UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentTemplate must create a UIElement");
    }
    return created;
}

// ============================================================================
// ContentPresenter
// ============================================================================

ContentPresenter::ContentPresenter() noexcept
    : FrameworkElement(StaticTypeId()) {}

static void DetachSubtreeBindings(
    Aero::Media::Visual& root,
    Aero::BindingEngine* bindings) noexcept {
    if (bindings != nullptr) {
        static_cast<void>(bindings->DetachObject(root));
    }
    const std::uint32_t logicalCount = Aero::LogicalTreeHelper::GetChildrenCount(root);
    for (std::uint32_t i = 0U; i < logicalCount; ++i) {
        if (auto* child = Aero::LogicalTreeHelper::GetChild(root, i)) {
            if (bindings != nullptr) static_cast<void>(bindings->DetachObject(*child));
        }
    }
    const std::uint32_t count = Aero::Media::VisualTreeHelper::GetChildrenCount(root);
    for (std::uint32_t i = 0U; i < count; ++i) {
        if (auto* child = Aero::Media::VisualTreeHelper::GetChild(root, i)) {
            DetachSubtreeBindings(*child, bindings);
        }
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
        Aero::BindingEngine* bindings = ElementTree::BindingsOf(*this);
        DetachSubtreeBindings(hosted, bindings);
        if (tree == nullptr) {
            return;
        }
        ::Aero::VisualAttachment state;
        state.visualParent = hosted.GetVisualParent() != nullptr
            ? hosted.GetVisualParent()
            : static_cast<::Aero::Media::Visual*>(this);
        state.child = &hosted;
        state.visualAttached = hosted.GetVisualParent() != nullptr;
        state.layoutAttached = hosted.GetIsLayoutAttached() && hosted.LayoutParent() != nullptr;
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
            state.layoutAttached = element.GetIsLayoutAttached() && element.LayoutParent() != nullptr;
            state.renderAttached = false;
            static_cast<void>(detachTree->DetachVisual(state));
        }
    }
    if (tree != nullptr &&
        (element.GetVisualParent() != this || !element.GetIsLayoutAttached())) {
        if (VisualTree(element) == nullptr && element.GetLogicalParent() == nullptr) {
            static_cast<void>(tree->AttachElement(*this, element));
        } else if (element.GetVisualParent() == nullptr || element.GetVisualParent() == this) {
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
    appliedTemplate_.Reset();
    if (content_ == nullptr) return;
    ElementTree* tree = VisualTree(this);
    Aero::BindingEngine* bindings = ElementTree::BindingsOf(*this);
    DetachSubtreeBindings(*content_, bindings);
    if (tree != nullptr) {
        ::Aero::VisualAttachment state;
        state.visualParent = content_->GetVisualParent() != nullptr
            ? content_->GetVisualParent()
            : static_cast<::Aero::Media::Visual*>(this);
        state.child = content_;
        state.visualAttached = content_->GetVisualParent() != nullptr;
        state.layoutAttached = content_->GetIsLayoutAttached() && content_->LayoutParent() != nullptr;
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
    Base::Ref<Base::Object> templateObj = GetValue(ContentTemplateProperty);

    if (GetContentSource() == Base::StringView("SelectionBoxItem")) {
        if (auto* comboBox = ::Aero::TryCast<ComboBox>(GetTemplatedParent())) {
            if (contentValue_.Kind() == Meta::ValueKind::Unset || contentValue_.IsNullObject()) {
                contentValue_ = comboBox->GetSelectionBoxItem();
            }
            if (!templateObj) {
                templateObj = comboBox->GetSelectionBoxItemTemplate();
            }
        }
    }
    const Value& value = contentValue_;

    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Object* obj = value.AsObject().Get();
        if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                obj->RuntimeType(), UIElement::StaticTypeId())) {
            appliedTemplate_.Reset();
            auto* element = static_cast<UIElement*>(obj);
            HostUiElement(value.AsObject(), *element);
            return;
        }
    }

    if (templateObj && templateObj->RuntimeType() == DataTemplate::StaticTypeId()) {
        auto* dataTemplate = static_cast<DataTemplate*>(templateObj.Get());
        Base::Ref<Base::Object> itemObj;
        if (value.Kind() == Meta::ValueKind::Object && !value.IsNullObject()) {
            itemObj = value.AsObject();
        }
        if (itemObj) {
            if (content_ != nullptr && appliedTemplate_ == templateObj) {
                if (auto* fe = ::Aero::TryCast<FrameworkElement>(content_)) {
                    fe->SetDataContext(Value::FromObject(itemObj->RuntimeType(), itemObj));
                }
                return;
            }
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
                    appliedTemplate_ = templateObj;
                    HostUiElement(owned, *element);
                    return;
                }
            }
        }
    }

    if (value.Kind() == Meta::ValueKind::Unset ||
        (value.Kind() == Meta::ValueKind::Object && value.IsNullObject())) {
        ClearHostedContent();
        return;
    }

    appliedTemplate_.Reset();
    if (content_ == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
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
    auto& presenter = static_cast<ContentPresenter&>(object);
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

Base::Result<void> ContentPresenter::UpdatePresentedText() noexcept {
    if (appliedTemplate_ != nullptr) {
        return {};
    }
    if (content_ == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
        return {};
    }
    if (contentValue_.Kind() == Meta::ValueKind::Object && !contentValue_.IsNullObject()) {
        auto* obj = contentValue_.AsObject().Get();
        if (obj == content_ ||
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                obj->RuntimeType(), UIElement::StaticTypeId())) {
            return {};
        }
    }
    Base::String text;
    switch (contentValue_.Kind()) {
    case Meta::ValueKind::String:
        {
            Base::Result<void> assigned = text.Assign(contentValue_.AsString());
            if (!assigned) return assigned.GetStatus();
        }
        break;
    case Meta::ValueKind::Boolean:
        {
            Base::Result<void> assigned = text.Assign(
                contentValue_.AsBoolean() ? Base::StringView("True") : Base::StringView("False"));
            if (!assigned) return assigned.GetStatus();
        }
        break;
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double:
        {
            char raw[64]{};
            if (contentValue_.Kind() == Meta::ValueKind::SignedInteger) {
                std::snprintf(raw, sizeof(raw), "%lld",
                    static_cast<long long>(contentValue_.AsSignedInteger()));
            } else if (contentValue_.Kind() == Meta::ValueKind::UnsignedInteger) {
                std::snprintf(raw, sizeof(raw), "%llu",
                    static_cast<unsigned long long>(contentValue_.AsUnsignedInteger()));
            } else {
                std::snprintf(raw, sizeof(raw), "%.15g", contentValue_.AsDouble());
            }
            Base::Result<void> assigned = text.Assign(raw);
            if (!assigned) return assigned.GetStatus();
        }
        break;
    case Meta::ValueKind::Object:
        if (!contentValue_.IsNullObject()) {
            auto* obj = contentValue_.AsObject().Get();
            if (obj != nullptr) {
                if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                        obj->RuntimeType(), UIElement::StaticTypeId())) {
                    Aero::BindingEngine* bindings = ElementTree::BindingsOf(*this);
                    Meta::Registry* metadata = bindings != nullptr ? bindings->Metadata() : Meta::CurrentObjectFactory().metadata;
                    const Meta::PropertyInfo* prop = metadata != nullptr
                        ? metadata->Types().FindProperty(obj->RuntimeType(), Base::StringView("Name"), true)
                        : nullptr;
                    if (prop != nullptr) {
                        Base::Result<Meta::Value> displayed = metadata->GetProperty(*obj, prop->Id());
                        if (displayed && displayed.Value().Kind() == Meta::ValueKind::String) {
                            (void)text.Assign(displayed.Value().AsString());
                        }
                    }
                }
                if (text.Empty()) {
                    const auto* typeInfo = DependencyObjectAccess::PropertyRegistry((*this)).Types().FindType(obj->RuntimeType());
                    if (typeInfo != nullptr) {
                        (void)text.Assign(typeInfo->Name());
                    }
                }
            }
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

void ContentPresenter::SetContentSource(Base::StringView value) noexcept {
    SetValue(ContentSourceProperty, value);
}

void ContentPresenter::RefreshContentSource() noexcept {
    ApplyContent();
}

bool ContentPresenter::IsOnlyAttachedContent(const UIElement& content) const noexcept {
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
    InvalidateMeasure();
    if (ElementTree* tree = VisualTree(this)) {
        AttachOwnedContentSubtree(*tree, content);
    }
}

Base::Result<void> ContentPresenter::ValidateContent(UIElement* content) const noexcept {
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

Size ContentPresenter::MeasureOverride(Size availableSize) noexcept {
    if (content_ != nullptr && content_->GetVisualParent() != this) {
        if (ElementTree* tree = VisualTree(this)) {
            if (content_->GetVisualParent() != nullptr &&
                content_->GetVisualParent() != this) {
                ::Aero::VisualAttachment state;
                state.visualParent = content_->GetVisualParent();
                state.child = content_;
                state.visualAttached = true;
                state.layoutAttached = content_->GetIsLayoutAttached() && content_->LayoutParent() != nullptr;
                state.renderAttached = false;
                static_cast<void>(tree->DetachVisual(state));
            }
            if (VisualTree(*content_) == nullptr && content_->GetLogicalParent() == nullptr) {
                (void)tree->AttachElement(*this, *content_);
            } else if (content_->GetVisualParent() == nullptr || content_->GetVisualParent() == this) {
                (void)tree->AttachVisualChild(*this, *content_);
            }
            AttachOwnedContentSubtree(*tree, *content_);
        }
    }
    if (content_ == nullptr) {
        return Size{};
    }
    MeasureChild(*content_, availableSize);
    return content_->GetDesiredSize();
}

Size ContentPresenter::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    ArrangeChild(*content_, {0.0, 0.0, finalSize.width, finalSize.height});
    return finalSize;
}

// ============================================================================
// HeaderedContentControl
// ============================================================================

HeaderedContentControl::HeaderedContentControl(TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {}

HeaderedContentControl::~HeaderedContentControl() = default;

void HeaderedContentControl::OnHeaderChanged(const Value&, const Value&) {
    ProjectHeaderContent();
}

void HeaderedContentControl::OnHeaderTemplateChanged(
    const Ref<DataTemplate>&,
    const Ref<DataTemplate>&) {}

void HeaderedContentControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ContentControl::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == HeaderProperty) {
        OnHeaderChanged(args.GetOldValue(), args.GetNewValue());
    } else if (prop == HeaderTemplateProperty) {
        const auto toTemplate = [](const Value& v) -> Ref<DataTemplate> {
            if (v.Kind() == ValueKind::Object && v.AsObject()) {
                if (auto* dt = TryCast<DataTemplate>(v.AsObject().Get())) {
                    return Ref<DataTemplate>::FromBorrowed(*dt);
                }
            }
            return {};
        };
        OnHeaderTemplateChanged(
            toTemplate(args.GetOldValue()),
            toTemplate(args.GetNewValue()));
    }
}

Meta::Value HeaderedContentControl::GetHeader() const noexcept {
    return GetValue(HeaderProperty);
}

void HeaderedContentControl::SetHeader(const Meta::Value& value) noexcept {
    SetValue(HeaderProperty, value);
}

void HeaderedContentControl::SetHeader(Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(Meta::TypeOf<Base::String>(), value);
    if (!boxed) { AERO_ASSERT(false); return; }
    SetHeader(std::move(boxed).Value());
}

Base::Ref<DataTemplate> HeaderedContentControl::GetHeaderTemplate() const noexcept {
    return GetValue(HeaderTemplateProperty);
}

void HeaderedContentControl::SetHeaderTemplate(Base::Ref<DataTemplate> value) noexcept {
    SetValue(HeaderTemplateProperty, std::move(value));
}

void HeaderedContentControl::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    ProjectHeaderContent();
}

void HeaderedContentControl::ProjectHeaderContent() noexcept {
    const Value header = GetHeader();
    if (header.Kind() != ValueKind::Object || header.IsNullObject() || !header.AsObject()) {
        return;
    }
    Base::Object* obj = header.AsObject().Get();
    if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            obj->RuntimeType(), UIElement::StaticTypeId())) {
        return;
    }
    auto* element = static_cast<UIElement*>(obj);
    DependencyObject* part = GetTemplateChild(Base::StringView("HeaderHost"));
    if (part == nullptr) {
        part = GetTemplateChild(Base::StringView("PART_Header"));
    }
    auto* presenter = part != nullptr ? ::Aero::TryCast<ContentPresenter>(part) : nullptr;
    if (presenter == nullptr) {
        return;
    }
    presenter->HostUiElement(header.AsObject(), *element);
}

// ============================================================================
// Expander
// ============================================================================

Expander::Expander() noexcept
    : HeaderedContentControl(StaticTypeId()),
      headerCheckedHandler_(this, &Expander::OnHeaderCheckedChanged) {}

Expander::~Expander() {
    UnbindHeaderToggle();
}

bool Expander::GetIsExpanded() const noexcept {
    return GetValue(IsExpandedProperty);
}

void Expander::SetIsExpanded(bool value) noexcept {
    const bool old = GetIsExpanded();
    if (old == value) return;
    SetValue(IsExpandedProperty, value);
}

void Expander::OnExpanded() {
    RoutedEventArgs eventArgs;
    RaiseEvent(ExpandedEvent, &eventArgs);
}

void Expander::OnCollapsed() {
    RoutedEventArgs eventArgs;
    RaiseEvent(CollapsedEvent, &eventArgs);
}

void Expander::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    HeaderedContentControl::OnPropertyChanged(args);
    if (args.GetProperty() == IsExpandedProperty) {
        const bool expanded = args.GetNewValue().AsBoolean();
        if (!synchronizingHeader_ && headerToggle_ != nullptr) {
            const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
            const bool checked = isChecked.GetHasValue() ? isChecked.GetValue() : false;
            if (checked != expanded) {
                synchronizingHeader_ = true;
                headerToggle_->SetIsChecked(Nullable<bool>{expanded});
                synchronizingHeader_ = false;
            }
        }
        InvalidateMeasure();
        if (expanded) {
            OnExpanded();
        } else {
            OnCollapsed();
        }
    }
}

void Expander::OnHeaderCheckedChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (synchronizingHeader_ || headerToggle_ == nullptr) {
        return;
    }
    const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
    const bool checked = isChecked.GetHasValue() ? isChecked.GetValue() : false;
    if (checked == GetIsExpanded()) {
        return;
    }
    synchronizingHeader_ = true;
    SetIsExpanded(checked);
    synchronizingHeader_ = false;
}

void Expander::UnbindHeaderToggle() noexcept {
    if (headerToggle_ == nullptr) {
        return;
    }
    static_cast<void>(headerToggle_->RemoveValueChangedHandler(
        ToggleButton::IsCheckedProperty,
        headerCheckedHandler_));
    headerToggle_ = nullptr;
}

void Expander::BindHeaderToggle() noexcept {
    UnbindHeaderToggle();
    UIElement* root = GetTemplateRoot();
    if (root == nullptr) {
        return;
    }
    const auto findHeader = [this](auto& self, Aero::Media::Visual& visual) -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) {
            if (toggle->GetTemplatedParent() == this) {
                return toggle;
            }
        }
        const std::uint32_t count = Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child = Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (ToggleButton* found = self(self, *child)) {
                return found;
            }
        }
        return nullptr;
    };
    headerToggle_ = findHeader(findHeader, *root);
    if (headerToggle_ == nullptr) {
        return;
    }
    static_cast<void>(headerToggle_->AddValueChangedHandler(
        ToggleButton::IsCheckedProperty,
        headerCheckedHandler_));
    const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
    const bool checked = isChecked.GetHasValue() ? isChecked.GetValue() : false;
    if (checked != GetIsExpanded()) {
        synchronizingHeader_ = true;
        headerToggle_->SetIsChecked(Nullable<bool>{GetIsExpanded()});
        synchronizingHeader_ = false;
    }
}

void Expander::OnApplyTemplate() noexcept {
    HeaderedContentControl::OnApplyTemplate();
    BindHeaderToggle();
}

void Expander::OnTemplateDetached() noexcept {
    UnbindHeaderToggle();
    HeaderedContentControl::OnTemplateDetached();
}

ExpandDirection Expander::GetDirection() const noexcept {
    return GetValue(ExpandDirectionProperty);
}

void Expander::SetDirection(ExpandDirection value) noexcept {
    SetValue(ExpandDirectionProperty, value);
}

Size Expander::MeasureOverride(Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::MeasureOverride(availableSize);
    }
    constexpr double HeaderExtent = 24.0;
    if (!GetIsExpanded() || GetContentElement() == nullptr) {
        return GetDirection() == ExpandDirection::Left || GetDirection() == ExpandDirection::Right
            ? Size{HeaderExtent, 0.0}
            : Size{0.0, HeaderExtent};
    }
    Size childAvailable = availableSize;
    if (GetDirection() == ExpandDirection::Left || GetDirection() == ExpandDirection::Right) {
        childAvailable.width = std::max(0.0, childAvailable.width - HeaderExtent);
    } else {
        childAvailable.height = std::max(0.0, childAvailable.height - HeaderExtent);
    }
    MeasureChild(*GetContentElement(), childAvailable);
    const Size desired = GetContentElement()->GetDesiredSize();
    return GetDirection() == ExpandDirection::Left || GetDirection() == ExpandDirection::Right
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Size Expander::ArrangeOverride(Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::ArrangeOverride(finalSize);
    }
    if (!GetIsExpanded() || GetContentElement() == nullptr) {
        return finalSize;
    }
    constexpr double HeaderExtent = 24.0;
    Rect slot{0.0, 0.0, finalSize.width, finalSize.height};
    switch (GetDirection()) {
    case ExpandDirection::Down:
        slot.y += HeaderExtent;
        slot.height = std::max(0.0, slot.height - HeaderExtent);
        break;
    case ExpandDirection::Up:
        slot.height = std::max(0.0, slot.height - HeaderExtent);
        break;
    case ExpandDirection::Right:
        slot.x += HeaderExtent;
        slot.width = std::max(0.0, slot.width - HeaderExtent);
        break;
    case ExpandDirection::Left:
        slot.width = std::max(0.0, slot.width - HeaderExtent);
        break;
    }
    ArrangeChild(*GetContentElement(), slot);
    return finalSize;
}

// ============================================================================
// Page
// ============================================================================

class PageContentHost : public ContentControl {
public:
    PageContentHost() noexcept : ContentControl(ContentControl::StaticTypeId()) {}
};

void Page::EnsureHost() noexcept {
    if (host_) return;
    Base::Result<Ref<PageContentHost>> made = Base::MakeRef<PageContentHost>();
    if (!made) return;
    host_ = std::move(made).Value();
    if (host_->GetVisualParent() == nullptr) { AddVisualChild(host_.Get()); }
}

UIElement* Page::GetContent() const noexcept {
    if (!host_) return nullptr;
    const Value value = host_->GetContent();
    if (value.Kind() != ValueKind::Object || !value.AsObject()) return nullptr;
    return TryCast<UIElement>(value.AsObject().Get());
}

Value Page::GetContentValue() const noexcept {
    return host_ ? host_->GetContent() : Value::NullObject(Meta::TypeOf<Base::Object>());
}

void Page::SetContent(UIElement* content) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    EnsureHost();
    if (!host_) return;
    synchronizingContent_ = true;
    Value propertyValue = content != nullptr
        ? Value::FromObject(content->RuntimeType(), Ref<Base::Object>::FromBorrowed(*content))
        : Value::NullObject(Meta::TypeOf<Base::Object>());
    SetValue(ContentProperty, std::move(propertyValue));
    host_->SetContent(content);
    synchronizingContent_ = false;
    InvalidateMeasure();
}

void Page::SetContent(Ref<UIElement> content) noexcept {
    SetContent(content.Get());
}

void Page::SetContent(StringView text) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    EnsureHost();
    if (!host_) return;
    synchronizingContent_ = true;
    Result<Value> encoded = Value::TryFromString(Meta::TypeOf<String>(), text);
    if (encoded) SetValue(ContentProperty, std::move(encoded).Value());
    host_->SetContent(text);
    synchronizingContent_ = false;
    InvalidateMeasure();
}

void Page::SetContent(Value value) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    EnsureHost();
    if (!host_) return;
    synchronizingContent_ = true;
    SetValue(ContentProperty, value);
    host_->SetContent(std::move(value));
    synchronizingContent_ = false;
    InvalidateMeasure();
}

Ref<Base::Object> Page::GetContentTemplate() const noexcept {
    return GetValue(ContentTemplateProperty);
}

void Page::SetContentTemplate(Ref<Base::Object> value) noexcept {
    SetValue(ContentTemplateProperty, std::move(value));
}

void Page::OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept {
    FrameworkElement::OnPropertyChanged(args);
    if (synchronizingContent_) return;
    if (args.GetProperty() == ContentProperty) {
        EnsureHost();
        if (host_) host_->SetContent(args.GetNewValue());
    } else if (args.GetProperty() == ContentTemplateProperty) {
        EnsureHost();
        if (host_) host_->SetContentTemplate(GetContentTemplate());
    }
}

Size Page::MeasureOverride(Size availableSize) noexcept {
    if (!host_) return Size{};
    MeasureChild(*host_, availableSize);
    return host_->GetDesiredSize();
}

Size Page::ArrangeOverride(Size finalSize) noexcept {
    if (!host_) return finalSize;
    ArrangeChild(*host_, {0.0, 0.0, finalSize.width, finalSize.height});
    return finalSize;
}

// ============================================================================
// Internal Helpers & Describe Registrations
// ============================================================================

namespace {

class BasicControl : public Control {
public:
    BasicControl() noexcept : Control(Control::StaticTypeId()) {}
};

class BasicContentControl : public ContentControl {
public:
    BasicContentControl() noexcept : ContentControl(ContentControl::StaticTypeId()) {}
};

class BasicHeaderedContentControl : public HeaderedContentControl {
public:
    BasicHeaderedContentControl() noexcept
        : HeaderedContentControl(HeaderedContentControl::StaticTypeId()) {}
};

void SetContentControlContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) return;
    static_cast<ContentControl&>(owner).SetContentValue(child);
}

void ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ContentControl&>(owner).SetContentValue(Meta::Value::NullObject(Meta::TypeOf<Base::Object>()));
}

void SetContentPresenterContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) return;
    static_cast<ContentPresenter&>(owner).SetOwnedContent(
        child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearContentPresenterContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ContentPresenter&>(owner).SetContent(nullptr);
}

void SetPageContent(Base::Object& owner, const Base::Ref<Base::Object>& child, void*) noexcept {
    if (!child) return;
    UIElement* element = TryCast<UIElement>(child.Get());
    if (element == nullptr) return;
    Ref<UIElement> retained = Ref<UIElement>::TryFromBorrowed(*element);
    if (!retained) return;
    static_cast<Page&>(owner).SetContent(std::move(retained));
}

void ClearPageContent(Base::Object& owner, void*) noexcept {
    static_cast<Page&>(owner).SetContent(static_cast<UIElement*>(nullptr));
}

} // namespace

AERO_DESCRIBE(Control) {
    using namespace Aero::Meta;
    Register<Control>(context)
        .Event(Control::PreviewMouseDoubleClickEvent, RoutingStrategy::Tunnel)
        .Event(Control::MouseDoubleClickEvent)
        .Override(UIElement::FocusableProperty, true, FrameworkPropertyMetadataOptions::None)
        .Override(UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Property(Control::BackgroundProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Control::BorderBrushProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Control::BorderThicknessProperty, Aero::Thickness{}, AffectsMeasure | AffectsRender, &ValidateThicknessValue)
        .Property(Control::PaddingProperty, Aero::Thickness{}, AffectsMeasure, &ValidateThicknessValue)
        .Property(Control::FontWeightProperty, FontWeight::Normal, AffectsMeasure)
        .Property(Control::HorizontalContentAlignmentProperty, Aero::HorizontalAlignment::Left, AffectsArrange)
        .Property(Control::VerticalContentAlignmentProperty, Aero::VerticalAlignment::Top, AffectsArrange)
        .Property(Control::FontSizeProperty, 15.0, Inherits | AffectsMeasure, &ValidatePositiveFiniteDouble)
        .Property(Control::FocusVisualStyleProperty, Base::Ref<Aero::Style>{})
        .Property(Control::OverridesDefaultStyleProperty, false)
        .Property(Control::TemplateProperty, Base::Ref<ControlTemplate>{}, AffectsMeasure)
        .Factory<BasicControl>();
}

AERO_DESCRIBE(ContentControl) {
    using namespace Aero::Meta;
    Register<ContentControl>(context)
        .Property(ContentControl::ContentProperty, FrameworkPropertyMetadata(Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure).Structural())
        .Property(ContentControl::ContentTemplateProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .Property(ContentControl::ContentTemplateSelectorProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .ContentAccessor(MakeMemberId(ContentControl::StaticTypeId(), MemberKind::Property, "Content"), ContentKind::Single, &SetContentControlContent, &ClearContentControlContent, ContentFlags::Visual)
        .Factory<BasicContentControl>();
}

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

AERO_DESCRIBE(HeaderedContentControl) {
    using namespace Aero::Meta;
    Register<HeaderedContentControl>(context)
        .Property(HeaderedContentControl::HeaderProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure)
        .Property(HeaderedContentControl::HeaderTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Factory<BasicHeaderedContentControl>();
}

AERO_DESCRIBE(UserControl) {
    using namespace Aero::Meta;
    Register<UserControl>(context)
        .Factory();
}

AERO_DESCRIBE(GroupBox) {
    using namespace Aero::Meta;
    Register<GroupBox>(context)
        .Factory();
}

AERO_DESCRIBE(Label) {
    using namespace Aero::Meta;
    Register<Label>(context)
        .Factory();
}

AERO_DESCRIBE(Expander) {
    using namespace Aero::Meta;
    Register<Expander>(context)
        .Event(Expander::ExpandedEvent)
        .Event(Expander::CollapsedEvent)
        .Property(Expander::IsExpandedProperty, false, AffectsMeasure)
        .Property(Expander::ExpandDirectionProperty, ExpandDirection::Down, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(Page) {
    using namespace Aero::Meta;
    Register<Page>(context)
        .Property(
            Page::ContentProperty,
            FrameworkPropertyMetadata(
                Meta::Value::NullObject(Meta::TypeOf<Base::Object>()),
                AffectsMeasure)
                .Structural())
        .Property(Page::ContentTemplateProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .ContentAccessor(
            MakeMemberId(Page::StaticTypeId(), MemberKind::Property, "Content"),
            ContentKind::Single,
            &SetPageContent,
            &ClearPageContent,
            ContentFlags::Visual)
        .Factory();
}

} // namespace Aero::Controls
