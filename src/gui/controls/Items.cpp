#include <Aero/Controls.hpp>
#include <Aero/Controls/ItemsPresenter.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Data/CollectionViewSource.hpp>
#include <Aero/DataTemplateSelector.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/TryCast.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/State.hpp" 
#include "gui/templates/TemplateState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cstdio>
#include <new>
#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Controls::TemplateEngine;

Base::Result<Value> AlternationConverter::Convert(
    const Value& value,
    const Value&) noexcept {
    if (values_.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "AlternationConverter has no values");
    }

    std::uint64_t index = 0U;
    if (value.Kind() == Meta::ValueKind::UnsignedInteger) {
        index = value.AsUnsignedInteger();
    } else if (value.Kind() == Meta::ValueKind::SignedInteger &&
               value.AsSignedInteger() >= 0) {
        index = static_cast<std::uint64_t>(
            value.AsSignedInteger());
    } else {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "AlternationConverter requires a non-negative integer index");
    }

    const Base::Ref<Base::Object>& selected =
        values_[static_cast<std::uint32_t>(
            index % values_.Size())];
    if (!selected) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AlternationConverter contains a null value");
    }
    return Value::FromObject(
        selected->RuntimeType(),
        selected);
}

Base::Result<Value> AlternationConverter::ConvertBack(
    const Value& value,
    const Value&) noexcept {
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "AlternationConverter ConvertBack requires an object value");
    }
    for (std::uint32_t index = 0U;
         index < values_.Size();
         ++index) {
        if (values_[index].Get() == value.AsObject().Get()) {
            return Meta::ValueCodec<std::uint32_t>::Encode(index);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "AlternationConverter value was not found");
}

Panel* ItemsPresenter::GetItemsHost() const noexcept {
    UIElement* child = GetChild();
    return child != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            child->RuntimeType(), Panel::StaticTypeId())
        ? static_cast<Panel*>(child)
        : nullptr;
}

void ItemsPresenter::SetItemsHost(
    const Base::Ref<Base::Object>& owner,
    Panel& panel) noexcept {
    Base::Result<void> assigned =
        AeroGuiInternal::DecoratorSetOwnedChild(
            *this, owner, panel);
    if (assigned) {
        static_cast<void>(InvalidateMeasure());
    }
}


using namespace ::Aero::Controls;
using namespace ::Aero;

Base::Result<void> AddBoxedItem(
    Collections::ObservableCollection<Base::Object>& source,
    Meta::Value value) noexcept {
    Base::Result<Base::Ref<::Aero::Controls::BoxedItemValue>> boxed =
        Base::MakeRef<::Aero::Controls::BoxedItemValue>(std::move(value));
    if (!boxed) return boxed.GetStatus();
    return source.Add(
        Base::Ref<Base::Object>(std::move(boxed).Value()));
}

Base::Result<void> AddBoxedStringItem(
    Collections::ObservableCollection<Base::Object>& source,
    Base::StringView value) noexcept {
    Base::Result<Meta::Value> boxed =
        Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(), value);
    if (!boxed) return boxed.GetStatus();
    return AddBoxedItem(
        source, std::move(boxed).Value());
}

ContentControl::ContentControl(
    TypeId runtimeType) noexcept
    : Control(runtimeType),
      foregroundChangedHandler_(
          this,
          &ContentControl::OnForegroundChanged),
      fontSizeChangedHandler_(
          this,
          &ContentControl::OnFontSizeChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        Control::FontSizeProperty,
        fontSizeChangedHandler_));
}

ContentControl::~ContentControl() {
    static_cast<void>(RemoveValueChangedHandler(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        Control::FontSizeProperty,
        fontSizeChangedHandler_));
}

void ContentControl::SyncGeneratedTextFormatting() noexcept {
    if (!literalTextContent_ || content_ == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
        return;
    }
    auto* text = static_cast<TextBlock*>(content_);
    // Do not copy Foreground as a local value. WPF generated TextBlock
    // content inherits Foreground (including ContentPresenter
    // TextElement.Foreground). A local copy would hide template opacity.
    text->SetValue(TextBlock::FontSizeProperty, GetFontSize());
}

void ContentControl::OnForegroundChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    SyncGeneratedTextFormatting();
}

void ContentControl::OnFontSizeChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    SyncGeneratedTextFormatting();
}

void ContentControl::SetGeneratedTextContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!PropertyRegistry().Types().IsDerivedFrom(
            content.RuntimeType(),
            TextBlock::StaticTypeId())) {
        return;
    }
    SetOwnedContent(contentObject, content);
    literalTextContent_ = true;
    SyncGeneratedTextFormatting();
}

Base::Ref<Base::Object> ItemCollection::GetItem(
    std::uint32_t index) const noexcept {
    return index < items_.Size()
        ? items_[index]
        : Base::Ref<Base::Object>();
}

void ItemCollection::Notify(
    const ItemsChangedEvent& event) noexcept {
    if (!changed_.Empty()) changed_.Invoke(event);
}

Base::Result<void> ItemCollection::Add(
    Base::Ref<Base::Object> item) noexcept {
    return Insert(items_.Size(), std::move(item));
}

Base::Result<void> ItemCollection::Insert(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index > items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemCollection insert is invalid");
    }
    Base::Result<void> reserved =
        items_.Reserve(items_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<void> added =
        items_.PushBack(std::move(item));
    if (!added) return added.GetStatus();
    Base::Ref<Base::Object> moving =
        std::move(items_.Back());
    for (std::uint32_t current =
            items_.Size() - 1U;
        current > index; --current) {
        items_[current] =
            std::move(items_[current - 1U]);
    }
    items_[index] = std::move(moving);
    Notify({
        ItemsChangeAction::Add,
        UINT32_MAX,
        index,
        0U,
        1U});
    return {};
}

Base::Result<Base::Ref<Base::Object>>
ItemCollection::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemCollection remove index is out of range");
    }
    Base::Ref<Base::Object> removed =
        std::move(items_[index]);
    for (std::uint32_t current = index;
        current + 1U < items_.Size(); ++current) {
        items_[current] =
            std::move(items_[current + 1U]);
    }
    items_.PopBack();
    Notify({
        ItemsChangeAction::Remove,
        index,
        UINT32_MAX,
        1U,
        0U});
    return removed;
}

Base::Result<void> ItemCollection::Replace(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemCollection replacement is invalid");
    }
    items_[index] = std::move(item);
    Notify({
        ItemsChangeAction::Replace,
        index,
        index,
        1U,
        1U});
    return {};
}

Base::Result<void> ItemCollection::Move(
    std::uint32_t oldIndex,
    std::uint32_t newIndex) noexcept {
    if (oldIndex >= items_.Size() ||
        newIndex >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemCollection move index is out of range");
    }
    if (oldIndex == newIndex) return {};
    Base::Ref<Base::Object> moving =
        std::move(items_[oldIndex]);
    if (oldIndex < newIndex) {
        for (std::uint32_t index = oldIndex;
            index < newIndex; ++index) {
            items_[index] =
                std::move(items_[index + 1U]);
        }
    } else {
        for (std::uint32_t index = oldIndex;
            index > newIndex; --index) {
            items_[index] =
                std::move(items_[index - 1U]);
        }
    }
    items_[newIndex] = std::move(moving);
    Notify({
        ItemsChangeAction::Move,
        oldIndex,
        newIndex,
        1U,
        1U});
    return {};
}

void ItemCollection::Reset() noexcept {
    const std::uint32_t oldCount = items_.Size();
    items_.Clear();
    Notify({
        ItemsChangeAction::Reset,
        0U,
        0U,
        oldCount,
        0U});
}

Base::Result<void> ItemCollection::Reset(
    Base::Span<const Base::Ref<Base::Object>>
        items) noexcept {
    Base::Vector<Base::Ref<Base::Object>> replacement;
    Base::Result<void> reserved =
        replacement.Reserve(items.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::Ref<Base::Object>& item : items) {
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ItemCollection reset item must not be null");
        }
        Base::Result<void> added =
            replacement.PushBack(item);
        if (!added) return added.GetStatus();
    }
    const std::uint32_t oldCount = items_.Size();
    items_ = std::move(replacement);
    Notify({
        ItemsChangeAction::Reset,
        0U,
        0U,
        oldCount,
        items_.Size()});
    return {};
}

Base::Result<void> ContentControl::StoreContentProperty(
    Meta::Value value) noexcept {
    if (synchronizingContentProperty_) return {};
    synchronizingContentProperty_ = true;
    SetValue(ContentProperty, std::move(value));
    synchronizingContentProperty_ = false;
    return {};
}

void ContentControl::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& control = static_cast<ContentControl&>(object);
    if (control.synchronizingContentProperty_) return;
    control.synchronizingContentProperty_ = true;
    static_cast<void>(
        AeroGuiInternal::SetContentValue(control, change.GetNewValue()));
    control.synchronizingContentProperty_ = false;
}

void ContentControl::SetContentValue(
    Base::Ref<Base::Object> value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (value &&
        PropertyRegistry().Types().IsDerivedFrom(
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
            if (ElementTree* tree = GetTree()) {
                if (content.GetTree() == nullptr &&
                    content.GetLogicalParent() == nullptr) {
                    (void)tree->AttachElement(*this, content);
                } else if (content.GetVisualParent() != this) {
                    (void)tree->AttachVisualChild(*this, content);
                }
            }
        }
        if (ElementTree* tree = GetTree()) {
            if (PropertyRegistry().Types().IsDerivedFrom(
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
                    if (nested->GetTree() == nullptr &&
                        nested->GetLogicalParent() == nullptr) {
                        (void)tree->AttachElement(panel, *nested);
                    } else {
                        (void)tree->AttachVisualChild(panel, *nested);
                    }
                }
                (void)panel.InvalidateMeasure();
            }
        }
        (void)InvalidateMeasure();
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
    (void)InvalidateMeasure();
}

void ContentControl::EnsureHostedContent() noexcept {
    if (GetTemplateRoot() != nullptr || content_ == nullptr) {
        return;
    }
    if (content_->GetVisualParent() == this &&
        content_->GetIsLayoutAttached()) {
        return;
    }
    ElementTree* tree = GetTree();
    if (tree == nullptr) {
        if (content_->GetVisualParent() == nullptr) {
            AddVisualChild(content_);
        }
        return;
    }
    if (content_->GetTree() == nullptr &&
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
        (void)InvalidateMeasure();
        return;
    }

    if (literalTextContent_ && content_ != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
        auto* textBlock = static_cast<TextBlock*>(content_);
        textBlock->SetValue(RichText::TextProperty, value.AsString());
        textBlock->SetText(value.AsString());
        (void)StoreContentProperty(value);
        authoredContent_ = std::move(value);
        contentValue_.Reset();
        (void)InvalidateMeasure();
        (void)InvalidateVisual();
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
        if (ElementTree* tree = GetTree()) {
            (void)tree->AttachElement(*this, *created.Value());
        }
    }
    (void)StoreContentProperty(value);
    authoredContent_ = std::move(value);
    contentValue_.Reset();
    literalTextContent_ = true;
    return;
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
        DataTemplateRuntime::Instantiate(
            *static_cast<DataTemplate*>(contentTemplate.Get()),
            contentValue_,
            AeroGuiInternal::BindingEngineOf(*this));
    if (!created) return created.GetStatus();
    if (!created.Value() ||
        !PropertyRegistry().Types().IsDerivedFrom(
            created.Value()->RuntimeType(),
            UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentTemplate must create a UIElement");
    }
    return created;
}

ItemsControl::ItemsControl() noexcept
    : ItemsControl(StaticTypeId()) {}

ItemsControl::ItemsControl(TypeId runtimeType) noexcept
    : Control(runtimeType),
      localHandler_(
          this, &ItemsControl::OnLocalChanged),
      sourceHandler_(
          this, &ItemsControl::OnSourceChanged) {
    static_cast<void>(
        items_.AddItemsChanged(localHandler_));
}

ItemsControl::~ItemsControl() {
    if (generator_ != nullptr) {
        static_cast<void>(generator_->Detach());
    }
    static_cast<void>(
        items_.RemoveItemsChanged(localHandler_));
    if (source_ != nullptr) {
        static_cast<void>(
            source_->RemoveItemsChanged(
                sourceHandler_));
    }
}

void ItemsControl::OnApplyTemplate() noexcept {
    DependencyObject* part =
        GetTemplateChild("ItemsHost");
    if (part == nullptr) {
        part = GetTemplateChild("ItemsPresenter");
    }
    if (part == nullptr) {
        part = GetTemplateChild(
            ItemsPresenter::StaticTypeId());
    }
    if (part == nullptr) {
        // Reference XAML is also allowed to declare an items host directly
        // (<StackPanel IsItemsHost="True"/>), without an ItemsPresenter or a
        // PART name. TemplateEngine resolves such a panel before returning a
        // generic Panel part.
        part = GetTemplateChild(Panel::StaticTypeId());
    }
    if (part == nullptr) {
        // Content inside a Popup is structurally projected by the template
        // builder and is therefore not necessarily present in the outer
        // template part table. Discover the direct IsItemsHost declaration
        // from the complete applied visual subtree.
        Base::Vector<::Aero::Media::Visual*> pending;
        UIElement* root = GetTemplateRoot();
        if (root != nullptr) {
            static_cast<void>(pending.PushBack(root));
        }
        while (!pending.Empty() && part == nullptr) {
            ::Aero::Media::Visual* current = pending.Back();
            pending.PopBack();
            if (current == nullptr) continue;
            if (PropertyRegistry().Types().IsDerivedFrom(
                    current->RuntimeType(), Panel::StaticTypeId())) {
                auto& panel = *static_cast<Panel*>(current);
                if (panel.GetValueOr(Panel::IsItemsHostProperty, false)) {
                    part = current;
                    break;
                }
            }
            if (PropertyRegistry().Types().IsDerivedFrom(
                    current->RuntimeType(), ContentControl::StaticTypeId())) {
                UIElement* content = AeroGuiInternal::ContentControlContent(
                    *static_cast<ContentControl*>(current));
                if (content != nullptr) {
                    static_cast<void>(pending.PushBack(content));
                }
            }
            for (::Aero::Media::Visual* child :
                     AeroGuiInternal::RenderChildren(*current)) {
                if (child != nullptr) {
                    static_cast<void>(pending.PushBack(child));
                }
            }
        }
    }
    if (part == nullptr) {
        // AeroTheme.Styles (copied from Noesis App Theme) does not set
        // ItemsControl.Template. Synthesize ItemsPresenter + ItemsPanel so
        // UniformGrid / StackPanel ItemsPanel templates still apply.
        static_cast<void>(EnsureDefaultItemsPresenter());
        return;
    }
    if (PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(),
            ItemsPresenter::StaticTypeId())) {
        itemsHost_ =
            static_cast<ItemsPresenter*>(part)->
                GetItemsHost();
        if (itemsHost_ == nullptr) {
            static_cast<void>(EnsureDefaultItemsPresenter());
        }
    } else if (PropertyRegistry().Types().IsDerivedFrom(
                   part->RuntimeType(),
                   Panel::StaticTypeId())) {
        itemsHost_ = static_cast<Panel*>(part);
    }
    if (itemsHost_ == nullptr) {
        return;
    }
    return;
}

void ItemsControl::OnTemplateDetached() noexcept {
    if (generator_ != nullptr) {
        static_cast<void>(generator_->Detach());
    }
    itemsHost_ = nullptr;
    defaultItemsPresenter_.Reset();
}

Size ItemsControl::MeasureOverride(Size availableSize) noexcept {
    // Noesis synthesizes the default ItemsPresenter during Measure when the
    // control has no template. OnApplyTemplate can run before the element is
    // in a tree; retry here so ItemsPanel (UniformGrid) still materializes.
    if (itemsHost_ == nullptr) {
        static_cast<void>(EnsureDefaultItemsPresenter());
    }
    return Control::MeasureOverride(availableSize);
}

bool ItemsControl::EnsureDefaultItemsPresenter() noexcept {
    if (itemsHost_ != nullptr) return true;
    ::Aero::ElementTree* tree = GetTree();
    if (tree == nullptr) return false;

    const auto makeHostPanel = [&]() noexcept -> std::pair<Base::Ref<Base::Object>, Panel*> {
        Base::Ref<Base::Object> panelOwner;
        Panel* panel = nullptr;
        const ItemsPanelTemplate* itemsPanel = GetItemsPanel();
        if (itemsPanel != nullptr) {
            Base::Result<Base::Ref<Base::Object>> created =
                ::Aero::Controls::ItemsPanelTemplateRuntime::Instantiate(*itemsPanel);
            if (created && created.Value() &&
                PropertyRegistry().Types().IsDerivedFrom(
                    created.Value()->RuntimeType(), Panel::StaticTypeId())) {
                panelOwner = std::move(created).Value();
                panel = static_cast<Panel*>(panelOwner.Get());
            }
        }
        if (panel == nullptr) {
            Base::Result<Base::Ref<StackPanel>> stack =
                Base::MakeRef<StackPanel>();
            if (!stack || !stack.Value()) return {};
            panelOwner = Base::Ref<Base::Object>(stack.Value());
            panel = stack.Value().Get();
        }
        panel->SetValue(Panel::IsItemsHostProperty, true);
        return {std::move(panelOwner), panel};
    };

    ItemsPresenter* templatedPresenter = nullptr;
    DependencyObject* part = GetTemplateChild("ItemsHost");
    if (part == nullptr) {
        part = GetTemplateChild("ItemsPresenter");
    }
    if (part != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(), ItemsPresenter::StaticTypeId())) {
        templatedPresenter = static_cast<ItemsPresenter*>(part);
    }
    if (templatedPresenter != nullptr) {
        if (templatedPresenter->GetItemsHost() == nullptr) {
            auto made = makeHostPanel();
            if (made.second == nullptr) return false;
            Base::Result<Aero::ElementAttachment> panelMounted =
                tree->AttachElement(*templatedPresenter, *made.second);
            if (!panelMounted) return false;
            templatedPresenter->SetItemsHost(made.first, *made.second);
        }
        itemsHost_ = templatedPresenter->GetItemsHost();
        return itemsHost_ != nullptr;
    }

    if (GetTemplateRoot() != nullptr) return false;

    Base::Result<Base::Ref<ItemsPresenter>> presenter =
        Base::MakeRef<ItemsPresenter>();
    if (!presenter || !presenter.Value()) return false;
    static_cast<void>(
        AeroGuiInternal::SetTemplatedParent(*presenter.Value(), this));

    auto made = makeHostPanel();
    Base::Ref<Base::Object> panelOwner = std::move(made.first);
    Panel* panel = made.second;
    if (panel == nullptr) return false;

    Base::Result<Aero::ElementAttachment> presenterMounted =
        tree->AttachElement(*this, *presenter.Value());
    if (!presenterMounted) return false;
    static_cast<void>(
        AeroGuiInternal::SetTemplateRoot(*this, presenter.Value().Get()));

    Base::Result<Aero::ElementAttachment> panelMounted =
        tree->AttachElement(*presenter.Value(), *panel);
    if (!panelMounted) return false;
    presenter.Value()->SetItemsHost(panelOwner, *panel);
    itemsHost_ = presenter.Value()->GetItemsHost();
    defaultItemsPresenter_ =
        Base::Ref<Base::Object>(std::move(presenter).Value());
    return itemsHost_ != nullptr;
}

std::uint32_t ItemsControl::GetCount() const noexcept {
    return source_ != nullptr
        ? source_->GetCount()
        : items_.GetCount();
}

std::uint32_t ItemsControl::GetRealizedItemCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GetGeneratedCount()
        : 0U;
}

std::uint32_t ItemsControl::GetCreatedContainerCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GetCreatedContainerCount()
        : 0U;
}

std::uint32_t
ItemsControl::GetRecycledContainerUseCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GetRecycledContainerUseCount()
        : 0U;
}

Base::Ref<Base::Object> ItemsControl::GetItem(
    std::uint32_t index) const noexcept {
    return source_ != nullptr
        ? source_->GetItem(index)
        : items_.GetItem(index);
}

void ItemsControl::SetItemsSourceCore(
    Collections::IItemsSource* source) noexcept {
    if (source != nullptr) {
        if (Data::CollectionView* view =
                Data::CollectionViewSource::GetDefaultView(source)) {
            source = view;
        }
    }
    if (source_ == source) return;
    if (source != nullptr) {
        source->AddItemsChanged(sourceHandler_);
    }
    if (source_ != nullptr) {
        static_cast<void>(
            source_->RemoveItemsChanged(
                sourceHandler_));
    }
    source_ = source;
    PublishItemCount();
    PublishReset();
    OnItemsSourceCoreChanged();
}

void ItemsControl::SetItemTemplateCore(
    const DataTemplate* value) noexcept {
    if (itemTemplate_ == value) return;
    itemTemplate_ = value;
    PublishReset();
}

void ItemsControl::SetItemTemplateSelectorCore(
    const DataTemplateSelector* value) noexcept {
    if (itemTemplateSelector_ == value) return;
    itemTemplateSelector_ = value;
    PublishReset();
}

Base::Ref<DataTemplate> ItemsControl::ResolveItemTemplate(
    const Base::Ref<Base::Object>& item,
    std::uint32_t) const noexcept {
    if (itemTemplateSelector_ != nullptr) {
        DataTemplateSelector* selector =
            const_cast<DataTemplateSelector*>(itemTemplateSelector_);
        Base::Ref<DataTemplate> selected = selector->SelectTemplate(
            item.Get(),
            const_cast<ItemsControl*>(this));
        if (selected) {
            return selected;
        }
    }
    if (itemTemplate_ != nullptr) {
        return Base::Ref<DataTemplate>::FromBorrowed(
            *const_cast<DataTemplate*>(itemTemplate_));
    }
    if (!item) {
        return {};
    }
    const Meta::TypeRegistry& types = PropertyRegistry().Types();
    Meta::TypeId type = item->RuntimeType();
    while (type != Meta::InvalidTypeId) {
        Base::Result<ResourceValue> found =
            TryFindResource(ResourceKey::FromType(type));
        if (found &&
            found.Value().Kind() == Meta::ValueKind::Object &&
            !found.Value().IsNullObject() &&
            found.Value().AsObject()) {
            if (DataTemplate* dataTemplate = TryCast<DataTemplate>(
                    found.Value().AsObject().Get())) {
                return Base::Ref<DataTemplate>::FromBorrowed(*dataTemplate);
            }
        }
        const Meta::TypeInfo* info = types.FindType(type);
        if (info == nullptr) {
            break;
        }
        const Meta::TypeId parent = info->BaseType();
        if (parent == type || parent == Meta::InvalidTypeId) {
            break;
        }
        type = parent;
    }
    return {};
}

void ItemsControl::SetItemsPanelCore(
    const ItemsPanelTemplate* value) noexcept {
    if (itemsPanel_ == value) return;
    itemsPanel_ = value;
    PublishReset();
}

void ItemsControl::SetItemContainerStyleCore(
    const Style* value) noexcept {
    if (itemContainerStyle_ == value) return;
    itemContainerStyle_ = value;
    PublishReset();
}

void ItemsControl::OnLocalChanged(
    const ItemsChangedEvent& event) noexcept {
    if (source_ != nullptr) return;
    PublishItemCount();
    if (!changed_.Empty()) changed_.Invoke(event);
}

void ItemsControl::OnSourceChanged(
    const ItemsChangedEvent& event) noexcept {
    PublishItemCount();
    if (!changed_.Empty()) changed_.Invoke(event);
}

void ItemsControl::PublishReset() noexcept {
    if (!changed_.Empty()) {
        changed_.Invoke({
            ItemsChangeAction::Reset,
            0U,
            0U,
            GetCount(),
            GetCount()});
    }
}

void ItemsControl::PublishItemCount() noexcept {
    const std::uint32_t count = GetCount();
    static_cast<void>(SetReadOnlyCurrentValue(
        ItemCountProperty, count));
    static_cast<void>(SetReadOnlyCurrentValue(
        HasItemsProperty, count != 0U));
}

Base::Result<Base::Ref<FrameworkElement>>
ItemsControl::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    // WPF/Noesis GetContainerForItemOverride returns ContentPresenter so
    // ItemContainerStyle TargetType="ContentPresenter" can apply. A generated
    // ContentControl rejects that style and aborts item UI activation.
    Base::Result<Base::Ref<ContentPresenter>> made =
        Base::MakeRef<ContentPresenter>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

Base::Result<void> ItemsControl::PrepareContainer(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    if (item && item.Get() != &container) {
        container.SetDataContext(
            Value::FromObject(
                item->RuntimeType(), item));
    }
    if (!item ||
        !PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(), ItemsControl::StaticTypeId())) {
        return {};
    }

    const Base::Ref<DataTemplate> resolved =
        ResolveItemTemplate(item, index);
    const HierarchicalDataTemplate* hierarchical =
        TryCast<HierarchicalDataTemplate>(resolved.Get());
    if (hierarchical == nullptr) {
        return {};
    }

    const Base::Ref<Base::Object> hierarchicalSource =
        hierarchical->GetItemsSource();
    const Base::Ref<Base::Object> hierarchicalTemplate =
        hierarchical->GetItemTemplate();
    if (!hierarchicalSource && !hierarchicalTemplate) return {};

    auto& childItems = static_cast<ItemsControl&>(container);
    Base::Ref<DataTemplate> childItemTemplate;
    if (hierarchicalTemplate &&
        PropertyRegistry().Types().IsDerivedFrom(
            hierarchicalTemplate->RuntimeType(),
            DataTemplate::StaticTypeId())) {
        childItemTemplate = Base::Ref<DataTemplate>::FromBorrowed(
            static_cast<DataTemplate&>(*hierarchicalTemplate));
    }
    if (!hierarchicalSource) {
        childItems.SetItemTemplate(std::move(childItemTemplate));
        return {};
    }
    if (!PropertyRegistry().Types().IsDerivedFrom(
            hierarchicalSource->RuntimeType(),
            Data::Binding::StaticTypeId())) {
        if (PropertyRegistry().Types().IsDerivedFrom(
                container.RuntimeType(),
                TreeViewItem::StaticTypeId())) {
            static_cast<TreeViewItem&>(container).SetHierarchicalContent(
                hierarchicalSource,
                std::move(childItemTemplate));
            return {};
        }
        childItems.SetItemTemplate(std::move(childItemTemplate));
        childItems.SetItemsSource(hierarchicalSource);
        return {};
    }

    auto* bindings = AeroGuiInternal::BindingEngineOf(childItems);
    if (bindings == nullptr || bindings->Metadata() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "HierarchicalDataTemplate Binding services are unavailable");
    }
    const auto& binding =
        static_cast<const Data::Binding&>(*hierarchicalSource);
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            TreeViewItem::StaticTypeId())) {
        static_cast<TreeViewItem&>(container).SetHierarchicalBinding(
            Base::Ref<Data::Binding>::FromBorrowed(
                const_cast<Data::Binding&>(binding)),
            item,
            std::move(childItemTemplate));
        return {};
    }
    childItems.SetItemTemplate(std::move(childItemTemplate));
    Data::MetadataBindingDescriptor descriptor;
    descriptor.metadata = bindings->Metadata();
    descriptor.source = item.Get();
    descriptor.target = &childItems;
    descriptor.targetProperty = ItemsSourceProperty.Handle();
    descriptor.path = binding.GetPathText();
    descriptor.stringFormat = binding.GetStringFormat();
    descriptor.mode = bindings->ResolveBindingMode(
        childItems,
        ItemsSourceProperty.Handle(),
        binding.GetMode());
    descriptor.updateSourceTrigger =
        bindings->ResolveUpdateSourceTrigger(
            childItems,
            ItemsSourceProperty.Handle(),
            binding.GetUpdateSourceTrigger());
    descriptor.converterResource = binding.GetConverter();
    descriptor.converterParameter = binding.GetConverterParameter();
    descriptor.fallbackValue = binding.GetFallbackValue();
    descriptor.targetNullValue = binding.GetTargetNullValue();
    Base::Result<void> queued = bindings->QueueDeferred(descriptor);
    if (!queued) return queued.GetStatus();
    return bindings->ActivateDeferredWhenReady(childItems);
}

void ItemsControl::ClearContainer(
    FrameworkElement& container) noexcept {
    Base::Ref<Base::Object> item;
    const Value dataContext = container.GetDataContext();
    if (dataContext.Kind() == Meta::ValueKind::Object &&
        !dataContext.IsNullObject()) {
        item = dataContext.AsObject();
    }
    const Base::Ref<DataTemplate> resolved =
        ResolveItemTemplate(item, 0U);
    if (TryCast<HierarchicalDataTemplate>(resolved.Get()) != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(), ItemsControl::StaticTypeId())) {
        auto& childItems = static_cast<ItemsControl&>(container);
        if (PropertyRegistry().Types().IsDerivedFrom(
                container.RuntimeType(),
                TreeViewItem::StaticTypeId())) {
            static_cast<TreeViewItem&>(container)
                .ClearHierarchicalContent();
        }
        childItems.SetItemsSource(Base::Ref<Base::Object>{});
        childItems.SetItemTemplate(Base::Ref<DataTemplate>{});
    }
    container.ClearValue(
        FrameworkElement::DataContextProperty);
}

} // namespace Aero::Controls
