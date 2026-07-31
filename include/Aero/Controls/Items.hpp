#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Collections/ItemsSource.hpp>
#include <Aero/Detail/MountService.hpp>
#include <Aero/Style.hpp>

#include <utility>

namespace Aero::Render { class RenderManager; }

namespace Aero::Controls {

using ItemsChangeAction = Core::ItemsChangeAction;
using ItemsChangedEvent = Core::ItemsChangedEvent;
using ItemsChangedHandler = Core::ItemsChangedHandler;

enum class ItemSubtreeChange : std::uint8_t {
    Mounted = 0U,
    Unmounting,
};

using ItemSubtreeCallback = Base::Result<void> (*)(
    Aero::Visual& root,
    ItemSubtreeChange change,
    void* context) noexcept;

using IItemsSource = Core::IItemsSource;

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

// Reference-counted collection source for view-model properties. ItemsControl
// keeps its lightweight embedded ItemsCollection for authored child content;
// bindings use this object form so the source can travel through metadata and
// dependency-property values without losing IItemsSource semantics.
class AERO_API ObjectItemsSource final :
    public Base::Object,
    public IItemsSource {
    AERO_DECLARE_TYPE(ObjectItemsSource, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    std::uint32_t Count() const noexcept override {
        return items_.Count();
    }
    Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept override {
        return items_.ItemAt(index);
    }
    Base::Result<void> Add(
        Base::Ref<Base::Object> item) noexcept {
        return items_.Add(std::move(item));
    }
    Base::Result<void> Insert(
        std::uint32_t index,
        Base::Ref<Base::Object> item) noexcept {
        return items_.Insert(index, std::move(item));
    }
    Base::Result<Base::Ref<Base::Object>> RemoveAt(
        std::uint32_t index) noexcept {
        return items_.RemoveAt(index);
    }
    void Reset() noexcept {
        items_.Reset();
    }
    Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return items_.TryAddItemsChanged(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return items_.RemoveItemsChanged(handler);
    }

private:
    ItemsCollection items_;
};

// WPF AlternationConverter selects an authored value by alternation index.
// Keeping object values intact lets a binding later return brushes, strings,
// and other resources without lossy text conversion.
class AERO_API AlternationConverter final : public Base::Object {
    AERO_DECLARE_TYPE(AlternationConverter, Base::Object)
public:
    AlternationConverter() noexcept
        : values_(&Base::GetDefaultAllocator()) {}
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<Base::Object>> Values() const noexcept {
        return values_.AsSpan();
    }
    Base::Result<void> AddValue(
        Base::Ref<Base::Object> value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AlternationConverter values cannot be null");
        }
        return values_.TryPushBack(std::move(value));
    }
    void ClearValues() noexcept { values_.Clear(); }
private:
    Base::Vector<Base::Ref<Base::Object>> values_;
};

// Object wrapper for primitive values used by ItemsSource. This is the
// collection equivalent of WPF boxing: business collections can contain
// strings and other scalar values without manufacturing UIElement objects.
class AERO_API BoxedItemValue final : public Base::Object {
    AERO_DECLARE_TYPE(BoxedItemValue, Base::Object)
public:
    explicit BoxedItemValue(Core::Value value) noexcept
        : value_(std::move(value)) {}

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    const Core::Value& Value() const noexcept {
        return value_;
    }

private:
    Core::Value value_;
};

inline Base::Result<void> AddBoxedItem(
    ObjectItemsSource& source,
    Core::Value value) noexcept {
    Base::Result<Base::Ref<BoxedItemValue>> boxed =
        Base::MakeRef<BoxedItemValue>(std::move(value));
    if (!boxed) return boxed.GetStatus();
    return source.Add(
        Base::Ref<Base::Object>(
            std::move(boxed).Value()));
}

inline Base::Result<void> AddBoxedStringItem(
    ObjectItemsSource& source,
    Base::StringView value) noexcept {
    Base::Result<Core::Value> boxed =
        Core::Value::TryFromString(
            Core::TypeOf<Base::String>(), value);
    if (!boxed) return boxed.GetStatus();
    return AddBoxedItem(
        source, std::move(boxed).Value());
}

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
    Base::Ref<Base::Object> HierarchicalItemsSource() const noexcept {
        return hierarchicalItemsSource_;
    }
    Base::Result<void> SetHierarchicalItemsSource(
        Base::Ref<Base::Object> value) noexcept {
        hierarchicalItemsSource_ = std::move(value);
        return {};
    }
    Base::Ref<Base::Object> HierarchicalItemTemplate() const noexcept {
        return hierarchicalItemTemplate_;
    }
    Base::Result<void> SetHierarchicalItemTemplate(
        Base::Ref<Base::Object> value) noexcept {
        hierarchicalItemTemplate_ = std::move(value);
        return {};
    }
      Base::Result<void> SetAuthoredVisualTree(
          const Base::Ref<Base::Object>& value) noexcept;
      Base::Result<void> TryAddAuthoredTrigger(
          Base::Ref<Aero::TriggerBase> trigger) noexcept {
          if (!trigger || program_.IsValid()) {
              return Base::Status::Failure(
                  Base::ErrorCode::InvalidState,
                  "DataTemplate Trigger cannot be added after sealing");
          }
          return authoredTriggers_.TryPushBack(
              std::move(trigger));
      }
      void ClearAuthoredTriggers() noexcept {
          authoredTriggers_.Clear();
      }
      Base::Span<const Base::Ref<
          Aero::TriggerBase>>
      AuthoredTriggers() const noexcept {
          return {
              authoredTriggers_.Data(),
              authoredTriggers_.Size()};
      }
    Base::Result<void> RegisterAuthoredName(
        Base::StringView name,
        Base::Object& object) noexcept {
        return authoredNames_.TryRegister(
            name, object);
    }
    const Aero::NameScope&
    AuthoredNames() const noexcept {
        return authoredNames_;
    }
    void ClearAuthoredNames() noexcept {
        authoredNames_.Clear();
    }
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
    Base::Result<void> SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;
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
      Base::Ref<Base::Object> hierarchicalItemsSource_;
      Base::Ref<Base::Object> hierarchicalItemTemplate_;
      ResourceDictionary resources_;
      Base::Ref<Base::Object> authoredVisualTree_;
      Base::Vector<Base::Ref<
          Aero::TriggerBase>>
          authoredTriggers_;
      Aero::NameScope authoredNames_;
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
    Base::Result<void> SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;
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
    Base::Ref<Base::Object>
    BoundItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Base::Ref<Base::Object>{});
    }
    std::uint32_t ItemCount() const noexcept;
    Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept;
    Base::Result<void> SetItemsSource(
        IItemsSource* source) noexcept;
    Base::Result<void> SetBoundItemsSource(
        Base::Ref<Base::Object> source) noexcept {
        return SetValue(
            ItemsSourceProperty,
            std::move(source));
    }
    std::uint32_t AlternationCount() const noexcept {
        return GetValueOr(AlternationCountProperty, 0U);
    }
    Base::Result<void> SetAlternationCount(
        std::uint32_t value) noexcept {
        return SetValue(AlternationCountProperty, value);
    }

    const DataTemplate* ItemTemplate() const noexcept {
        return itemTemplate_;
    }
    Base::Ref<DataTemplate>
    ItemTemplateValue() const noexcept {
        return GetValueOr(
            ItemTemplateProperty,
            Base::Ref<DataTemplate>{});
    }
    void SetItemTemplate(
        const DataTemplate* value) noexcept;
    Base::Result<void> SetItemTemplateValue(
        Base::Ref<DataTemplate> value) noexcept {
        return SetValue(
            ItemTemplateProperty,
            std::move(value));
    }
    const ItemsPanelTemplate* ItemsPanel() const noexcept {
        return itemsPanel_;
    }
    Base::Ref<ItemsPanelTemplate>
    ItemsPanelValue() const noexcept {
        return GetValueOr(
            ItemsPanelProperty,
            Base::Ref<ItemsPanelTemplate>{});
    }
    void SetItemsPanel(
        const ItemsPanelTemplate* value) noexcept;
    Base::Result<void> SetItemsPanelValue(
        Base::Ref<ItemsPanelTemplate> value) noexcept {
        return SetValue(
            ItemsPanelProperty,
            std::move(value));
    }
    const Style* ItemContainerStyle() const noexcept {
        return itemContainerStyle_;
    }
    Base::Ref<Style>
    ItemContainerStyleValue() const noexcept {
        return GetValueOr(
            ItemContainerStyleProperty,
            Base::Ref<Style>{});
    }
    void SetItemContainerStyle(
        const Style* value) noexcept;
    Base::Result<void> SetItemContainerStyleValue(
        Base::Ref<Style> value) noexcept {
        return SetValue(
            ItemContainerStyleProperty,
            std::move(value));
    }

    Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        return changed_.TryAdd(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        return changed_.Remove(handler);
    }
    Panel* ItemsHost() const noexcept {
        return itemsHost_;
    }
    std::uint32_t RealizedItemCount() const noexcept;
    std::uint32_t CreatedContainerCount() const noexcept;
    std::uint32_t RecycledContainerUseCount() const noexcept;

    inline static constexpr Members::ReadOnlyProperty<
        std::uint32_t> ItemCountProperty{"ItemCount"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        HasItemsProperty{"HasItems"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ItemsSourceProperty{"ItemsSource"};
    inline static constexpr Members::Property<std::uint32_t>
        AlternationCountProperty{"AlternationCount"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr Members::Property<
        Base::Ref<ItemsPanelTemplate>>
        ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr Members::Property<
        Base::Ref<Style>>
        ItemContainerStyleProperty{"ItemContainerStyle"};

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
    Base::Result<void> OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class ItemContainerGenerator;
    ItemsCollection items_;
    IItemsSource* source_ = nullptr;
    const DataTemplate* itemTemplate_ = nullptr;
    const ItemsPanelTemplate* itemsPanel_ = nullptr;
    const Style* itemContainerStyle_ = nullptr;
    ItemContainerGenerator* generator_ = nullptr;
    Panel* itemsHost_ = nullptr;
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

// WPF's header-bearing items base. It retains ItemsControl's generation and
// layout behavior while exposing the header metadata consumed by theme styles.
class AERO_API HeaderedItemsControl : public ItemsControl {
    AERO_DECLARE_TYPE(HeaderedItemsControl, ItemsControl)
public:
    HeaderedItemsControl() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~HeaderedItemsControl() override = default;

    Base::StringView Header() const noexcept {
        return GetValueOr(HeaderProperty, Base::StringView{});
    }
    Base::Result<void> SetHeader(Base::StringView value) noexcept {
        return SetValue(HeaderProperty, value);
    }
    Base::Ref<DataTemplate> HeaderTemplate() const noexcept {
        return GetValueOr(
            HeaderTemplateProperty, Base::Ref<DataTemplate>{});
    }
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        return SetValue(HeaderTemplateProperty, std::move(value));
    }

    inline static constexpr Members::Property<Base::String>
        HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>>
        HeaderTemplateProperty{"HeaderTemplate"};
};

class AERO_API ItemContainerGenerator final {
public:
    ItemContainerGenerator(
        ObjectTree& tree,
        LayoutManager& layout,
        EffectiveValueEngine& values,
        StyleManager* styles = nullptr,
        RenderManager* renderer = nullptr,
        TemplateManager* templates = nullptr,
        ItemSubtreeCallback subtreeCallback = nullptr,
        void* subtreeContext = nullptr) noexcept;
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
        Base::Vector<MountEdgeState> subtreeMounts;
        const Style* appliedStyle = nullptr;
        bool itemIsOwnContainer = false;
        bool generatedTextContent = false;
        bool subtreeMounted = false;
    };

    ObjectTree* tree_ = nullptr;
    LayoutManager* layout_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    StyleManager* styles_ = nullptr;
    RenderManager* renderer_ = nullptr;
    TemplateManager* templates_ = nullptr;
    ItemSubtreeCallback subtreeCallback_ = nullptr;
    void* subtreeContext_ = nullptr;
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
    Base::Result<void> AttachOwnedSubtree(
        Record& record,
        Aero::Visual& root) noexcept;
    Base::Result<void> DetachOwnedSubtree(
        Record& record) noexcept;
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
