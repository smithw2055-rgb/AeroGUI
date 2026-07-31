#pragma once

#include <Aero/Core/Collections/ItemsSource.hpp>
#include <Aero/Styling.hpp>
#include <utility>
#include <Aero/Input.hpp>
#include <Aero/Data.hpp>
#include <Aero/Controls/Base.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Primitives.hpp>

namespace Aero::Detail { class ControlRuntimeAccess; }
namespace Aero::Controls::Detail {
class ItemContainerGeneratorImpl;
class ItemContainerGeneratorAccess;
}

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
        UIElement* child = GetChild();
        return child != nullptr &&
            PropertyRegistry().Types().IsDerivedFrom(
                child->RuntimeType(), Panel::StaticTypeId())
            ? static_cast<Panel*>(child)
            : nullptr;
    }
    Base::Result<void> SetItemsHost(
        const Base::Ref<Base::Object>& owner,
        Panel& panel) noexcept;
};

class ItemContainerGenerator;
class VirtualizingStackPanel;

class AERO_API ItemsControl : public Control {
    AERO_DECLARE_TYPE(ItemsControl, Control)
public:
    ItemsControl() noexcept;
    ~ItemsControl() override;

    ItemsCollection& GetItems() noexcept {
        return items_;
    }
    const ItemsCollection& GetItems() const noexcept {
        return items_;
    }
    IItemsSource* GetItemsSource() const noexcept {
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

    const DataTemplate* GetItemTemplate() const noexcept {
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
    const ItemsPanelTemplate* GetItemsPanel() const noexcept {
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
    const Style* GetItemContainerStyle() const noexcept {
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

    inline static constexpr Members::ReadOnlyProperty<std::uint32_t> ItemCountProperty{"ItemCount"};
    inline static constexpr Members::ReadOnlyProperty<bool> HasItemsProperty{"HasItems"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ItemsSourceProperty{"ItemsSource"};
    inline static constexpr Members::Property<std::uint32_t> AlternationCountProperty{"AlternationCount"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr Members::Property<Base::Ref<ItemsPanelTemplate>> ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr Members::Property<Base::Ref<Style>> ItemContainerStyleProperty{"ItemContainerStyle"};

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
    friend class Detail::ItemContainerGeneratorImpl;
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

    inline static constexpr Members::Property<Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
};

class AERO_API ItemContainerGenerator final {
public:
    ~ItemContainerGenerator() noexcept;

    ItemContainerGenerator(const ItemContainerGenerator&) = delete;
    ItemContainerGenerator& operator=(const ItemContainerGenerator&) = delete;

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

    std::uint32_t GeneratedCount() const noexcept;
    std::uint32_t FirstGeneratedIndex() const noexcept;
    std::uint32_t CreatedContainerCount() const noexcept;
    std::uint32_t RecycledContainerUseCount() const noexcept;
    ItemContainer* ContainerFromIndex(
        std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const ItemContainer& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const ItemContainer& container) const noexcept;
    Base::Status LastError() const noexcept;

private:
    friend class Detail::ItemContainerGeneratorAccess;

    ItemContainerGenerator() noexcept = default;

    void* impl_ = nullptr;
};
} // namespace Aero::Controls

namespace Aero::Controls {

enum class ExpandDirection : std::uint8_t {
    Down = 0U,
    Up,
    Left,
    Right,
};

namespace Primitives {

enum class PlacementMode : std::uint8_t {
    Bottom = 0U,
    Top,
    Left,
    Right,
    Center,
    Mouse,
};

enum class PopupAnimation : std::uint8_t {
    None = 0U,
    Fade,
    Slide,
    Scroll,
};

class AERO_API Popup : public ContentControl {
    AERO_DECLARE_TYPE(Popup, ContentControl)
public:
    Popup() noexcept;
    ~Popup() override;

    bool IsOpen() const noexcept;
    Base::Result<void> SetIsOpen(bool value) noexcept;
    PlacementMode Placement() const noexcept;
    Base::Result<void> SetPlacement(
        PlacementMode value) noexcept;
    double HorizontalOffset() const noexcept;
    Base::Result<void> SetHorizontalOffset(
        double value) noexcept;
    double VerticalOffset() const noexcept;
    Base::Result<void> SetVerticalOffset(
        double value) noexcept;
    bool StaysOpen() const noexcept;
    Base::Result<void> SetStaysOpen(
        bool value) noexcept;
    bool MatchPlacementTargetWidth() const noexcept;
    Base::Result<void> SetMatchPlacementTargetWidth(
        bool value) noexcept;
    Base::Ref<UIElement>
        PlacementTarget() const noexcept;
    Base::Result<void> SetPlacementTarget(
        Base::Ref<UIElement> value) noexcept;
    PopupAnimation GetPopupAnimation() const noexcept;
    Base::Result<void> SetPopupAnimation(
        PopupAnimation value) noexcept;
    bool AllowsTransparency() const noexcept;
    Base::Result<void> SetAllowsTransparency(
        bool value) noexcept;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};
    UIElement::Event<RoutedEventHandler>
        Opened() noexcept {
        return GetEvent(OpenedEvent);
    }
    UIElement::Event<RoutedEventHandler>
        Closed() noexcept {
        return GetEvent(ClosedEvent);
    }

    inline static constexpr Members::Property<bool> IsOpenProperty{"IsOpen"};
    inline static constexpr Members::Property<PlacementMode> PlacementProperty{"Placement"};
    inline static constexpr Members::Property<double> HorizontalOffsetProperty{"HorizontalOffset"};
    inline static constexpr Members::Property<double> VerticalOffsetProperty{"VerticalOffset"};
    inline static constexpr Members::Property<bool> StaysOpenProperty{"StaysOpen"};
    inline static constexpr Members::Property<bool> MatchPlacementTargetWidthProperty{"MatchPlacementTargetWidth"};
    inline static constexpr Members::Property<Base::Ref<UIElement>> PlacementTargetProperty{"PlacementTarget"};
    inline static constexpr Members::Property<PopupAnimation> PopupAnimationProperty{"PopupAnimation"};
    inline static constexpr Members::Property<bool> AllowsTransparencyProperty{"AllowsTransparency"};

protected:
    explicit Popup(TypeId runtimeType) noexcept;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    Size popupDesiredSize_;
    void OnOpenPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

} // namespace Primitives

class AERO_API HeaderedContentControl
    : public ContentControl {
    AERO_DECLARE_TYPE(
        HeaderedContentControl,
        ContentControl)
public:
    Core::Value Header() const noexcept;
    Base::Result<void> SetHeader(
        const Core::Value& value) noexcept;
    Base::Ref<DataTemplate> HeaderTemplate() const noexcept;
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;

    // WPF headers are content, not just text. They can hold an element, a
    // resource object, a scalar, or x:Null and are consumed by a
    // ContentPresenter through ContentSource="Header".
    inline static constexpr Members::Property<Core::Value> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};

protected:
    explicit HeaderedContentControl(
        TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~HeaderedContentControl() override = default;
};

class AERO_API GroupBox final
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        GroupBox,
        HeaderedContentControl)
public:
    GroupBox() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~GroupBox() override = default;
};

class AERO_API Label final : public ContentControl {
    AERO_DECLARE_TYPE(Label, ContentControl)
public:
    Label() noexcept : ContentControl(StaticTypeId()) {}
};

class AERO_API Expander final
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        Expander,
        HeaderedContentControl)
public:
    Expander() noexcept;
    ~Expander() override;

    bool IsExpanded() const noexcept;
    Base::Result<void> SetIsExpanded(
        bool value) noexcept;
    ExpandDirection Direction() const noexcept;
    Base::Result<void> SetDirection(
        ExpandDirection value) noexcept;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    UIElement::Event<RoutedEventHandler>
        Expanded() noexcept {
        return GetEvent(ExpandedEvent);
    }
    UIElement::Event<RoutedEventHandler>
        Collapsed() noexcept {
        return GetEvent(CollapsedEvent);
    }
    inline static constexpr Members::Property<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr Members::Property<ExpandDirection> ExpandDirectionProperty{"ExpandDirection"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        expandedChangedHandler_;
    void OnExpandedPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API TabItem final
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        TabItem,
        HeaderedContentControl)
public:
    TabItem() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~TabItem() override = default;

    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(
        bool value) noexcept;
    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
};

class AERO_API TabControl final : public Control {
    AERO_DECLARE_TYPE(TabControl, Control)
public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t TabCount() const noexcept {
        return tabs_.Size();
    }
    std::uint32_t SelectedIndex() const noexcept;
    TabItem* SelectedTab() const noexcept;
    Core::Value SelectedContent() const noexcept {
        return GetValueOr(
            SelectedContentProperty,
            Core::Value::NullObject(
                Core::TypeOf<Base::Object>()));
    }
    Base::Result<void> AddOwnedTab(
        Base::Ref<TabItem> tab) noexcept;
    Base::Result<void> ClearOwnedTabs() noexcept;
    Base::Result<bool> SetSelectedIndex(
        std::uint32_t value) noexcept;
    // Kept as dependency properties even while the lightweight tab host is
    // being upgraded to the full selector pipeline. This preserves authored
    // ItemsControl binding/template declarations instead of reducing them to
    // loader-only markup.
    Base::Ref<Base::Object> GetItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Base::Ref<Base::Object>{});
    }
    Base::Result<void> SetItemsSource(
        Base::Ref<Base::Object> value) noexcept {
        return SetValue(ItemsSourceProperty, std::move(value));
    }
    Base::Ref<DataTemplate> GetItemTemplate() const noexcept {
        return GetValueOr(
            ItemTemplateProperty,
            Base::Ref<DataTemplate>{});
    }
    Base::Result<void> SetItemTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        return SetValue(ItemTemplateProperty, std::move(value));
    }
    Base::Ref<DataTemplate> GetContentTemplate() const noexcept {
        return GetValueOr(
            ContentTemplateProperty,
            Base::Ref<DataTemplate>{});
    }
    Base::Result<void> SetContentTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        return SetValue(ContentTemplateProperty, std::move(value));
    }
    Dock TabStripPlacement() const noexcept {
        return GetValueOr(TabStripPlacementProperty, Dock::Top);
    }
    Base::Result<void> SetTabStripPlacement(Dock value) noexcept {
        return SetValue(TabStripPlacementProperty, value);
    }

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectionChangedEvent{"SelectionChanged"};
    UIElement::Event<RoutedEventHandler>
        SelectionChanged() noexcept {
        return GetEvent(SelectionChangedEvent);
    }
    inline static constexpr Members::Property<std::uint32_t> SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr Members::ReadOnlyProperty<Core::Value> SelectedContentProperty{"SelectedContent"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ItemsSourceProperty{"ItemsSource"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr Members::Property<Dock> TabStripPlacementProperty{"TabStripPlacement"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Base::Vector<Base::Ref<TabItem>> tabs_;
    DependencyPropertyChangedEventHandler
        selectionChangedHandler_;
    void OnSelectionPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Base::Result<void> SynchronizeSelection() noexcept;
};

// Wraps tab headers according to the nearest templated TabControl's strip
// placement, matching the WPF TabPanel layout contract.
class AERO_API TabPanel final : public Panel {
    AERO_DECLARE_TYPE(TabPanel, Panel)
public:
    TabPanel() noexcept : Panel(StaticTypeId()) {}
    ~TabPanel() override = default;

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    bool IsVertical() const noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::ExpandDirection> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ExpandDirection");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ExpandDirection";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::Primitives::PlacementMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("PlacementMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "PlacementMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::Primitives::PopupAnimation> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("PopupAnimation");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "PopupAnimation";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core

namespace Aero::Controls {

class VisualStateManager;
namespace Primitives { class Popup; }
class TextBox;

enum class SelectionMode : std::uint8_t {
    Single = 0U,
    Multiple,
    Extended,
};

struct SelectionChangedEvent final {
    Base::Span<const std::uint32_t> removedIndices;
    Base::Span<const std::uint32_t> addedIndices;
    std::uint32_t oldPrimaryIndex = UINT32_MAX;
    std::uint32_t newPrimaryIndex = UINT32_MAX;
    Base::Ref<Base::Object> oldPrimaryItem;
    Base::Ref<Base::Object> newPrimaryItem;
};

namespace Primitives { class Selector; }

using SelectionChangedHandler =
    Base::Delegate<void(
        Primitives::Selector&, const SelectionChangedEvent&)>;

class AERO_API ListBoxItem : public ItemContainer {
    AERO_DECLARE_TYPE(ListBoxItem, ItemContainer)
public:
    ListBoxItem() noexcept : ItemContainer(StaticTypeId()) {}
    ~ListBoxItem() override = default;

    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(bool value) noexcept;

    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
protected:
    explicit ListBoxItem(TypeId runtimeType) noexcept
        : ItemContainer(runtimeType) {}
};

namespace Primitives {

class AERO_API Selector : public ItemsControl {
    AERO_DECLARE_TYPE(Selector, ItemsControl)
public:
    Selector() noexcept;
    ~Selector() override;

    SelectionMode GetSelectionMode() const noexcept;
    std::uint32_t SelectedIndex() const noexcept;
    Base::Ref<Base::Object> SelectedItem() const noexcept;
    Base::Ref<Base::Object> SelectedValue() const noexcept;
    Base::StringView SelectedValuePath() const noexcept {
        return GetValueOr(
            SelectedValuePathProperty,
            Base::StringView{});
    }
    Base::Span<const std::uint32_t> SelectedIndices() const noexcept {
        return {selectedIndices_.Data(), selectedIndices_.Size()};
    }
    std::uint32_t SelectedCount() const noexcept {
        return selectedIndices_.Size();
    }
    bool IsSelected(std::uint32_t index) const noexcept;
    std::uint32_t IndexOfItem(
        const Base::Object* item) const noexcept;

    Base::Result<void> SetSelectionMode(
        SelectionMode value) noexcept;
    Base::Result<bool> SetSelectedIndex(
        std::uint32_t index) noexcept;
    Base::Result<bool> SetSelectedItem(
        Base::Ref<Base::Object> item) noexcept;
    Base::Result<bool> SetSelectedValue(
        Base::Ref<Base::Object> value) noexcept;
    Base::Result<bool> Select(
        std::uint32_t index) noexcept;
    Base::Result<bool> Unselect(
        std::uint32_t index) noexcept;
    Base::Result<bool> Toggle(
        std::uint32_t index) noexcept;
    Base::Result<bool> SelectRange(
        std::uint32_t first,
        std::uint32_t last,
        bool preserveExisting = false) noexcept;
    Base::Result<bool> ClearSelection() noexcept;

    Base::Result<void> TryAddSelectionChanged(
        const SelectionChangedHandler& handler) noexcept {
        return selectionChanged_.TryAdd(handler);
    }
    bool RemoveSelectionChanged(
        const SelectionChangedHandler& handler) noexcept {
        return selectionChanged_.Remove(handler);
    }
    Base::Status LastSelectionError() const noexcept {
        return lastSelectionError_;
    }

    inline static constexpr Members::Property<SelectionMode> SelectionModeProperty{"SelectionMode"};
    inline static constexpr Members::Property<std::uint32_t> SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> SelectedItemProperty{"SelectedItem"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> SelectedValueProperty{"SelectedValue"};
    inline static constexpr Members::Property<Base::String> SelectedValuePathProperty{"SelectedValuePath"};
    inline static constexpr Members::AttachedProperty<bool> IsSelectedProperty{"IsSelected"};
    // WPF Selector.SelectionChanged is a bubbling routed event. Keep the
    // strongly typed selection notification above for model-facing code while
    // also publishing the routed surface used by EventTrigger.
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectionChangedRoutedEvent{"SelectionChanged"};
    UIElement::Event<RoutedEventHandler>
        SelectionChanged() noexcept {
        return GetEvent(
            SelectionChangedRoutedEvent);
    }

protected:
    explicit Selector(TypeId runtimeType) noexcept;
    Base::Result<void> PrepareContainer(
        ItemContainer& container,
        const Base::Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainer(
        ItemContainer& container) noexcept override;
    void OnContainersChanged() noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    Base::Vector<std::uint32_t> selectedIndices_;
    std::uint32_t primaryIndex_ = UINT32_MAX;
    std::uint32_t pendingIndex_ = UINT32_MAX;
    SelectionChangedHandler selectionChanged_;
    ItemsChangedHandler itemsChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Base::Status lastSelectionError_;
    VisualStateManager* states_ = nullptr;
    DependencyPropertyHandle activeProperty_;
    bool synchronizingProperties_ = false;

    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    Base::Result<bool> ApplySelection(
        Base::Span<const std::uint32_t> indices,
        std::uint32_t primaryIndex) noexcept;
    Base::Result<void> PublishProperties() noexcept;
    void SyncContainers() noexcept;
};

} // namespace Primitives

class AERO_API ListBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ListBox, Primitives::Selector)
public:
    ListBox() noexcept : Primitives::Selector(StaticTypeId()) {}
    ~ListBox() override;

    Base::Result<bool> BringIntoView(
        std::uint32_t index) noexcept;

protected:
    explicit ListBox(TypeId runtimeType) noexcept
        : Primitives::Selector(runtimeType) {}
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item) noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    void* interactions_ = nullptr;
};

class AERO_API ComboBoxItem final
    : public ItemContainer {
    AERO_DECLARE_TYPE(ComboBoxItem, ItemContainer)
public:
    ComboBoxItem() noexcept
        : ItemContainer(StaticTypeId()) {}
    ~ComboBoxItem() override = default;

    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(
        bool value) noexcept;

    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
};


class AERO_API ComboBox final : public Primitives::Selector {
    AERO_DECLARE_TYPE(ComboBox, Primitives::Selector)
public:
    ComboBox() noexcept;
    ~ComboBox() override;

    bool IsDropDownOpen() const noexcept;
    Base::Result<void> SetIsDropDownOpen(
        bool value) noexcept;
    double MaxDropDownHeight() const noexcept;
    Base::Result<void> SetMaxDropDownHeight(
        double value) noexcept;
    bool IsEditable() const noexcept;
    Base::Result<void> SetIsEditable(
        bool value) noexcept;
    bool IsReadOnly() const noexcept;
    Base::Result<void> SetIsReadOnly(
        bool value) noexcept;
    Base::StringView Text() const noexcept;
    Base::Result<void> SetText(
        Base::StringView value) noexcept;
    Base::StringView Placeholder() const noexcept {
        return GetValueOr(
            PlaceholderProperty, Base::StringView{});
    }
    Base::Result<void> SetPlaceholder(
        Base::StringView value) noexcept {
        return SetValue(PlaceholderProperty, value);
    }
    Base::String SelectionBoxText() const noexcept;
    Core::Value SelectionBoxItem() const noexcept {
        return GetValueOr(
            SelectionBoxItemProperty,
            Core::Value::NullObject(
                Core::TypeOf<Base::Object>()));
    }

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> DropDownOpenedEvent{"DropDownOpened"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> DropDownClosedEvent{"DropDownClosed"};
    UIElement::Event<RoutedEventHandler>
        DropDownOpened() noexcept {
        return GetEvent(DropDownOpenedEvent);
    }
    UIElement::Event<RoutedEventHandler>
        DropDownClosed() noexcept {
        return GetEvent(DropDownClosedEvent);
    }

    inline static constexpr Members::Property<bool> IsDropDownOpenProperty{"IsDropDownOpen"};
    inline static constexpr Members::Property<double> MaxDropDownHeightProperty{"MaxDropDownHeight"};
    inline static constexpr Members::Property<bool> IsEditableProperty{"IsEditable"};
    inline static constexpr Members::Property<bool> IsReadOnlyProperty{"IsReadOnly"};
    inline static constexpr Members::Property<Base::String> TextProperty{"Text"};
    inline static constexpr Members::Property<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr Members::ReadOnlyProperty<Base::String> SelectionBoxTextProperty{"SelectionBoxText"};
    inline static constexpr Members::ReadOnlyProperty<Core::Value> SelectionBoxItemProperty{"SelectionBoxItem"};

protected:
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;
    Base::Result<void> PrepareContainer(
        ItemContainer& container,
        const Base::Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainer(
        ItemContainer& container) noexcept override;
    void OnContainersChanged() noexcept override;
    Base::Result<void> OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    void* interactions_ = nullptr;
    TextBlock* selectionBox_ = nullptr;
    ContentPresenter* selectionPresenter_ =
        nullptr;
    TextBox* editableTextBox_ = nullptr;
    Primitives::Popup* popup_ = nullptr;
    FrameworkElement* dropDownBorder_ = nullptr;
    SelectionChangedHandler selectionChangedHandler_;
    DependencyPropertyChangedEventHandler
        dropDownChangedHandler_;
    DependencyPropertyChangedEventHandler
        maxDropDownHeightChangedHandler_;
    DependencyPropertyChangedEventHandler
        editableChangedHandler_;
    DependencyPropertyChangedEventHandler
        textChangedHandler_;
    DependencyPropertyChangedEventHandler
        foregroundChangedHandler_;
    RoutedEventHandler editableTextChangedHandler_;
    bool synchronizingEditableText_ = false;

    void OnSelectionChanged(
        Selector& selector,
        const SelectionChangedEvent& event)
        noexcept;
    void OnDropDownPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnMaxDropDownHeightPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnEditablePropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnTextPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnForegroundPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnEditableTextChanged(
        Base::Object* sender,
        const RoutedEventArgs& args) noexcept;
    Base::Result<void>
        UpdateSelectionBox() noexcept;
    Base::Result<void>
        UpdateEditableVisualState() noexcept;
    void SynchronizeContainers() noexcept;
    std::uint32_t FindContainerIndex(
        Base::Object* source) const noexcept;
};



} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::SelectionMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("SelectionMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "SelectionMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core

namespace Aero::Controls {

enum class GridViewColumnHeaderRole : std::uint8_t {
    Normal = 0U,
    Floating,
    Padding
};

// Standard GridView header container. The presenter owns header generation
// and interaction; this control supplies the WPF-visible Role state used by
// the default templates.
class AERO_API GridViewColumnHeader final
    : public ContentControl {
    AERO_DECLARE_TYPE(GridViewColumnHeader, ContentControl)
public:
    GridViewColumnHeader() noexcept
        : ContentControl(StaticTypeId()) {}

    GridViewColumnHeaderRole Role() const noexcept {
        return GetValueOr(
            RoleProperty, GridViewColumnHeaderRole::Normal);
    }
    Base::Result<void> SetRole(
        GridViewColumnHeaderRole value) noexcept {
        return SetValue(RoleProperty, value);
    }

    inline static constexpr Members::Property<GridViewColumnHeaderRole> RoleProperty{"Role"};
};

class AERO_API GridViewColumn final
    : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    GridViewColumn() noexcept
        : DependencyObject(StaticTypeId()) {}
    Base::StringView Header() const noexcept;
    Base::Result<void> SetHeader(
        Base::StringView value) noexcept;
    double Width() const noexcept;
    Base::Result<void> SetWidth(
        double value) noexcept;
    Base::Ref<DataTemplate>
        CellTemplate() const noexcept;
    Base::Result<void> SetCellTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::Ref<DataTemplate>
        HeaderTemplate() const noexcept;
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::StringView DisplayMemberPath()
        const noexcept;
    Base::Result<void> SetDisplayMemberPath(
        Base::StringView value) noexcept;
    Base::Ref<Aero::Data::Binding>
        DisplayMemberBinding() const noexcept;
    Base::Result<void> SetDisplayMemberBinding(
        Base::Ref<Aero::Data::Binding> value) noexcept;
    Base::Ref<Style> HeaderContainerStyle() const noexcept {
        return GetValueOr(
            HeaderContainerStyleProperty,
            Base::Ref<Style>{});
    }
    Base::Result<void> SetHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        return SetValue(
            HeaderContainerStyleProperty, std::move(value));
    }

    inline static constexpr Members::Property<Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<double> WidthProperty{"Width"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> CellTemplateProperty{"CellTemplate"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<Base::String> DisplayMemberPathProperty{"DisplayMemberPath"};
    inline static constexpr Members::Property<Base::Ref<Aero::Data::Binding>> DisplayMemberBindingProperty{"DisplayMemberBinding"};
    inline static constexpr Members::Property<Base::Ref<Style>> HeaderContainerStyleProperty{"HeaderContainerStyle"};
};

class AERO_API GridView final
    : public Base::Object {
    AERO_DECLARE_TYPE(GridView, Base::Object)
public:
    GridView() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<GridViewColumn>>
        Columns() const noexcept {
        return {
            columns_.Data(),
            columns_.Size()};
    }
    Base::Result<void> AddColumn(
        Base::Ref<GridViewColumn> column)
        noexcept;
    void ClearColumns() noexcept {
        columns_.Clear();
    }
    Base::Ref<Style> ColumnHeaderContainerStyle() const noexcept {
        return columnHeaderContainerStyle_;
    }
    Base::Result<void> SetColumnHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        columnHeaderContainerStyle_ = std::move(value);
        return {};
    }

private:
    Base::Vector<Base::Ref<GridViewColumn>>
        columns_;
    Base::Ref<Style> columnHeaderContainerStyle_;
};

// Hosts GridView column headers inside the ListView ScrollViewer template.
// The column collection is normally supplied by a template binding from the
// owning ListView's GridView and is consumed by the view implementation.
class AERO_API GridViewHeaderRowPresenter final
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewHeaderRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewHeaderRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    bool AllowsColumnReorder() const noexcept {
        return GetValueOr(AllowsColumnReorderProperty, false);
    }
    Base::Result<void> SetAllowsColumnReorder(bool value) noexcept {
        return SetValue(AllowsColumnReorderProperty, value);
    }

    inline static constexpr Members::Property<bool> AllowsColumnReorderProperty{"AllowsColumnReorder"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderContainerStyleProperty{"ColumnHeaderContainerStyle"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderContextMenuProperty{"ColumnHeaderContextMenu"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderTemplateProperty{"ColumnHeaderTemplate"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderTemplateSelectorProperty{"ColumnHeaderTemplateSelector"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderToolTipProperty{"ColumnHeaderToolTip"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnsProperty{"Columns"};
};

// The row counterpart to GridViewHeaderRowPresenter. It is instantiated by
// ListViewItem templates and receives the active GridView columns/content
// during ListView container realization.
class AERO_API GridViewRowPresenter final
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnsProperty{"Columns"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ContentProperty{"Content"};
};

class AERO_API ListViewItem final
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)
public:
    ListViewItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};

class AERO_API ListView final
    : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)
public:
    ListView() noexcept
        : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Base::Ref<GridView> View() const noexcept;
    Base::Result<void> SetView(
        Base::Ref<GridView> value) noexcept;

    inline static constexpr Members::Property<Base::Ref<GridView>> ViewProperty{"View"};

protected:
    Base::Result<void>
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    Base::Result<void>
        SynchronizeColumnHeaders() noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::GridViewColumnHeaderRole> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("GridViewColumnHeaderRole");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "GridViewColumnHeaderRole";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core

namespace Aero::Controls {

class VisualStateManager;

class AERO_API TreeViewItem
    : public ItemContainer,
      public IItemsSource {
    AERO_DECLARE_TYPE(TreeViewItem, ItemContainer)
public:
    TreeViewItem() noexcept;
    ~TreeViewItem() override;

    Base::StringView Header() const noexcept;
    Base::Result<void> SetHeader(
        Base::StringView value) noexcept;
    Base::StringView Icon() const noexcept;
    Base::Result<void> SetIcon(
        Base::StringView value) noexcept;
    Base::Ref<DataTemplate>
        HeaderTemplate() const noexcept;
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    bool IsExpanded() const noexcept;
    Base::Result<void> SetIsExpanded(
        bool value) noexcept;
    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(
        bool value) noexcept;
    bool HasItems() const noexcept {
        return GetValueOr(HasItemsProperty, false);
    }

    ItemsCollection& GetItems() noexcept {
        return items_;
    }
    const ItemsCollection& GetItems() const noexcept {
        return items_;
    }
    std::uint32_t Count() const noexcept override {
        return items_.Count();
    }
    Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept override {
        return items_.ItemAt(index);
    }
    Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        return items_.TryAddItemsChanged(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        return items_.RemoveItemsChanged(handler);
    }

    inline static constexpr Members::Property<Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::String> IconProperty{"Icon"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
    inline static constexpr Members::ReadOnlyProperty<bool> HasItemsProperty{"HasItems"};
    // WPF item hosts accept an ItemsPanelTemplate from a style. The current
    // tree realization retains the value while it supplies its own host.
    inline static constexpr Members::Property<Base::Ref<ItemsPanelTemplate>> ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> UnselectedEvent{"Unselected"};

protected:
    explicit TreeViewItem(TypeId runtimeType) noexcept;
    Base::Result<void>
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    ItemsCollection items_;
    TextBlock* headerText_ = nullptr;
    TextBlock* iconText_ = nullptr;
    TextBlock* expanderGlyph_ = nullptr;
    ItemsControl* childItems_ = nullptr;
    DependencyPropertyChangedEventHandler
        headerChangedHandler_;
    DependencyPropertyChangedEventHandler
        iconChangedHandler_;
    DependencyPropertyChangedEventHandler
        expandedChangedHandler_;
    DependencyPropertyChangedEventHandler
        selectedChangedHandler_;
    ItemsChangedHandler itemsChangedHandler_;

    void OnHeaderChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnExpandedChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnSelectedChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<void>
        SynchronizeTemplate() noexcept;
};

class AERO_API TreeView final
    : public ItemsControl {
    AERO_DECLARE_TYPE(TreeView, ItemsControl)
public:
    TreeView() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~TreeView() override;

    Base::Ref<Base::Object>
        SelectedItem() const noexcept;
    Base::Result<bool> SelectItem(
        TreeViewItem* item) noexcept;
    inline static constexpr Members::ReadOnlyProperty<Base::Ref<Base::Object>> SelectedItemProperty{"SelectedItem"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectedItemChangedEvent{"SelectedItemChanged"};

protected:
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    void* interactions_ =
        nullptr;
    VisualStateManager* states_ = nullptr;
};


} // namespace Aero::Controls

namespace Aero::Controls {

enum class ScrollUnit : std::uint8_t { Item = 0U, Pixel };
enum class VirtualizationMode : std::uint8_t { Standard = 0U, Recycling };

// WPF attached-property owner shared by all virtualizing panels. The current
// panel implementation is pixel-based; exposing this owner preserves the
// authored contract while item-unit realization is added.
class AERO_API VirtualizingPanel : public Base::Object {
    AERO_DECLARE_TYPE(VirtualizingPanel, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Members::AttachedProperty<ScrollUnit> ScrollUnitProperty{"ScrollUnit"};
    inline static constexpr Members::AttachedProperty<VirtualizationMode> VirtualizationModeProperty{"VirtualizationMode"};
};

class AERO_API VirtualizingStackPanel final
    : public Panel,
      public IScrollInfo {
    AERO_DECLARE_TYPE(VirtualizingStackPanel, Panel)
public:
    VirtualizingStackPanel() noexcept;
    ~VirtualizingStackPanel() override;

    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    std::uint32_t OverscanCount() const noexcept;
    Base::Result<void> SetOverscanCount(
        std::uint32_t value) noexcept;
    double EstimatedItemExtent() const noexcept;
    Base::Result<void> SetEstimatedItemExtent(
        double value) noexcept;

    std::uint32_t VisibleFirstIndex() const noexcept {
        return visibleFirstIndex_;
    }
    std::uint32_t VisibleCount() const noexcept {
        return visibleCount_;
    }
    std::uint32_t RealizedFirstIndex() const noexcept {
        return desiredFirstIndex_;
    }
    std::uint32_t RealizedCount() const noexcept {
        return desiredCount_;
    }
    double ItemExtent(
        std::uint32_t index) const noexcept;
    double ItemOffset(
        std::uint32_t index) const noexcept;

    ScrollData Data() const noexcept override {
        return data_;
    }
    Base::Result<bool> SetViewport(
        Size viewport) noexcept override;
    Base::Result<bool> SetHorizontalOffset(
        double value) noexcept override;
    Base::Result<bool> SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;

    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<std::uint32_t> OverscanCountProperty{"OverscanCount"};
    inline static constexpr Members::Property<double> EstimatedItemExtentProperty{"EstimatedItemExtent"};

protected:
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    friend class ItemContainerGenerator;
    friend class Detail::ItemContainerGeneratorImpl;

    ItemContainerGenerator* generator_ = nullptr;
    Base::Vector<double> itemExtents_;
    Base::Vector<double> extentTree_;
    ScrollData data_;
    double crossExtent_ = 0.0;
    double estimatedItemExtent_ = 24.0;
    std::uint32_t overscanCount_ = 2U;
    Orientation orientation_ = Orientation::Vertical;
    std::uint32_t visibleFirstIndex_ = 0U;
    std::uint32_t visibleCount_ = 0U;
    std::uint32_t desiredFirstIndex_ = 0U;
    std::uint32_t desiredCount_ = 0U;

    Base::Result<void> AttachGenerator(
        ItemContainerGenerator& generator,
        std::uint32_t itemCount) noexcept;
    void DetachGenerator(
        ItemContainerGenerator& generator) noexcept;
    Base::Result<void> OnItemsChanged(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Base::Result<void> ResizeExtentCache(
        std::uint32_t itemCount) noexcept;
    Base::Result<void> ApplyExtentDelta(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Base::Result<void> UpdateRealization(
        bool notifyGenerator) noexcept;
    void CalculateRealizationRange() noexcept;
    std::uint32_t ItemIndexAtOffset(
        double offset) const noexcept;
    double MainOffset() const noexcept;
    double MainViewport() const noexcept;
    double MainExtent() const noexcept;
    void SetMainOffset(double value) noexcept;
    void SetMainExtent(double value) noexcept;
    void ClampOffsets() noexcept;
    double ExtentForIndex(
        std::uint32_t index) const noexcept;
    Base::Result<void> RebuildExtentTree() noexcept;
    void AddExtentDeviation(
        std::uint32_t index,
        double delta) noexcept;
    double PrefixDeviation(
        std::uint32_t count) const noexcept;
    void SetMeasuredExtent(
        std::uint32_t index,
        double value) noexcept;
    Base::Result<bool> SetMainScrollOffset(
        double value) noexcept;
    Base::Result<bool> SetCrossScrollOffset(
        double value) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::ScrollUnit> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ScrollUnit");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ScrollUnit";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::VirtualizationMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("VirtualizationMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "VirtualizationMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
