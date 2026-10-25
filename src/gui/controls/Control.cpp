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
#include "gui/controls/ItemsContainers.hpp"
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

namespace {

void DetachVisualAttachment(ElementTree* tree, ::Aero::Media::Visual* parent, UIElement* child) noexcept {
    if (tree == nullptr || child == nullptr) {
        return;
    }
    ::Aero::VisualAttachment state;
    state.visualParent = parent != nullptr ? parent : child->GetVisualParent();
    state.child = child;
    state.visualAttached = child->GetVisualParent() != nullptr;
    state.layoutAttached = child->GetIsLayoutAttached() && child->LayoutParent() != nullptr;
    state.renderAttached = false;
    if (state.IsAttached()) {
        static_cast<void>(tree->DetachVisual(state));
    }
}

} // namespace

// ============================================================================
// Control
// ============================================================================

Size Control::MeasureOverride(Size availableSize) noexcept {
    if (templateChild_ == nullptr && !IsTemplateApplied()) {
        ApplyTemplate();
    }
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
    PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height});
}

// ============================================================================
// AttachOwnedContentSubtree helper
// ============================================================================

void AttachOwnedContentSubtree(
    ElementTree& tree,
    UIElement& parent) noexcept {
    const std::uint32_t count = Aero::Media::VisualTreeHelper::GetChildrenCount(parent);
    for (std::uint32_t index = 0U; index < count; ++index) {
        if (auto* childVisual = Aero::Media::VisualTreeHelper::GetChild(parent, index)) {
            if (auto* child = ::Aero::TryCast<UIElement>(childVisual)) {
                if (child->GetVisualParent() != nullptr &&
                    child->GetVisualParent() != &parent) {
                    static_cast<void>(tree.DetachVisual(
                        *child->GetVisualParent(),
                        static_cast<::Aero::Media::Visual&>(*child)));
                }
                if (child->GetVisualParent() != &parent || !child->GetIsLayoutAttached()) {
                    static_cast<void>(tree.AttachVisualChild(parent, *child));
                }
                if (Aero::BindingEngine* bindings = Aero::ElementTree::BindingsOf(*child)) {
                    static_cast<void>(bindings->ActivateDeferredWhenReady(*child));
                }
                AttachOwnedContentSubtree(tree, *child);
            }
        }
    }
}

// ============================================================================
// ContentControl
// ============================================================================

ContentControl::ContentControl(TypeId runtimeType) noexcept
    : Control(runtimeType) {}

ContentControl::~ContentControl() {
    ClearFallbackPresenter();
}

void ContentControl::OnContentChanged(const Value&, const Value&) {}

void ContentControl::OnContentTemplateChanged(
    const Ref<Base::Object>&,
    const Ref<Base::Object>&) {}

void ContentControl::OnContentTemplateSelectorChanged(
    const Ref<Base::Object>&,
    const Ref<Base::Object>&) {}

void ContentControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Control::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == ContentProperty) {
        OnContentPropertyChanged(*this, args);
        OnContentChanged(args.GetOldValue(), args.GetNewValue());
    } else if (prop == ContentTemplateProperty) {
        if (fallbackPresenter_) {
            fallbackPresenter_->SetValue(ContentPresenter::ContentTemplateProperty, args.GetNewValue());
        }
        OnContentTemplateChanged(
            args.GetOldValue().Kind() == Meta::ValueKind::Object ? args.GetOldValue().AsObject() : Ref<Base::Object>{},
            args.GetNewValue().Kind() == Meta::ValueKind::Object ? args.GetNewValue().AsObject() : Ref<Base::Object>{});
    } else if (prop == ContentTemplateSelectorProperty) {
        OnContentTemplateSelectorChanged(
            args.GetOldValue().Kind() == Meta::ValueKind::Object ? args.GetOldValue().AsObject() : Ref<Base::Object>{},
            args.GetNewValue().Kind() == Meta::ValueKind::Object ? args.GetNewValue().AsObject() : Ref<Base::Object>{});
    }
}

void ContentControl::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& change) noexcept {
    auto& control = static_cast<ContentControl&>(object);
    const Value& val = change.GetNewValue();
    if (val.Kind() == Meta::ValueKind::Object && !val.IsNullObject()) {
        control.ownedContent_ = val.AsObject();
    } else {
        control.ownedContent_.Reset();
    }
    if (control.fallbackPresenter_) {
        control.fallbackPresenter_->SetContentValue(val);
    }
}

void ContentControl::SetContentValue(Base::Ref<Base::Object> value) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    if (value) {
        SetValue(ContentProperty, Value::FromObject(value->RuntimeType(), value));
    } else {
        SetValue(ContentProperty, Value::NullObject(Meta::TypeOf<Base::Object>()));
    }
}

void ContentControl::SetContentValue(Meta::Value value) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    SetValue(ContentProperty, std::move(value));
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
    if (content != nullptr) {
        ownedContent_ = Ref<Base::Object>::FromBorrowed(*content);
        SetContentValue(Value::FromObject(content->RuntimeType(), ownedContent_));
    } else {
        ownedContent_.Reset();
        SetContentValue(Value::NullObject(Meta::TypeOf<Base::Object>()));
    }
}

void ContentControl::SetOwnedContent(const Ref<Base::Object>& contentObject, UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) { return; }
    Result<void> access = VerifyAccess();
    if (!access) return;
    ownedContent_ = contentObject;
    SetContentValue(Value::FromObject(contentObject->RuntimeType(), contentObject));
}

void ContentControl::SetGeneratedTextContent(const Ref<Base::Object>& contentObject, UIElement& content) noexcept {
    SetOwnedContent(contentObject, content);
}

UIElement* ContentControl::GetContentElement() const noexcept {
    if (ownedContent_) {
        return TryCast<UIElement>(ownedContent_.Get());
    }
    const Value contentVal = GetContent();
    if (contentVal.Kind() == Meta::ValueKind::Object && !contentVal.IsNullObject()) {
        return TryCast<UIElement>(contentVal.AsObject().Get());
    }
    return nullptr;
}

ContentPresenter* ContentControl::GetFallbackPresenter() const noexcept {
    return fallbackPresenter_.Get();
}

void ContentControl::EnsureFallbackPresenter() noexcept {
    if (GetTemplateRoot() != nullptr) {
        ClearFallbackPresenter();
        return;
    }
    const Value contentVal = GetContent();
    if (contentVal.IsUnset() || (contentVal.Kind() == Meta::ValueKind::Object && contentVal.IsNullObject())) {
        ClearFallbackPresenter();
        return;
    }
    if (!fallbackPresenter_) {
        fallbackPresenter_ = Base::MakeRef<ContentPresenter>();
        if (!fallbackPresenter_) return;
        UIElement* existingContent = ownedContent_ ? TryCast<UIElement>(ownedContent_.Get()) : nullptr;
        if (existingContent != nullptr && existingContent->GetVisualParent() == this) {
            if (ElementTree* tree = VisualTree(this)) {
                DetachVisualAttachment(tree, this, existingContent);
            } else {
                RemoveVisualChild(existingContent);
            }
        }
    }
    if (ElementTree* tree = VisualTree(this)) {
        if (fallbackPresenter_->GetVisualParent() != this ||
            !fallbackPresenter_->GetIsLayoutAttached()) {
            static_cast<void>(tree->AttachVisualChild(*this, *fallbackPresenter_));
        }
    } else if (fallbackPresenter_->GetVisualParent() == nullptr) {
        AddVisualChild(fallbackPresenter_.Get());
    }
    fallbackPresenter_->SetValue(ContentPresenter::ContentTemplateProperty, GetContentTemplate());
    fallbackPresenter_->SetContentValue(contentVal);
}

void ContentControl::ClearFallbackPresenter() noexcept {
    if (!fallbackPresenter_) return;
    fallbackPresenter_->ClearHostedContent();
    if (fallbackPresenter_->GetVisualParent() == this) {
        if (ElementTree* tree = VisualTree(this)) {
            DetachVisualAttachment(tree, this, fallbackPresenter_.Get());
        } else {
            RemoveVisualChild(fallbackPresenter_.Get());
        }
    }
    fallbackPresenter_.Reset();
}

void ContentControl::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    if (GetTemplateRoot() != nullptr) {
        ClearFallbackPresenter();
    }
}

std::uint32_t ContentControl::GetVisualChildrenCount() const noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::GetVisualChildrenCount(); }
    if (fallbackPresenter_ != nullptr && fallbackPresenter_->GetVisualParent() == this) {
        return 1U;
    }
    UIElement* content = GetContentElement();
    if (content != nullptr && content->GetVisualParent() == this) {
        return 1U;
    }
    return 0U;
}

::Aero::Media::Visual* ContentControl::GetVisualChild(std::uint32_t index) const noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::GetVisualChild(index); }
    if (index != 0U) {
        return nullptr;
    }
    if (fallbackPresenter_ != nullptr && fallbackPresenter_->GetVisualParent() == this) {
        return fallbackPresenter_.Get();
    }
    UIElement* content = GetContentElement();
    if (content != nullptr && content->GetVisualParent() == this) {
        return content;
    }
    return nullptr;
}

std::uint32_t ContentControl::GetLogicalChildrenCount() const noexcept {
    UIElement* elem = GetContentElement();
    return elem != nullptr ? 1U : Control::GetLogicalChildrenCount();
}

DependencyObject* ContentControl::GetLogicalChild(std::uint32_t index) const noexcept {
    UIElement* elem = GetContentElement();
    if (elem != nullptr) { return index == 0U ? elem : nullptr; }
    return Control::GetLogicalChild(index);
}

Size ContentControl::MeasureOverride(Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::MeasureOverride(availableSize); }
    EnsureFallbackPresenter();
    if (fallbackPresenter_ == nullptr) { return Size{}; }
    MeasureChild(*fallbackPresenter_, availableSize);
    return fallbackPresenter_->GetDesiredSize();
}

Size ContentControl::ArrangeOverride(Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) { return Control::ArrangeOverride(finalSize); }
    EnsureFallbackPresenter();
    if (fallbackPresenter_ == nullptr) return finalSize;
    ArrangeChild(*fallbackPresenter_, {0.0, 0.0, finalSize.width, finalSize.height});
    return finalSize;
}

// ============================================================================
// ContentPresenter
// ============================================================================

ContentPresenter::ContentPresenter() noexcept
    : ContentPresenter(StaticTypeId()) {}

ContentPresenter::ContentPresenter(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType) {}

static void DetachSubtreeBindings(
    Aero::Media::Visual& root,
    Aero::BindingEngine* bindings) noexcept {
    if (bindings != nullptr) {
        static_cast<void>(bindings->DetachObject(root));
    }
    const std::uint32_t logicalCount = Aero::LogicalTreeHelper::GetChildrenCount(root);
    for (std::uint32_t i = 0U; i < logicalCount; ++i) {
        if (auto* child = Aero::LogicalTreeHelper::GetChild(root, i)) {
            if (child != &root && bindings != nullptr) static_cast<void>(bindings->DetachObject(*child));
        }
    }
    const std::uint32_t count = Aero::Media::VisualTreeHelper::GetChildrenCount(root);
    for (std::uint32_t i = 0U; i < count; ++i) {
        if (auto* child = Aero::Media::VisualTreeHelper::GetChild(root, i)) {
            if (child != &root) {
                DetachSubtreeBindings(*child, bindings);
            }
        }
    }
}

void ContentPresenter::HostUiElement(
    const Base::Ref<Base::Object>& owner,
    UIElement& element) noexcept {
    if (!owner || owner.Get() != &element || &element == this) {
        return;
    }
    ElementTree* tree = VisualTree(this);
    const auto detachHosted = [&](UIElement& hosted) noexcept {
        Aero::BindingEngine* bindings = ElementTree::BindingsOf(*this);
        DetachSubtreeBindings(hosted, bindings);
        DetachVisualAttachment(tree, hosted.GetVisualParent() != nullptr ? hosted.GetVisualParent() : this, &hosted);
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
        DetachVisualAttachment(detachTree, element.GetVisualParent(), &element);
    }
    if (element.GetVisualParent() != this || !element.GetIsLayoutAttached()) {
        if (tree != nullptr) {
            static_cast<void>(tree->AttachVisualChild(*this, element));
        } else if (element.GetVisualParent() == nullptr) {
            AddVisualChild(&element);
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
    DetachVisualAttachment(tree, content_->GetVisualParent() != nullptr ? content_->GetVisualParent() : this, content_);
    content_ = nullptr;
    ownedContent_.Reset();
    InvalidateMeasure();
}

void ContentPresenter::ApplyContent() noexcept {
    contentValue_ = GetValue(ContentProperty);
    Base::Ref<Base::Object> templateObj = GetValue(ContentTemplateProperty);

    DependencyObject* templatedParent = GetTemplatedParent();
    Base::StringView source = GetContentSource();
    if (templatedParent != nullptr && !source.Empty()) {
        auto& registry = DependencyObjectAccess::PropertyRegistry(*templatedParent);
        if (contentValue_.Kind() == Meta::ValueKind::Unset ||
            (contentValue_.Kind() == Meta::ValueKind::Object && contentValue_.IsNullObject())) {
            if (const Meta::DependencyProperty* prop =
                    registry.Find(templatedParent->RuntimeType(), source)) {
                contentValue_ = templatedParent->GetValue(prop->Handle());
            }
        }
        if (!templateObj) {
            Base::String templatePropName;
            if (templatePropName.Assign(source) && templatePropName.Append(Base::StringView("Template"))) {
                if (const Meta::DependencyProperty* tProp =
                        registry.Find(templatedParent->RuntimeType(), templatePropName.View())) {
                    Value tVal = templatedParent->GetValue(tProp->Handle());
                    if (tVal.Kind() == Meta::ValueKind::Object && !tVal.IsNullObject()) {
                        templateObj = tVal.AsObject();
                    }
                }
            }
        }
    }
    const Value& value = contentValue_;

    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Object* obj = value.AsObject().Get();
        if (obj == this) {
            ClearHostedContent();
            return;
        }
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
        Base::Ref<TextBlock> made = Base::MakeRef<TextBlock>();
        if (made) {
            Base::Ref<Base::Object> owned = made;
            auto* tb = static_cast<TextBlock*>(owned.Get());
            HostUiElement(owned, *tb);
        }
    }
    UpdatePresentedText();
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
        args.GetProperty() == ContentTemplateProperty.Handle() ||
        args.GetProperty() == ContentTemplateSelectorProperty.Handle()) {
        ApplyContent();
    }
    FrameworkElement::OnPropertyChanged(args);
}

void ContentPresenter::UpdatePresentedText() noexcept {
    if (appliedTemplate_ != nullptr) {
        return;
    }
    if (content_ == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
        return;
    }
    if (contentValue_.Kind() == Meta::ValueKind::Object && !contentValue_.IsNullObject()) {
        auto* obj = contentValue_.AsObject().Get();
        if (obj == content_ ||
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                obj->RuntimeType(), UIElement::StaticTypeId())) {
            return;
        }
    }
    Base::String text;
    switch (contentValue_.Kind()) {
    case Meta::ValueKind::String:
    case Meta::ValueKind::Boolean:
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double:
        {
            Base::Result<void> formatted = Base::ValueConversion::FormatScalarValue(text, contentValue_);
            if (!formatted) return;
        }
        break;
    case Meta::ValueKind::Object:
        if (!contentValue_.IsNullObject()) {
            auto* obj = contentValue_.AsObject().Get();
            if (obj != nullptr) {
                if (obj->RuntimeType() == ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
                    const Meta::Value& inner = static_cast<const ::Aero::Controls::BoxedItemValue&>(*obj).Value();
                    (void)Base::ValueConversion::FormatScalarValue(text, inner);
                } else if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
        return;
    }
    auto* textBlock = static_cast<TextBlock*>(content_);
    textBlock->SetText(text.View());
    if (DependencyObject* templatedParent = GetTemplatedParent()) {
        if (auto* control = ::Aero::TryCast<Control>(templatedParent)) {
            textBlock->SetValue(TextBlock::FontSizeProperty, control->GetValue(Control::FontSizeProperty));
            textBlock->SetValue(TextBlock::FontFamilyProperty, control->GetValue(Control::FontFamilyProperty));
            const Meta::PropertyValueSourceInfo cpFgSource =
                GetValueSourceInfo(FrameworkElement::ForegroundProperty.Handle());
            if (!cpFgSource.isInherited && cpFgSource.rank > Meta::PropertyValueRank::Inherited) {
                textBlock->SetValue(TextBlock::ForegroundProperty, GetValue(FrameworkElement::ForegroundProperty));
            } else {
                textBlock->SetValue(TextBlock::ForegroundProperty, control->GetValue(Control::ForegroundProperty));
            }
        }
    }
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
    if (!CheckAccess() || !ValidateContent(content)) return;
    if (content == content_) return;
    if (content_ != nullptr && content_->GetVisualParent() == this) {
        if (ElementTree* tree = VisualTree(this)) {
            DetachVisualAttachment(tree, this, content_);
        } else {
            RemoveVisualChild(content_);
        }
    }
    content_ = content;
    if (content == nullptr) {
        ownedContent_.Reset();
    } else {
        if (content->GetVisualParent() != this || !content->GetIsLayoutAttached()) {
            if (ElementTree* tree = VisualTree(this)) {
                static_cast<void>(tree->AttachVisualChild(*this, *content));
                AttachOwnedContentSubtree(*tree, *content);
            } else if (content->GetVisualParent() == nullptr) {
                AddVisualChild(content);
            }
        }
    }
    InvalidateMeasure();
}

void ContentPresenter::SetOwnedContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) {
        return;
    }
    if (!CheckAccess() || !ValidateContent(&content)) return;
    if (content_ != nullptr && content_ != &content && content_->GetVisualParent() == this) {
        if (ElementTree* tree = VisualTree(this)) {
            DetachVisualAttachment(tree, this, content_);
        } else {
            RemoveVisualChild(content_);
        }
    }
    content_ = &content;
    ownedContent_ = contentObject;
    if (content.GetVisualParent() != this || !content.GetIsLayoutAttached()) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachVisualChild(*this, content));
            AttachOwnedContentSubtree(*tree, content);
        } else if (content.GetVisualParent() == nullptr) {
            AddVisualChild(&content);
        }
    }
    InvalidateMeasure();
}

bool ContentPresenter::ValidateContent(UIElement* content) const noexcept {
    if (content == this) {
        return false;
    }
    if (content == nullptr) {
        return LayoutChildren().Empty();
    }
    return LayoutChildren().Empty() || IsOnlyAttachedContent(*content);
}

Size ContentPresenter::MeasureOverride(Size availableSize) noexcept {
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
    DependencyObject* part = GetTemplateChild(Base::StringView("HeaderHost"));
    if (part == nullptr) {
        part = GetTemplateChild(Base::StringView("PART_Header"));
    }
    if (auto* presenter = TryCast<ContentPresenter>(part)) {
        presenter->RefreshContentSource();
    }
}

void HeaderedContentControl::OnHeaderTemplateChanged(
    const Ref<DataTemplate>&,
    const Ref<DataTemplate>&) {
    DependencyObject* part = GetTemplateChild(Base::StringView("HeaderHost"));
    if (part == nullptr) {
        part = GetTemplateChild(Base::StringView("PART_Header"));
    }
    if (auto* presenter = TryCast<ContentPresenter>(part)) {
        presenter->RefreshContentSource();
    }
}

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
    ContentControl::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild(Base::StringView("HeaderHost"));
    if (part == nullptr) {
        part = GetTemplateChild(Base::StringView("PART_Header"));
    }
    if (auto* presenter = TryCast<ContentPresenter>(part)) {
        presenter->RefreshContentSource();
    }
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
    EnsureFallbackPresenter();
    UIElement* contentChild = GetFallbackPresenter() != nullptr
        ? static_cast<UIElement*>(GetFallbackPresenter())
        : GetContentElement();
    constexpr double HeaderExtent = 24.0;
    if (!GetIsExpanded() || contentChild == nullptr) {
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
    MeasureChild(*contentChild, childAvailable);
    const Size desired = contentChild->GetDesiredSize();
    return GetDirection() == ExpandDirection::Left || GetDirection() == ExpandDirection::Right
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Size Expander::ArrangeOverride(Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::ArrangeOverride(finalSize);
    }
    EnsureFallbackPresenter();
    UIElement* contentChild = GetFallbackPresenter() != nullptr
        ? static_cast<UIElement*>(GetFallbackPresenter())
        : GetContentElement();
    if (!GetIsExpanded() || contentChild == nullptr) {
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
    ArrangeChild(*contentChild, slot);
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
        .Property(ContentPresenter::ContentTemplateSelectorProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
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
        .Factory();
}

} // namespace Aero::Controls
