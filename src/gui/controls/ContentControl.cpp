#include <Aero/Controls.hpp>
#include "gui/core/Describe.hpp"
#include <Aero/Controls/ItemsPresenter.hpp>
#include <Aero/Controls/AlternationConverter.hpp>
#include <Aero/Controls/ItemsPanelTemplate.hpp>
#include <Aero/Style.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Data/CollectionView.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Collections.hpp>
#include <Aero/TryCast.hpp>
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/controls/ItemsContainers.hpp" 
#include "gui/templates/TemplateInstance.hpp"

#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cstdio>
#include <new>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"



namespace Aero::Controls {

using Aero::Controls::TemplateEngine;
using namespace ::Aero::Controls;
using namespace ::Aero;

ContentControl::ContentControl(
    TypeId runtimeType) noexcept
    : Control(runtimeType) {}

ContentControl::~ContentControl() = default;

void ContentControl::OnContentChanged(
    const Value&,
    const Value&) {}

void ContentControl::OnContentTemplateChanged(
    const Ref<Base::Object>&,
    const Ref<Base::Object>&) {}

void ContentControl::OnContentTemplateSelectorChanged(
    const Ref<Base::Object>&,
    const Ref<Base::Object>&) {}

void ContentControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    // Former OnContentPropertyChanged Changed-delegate body: runs before base
    // handling to preserve delegate-then-virtual firing order.
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
    // Do not copy Foreground as a local value. WPF generated TextBlock
    // content inherits Foreground (including ContentPresenter
    // TextElement.Foreground). A local copy would hide template opacity.
    text->SetValue(TextBlock::FontSizeProperty, GetFontSize());
}

void ContentControl::SetGeneratedTextContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            content.RuntimeType(),
            TextBlock::StaticTypeId())) {
        return;
    }
    SetOwnedContent(contentObject, content);
    literalTextContent_ = true;
    SyncGeneratedTextFormatting();
}


void ContentControl::StoreContentProperty(
    Meta::Value value) noexcept {
    if (synchronizingContentProperty_) return;
    synchronizingContentProperty_ = true;
    SetValue(ContentProperty, std::move(value));
    synchronizingContentProperty_ = false;
}

void ContentControl::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& control = static_cast<ContentControl&>(object);
    if (control.synchronizingContentProperty_) return;
    control.synchronizingContentProperty_ = true;
    static_cast<void>(
        (control).SetContentValue( change.GetNewValue()));
    control.synchronizingContentProperty_ = false;
}

void ContentControl::SetContentValue(
    Base::Ref<Base::Object> value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (value &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            value->RuntimeType(),
            UIElement::StaticTypeId())) {
        authoredContent_ = Meta::Value::FromObject(
            value->RuntimeType(), value);
        auto& content = *static_cast<UIElement*>(value.Get());
        SetOwnedContent(value, content);
        // UserControl / untemplated ContentControl host content as a direct
        // visual child. LoadComponent assigns Content after the control is
        // already in a View; without this attach, MeasureOverride sees an
        // unattached pointer and reports 0x0 (BlendTutorial ColorSelector).
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
        if (ElementTree* tree = VisualTree(this)) {
            if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    content.RuntimeType(), Panel::StaticTypeId())) {
                auto& panel = static_cast<Panel&>(content);
                const std::uint32_t count = panel.GetChildren().GetCount();
                for (std::uint32_t index = 0U; index < count; ++index) {
                    UIElement* nested = panel.GetChildren().GetItem(index);
                    if (nested == nullptr) {
                        continue;
                    }
                    if (nested->GetVisualParent() != nullptr &&
                        nested->GetVisualParent() != &panel) {
                        continue;
                    }
                    if (nested->GetVisualParent() == &panel &&
                        nested->GetIsLayoutAttached()) {
                        continue;
                    }
                    if (VisualTree(nested) == nullptr &&
                        nested->GetLogicalParent() == nullptr) {
                        (void)tree->AttachElement(panel, *nested);
                    } else {
                        (void)tree->AttachVisualChild(panel, *nested);
                    }
                }
                panel.InvalidateMeasure();
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
        ? Meta::Value::FromObject(
            contentValue_->RuntimeType(),
            contentValue_)
        : Meta::Value::NullObject(
            Meta::TypeOf<Base::Object>());
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
        return;
    }
    (void)tree->AttachVisualChild(*this, *content_);
}

void ContentControl::SetContentValue(
    Meta::Value value) noexcept {
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

    Base::Result<Base::Ref<TextBlock>> created =
        Base::MakeRef<TextBlock>();
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
    return;
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

Base::Result<Base::Ref<Base::Object>>
ContentControl::CreateTemplatedContent() const noexcept {
    if (content_ != nullptr) {
        return ownedContent_;
    }
    if (!contentValue_) {
        return Base::Ref<Base::Object>{};
    }
    Base::Ref<Base::Object> contentTemplate =
        GetContentTemplate();
    if (!contentTemplate) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ContentControl business content requires a ContentTemplate");
    }
    if (contentTemplate->RuntimeType() !=
        DataTemplate::StaticTypeId()) {
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

namespace {

class BasicContentControl : public ContentControl {
public:
    BasicContentControl() noexcept
        : ContentControl(ContentControl::StaticTypeId()) {}
};

void SetContentControlContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)(static_cast<ContentControl&>(owner)).SetContentValue( child);
}

void ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    (void)(static_cast<ContentControl&>(owner)).SetContentValue( Meta::Value::NullObject(Meta::TypeOf<Base::Object>()));
}

} // namespace

AERO_DESCRIBE(ContentControl) {
    using namespace Aero::Meta;
    Register<ContentControl>(context)
        .Property(ContentControl::ContentProperty, FrameworkPropertyMetadata(Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure).Structural())
        .Property(ContentControl::ContentTemplateProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .Property(ContentControl::ContentTemplateSelectorProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .ContentAccessor(MakeMemberId(ContentControl::StaticTypeId(), MemberKind::Property, "Content"), ContentKind::Single, &SetContentControlContent, &ClearContentControlContent, ContentFlags::Visual)
        .Factory<BasicContentControl>();
}

} // namespace Aero::Controls
