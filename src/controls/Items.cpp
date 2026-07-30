#include <Aero/Controls/Items.hpp>

#include "presentation/RenderingInternal.hpp"

#include "../presentation/ResourceAssignment.hpp"
#include <Aero/Controls/Virtualization.hpp>

#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <algorithm>
#include <utility>

namespace Aero::Controls {

ContentControl::ContentControl(
    TypeId runtimeType) noexcept
    : Control(runtimeType),
      foregroundChangedHandler_(
          this,
          &ContentControl::OnForegroundChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
}

ContentControl::~ContentControl() {
    static_cast<void>(RemoveValueChangedHandler(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
}

void ContentControl::OnForegroundChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    if (!literalTextContent_ ||
        content_ == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(),
            TextBlock::StaticTypeId())) {
        return;
    }
    static_cast<void>(
        static_cast<TextBlock*>(content_)->
            SetForeground(Foreground()));
}

Base::Result<void> ContentControl::SetGeneratedTextContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!PropertyRegistry().Types().IsDerivedFrom(
            content.RuntimeType(),
            TextBlock::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Generated text content must be a TextBlock");
    }
    Base::Result<void> assigned =
        SetOwnedContent(contentObject, content);
    if (!assigned) return assigned.GetStatus();
    literalTextContent_ = true;
    return static_cast<TextBlock&>(content).
        SetForeground(Foreground());
}

Base::Ref<Base::Object> ItemsCollection::ItemAt(
    std::uint32_t index) const noexcept {
    return index < items_.Size()
        ? items_[index]
        : Base::Ref<Base::Object>();
}

void ItemsCollection::Notify(
    const ItemsChangedEvent& event) noexcept {
    if (!changed_.Empty()) changed_.Invoke(event);
}

Base::Result<void> ItemsCollection::Add(
    Base::Ref<Base::Object> item) noexcept {
    return Insert(items_.Size(), std::move(item));
}

Base::Result<void> ItemsCollection::Insert(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index > items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemsCollection insert is invalid");
    }
    Base::Result<void> reserved =
        items_.TryReserve(items_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<void> added =
        items_.TryPushBack(std::move(item));
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
ItemsCollection::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemsCollection remove index is out of range");
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

Base::Result<void> ItemsCollection::Replace(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemsCollection replacement is invalid");
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

Base::Result<void> ItemsCollection::Move(
    std::uint32_t oldIndex,
    std::uint32_t newIndex) noexcept {
    if (oldIndex >= items_.Size() ||
        newIndex >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemsCollection move index is out of range");
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

void ItemsCollection::Reset() noexcept {
    const std::uint32_t oldCount = items_.Size();
    items_.Clear();
    Notify({
        ItemsChangeAction::Reset,
        0U,
        0U,
        oldCount,
        0U});
}

Base::Result<void> ItemsCollection::Reset(
    Base::Span<const Base::Ref<Base::Object>>
        items) noexcept {
    Base::Vector<Base::Ref<Base::Object>> replacement;
    Base::Result<void> reserved =
        replacement.TryReserve(items.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::Ref<Base::Object>& item : items) {
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ItemsCollection cannot contain null");
        }
        Base::Result<void> added =
            replacement.TryPushBack(item);
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

Base::Result<void> DeferredObjectProgram::Configure(
    DeferredObjectFactory factory,
    void* context) noexcept {
    return Configure(factory, context, {});
}

Base::Result<void> DeferredObjectProgram::Configure(
    DeferredObjectFactory factory,
    void* context,
    Base::Ref<Base::Object> factoryOwner) noexcept {
    if (sealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred object program is sealed");
    }
    if (factory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Deferred object factory is null");
    }
    factory_ = factory;
    context_ = context;
    factoryOwner_ = std::move(factoryOwner);
    return {};
}

Base::Result<void> DeferredObjectProgram::SetBaseUri(
    const Base::ResourceUri& value) noexcept {
    if (sealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred object program is sealed");
    }
    baseUri_ = value;
    return {};
}

Base::Result<void> DeferredObjectProgram::Seal() noexcept {
    if (factory_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred object program has no factory");
    }
    sealed_ = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>>
DeferredObjectProgram::Instantiate(
    const Base::Ref<Base::Object>& payload) const noexcept {
    if (factory_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred object program is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result =
        factory_(payload, context_);
    if (!result || result.Value()) return result;
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Deferred object factory returned null");
}

Base::Result<Base::Ref<Base::Object>>
DataTemplate::Instantiate(
    const Base::Ref<Base::Object>& item) const noexcept {
    if (!program_.IsValid() || !item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result =
        program_.Instantiate(item);
    if (!result || result.Value()) return result;
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "DataTemplate returned null");
}

Base::Result<void> DataTemplate::Configure(
    DataTemplateFactory factory,
    void* context,
    Base::Ref<Base::Object> factoryOwner) noexcept {
    return program_.Configure(
        factory, context, std::move(factoryOwner));
}

Base::Result<void> DataTemplate::SetDataType(
    TypeId value) noexcept {
    if (program_.IsSealed() ||
        value == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DataTemplate DataType is invalid");
    }
    dataType_ = value;
    return {};
}

Base::Result<void> DataTemplate::SetAuthoredVisualTree(
    const Base::Ref<Base::Object>& value) noexcept {
    if (program_.IsSealed() || !value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate VisualTree assignment is invalid");
    }
    authoredVisualTree_ = value;
    return {};
}

Base::Result<void> DataTemplate::Seal() noexcept {
    Base::Result<void> program = program_.Seal();
    if (!program) return program.GetStatus();
    return resources_.Seal();
}

Base::Result<Base::Ref<Base::Object>>
ItemsPanelTemplate::Instantiate() const noexcept {
    if (!program_.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsPanelTemplate is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result =
        program_.Instantiate();
    if (!result || result.Value()) return result;
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "ItemsPanelTemplate returned null");
}

Base::Result<void> ItemsPanelTemplate::Configure(
    DeferredObjectFactory factory,
    void* context,
    Base::Ref<Base::Object> factoryOwner) noexcept {
    return program_.Configure(
        factory, context, std::move(factoryOwner));
}

Base::Result<void> ItemsPanelTemplate::SetAuthoredVisualTree(
    const Base::Ref<Base::Object>& value) noexcept {
    if (program_.IsSealed() || !value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsPanelTemplate VisualTree assignment is invalid");
    }
    authoredVisualTree_ = value;
    return {};
}

Base::Result<void> ItemsPanelTemplate::Seal() noexcept {
    Base::Result<void> program = program_.Seal();
    if (!program) return program.GetStatus();
    return resources_.Seal();
}

Base::Result<void> ContentControl::StoreContentProperty(
    Core::Value value) noexcept {
    if (synchronizingContentProperty_) return {};
    synchronizingContentProperty_ = true;
    Base::Result<void> stored =
        SetValue(ContentProperty, std::move(value));
    synchronizingContentProperty_ = false;
    return stored;
}

void ContentControl::OnContentPropertyChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& control = static_cast<ContentControl&>(object);
    if (control.synchronizingContentProperty_) return;
    control.synchronizingContentProperty_ = true;
    static_cast<void>(
        control.SetContentValue(change.newValue));
    control.synchronizingContentProperty_ = false;
}

Base::Result<void> ContentControl::SetContentValue(
    Base::Ref<Base::Object> value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (value &&
        PropertyRegistry().Types().IsDerivedFrom(
            value->RuntimeType(),
            UIElement::StaticTypeId())) {
        authoredContent_ = Core::Value::FromObject(
            value->RuntimeType(), value);
        return SetOwnedContent(
            value,
            *static_cast<UIElement*>(value.Get()));
    }
    literalTextContent_ = false;
    if (content_ != nullptr) {
        Base::Result<void> cleared = SetContent(nullptr);
        if (!cleared) return cleared.GetStatus();
    }
    contentValue_ = std::move(value);
    ownedContent_.Reset();
    authoredContent_ = contentValue_
        ? Core::Value::FromObject(
            contentValue_->RuntimeType(),
            contentValue_)
        : Core::Value::NullObject(
            Core::TypeOf<Base::Object>());
    return InvalidateMeasure();
}

Base::Result<void> ContentControl::SetContentValue(
    Core::Value value) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentControl Content cannot be unset");
    }
    if (value.Kind() == Core::ValueKind::Object) {
        authoredContent_ = value;
        return SetContentValue(value.AsObject());
    }
    if (value.Kind() != Core::ValueKind::String) {
        Base::Result<void> stored =
            StoreContentProperty(value);
        if (!stored) return stored.GetStatus();
        authoredContent_ = std::move(value);
        contentValue_.Reset();
        ownedContent_.Reset();
        return InvalidateMeasure();
    }

    Base::Result<Base::Ref<TextBlock>> created =
        Base::MakeRef<TextBlock>();
    if (!created) return created.GetStatus();
    Base::Result<void> text =
        created.Value()->SetText(value.AsString());
    if (!text) return text.GetStatus();
    text = created.Value()->SetForeground(Foreground());
    if (!text) return text.GetStatus();
    Base::Ref<Base::Object> retained(created.Value());
    Base::Result<void> assigned = SetOwnedContent(
        retained, *created.Value());
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> stored =
        StoreContentProperty(value);
    if (!stored) return stored.GetStatus();
    authoredContent_ = std::move(value);
    contentValue_.Reset();
    literalTextContent_ = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>>
ContentControl::TryCreateTemplatedContent() const noexcept {
    if (content_ != nullptr) {
        return ownedContent_;
    }
    if (!contentValue_) {
        return Base::Ref<Base::Object>{};
    }
    Base::Ref<Base::Object> contentTemplate =
        ContentTemplate();
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
        static_cast<DataTemplate*>(
            contentTemplate.Get())->Instantiate(
                contentValue_);
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
        items_.TryAddItemsChanged(localHandler_));
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

Base::Result<void> ItemsControl::OnApplyTemplate() noexcept {
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
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ItemsControl template is missing required part 'ItemsHost'");
    }
    if (PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(),
            ItemsPresenter::StaticTypeId())) {
        itemsHost_ =
            static_cast<ItemsPresenter*>(part)->
                ItemsHost();
    } else if (PropertyRegistry().Types().IsDerivedFrom(
                   part->RuntimeType(),
                   Panel::StaticTypeId())) {
        itemsHost_ = static_cast<Panel*>(part);
    }
    if (itemsHost_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsControl ItemsHost part must be a Panel or populated ItemsPresenter");
    }
    return {};
}

void ItemsControl::OnTemplateDetached() noexcept {
    if (generator_ != nullptr) {
        static_cast<void>(generator_->Detach());
    }
    itemsHost_ = nullptr;
}

std::uint32_t ItemsControl::ItemCount() const noexcept {
    return source_ != nullptr
        ? source_->Count()
        : items_.Count();
}

std::uint32_t ItemsControl::RealizedItemCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GeneratedCount()
        : 0U;
}

std::uint32_t ItemsControl::CreatedContainerCount() const noexcept {
    return generator_ != nullptr
        ? generator_->CreatedContainerCount()
        : 0U;
}

std::uint32_t
ItemsControl::RecycledContainerUseCount() const noexcept {
    return generator_ != nullptr
        ? generator_->RecycledContainerUseCount()
        : 0U;
}

Base::Ref<Base::Object> ItemsControl::ItemAt(
    std::uint32_t index) const noexcept {
    return source_ != nullptr
        ? source_->ItemAt(index)
        : items_.ItemAt(index);
}

Base::Result<void> ItemsControl::SetItemsSource(
    IItemsSource* source) noexcept {
    if (source_ == source) return {};
    if (source != nullptr) {
        Base::Result<void> subscribed =
            source->TryAddItemsChanged(
                sourceHandler_);
        if (!subscribed) {
            return subscribed.GetStatus();
        }
    }
    if (source_ != nullptr) {
        static_cast<void>(
            source_->RemoveItemsChanged(
                sourceHandler_));
    }
    source_ = source;
    PublishItemCount();
    PublishReset();
    return {};
}

void ItemsControl::SetItemTemplate(
    const DataTemplate* value) noexcept {
    if (itemTemplate_ == value) return;
    itemTemplate_ = value;
    PublishReset();
}

void ItemsControl::SetItemsPanel(
    const ItemsPanelTemplate* value) noexcept {
    if (itemsPanel_ == value) return;
    itemsPanel_ = value;
    PublishReset();
}

void ItemsControl::SetItemContainerStyle(
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
            ItemCount(),
            ItemCount()});
    }
}

void ItemsControl::PublishItemCount() noexcept {
    const std::uint32_t count = ItemCount();
    static_cast<void>(SetReadOnlyCurrentValue(
        ItemCountProperty, count));
    static_cast<void>(SetReadOnlyCurrentValue(
        HasItemsProperty, count != 0U));
}

Base::Result<Base::Ref<ItemContainer>>
ItemsControl::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    return Base::MakeRef<ItemContainer>();
}

Base::Result<void> ItemsControl::PrepareContainer(
    ItemContainer&,
    const Base::Ref<Base::Object>&,
    std::uint32_t) noexcept {
    return {};
}

void ItemsControl::ClearContainer(
    ItemContainer&) noexcept {}

ItemContainerGenerator::ItemContainerGenerator(
    ObjectTree& tree,
    LayoutManager& layout,
    EffectiveValueEngine& values,
    StyleManager* styles,
    RenderManager* renderer,
    TemplateManager* templates,
    ItemSubtreeCallback subtreeCallback,
    void* subtreeContext) noexcept
    : tree_(&tree),
      layout_(&layout),
      values_(&values),
      styles_(styles),
      renderer_(renderer),
      templates_(templates),
      subtreeCallback_(subtreeCallback),
      subtreeContext_(subtreeContext),
      mounts_(tree, &layout, renderer),
      changedHandler_(
          this,
          &ItemContainerGenerator::OnItemsChanged) {}

ItemContainerGenerator::~ItemContainerGenerator() noexcept {
    static_cast<void>(Detach());
}

Base::Result<void> ItemContainerGenerator::Attach(
    ItemsControl& owner,
    Panel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        owner.OwningTree() != tree_ ||
        itemsHost.OwningTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemContainerGenerator attach state is invalid");
    }
    Base::Result<void> subscribed =
        owner.TryAddItemsChanged(changedHandler_);
    if (!subscribed) return subscribed.GetStatus();
    owner_ = &owner;
    host_ = &itemsHost;
    virtualizingHost_ = nullptr;
    firstGeneratedIndex_ = 0U;
    owner.generator_ = this;
    Base::Result<void> refreshed = Refresh();
    if (!refreshed) {
        static_cast<void>(
            owner.RemoveItemsChanged(
                changedHandler_));
        for (std::uint32_t index = records_.Size();
            index > 0U; --index) {
            static_cast<void>(
                DetachRecord(records_[index - 1U]));
        }
        records_.Clear();
        owner_ = nullptr;
        host_ = nullptr;
        owner.generator_ = nullptr;
        return refreshed.GetStatus();
    }
    return {};
}

Base::Result<void>
ItemContainerGenerator::AttachVirtualized(
    ItemsControl& owner,
    VirtualizingStackPanel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        owner.OwningTree() != tree_ ||
        itemsHost.OwningTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Virtualized item generator attach state is invalid");
    }
    Base::Result<void> subscribed =
        owner.TryAddItemsChanged(changedHandler_);
    if (!subscribed) return subscribed.GetStatus();
    owner_ = &owner;
    host_ = &itemsHost;
    virtualizingHost_ = &itemsHost;
    firstGeneratedIndex_ = 0U;
    owner.generator_ = this;
    Base::Result<void> attached =
        itemsHost.AttachGenerator(*this, owner.ItemCount());
    if (!attached) {
        static_cast<void>(
            owner.RemoveItemsChanged(changedHandler_));
        owner.generator_ = nullptr;
        owner_ = nullptr;
        host_ = nullptr;
        virtualizingHost_ = nullptr;
        return attached.GetStatus();
    }
    Base::Result<bool> realized =
        SetRealizationRangeInternal(
            itemsHost.desiredFirstIndex_,
            itemsHost.desiredCount_,
            true);
    if (!realized) {
        itemsHost.DetachGenerator(*this);
        static_cast<void>(
            owner.RemoveItemsChanged(changedHandler_));
        owner.generator_ = nullptr;
        owner_ = nullptr;
        host_ = nullptr;
        virtualizingHost_ = nullptr;
        return realized.GetStatus();
    }
    owner.OnContainersChanged();
    return {};
}

Base::Result<bool> ItemContainerGenerator::Detach() noexcept {
    if (owner_ == nullptr) return false;
    static_cast<void>(
        owner_->RemoveItemsChanged(
            changedHandler_));
    Base::Status firstError;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U]);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    records_.Clear();
    Base::Result<void> released =
        ReleaseRecycledContainers();
    if (!released && firstError.IsOk()) {
        firstError = released.GetStatus();
    }
    if (virtualizingHost_ != nullptr) {
        virtualizingHost_->DetachGenerator(*this);
    }
    owner_->generator_ = nullptr;
    owner_ = nullptr;
    host_ = nullptr;
    virtualizingHost_ = nullptr;
    firstGeneratedIndex_ = 0U;
    return firstError.IsOk()
        ? Base::Result<bool>(true)
        : Base::Result<bool>(firstError);
}

Base::Result<ItemContainerGenerator::Record>
ItemContainerGenerator::CreateRecord(
    std::uint32_t index) noexcept {
    if (owner_ == nullptr ||
        index >= owner_->ItemCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Item generation index is out of range");
    }
    Record record;
    record.item = owner_->ItemAt(index);
    if (!record.item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsSource returned null");
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.item->RuntimeType(),
            ItemContainer::StaticTypeId())) {
        record.container =
            Base::Ref<ItemContainer>::FromBorrowed(
                *static_cast<ItemContainer*>(
                    record.item.Get()));
        record.itemIsOwnContainer = true;
        return record;
    }
    const DataTemplate* itemTemplate =
        owner_->ItemTemplate();
    if (itemTemplate != nullptr) {
        Base::Result<Base::Ref<Base::Object>>
            content =
                itemTemplate->Instantiate(record.item);
        if (!content) return content.GetStatus();
        record.content =
            std::move(content).Value();
    } else if (
        record.item->RuntimeType() ==
            BoxedItemValue::StaticTypeId()) {
        const Core::Value& value =
            static_cast<const BoxedItemValue&>(
                *record.item).Value();
        if (value.Kind() !=
                Core::ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Boxed data item has no default text presentation");
        }
        Base::Result<Base::Ref<TextBlock>> text =
            Base::MakeRef<TextBlock>();
        if (!text) return text.GetStatus();
        Base::Result<void> assigned =
            text.Value()->SetText(value.AsString());
        if (!assigned) return assigned.GetStatus();
        record.content =
            Base::Ref<Base::Object>(
                std::move(text).Value());
        record.generatedTextContent = true;
    } else if (owner_->PropertyRegistry().Types()
        .IsDerivedFrom(
            record.item->RuntimeType(),
            UIElement::StaticTypeId())) {
        record.content = record.item;
    } else {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Data items require an ItemTemplate");
    }
    if (!record.content ||
        !owner_->PropertyRegistry().Types()
            .IsDerivedFrom(
                record.content->RuntimeType(),
                UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemTemplate must create a UIElement");
    }
    if (!recycledContainers_.Empty()) {
        record.container =
            std::move(recycledContainers_.Back());
        recycledContainers_.PopBack();
        ++recycledContainerUseCount_;
    } else {
        Base::Result<Base::Ref<ItemContainer>> made =
            owner_->CreateContainer(record.item);
        if (!made || !made.Value()) {
            if (!made) return made.GetStatus();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "ItemsControl returned null container");
        }
        record.container =
            std::move(made).Value();
        ++createdContainerCount_;
    }
    return record;
}

Base::Result<void>
ItemContainerGenerator::AttachOwnedSubtree(
    Record& record,
    Presentation::Visual& root) noexcept {
    Base::Vector<Presentation::Visual*> pending;
    Base::Result<void> pushed =
        pending.TryPushBack(&root);
    if (!pushed) return pushed.GetStatus();

    const auto attachChild =
        [this, &record, &pending](
            Presentation::Visual& parent,
            const Base::Ref<Base::Object>& owned)
            noexcept -> Base::Result<void> {
        if (!owned ||
            !owner_->PropertyRegistry().Types().
                IsDerivedFrom(
                    owned->RuntimeType(),
                    UIElement::StaticTypeId())) {
            return {};
        }
        auto& child =
            *static_cast<Presentation::Visual*>(
                owned.Get());
        if (child.VisualParent() == &parent &&
            child.OwningTree() == tree_) {
            return pending.TryPushBack(&child);
        }
        if (child.VisualParent() != nullptr ||
            child.LogicalParent() != nullptr ||
            child.OwningTree() != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Owned item-template child is already mounted elsewhere");
        }
        Base::Result<MountEdgeState> mounted =
            mounts_.Attach(parent, child);
        if (!mounted) return mounted.GetStatus();
        MountEdgeState edge =
            std::move(mounted).Value();
        Base::Result<void> tracked =
            record.subtreeMounts.TryPushBack(
                std::move(edge));
        if (!tracked) {
            (void)mounts_.Detach(edge);
            return tracked.GetStatus();
        }
        Base::Result<void> queued =
            pending.TryPushBack(&child);
        if (!queued) {
            MountEdgeState rollback =
                std::move(record.subtreeMounts.Back());
            record.subtreeMounts.PopBack();
            (void)mounts_.Detach(rollback);
            return queued.GetStatus();
        }
        return {};
    };

    while (!pending.Empty()) {
        Presentation::Visual* current =
            pending.Back();
        pending.PopBack();
        if (current == nullptr) continue;
        const Core::TypeId type =
            current->RuntimeType();
        if (owner_->PropertyRegistry().Types().
                IsDerivedFrom(
                    type, Panel::StaticTypeId())) {
            auto& panel =
                *static_cast<Panel*>(current);
            for (std::uint32_t index = 0U;
                 index < panel.OwnedChildCount();
                 ++index) {
                Base::Result<void> attached =
                    attachChild(
                        panel,
                        panel.OwnedChildAt(index));
                if (!attached) {
                    (void)DetachOwnedSubtree(record);
                    return attached.GetStatus();
                }
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           Decorator::StaticTypeId())) {
            auto& decorator =
                *static_cast<Decorator*>(current);
            Base::Result<void> attached =
                attachChild(
                    decorator,
                    decorator.OwnedChild());
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           ContentControl::StaticTypeId())) {
            auto& content =
                *static_cast<ContentControl*>(current);
            Base::Result<void> attached =
                attachChild(
                    content,
                    content.OwnedContent());
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           ContentPresenter::StaticTypeId())) {
            auto& presenter =
                *static_cast<ContentPresenter*>(current);
            Base::Result<void> attached =
                attachChild(
                    presenter,
                    presenter.OwnedContent());
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void>
ItemContainerGenerator::DetachOwnedSubtree(
    Record& record) noexcept {
    Base::Status firstError;
    for (std::uint32_t index =
             record.subtreeMounts.Size();
         index > 0U; --index) {
        Base::Result<void> detached =
            mounts_.Detach(
                record.subtreeMounts[index - 1U]);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    record.subtreeMounts.Clear();
    return firstError.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGenerator::AttachRecord(
    Record& record,
    std::uint32_t index) noexcept {
    ItemContainer& container = *record.container;

    Base::Result<MountEdgeState> containerMounted =
        mounts_.Attach(*owner_, *host_, container);
    if (!containerMounted) return containerMounted.GetStatus();
    record.containerMount = std::move(containerMounted).Value();

    if (!record.itemIsOwnContainer) {
        auto& content =
            *static_cast<UIElement*>(
                record.content.Get());
        Base::Result<MountEdgeState> contentMounted =
            mounts_.Attach(container, content);
        if (!contentMounted) {
            (void)mounts_.Detach(record.containerMount);
            return contentMounted.GetStatus();
        }
        record.contentMount =
            std::move(contentMounted).Value();

        Base::Result<void> selected =
            record.generatedTextContent
            ? container.SetGeneratedTextContent(
                  record.content, content)
            : container.SetOwnedContent(
                  record.content, content);
        if (!selected) {
            (void)mounts_.Detach(
                record.contentMount);
            (void)mounts_.Detach(
                record.containerMount);
            return selected.GetStatus();
        }
    }
    // Presentation activation starts at the generated container. This is
    // required for implicit container styles and control templates (for
    // example ComboBoxItem), while traversal still reaches DataTemplate
    // content mounted beneath it.
    Presentation::Visual& subtree =
        static_cast<Presentation::Visual&>(container);
    Base::Result<void> subtreeAttached =
        AttachOwnedSubtree(record, subtree);
    if (!subtreeAttached) {
        (void)DetachRecord(record);
        return subtreeAttached.GetStatus();
    }
    const Style* style = owner_->ItemContainerStyle();
    if (style != nullptr && styles_ != nullptr) {
        Base::Result<void> styled = styles_->Apply(container, *style);
        if (!styled) { (void)DetachRecord(record); return styled.GetStatus(); }
        record.appliedStyle = style;
    }
    Base::Result<void> prepared = owner_->PrepareContainer(container, record.item, index);
    if (!prepared) { (void)DetachRecord(record); return prepared.GetStatus(); }
    if (subtreeCallback_ != nullptr) {
        Base::Result<void> presented =
            subtreeCallback_(
                subtree,
                ItemSubtreeChange::Mounted,
                subtreeContext_);
        if (!presented) {
            (void)DetachRecord(record);
            return presented.GetStatus();
        }
        record.subtreeMounted = true;
    }
    return {};
}

Base::Result<void>
ItemContainerGenerator::DetachRecord(
    Record& record,
    bool recycleContainer) noexcept {
    if (!record.container) return {};
    ItemContainer& container = *record.container;
    Base::Status firstError;
    const auto capture = [&firstError](const Base::Result<void>& result) noexcept {
        if (!result && firstError.IsOk()) firstError = result.GetStatus();
    };
    if (record.subtreeMounted &&
        subtreeCallback_ != nullptr) {
        Presentation::Visual& subtree =
            static_cast<Presentation::Visual&>(
                container);
        capture(subtreeCallback_(
            subtree,
            ItemSubtreeChange::Unmounting,
            subtreeContext_));
        record.subtreeMounted = false;
    }
    capture(DetachOwnedSubtree(record));
    owner_->ClearContainer(container);
    if (record.appliedStyle != nullptr && styles_ != nullptr) {
        capture(styles_->Clear(container, *record.appliedStyle));
        record.appliedStyle = nullptr;
    }
    if (!record.itemIsOwnContainer &&
        templates_ != nullptr &&
        container.IsTemplateApplied()) {
        Base::Result<bool> cleared =
            templates_->Clear(container);
        if (!cleared) {
            capture(Base::Result<void>(
                cleared.GetStatus()));
        }
    }
    UIElement* content = record.itemIsOwnContainer
        ? nullptr
        : container.Content();
    if (content != nullptr) {
        capture(mounts_.Detach(record.contentMount));
        capture(container.SetContent(nullptr));
        capture(values_->DetachObject(*content));
    }
    capture(mounts_.Detach(record.containerMount));
    if (!record.itemIsOwnContainer &&
        recycleContainer && firstError.IsOk()) {
        Base::Result<void> recycled = recycledContainers_.TryPushBack(std::move(record.container));
        if (!recycled) {
            capture(values_->DetachObject(container));
            if (firstError.IsOk()) firstError = recycled.GetStatus();
        }
    } else {
        capture(values_->DetachObject(container));
    }
    record.item.Reset();
    record.content.Reset();
    record.containerMount = {};
    record.contentMount = {};
    record.itemIsOwnContainer = false;
    record.generatedTextContent = false;
    record.subtreeMounted = false;
    return firstError.IsOk() ? Base::Result<void>() : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGenerator::ReleaseRecycledContainers() noexcept {
    Base::Status firstError;
    for (Base::Ref<ItemContainer>& container :
        recycledContainers_) {
        if (!container) continue;
        Base::Result<void> detached =
            values_->DetachObject(*container);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    recycledContainers_.Clear();
    return firstError.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGenerator::InsertRecord(
    std::uint32_t index,
    Record record) noexcept {
    if (index > records_.Size()) {
        static_cast<void>(DetachRecord(record));
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Generated container insert is out of range");
    }
    Base::Result<void> reserved =
        records_.TryReserve(records_.Size() + 1U);
    if (!reserved) {
        const Base::Status error = reserved.GetStatus();
        static_cast<void>(DetachRecord(record));
        return error;
    }
    Base::Result<void> appended =
        records_.TryPushBack(std::move(record));
    if (!appended) {
        const Base::Status error = appended.GetStatus();
        static_cast<void>(DetachRecord(record));
        return error;
    }
    if (index + 1U == records_.Size()) return {};
    Record moving = std::move(records_.Back());
    for (std::uint32_t current =
            records_.Size() - 1U;
        current > index; --current) {
        records_[current] =
            std::move(records_[current - 1U]);
    }
    records_[index] = std::move(moving);
    return {};
}

void ItemContainerGenerator::RemoveRecordAt(
    std::uint32_t index) noexcept {
    for (std::uint32_t current = index;
        current + 1U < records_.Size(); ++current) {
        records_[current] =
            std::move(records_[current + 1U]);
    }
    records_.PopBack();
}

Base::Result<void>
ItemContainerGenerator::ReorderVisuals() noexcept {
    for (Record& record : records_) {
        Base::Result<void> detached = mounts_.DetachPresentation(record.containerMount);
        if (!detached) return detached.GetStatus();
    }
    for (Record& record : records_) {
        Base::Result<void> attached = mounts_.AttachPresentation(record.containerMount, *host_);
        if (!attached) return attached.GetStatus();
    }
    return {};
}

Base::Result<bool>
ItemContainerGenerator::SetRealizationRangeInternal(
    std::uint32_t firstIndex,
    std::uint32_t count,
    bool force) noexcept {
    if (owner_ == nullptr ||
        host_ == nullptr ||
        virtualizingHost_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Item realization range requires a virtualizing host");
    }
    const std::uint32_t itemCount =
        owner_->ItemCount();
    firstIndex = std::min(firstIndex, itemCount);
    count = std::min(count, itemCount - firstIndex);
    if (!force &&
        firstGeneratedIndex_ == firstIndex &&
        records_.Size() == count) {
        return false;
    }

    Base::Status firstError;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U], true);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    records_.Clear();
    firstGeneratedIndex_ = firstIndex;
    if (!firstError.IsOk()) {
        return firstError;
    }

    Base::Result<void> reserved =
        records_.TryReserve(count);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t offset = 0U;
        offset < count; ++offset) {
        const std::uint32_t index =
            firstIndex + offset;
        Base::Result<Record> made =
            CreateRecord(index);
        if (!made) {
            firstError = made.GetStatus();
            break;
        }
        Record record = std::move(made).Value();
        Base::Result<void> attached =
            AttachRecord(record, index);
        if (!attached) {
            firstError = attached.GetStatus();
            break;
        }
        Base::Result<void> added =
            records_.TryPushBack(std::move(record));
        if (!added) {
            firstError = added.GetStatus();
            static_cast<void>(
                DetachRecord(record, true));
            break;
        }
    }
    if (!firstError.IsOk()) {
        for (std::uint32_t index = records_.Size();
            index > 0U; --index) {
            static_cast<void>(
                DetachRecord(
                    records_[index - 1U], true));
        }
        records_.Clear();
        return firstError;
    }
    return true;
}

Base::Result<bool>
ItemContainerGenerator::SetRealizationRange(
    std::uint32_t firstIndex,
    std::uint32_t count) noexcept {
    Base::Result<bool> changed =
        SetRealizationRangeInternal(
            firstIndex, count, false);
    lastError_ = changed
        ? Base::Status{}
        : changed.GetStatus();
    if (changed && changed.Value() &&
        owner_ != nullptr) {
        owner_->OnContainersChanged();
    }
    return changed;
}

Base::Result<void> ItemContainerGenerator::Refresh() noexcept {
    if (owner_ == nullptr || host_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemContainerGenerator is not attached");
    }
    if (virtualizingHost_ != nullptr) {
        Base::Result<bool> realized =
            SetRealizationRangeInternal(
                virtualizingHost_->desiredFirstIndex_,
                virtualizingHost_->desiredCount_,
                true);
        return realized
            ? Base::Result<void>()
            : Base::Result<void>(
                realized.GetStatus());
    }
    firstGeneratedIndex_ = 0U;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U]);
        if (!detached) return detached.GetStatus();
    }
    records_.Clear();
    Base::Result<void> reserved =
        records_.TryReserve(owner_->ItemCount());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
        index < owner_->ItemCount(); ++index) {
        Base::Result<Record> made =
            CreateRecord(index);
        if (!made) {
            const Base::Status error = made.GetStatus();
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
        Record record = std::move(made).Value();
        Base::Result<void> attached =
            AttachRecord(record, index);
        if (!attached) {
            const Base::Status error = attached.GetStatus();
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
        Base::Result<void> added =
            records_.TryPushBack(
                std::move(record));
        if (!added) {
            const Base::Status error = added.GetStatus();
            static_cast<void>(DetachRecord(record));
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
    }
    return {};
}

Base::Result<void> ItemContainerGenerator::ApplyChange(
    const ItemsChangedEvent& event) noexcept {
    if (event.action == ItemsChangeAction::Reset) {
        return Refresh();
    }
    if (event.action == ItemsChangeAction::Add) {
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            const std::uint32_t index =
                event.newIndex + offset;
            Base::Result<Record> made =
                CreateRecord(index);
            if (!made) return made.GetStatus();
            Record record = std::move(made).Value();
            Base::Result<void> attached =
                AttachRecord(record, index);
            if (!attached) return attached.GetStatus();
            Base::Result<void> inserted =
                InsertRecord(
                    index, std::move(record));
            if (!inserted) {
                return inserted.GetStatus();
            }
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Remove) {
        if (event.oldIndex > records_.Size() ||
            event.oldCount >
                records_.Size() - event.oldIndex) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Items removal delta is invalid");
        }
        for (std::uint32_t count = 0U;
            count < event.oldCount; ++count) {
            Base::Result<void> detached =
                DetachRecord(
                    records_[event.oldIndex]);
            if (!detached) return detached.GetStatus();
            RemoveRecordAt(event.oldIndex);
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Replace) {
        if (event.oldIndex > records_.Size() ||
            event.oldCount >
                records_.Size() - event.oldIndex) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Items replacement delta is invalid");
        }
        for (std::uint32_t count = 0U;
            count < event.oldCount; ++count) {
            Base::Result<void> detached =
                DetachRecord(
                    records_[event.oldIndex]);
            if (!detached) return detached.GetStatus();
            RemoveRecordAt(event.oldIndex);
        }
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            const std::uint32_t index =
                event.newIndex + offset;
            Base::Result<Record> made =
                CreateRecord(index);
            if (!made) return made.GetStatus();
            Record record = std::move(made).Value();
            Base::Result<void> attached =
                AttachRecord(record, index);
            if (!attached) return attached.GetStatus();
            Base::Result<void> inserted =
                InsertRecord(
                    index, std::move(record));
            if (!inserted) {
                return inserted.GetStatus();
            }
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Move &&
        event.oldCount == 1U &&
        event.newCount == 1U &&
        event.oldIndex < records_.Size() &&
        event.newIndex < records_.Size()) {
        Record moving =
            std::move(records_[event.oldIndex]);
        if (event.oldIndex < event.newIndex) {
            for (std::uint32_t index = event.oldIndex;
                index < event.newIndex; ++index) {
                records_[index] =
                    std::move(records_[index + 1U]);
            }
        } else {
            for (std::uint32_t index = event.oldIndex;
                index > event.newIndex; --index) {
                records_[index] =
                    std::move(records_[index - 1U]);
            }
        }
        records_[event.newIndex] =
            std::move(moving);
        return ReorderVisuals();
    }
    return Refresh();
}

void ItemContainerGenerator::OnItemsChanged(
    const ItemsChangedEvent& event) noexcept {
    Base::Result<void> applied;
    if (virtualizingHost_ != nullptr &&
        owner_ != nullptr) {
        applied =
            virtualizingHost_->OnItemsChanged(
                event, owner_->ItemCount());
        if (applied) {
            Base::Result<bool> realized =
                SetRealizationRangeInternal(
                    virtualizingHost_->
                        desiredFirstIndex_,
                    virtualizingHost_->
                        desiredCount_,
                    true);
            if (!realized) {
                applied = realized.GetStatus();
            }
        }
    } else {
        applied = ApplyChange(event);
    }
    lastError_ = applied
        ? Base::Status{}
        : applied.GetStatus();
    if (applied && owner_ != nullptr) {
        owner_->OnContainersChanged();
    }
}

ItemContainer*
ItemContainerGenerator::ContainerFromIndex(
    std::uint32_t index) const noexcept {
    return index >= firstGeneratedIndex_ &&
        index - firstGeneratedIndex_ <
            records_.Size()
        ? records_[
            index - firstGeneratedIndex_]
              .container.Get()
        : nullptr;
}

std::uint32_t
ItemContainerGenerator::IndexFromContainer(
    const ItemContainer& container) const noexcept {
    for (std::uint32_t index = 0U;
        index < records_.Size(); ++index) {
        if (records_[index].container.Get() ==
            &container) {
            return firstGeneratedIndex_ + index;
        }
    }
    return UINT32_MAX;
}

Base::Ref<Base::Object>
ItemContainerGenerator::ItemFromContainer(
    const ItemContainer& container) const noexcept {
    const std::uint32_t index =
        IndexFromContainer(container);
    return index != UINT32_MAX
        ? records_[
            index - firstGeneratedIndex_].item
        : Base::Ref<Base::Object>();
}

Base::Result<void> DataTemplate::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    return Presentation::Detail::AssignResourceDictionary(
        resources_,
        std::move(value),
        "DataTemplate Resources is already assigned");
}

Base::Result<void> ItemsPanelTemplate::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    return Presentation::Detail::AssignResourceDictionary(
        resources_,
        std::move(value),
        "ItemsPanelTemplate Resources is already assigned");
}

} // namespace Aero::Controls
