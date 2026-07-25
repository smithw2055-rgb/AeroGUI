#pragma once

#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/Style.hpp>

namespace Aero::Presentation {
class RenderManager;
}

namespace Aero::Controls {

enum class ItemsChangeAction : std::uint8_t {
    Add = 0U,
    Remove,
    Replace,
    Move,
    Reset,
};

struct ItemsChangedEvent final {
    ItemsChangeAction action = ItemsChangeAction::Reset;
    std::uint32_t oldIndex = UINT32_MAX;
    std::uint32_t newIndex = UINT32_MAX;
    std::uint32_t oldCount = 0U;
    std::uint32_t newCount = 0U;
};

using ItemsChangedHandler =
    Base::Delegate<void(const ItemsChangedEvent&)>;

class AERO_API IItemsSource {
public:
    virtual ~IItemsSource() = default;
    virtual std::uint32_t Count() const noexcept = 0;
    virtual Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept = 0;
    virtual Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
    virtual bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
};

class AERO_API ItemsCollection final : public IItemsSource {
public:
    std::uint32_t Count() const noexcept override {
        return items_.Size();
    }
    Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept override;
    Base::Result<void> Add(
        Base::Ref<Base::Object> item) noexcept;
    Base::Result<void> Insert(
        std::uint32_t index,
        Base::Ref<Base::Object> item) noexcept;
    Base::Result<Base::Ref<Base::Object>> RemoveAt(
        std::uint32_t index) noexcept;
    Base::Result<void> Replace(
        std::uint32_t index,
        Base::Ref<Base::Object> item) noexcept;
    Base::Result<void> Move(
        std::uint32_t oldIndex,
        std::uint32_t newIndex) noexcept;
    void Reset() noexcept;
    Base::Result<void> Reset(
        Base::Span<const Base::Ref<Base::Object>>
            items) noexcept;
    Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.TryAdd(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }

private:
    Base::Vector<Base::Ref<Base::Object>> items_;
    ItemsChangedHandler changed_;
    void Notify(const ItemsChangedEvent& event) noexcept;
};

using DataTemplateFactory = Base::Result<
    Base::Ref<Base::Object>> (*)(
        const Base::Ref<Base::Object>& item,
        void* context) noexcept;

class AERO_API DataTemplate final {
public:
    DataTemplate(
        DataTemplateFactory factory,
        void* context = nullptr) noexcept
        : factory_(factory), context_(context) {}
    Base::Result<Base::Ref<Base::Object>>
        Instantiate(
            const Base::Ref<Base::Object>& item) const noexcept;
    bool IsValid() const noexcept {
        return factory_ != nullptr;
    }

private:
    DataTemplateFactory factory_ = nullptr;
    void* context_ = nullptr;
};

using ItemsPanelFactory = Base::Result<
    Base::Ref<Base::Object>> (*)(
        void* context) noexcept;

class AERO_API ItemsPanelTemplate final {
public:
    ItemsPanelTemplate(
        ItemsPanelFactory factory,
        void* context = nullptr) noexcept
        : factory_(factory), context_(context) {}
    Base::Result<Base::Ref<Base::Object>>
        Instantiate() const noexcept;
    bool IsValid() const noexcept {
        return factory_ != nullptr;
    }

private:
    ItemsPanelFactory factory_ = nullptr;
    void* context_ = nullptr;
};

class AERO_API ItemContainer : public ContentControl {
    AERO_TYPED_META(ItemContainer, ContentControl)
public:
    ItemContainer() noexcept
        : ContentControl(StaticTypeId()) {}
    ~ItemContainer() override = default;
protected:
    explicit ItemContainer(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
};

class AERO_API ItemsPresenter final : public Decorator {
    AERO_TYPED_META(ItemsPresenter, Decorator)
public:
    ItemsPresenter() noexcept
        : Decorator(StaticTypeId()) {}
    ~ItemsPresenter() override = default;
    Panel* ItemsHost() const noexcept {
        UIElement* child = Child();
        return child != nullptr &&
            PropertyRegistry().Types().IsDerivedFrom(
                child->RuntimeType(), Panel::StaticTypeId())
            ? static_cast<Panel*>(child)
            : nullptr;
    }
    Base::Result<void> SetItemsHost(
        const Base::Ref<Base::Object>& owner,
        Panel& panel) noexcept {
        return SetOwnedChild(owner, panel);
    }
};

class ItemContainerGenerator;
class VirtualizingStackPanel;

class AERO_API ItemsControl : public Control {
    AERO_TYPED_META(ItemsControl, Control)
public:
    ItemsControl() noexcept;
    ~ItemsControl() override;

    ItemsCollection& Items() noexcept {
        return items_;
    }
    const ItemsCollection& Items() const noexcept {
        return items_;
    }
    IItemsSource* ItemsSource() const noexcept {
        return source_;
    }
    std::uint32_t ItemCount() const noexcept;
    Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept;
    Base::Result<void> SetItemsSource(
        IItemsSource* source) noexcept;

    const DataTemplate* ItemTemplate() const noexcept {
        return itemTemplate_;
    }
    void SetItemTemplate(
        const DataTemplate* value) noexcept;
    const ItemsPanelTemplate* ItemsPanel() const noexcept {
        return itemsPanel_;
    }
    void SetItemsPanel(
        const ItemsPanelTemplate* value) noexcept;
    const Style* ItemContainerStyle() const noexcept {
        return itemContainerStyle_;
    }
    void SetItemContainerStyle(
        const Style* value) noexcept;

    Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        return changed_.TryAdd(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        return changed_.Remove(handler);
    }

    inline static constexpr DependencyPropertyHandle
        ItemCountProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "ItemCount");

protected:
    explicit ItemsControl(TypeId runtimeType) noexcept;
    ItemContainerGenerator* AttachedGenerator() const noexcept {
        return generator_;
    }
    virtual Base::Result<
        Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item) noexcept;
    virtual Base::Result<void> PrepareContainer(
        ItemContainer& container,
        const Base::Ref<Base::Object>& item,
        std::uint32_t index) noexcept;
    virtual void ClearContainer(
        ItemContainer& container) noexcept;
    virtual void OnContainersChanged() noexcept {}

private:
    friend class ItemContainerGenerator;
    ItemsCollection items_;
    IItemsSource* source_ = nullptr;
    const DataTemplate* itemTemplate_ = nullptr;
    const ItemsPanelTemplate* itemsPanel_ = nullptr;
    const Style* itemContainerStyle_ = nullptr;
    ItemContainerGenerator* generator_ = nullptr;
    ItemsChangedHandler changed_;
    ItemsChangedHandler localHandler_;
    ItemsChangedHandler sourceHandler_;

    void OnLocalChanged(
        const ItemsChangedEvent& event) noexcept;
    void OnSourceChanged(
        const ItemsChangedEvent& event) noexcept;
    void PublishReset() noexcept;
    void PublishItemCount() noexcept;
};

class AERO_API ItemContainerGenerator final {
public:
    ItemContainerGenerator(
        ObjectTree& tree,
        LayoutManager& layout,
        EffectiveValueEngine& values,
        StyleManager* styles = nullptr,
        RenderManager* renderer = nullptr) noexcept;
    ~ItemContainerGenerator() noexcept;

    Base::Result<void> Attach(
        ItemsControl& owner,
        Panel& itemsHost) noexcept;
    Base::Result<void> AttachVirtualized(
        ItemsControl& owner,
        VirtualizingStackPanel& itemsHost) noexcept;
    Base::Result<bool> Detach() noexcept;
    Base::Result<void> Refresh() noexcept;
    Base::Result<bool> SetRealizationRange(
        std::uint32_t firstIndex,
        std::uint32_t count) noexcept;
    std::uint32_t GeneratedCount() const noexcept {
        return records_.Size();
    }
    std::uint32_t FirstGeneratedIndex() const noexcept {
        return firstGeneratedIndex_;
    }
    std::uint32_t CreatedContainerCount() const noexcept {
        return createdContainerCount_;
    }
    std::uint32_t RecycledContainerUseCount() const noexcept {
        return recycledContainerUseCount_;
    }
    ItemContainer* ContainerFromIndex(
        std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const ItemContainer& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const ItemContainer& container) const noexcept;
    Base::Status LastError() const noexcept {
        return lastError_;
    }

private:
    struct Record final {
        Base::Ref<Base::Object> item;
        Base::Ref<ItemContainer> container;
        Base::Ref<Base::Object> content;
        const Style* appliedStyle = nullptr;
    };

    ObjectTree* tree_ = nullptr;
    LayoutManager* layout_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    StyleManager* styles_ = nullptr;
    RenderManager* renderer_ = nullptr;
    ItemsControl* owner_ = nullptr;
    Panel* host_ = nullptr;
    VirtualizingStackPanel* virtualizingHost_ = nullptr;
    Base::Vector<Record> records_;
    Base::Vector<Base::Ref<ItemContainer>> recycledContainers_;
    std::uint32_t firstGeneratedIndex_ = 0U;
    std::uint32_t createdContainerCount_ = 0U;
    std::uint32_t recycledContainerUseCount_ = 0U;
    ItemsChangedHandler changedHandler_;
    Base::Status lastError_;

    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<Record> CreateRecord(
        std::uint32_t index) noexcept;
    Base::Result<void> AttachRecord(
        Record& record,
        std::uint32_t index) noexcept;
    Base::Result<void> DetachRecord(
        Record& record,
        bool recycleContainer = false) noexcept;
    Base::Result<void> InsertRecord(
        std::uint32_t index,
        Record record) noexcept;
    void RemoveRecordAt(
        std::uint32_t index) noexcept;
    Base::Result<void> ReorderVisuals() noexcept;
    Base::Result<void> ApplyChange(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<bool> SetRealizationRangeInternal(
        std::uint32_t firstIndex,
        std::uint32_t count,
        bool force) noexcept;
    Base::Result<void> ReleaseRecycledContainers() noexcept;
};

} // namespace Aero::Controls
