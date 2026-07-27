#pragma once

#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/MountService.hpp>
#include <Aero/Presentation/Style.hpp>

#include <utility>
#include <Aero/Type.hpp>

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

using DeferredObjectFactory = Base::Result<
    Base::Ref<Base::Object>> (*)(
        const Base::Ref<Base::Object>& item,
        void* context) noexcept;

// Shared immutable deferred factory used by data and items-panel templates.
// The payload is the data item for DataTemplate and an empty reference for
// ItemsPanelTemplate. XAML compilers may retain node IR behind context.
class AERO_API DeferredObjectProgram final {
public:
    Base::Result<void> Configure(
        DeferredObjectFactory factory,
        void* context = nullptr) noexcept;
    Base::Result<void> Configure(
        DeferredObjectFactory factory,
        void* context,
        Base::Ref<Base::Object> factoryOwner) noexcept;
    Base::Result<void> SetBaseUri(
        const Base::ResourceUri& value) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<Base::Ref<Base::Object>> Instantiate(
        const Base::Ref<Base::Object>& payload = {}) const noexcept;

    DeferredObjectFactory Factory() const noexcept {
        return factory_;
    }
    void* FactoryContext() const noexcept {
        return context_;
    }
    const Base::Ref<Base::Object>& FactoryOwner() const noexcept {
        return factoryOwner_;
    }
    const Base::ResourceUri& BaseUri() const noexcept {
        return baseUri_;
    }
    bool IsValid() const noexcept {
        return factory_ != nullptr;
    }
    bool IsSealed() const noexcept {
        return sealed_;
    }

private:
    DeferredObjectFactory factory_ = nullptr;
    void* context_ = nullptr;
    Base::Ref<Base::Object> factoryOwner_;
    Base::ResourceUri baseUri_;
    bool sealed_ = false;
};

using DataTemplateFactory = DeferredObjectFactory;

class AERO_API DataTemplate final : public Base::Object {
    AERO_DECLARE_TYPE(DataTemplate, Base::Object)
public:
    DataTemplate() noexcept = default;
    DataTemplate(
        DataTemplateFactory factory,
        void* context = nullptr,
        Base::Ref<Base::Object> factoryOwner = {}) noexcept
        { (void)program_.Configure(
            factory, context, std::move(factoryOwner)); }
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Result<void> Configure(
        DataTemplateFactory factory,
        void* context = nullptr,
        Base::Ref<Base::Object> factoryOwner = {}) noexcept;
    Base::Result<void> SetDataType(
        TypeId value) noexcept;
    Base::Result<void> SetAuthoredVisualTree(
        const Base::Ref<Base::Object>& value) noexcept;
    const Base::Ref<Base::Object>&
    AuthoredVisualTree() const noexcept {
        return authoredVisualTree_;
    }
    void ClearAuthoredVisualTree() noexcept {
        authoredVisualTree_.Reset();
    }
    Base::Result<void> Seal() noexcept;
    TypeId DataType() const noexcept {
        return dataType_;
    }
    ResourceKey ImplicitKey() const noexcept {
        return ResourceKey::FromType(dataType_);
    }
    ResourceDictionary& Resources() noexcept {
        return resources_;
    }
    const ResourceDictionary& Resources() const noexcept {
        return resources_;
    }
    Base::Result<void> SetBaseUri(
        const Base::ResourceUri& value) noexcept {
        return program_.SetBaseUri(value);
    }
    const Base::ResourceUri& BaseUri() const noexcept {
        return program_.BaseUri();
    }
    Base::Result<Base::Ref<Base::Object>>
        Instantiate(
            const Base::Ref<Base::Object>& item) const noexcept;
    bool IsValid() const noexcept {
        return program_.IsValid();
    }
    DeferredObjectProgram& Program() noexcept {
        return program_;
    }
    const DeferredObjectProgram& Program() const noexcept {
        return program_;
    }

private:
    DeferredObjectProgram program_;
    TypeId dataType_ = InvalidTypeId;
    ResourceDictionary resources_;
    Base::Ref<Base::Object> authoredVisualTree_;
};

class AERO_API ItemsPanelTemplate final
    : public Base::Object {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, Base::Object)
public:
    ItemsPanelTemplate() noexcept = default;
    ItemsPanelTemplate(
        DeferredObjectFactory factory,
        void* context = nullptr,
        Base::Ref<Base::Object> factoryOwner = {}) noexcept
        { (void)program_.Configure(
            factory, context, std::move(factoryOwner)); }
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Result<void> Configure(
        DeferredObjectFactory factory,
        void* context = nullptr,
        Base::Ref<Base::Object> factoryOwner = {}) noexcept;
    Base::Result<void> SetAuthoredVisualTree(
        const Base::Ref<Base::Object>& value) noexcept;
    const Base::Ref<Base::Object>&
    AuthoredVisualTree() const noexcept {
        return authoredVisualTree_;
    }
    void ClearAuthoredVisualTree() noexcept {
        authoredVisualTree_.Reset();
    }
    Base::Result<void> Seal() noexcept;
    ResourceDictionary& Resources() noexcept {
        return resources_;
    }
    const ResourceDictionary& Resources() const noexcept {
        return resources_;
    }
    Base::Result<void> SetBaseUri(
        const Base::ResourceUri& value) noexcept {
        return program_.SetBaseUri(value);
    }
    const Base::ResourceUri& BaseUri() const noexcept {
        return program_.BaseUri();
    }
    Base::Result<Base::Ref<Base::Object>>
        Instantiate() const noexcept;
    bool IsValid() const noexcept {
        return program_.IsValid();
    }
    DeferredObjectProgram& Program() noexcept {
        return program_;
    }
    const DeferredObjectProgram& Program() const noexcept {
        return program_;
    }

private:
    DeferredObjectProgram program_;
    ResourceDictionary resources_;
    Base::Ref<Base::Object> authoredVisualTree_;
};

class AERO_API ItemContainer : public ContentControl {
    AERO_DECLARE_TYPE(ItemContainer, ContentControl)
public:
    ItemContainer() noexcept
        : ContentControl(StaticTypeId()) {}
    ~ItemContainer() override = default;
protected:
    explicit ItemContainer(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
};

class AERO_API ItemsPresenter final : public Decorator {
    AERO_DECLARE_TYPE(ItemsPresenter, Decorator)
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
    AERO_DECLARE_TYPE(ItemsControl, Control)
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
    inline static constexpr auto ItemCountProperty =
        Members::Property<std::uint32_t>{"ItemCount"};

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
        MountEdgeState containerMount;
        MountEdgeState contentMount;
        const Style* appliedStyle = nullptr;
    };

    ObjectTree* tree_ = nullptr;
    LayoutManager* layout_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    StyleManager* styles_ = nullptr;
    RenderManager* renderer_ = nullptr;
    MountService mounts_;
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
