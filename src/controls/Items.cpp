#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Virtualization.hpp>

#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <algorithm>
#include <utility>

namespace Aero::Controls {

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

Base::Result<Base::Ref<Base::Object>>
DataTemplate::Instantiate(
    const Base::Ref<Base::Object>& item) const noexcept {
    if (factory_ == nullptr || !item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result =
        factory_(item, context_);
    if (!result || result.Value()) return result;
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "DataTemplate returned null");
}

Base::Result<Base::Ref<Base::Object>>
ItemsPanelTemplate::Instantiate() const noexcept {
    if (factory_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsPanelTemplate is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result =
        factory_(context_);
    if (!result || result.Value()) return result;
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "ItemsPanelTemplate returned null");
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

std::uint32_t ItemsControl::ItemCount() const noexcept {
    return source_ != nullptr
        ? source_->Count()
        : items_.Count();
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
    static_cast<void>(SetReadOnlyCurrentValue(
        ItemCountProperty,
        Value::FromUnsignedInteger(
            BuiltinTypes::UnsignedInteger,
            ItemCount())));
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
    RenderManager* renderer) noexcept
    : tree_(&tree),
      layout_(&layout),
      values_(&values),
      styles_(styles),
      renderer_(renderer),
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
    const DataTemplate* itemTemplate =
        owner_->ItemTemplate();
    if (itemTemplate != nullptr) {
        Base::Result<Base::Ref<Base::Object>>
            content =
                itemTemplate->Instantiate(record.item);
        if (!content) return content.GetStatus();
        record.content =
            std::move(content).Value();
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
ItemContainerGenerator::AttachRecord(
    Record& record,
    std::uint32_t index) noexcept {
    ItemContainer& container = *record.container;
    auto& content = *static_cast<UIElement*>(record.content.Get());

    Base::Result<MountEdgeState> containerMounted =
        mounts_.Attach(*owner_, *host_, container);
    if (!containerMounted) return containerMounted.GetStatus();
    record.containerMount = std::move(containerMounted).Value();

    Base::Result<MountEdgeState> contentMounted = mounts_.Attach(container, content);
    if (!contentMounted) {
        (void)mounts_.Detach(record.containerMount);
        return contentMounted.GetStatus();
    }
    record.contentMount = std::move(contentMounted).Value();

    Base::Result<void> selected = container.SetOwnedContent(record.content, content);
    if (!selected) {
        (void)mounts_.Detach(record.contentMount);
        (void)mounts_.Detach(record.containerMount);
        return selected.GetStatus();
    }
    const Style* style = owner_->ItemContainerStyle();
    if (style != nullptr && styles_ != nullptr) {
        Base::Result<void> styled = styles_->Apply(container, *style);
        if (!styled) { (void)DetachRecord(record); return styled.GetStatus(); }
        record.appliedStyle = style;
    }
    Base::Result<void> prepared = owner_->PrepareContainer(container, record.item, index);
    if (!prepared) { (void)DetachRecord(record); return prepared.GetStatus(); }
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
    owner_->ClearContainer(container);
    if (record.appliedStyle != nullptr && styles_ != nullptr) {
        capture(styles_->Clear(container, *record.appliedStyle));
        record.appliedStyle = nullptr;
    }
    UIElement* content = container.Content();
    if (content != nullptr) {
        capture(mounts_.Detach(record.contentMount));
        capture(container.SetContent(nullptr));
        capture(values_->DetachObject(*content));
    }
    capture(mounts_.Detach(record.containerMount));
    if (recycleContainer && firstError.IsOk()) {
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

} // namespace Aero::Controls
