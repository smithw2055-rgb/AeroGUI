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
#include "gui/core/TypeRegistryDetail.hpp"
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
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            child->RuntimeType(), Panel::StaticTypeId())
        ? static_cast<Panel*>(child)
        : nullptr;
}

void ItemsPresenter::SetItemsHost(
    const Base::Ref<Base::Object>& owner,
    Panel& panel) noexcept {
    (*this).SetOwnedChild( owner, panel);
    InvalidateMeasure();
}


using namespace ::Aero::Controls;
using namespace ::Aero;

void AddBoxedItem(
    Collections::ObservableCollection<Base::Object>& source,
    Meta::Value value) noexcept {
    Base::Result<Base::Ref<::Aero::Controls::BoxedItemValue>> boxed =
        Base::MakeRef<::Aero::Controls::BoxedItemValue>(std::move(value));
    if (!boxed) { AERO_ASSERT(false); return; }
    source.Add(
        Base::Ref<Base::Object>(std::move(boxed).Value()));
}

void AddBoxedStringItem(
    Collections::ObservableCollection<Base::Object>& source,
    Base::StringView value) noexcept {
    Base::Result<Meta::Value> boxed =
        Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(), value);
    if (!boxed) { AERO_ASSERT(false); return; }
    AddBoxedItem(
        source, std::move(boxed).Value());
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

void ItemCollection::Add(
    Base::Ref<Base::Object> item) noexcept {
    Insert(items_.Size(), std::move(item));
}

void ItemCollection::Insert(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index > items_.Size()) { AERO_ASSERT(false); return; }
    items_.Reserve(items_.Size() + 1U);
    items_.PushBack(std::move(item));
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
    replacement.Reserve(items.Size());
    for (const Base::Ref<Base::Object>& item : items) {
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ItemCollection reset item must not be null");
        }
        replacement.PushBack(item);
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
            if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    current->RuntimeType(), Panel::StaticTypeId())) {
                auto& panel = *static_cast<Panel*>(current);
                if (panel.GetValue(Panel::IsItemsHostProperty)) {
                    part = current;
                    break;
                }
            }
            if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    current->RuntimeType(), ContentControl::StaticTypeId())) {
                UIElement* content = (*static_cast<ContentControl*>(current)).GetContentElement();
                if (content != nullptr) {
                    static_cast<void>(pending.PushBack(content));
                }
            }
            for (::Aero::Media::Visual* child :
                     (*current).RenderChildren()) {
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
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            part->RuntimeType(),
            ItemsPresenter::StaticTypeId())) {
        itemsHost_ =
            static_cast<ItemsPresenter*>(part)->
                GetItemsHost();
        if (itemsHost_ == nullptr) {
            static_cast<void>(EnsureDefaultItemsPresenter());
        }
    } else if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
    ::Aero::ElementTree* tree = VisualTree(this);
    if (tree == nullptr) return false;

    const auto makeHostPanel = [&]() noexcept -> std::pair<Base::Ref<Base::Object>, Panel*> {
        Base::Ref<Base::Object> panelOwner;
        Panel* panel = nullptr;
        const ItemsPanelTemplate* itemsPanel = GetItemsPanel();
        if (itemsPanel != nullptr) {
            Base::Result<Base::Ref<Base::Object>> created =
                ::Aero::Controls::FrameworkTemplateState::Instantiate(*itemsPanel);
            if (created && created.Value() &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
        (*presenter.Value()).SetTemplatedParent( this));

    auto made = makeHostPanel();
    Base::Ref<Base::Object> panelOwner = std::move(made.first);
    Panel* panel = made.second;
    if (panel == nullptr) return false;

    Base::Result<Aero::ElementAttachment> presenterMounted =
        tree->AttachElement(*this, *presenter.Value());
    if (!presenterMounted) return false;
    static_cast<void>(
        (*this).SetTemplateChildCore( presenter.Value().Get()));

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    OnItemsSourceCoreChanged();
#pragma GCC diagnostic pop
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

Base::Ref<DataTemplate> ItemsControl::GetTemplateForItemOverride(
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
    const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((*this)).Types();
    Meta::TypeId type = item->RuntimeType();
    while (type != Meta::InvalidTypeId) {
        ResourceValue found =
            TryFindResource(ResourceKey::FromType(type));
        if (found.Kind() == Meta::ValueKind::Object &&
            !found.IsNullObject() &&
            found.AsObject()) {
            if (DataTemplate* dataTemplate = TryCast<DataTemplate>(
                    found.AsObject().Get())) {
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

void ItemsControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == ItemsSourceProperty.Handle()) {
        Base::Object* obj = args.GetNewValue().Kind() == Meta::ValueKind::Object
            ? args.GetNewValue().AsObject().Get()
            : nullptr;
        Collections::IItemsSource* source =
            TryCastToInterface<Collections::IItemsSource>(obj);
        if (source == nullptr && obj != nullptr) {
            source = Collections::CollectionAsItemsSource(obj);
        }
        SetItemsSourceCore(source);
    } else if (prop == DisplayMemberPathProperty.Handle()) {
        PublishReset();
    } else if (prop == ItemTemplateProperty.Handle()) {
        Base::Object* obj = args.GetNewValue().Kind() == Meta::ValueKind::Object
            ? args.GetNewValue().AsObject().Get()
            : nullptr;
        SetItemTemplateCore(TryCast<DataTemplate>(obj));
    } else if (prop == ItemTemplateSelectorProperty.Handle()) {
        Base::Object* obj = args.GetNewValue().Kind() == Meta::ValueKind::Object
            ? args.GetNewValue().AsObject().Get()
            : nullptr;
        SetItemTemplateSelectorCore(TryCast<DataTemplateSelector>(obj));
    } else if (prop == ItemsPanelProperty.Handle()) {
        Base::Object* obj = args.GetNewValue().Kind() == Meta::ValueKind::Object
            ? args.GetNewValue().AsObject().Get()
            : nullptr;
        SetItemsPanelCore(TryCast<ItemsPanelTemplate>(obj));
    } else if (prop == ItemContainerStyleProperty.Handle()) {
        Base::Object* obj = args.GetNewValue().Kind() == Meta::ValueKind::Object
            ? args.GetNewValue().AsObject().Get()
            : nullptr;
        SetItemContainerStyleCore(TryCast<Style>(obj));
    }
    Control::OnPropertyChanged(args);
}

Base::Result<Base::Ref<FrameworkElement>>
ItemsControl::GetContainerForItemOverride() const noexcept {
    // WPF/Noesis GetContainerForItemOverride returns ContentPresenter so
    // ItemContainerStyle TargetType="ContentPresenter" can apply. A generated
    // ContentControl rejects that style and aborts item UI activation.
    Base::Result<Base::Ref<ContentPresenter>> made =
        Base::MakeRef<ContentPresenter>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

Base::Result<void> ItemsControl::PrepareContainerForItemOverride(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    if (item && item.Get() != &container) {
        container.SetDataContext(
            Value::FromObject(
                item->RuntimeType(), item));
    }
    if (!item ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(), ItemsControl::StaticTypeId())) {
        return {};
    }

    const Base::Ref<DataTemplate> resolved =
        GetTemplateForItemOverride(item, index);
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
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            hierarchicalTemplate->RuntimeType(),
            DataTemplate::StaticTypeId())) {
        childItemTemplate = Base::Ref<DataTemplate>::FromBorrowed(
            static_cast<DataTemplate&>(*hierarchicalTemplate));
    }
    if (!hierarchicalSource) {
        childItems.SetItemTemplate(std::move(childItemTemplate));
        return {};
    }
    if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            hierarchicalSource->RuntimeType(),
            Data::Binding::StaticTypeId())) {
        if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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

    auto* bindings = ElementTree::BindingsOf(childItems);
    if (bindings == nullptr || bindings->Metadata() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "HierarchicalDataTemplate Binding services are unavailable");
    }
    const auto& binding =
        static_cast<const Data::Binding&>(*hierarchicalSource);
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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

void ItemsControl::ClearContainerForItemOverride(
    FrameworkElement& container) noexcept {
    Base::Ref<Base::Object> item;
    const Value dataContext = container.GetDataContext();
    if (dataContext.Kind() == Meta::ValueKind::Object &&
        !dataContext.IsNullObject()) {
        item = dataContext.AsObject();
    }
    const Base::Ref<DataTemplate> resolved =
        GetTemplateForItemOverride(item, 0U);
    if (TryCast<HierarchicalDataTemplate>(resolved.Get()) != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(), ItemsControl::StaticTypeId())) {
        auto& childItems = static_cast<ItemsControl&>(container);
        if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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

// Legacy WPF-compat shims: forward to the new Override entry points so
// existing overrides keep working during migration.
Base::Ref<DataTemplate> ItemsControl::ResolveItemTemplate(
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) const noexcept {
    return GetTemplateForItemOverride(item, index);
}

Base::Result<Base::Ref<FrameworkElement>> ItemsControl::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    return GetContainerForItemOverride();
}

Base::Result<void> ItemsControl::PrepareContainer(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    return PrepareContainerForItemOverride(container, item, index);
}

void ItemsControl::ClearContainer(FrameworkElement& container) noexcept {
    ClearContainerForItemOverride(container);
}

void ItemsControl::OnItemsSourceCoreChanged() noexcept {
    OnItemsChanged({});
}

namespace {

void AddItemsControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return;
    }
    static_cast<ItemsControl&>(owner).GetItems().Add(item);
}

void ClearItemsControlItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ItemsControl&>(owner).GetItems().Reset();
}

} // namespace

AERO_DESCRIBE(ItemsControl) {
    using namespace Aero::Meta;
    Register<Collections::IItemsSource>(context, TypeFlags::Abstract);

    Register<Collections::ObservableCollectionBase>(context, TypeFlags::Abstract)
        .Implements<Collections::IItemsSource>();

    Register<Collections::ObservableObjectCollection>(context)
        .Factory()
        .Implements<Collections::IItemsSource>();

    Register<Data::CollectionView>(context)
        .Implements<Collections::IItemsSource>();

    Register<Data::CollectionViewSource>(context)
        .Factory();

    Register<AlternationConverter>(context)
        .Content<Base::Object>("Values", ContentKind::Collection,
            [](Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
                static_cast<AlternationConverter&>(owner).AddValue(value);
            },
            [](Base::Object& owner, void*) noexcept {
                static_cast<AlternationConverter&>(owner).ClearValues();
            })
        .Factory();

    Register<::Aero::Controls::BoxedItemValue>(context);

    Register<ItemsControl>(context)
        .Property(ItemsControl::ItemCountProperty, std::uint32_t{0})
        .Property(ItemsControl::HasItemsProperty, false)
        .Property(ItemsControl::ItemsSourceProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .Property(ItemsControl::AlternationCountProperty, std::uint32_t{0}, AffectsMeasure)
        .Property(ItemsControl::DisplayMemberPathProperty, Base::String{}, AffectsMeasure)
        .Property(ItemsControl::ItemTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Property(ItemsControl::ItemTemplateSelectorProperty, Base::Ref<DataTemplateSelector>{}, AffectsMeasure)
        .Property(ItemsControl::ItemsPanelProperty, Base::Ref<ItemsPanelTemplate>{}, AffectsMeasure)
        .Property(ItemsControl::ItemContainerStyleProperty, Base::Ref<Style>{}, AffectsMeasure)
        .Content<Base::Object>("Items", ContentKind::Collection, &AddItemsControlItem, &ClearItemsControlItems)
        .Factory();
}

AERO_DESCRIBE(HeaderedItemsControl) {
    using namespace Aero::Meta;
    Register<HeaderedItemsControl>(context, TypeFlags::Abstract)
        .Property(HeaderedItemsControl::HeaderProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure)
        .Property(HeaderedItemsControl::HeaderTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure);
}


void ItemsControl::AssignItemsSource(
    Base::Ref<Base::Object> source) noexcept {
    Collections::IItemsSource* directSource =
        TryCastToInterface<Collections::IItemsSource>(source.Get());
    if (directSource == nullptr) {
        directSource = Collections::CollectionAsItemsSource(source.Get());
    }
    SetItemsSourceCore(directSource);
}

} // namespace Aero::Controls
