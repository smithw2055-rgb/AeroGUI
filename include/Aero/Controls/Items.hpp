#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Value.hpp>
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Style.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Decorators.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

#include <utility>

namespace Aero::Controls {

class Panel;
class VirtualizingPanel;
class VirtualizingStackPanel;
class ItemContainerGenerator;
class ItemsControl;
class ItemContainerGeneratorFactory;

using ItemsChangeAction = Collections::ItemsChangeAction;
using ItemsChangedEvent = Collections::ItemsChangedEvent;
using ItemsChangedHandler = Collections::ItemsChangedHandler;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;

// ============================================================================
// AlternationConverter
// ============================================================================

class AERO_GUI_API AlternationConverter : public Data::IValueConverter {
    AERO_DECLARE_TYPE(AlternationConverter, Data::IValueConverter)

public:
    AlternationConverter() noexcept : values_(&Base::GetDefaultAllocator()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Span<const Ref<Base::Object>> GetValues() const noexcept { return values_.AsSpan(); }
    void AddValue(Ref<Base::Object> value) noexcept {
        if (value) { (void)values_.PushBack(std::move(value)); }
    }
    void ClearValues() noexcept { values_.Clear(); }

    Result<Value> Convert(const Value& value, const Value& parameter) noexcept override;
    Result<Value> ConvertBack(const Value& value, const Value& parameter) noexcept override;

private:
    Base::Vector<Ref<Base::Object>> values_;
};

// ============================================================================
// ItemCollection
// ============================================================================

class AERO_GUI_API ItemCollection : public Collections::IItemsSource {
public:
    std::uint32_t GetCount() const noexcept override { return items_.Size(); }
    Ref<Base::Object> GetItem(std::uint32_t index) const noexcept override;
    void Add(Ref<Base::Object> item) noexcept;
    void Insert(std::uint32_t index, Ref<Base::Object> item) noexcept;
    Result<Ref<Base::Object>> RemoveAt(std::uint32_t index) noexcept;
    Result<void> Replace(std::uint32_t index, Ref<Base::Object> item) noexcept;
    Result<void> Move(std::uint32_t oldIndex, std::uint32_t newIndex) noexcept;
    void Reset() noexcept;
    Result<void> Reset(Span<const Ref<Base::Object>> items) noexcept;
    void AddItemsChanged(const ItemsChangedHandler& handler) noexcept override { changed_.Add(handler); }
    bool RemoveItemsChanged(const ItemsChangedHandler& handler) noexcept override { return changed_.Remove(handler); }

private:
    Base::Vector<Ref<Base::Object>> items_;
    ItemsChangedHandler changed_;
    void Notify(const ItemsChangedEvent& event) noexcept;
};

AERO_GUI_API void AddBoxedItem(Collections::ObservableCollection<Base::Object>& source, Value value) noexcept;
AERO_GUI_API void AddBoxedStringItem(Collections::ObservableCollection<Base::Object>& source,
    StringView value) noexcept;

// ============================================================================
// ItemsPanelTemplate
// ============================================================================

class AERO_GUI_API ItemsPanelTemplate : public ::Aero::FrameworkTemplate {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, FrameworkTemplate)

public:
    ItemsPanelTemplate() noexcept;
    ~ItemsPanelTemplate() noexcept override;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    ItemsPanelTemplate(const ItemsPanelTemplate&) = delete;
    ItemsPanelTemplate& operator=(const ItemsPanelTemplate&) = delete;

    ResourceDictionary& GetResources() noexcept override;
    const ResourceDictionary& GetResources() const noexcept override;
    void SetResources(Ref<ResourceDictionary> value) noexcept override;
    bool GetIsSealed() const noexcept override;
};

// ============================================================================
// ItemsPresenter
// ============================================================================

class AERO_GUI_API ItemsPresenter : public Decorator {
    AERO_DECLARE_TYPE(ItemsPresenter, Decorator)

public:
    ItemsPresenter() noexcept : Decorator(StaticTypeId()) {}
    ~ItemsPresenter() override = default;
    Panel* GetItemsHost() const noexcept;
    void SetItemsHost(const Ref<Base::Object>& owner, Panel& panel) noexcept;
};

// ============================================================================
// ItemsControl
// ============================================================================

class AERO_GUI_API ItemsControl : public Control {
    AERO_DECLARE_TYPE(ItemsControl, Control)

public:
    ItemsControl() noexcept;
    ~ItemsControl() override;

    virtual Ref<DataTemplate> ResolveItemTemplate(const Ref<Base::Object>& item, std::uint32_t index) const noexcept;
    virtual Result<void> PrepareContainerForItemOverride(FrameworkElement& container, const Ref<Base::Object>& item,
        std::uint32_t index) noexcept;
    virtual void OnItemsChanged(const ItemsChangedEvent& event) noexcept {
        static_cast<void>(event);
        InvalidateMeasure();
        InvalidateArrange();
    }
    void AddItemsChanged(const ItemsChangedHandler& handler) noexcept { changed_.Add(handler); }
    bool RemoveItemsChanged(const ItemsChangedHandler& handler) noexcept { return changed_.Remove(handler); }
    void AssignItemsSource(Ref<Base::Object> source) noexcept;
    void PublishReset() noexcept;

    ItemCollection& GetItems() noexcept { return items_; }
    const ItemCollection& GetItems() const noexcept { return items_; }
    Ref<Base::Object> GetItemsSource() const noexcept { return GetValue(ItemsSourceProperty); }
    void SetItemsSource(Ref<Base::Object> source) noexcept { SetValue(ItemsSourceProperty, std::move(source)); }
    bool GetHasItems() const noexcept { return GetValue(HasItemsProperty); }
    std::uint32_t GetCount() const noexcept;
    Ref<Base::Object> GetItem(std::uint32_t index) const noexcept;
    std::uint32_t GetAlternationCount() const noexcept { return GetValue(AlternationCountProperty); }
    void SetAlternationCount(std::uint32_t value) noexcept { SetValue(AlternationCountProperty, value); }
    StringView GetDisplayMemberPath() const noexcept { return GetValue(DisplayMemberPathProperty); }
    void SetDisplayMemberPath(StringView value) noexcept { SetValue(DisplayMemberPathProperty, value); }
    const DataTemplate* GetItemTemplate() const noexcept { return itemTemplate_; }
    void SetItemTemplate(Ref<DataTemplate> value) noexcept { SetValue(ItemTemplateProperty, std::move(value)); }
    void SetItemTemplate(const DataTemplate* value) noexcept {
        Ref<DataTemplate> retained;
        if (value != nullptr) {
            retained = Ref<DataTemplate>::TryFromBorrowed(
                *const_cast<DataTemplate*>(value));
            if (!retained) return;
        }
        SetItemTemplate(std::move(retained));
    }
    const DataTemplateSelector* GetItemTemplateSelector() const noexcept { return itemTemplateSelector_; }
    void SetItemTemplateSelector(Ref<DataTemplateSelector> value) noexcept {
        SetValue(ItemTemplateSelectorProperty, std::move(value));
    }
    virtual bool IsItemItsOwnContainerOverride(Base::Object* item) const noexcept {
        static_cast<void>(item);
        return false;
    }
    virtual Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept;
    virtual void ClearContainerForItemOverride(FrameworkElement& container) noexcept;
    virtual Ref<DataTemplate> GetTemplateForItemOverride(const Ref<Base::Object>& item,
        std::uint32_t index) const noexcept;
    const ItemsPanelTemplate* GetItemsPanel() const noexcept { return itemsPanel_; }
    void SetItemsPanel(Ref<ItemsPanelTemplate> value) noexcept { SetValue(ItemsPanelProperty, std::move(value)); }
    void SetItemsPanel(const ItemsPanelTemplate* value) noexcept {
        Ref<ItemsPanelTemplate> retained;
        if (value != nullptr) {
            retained = Ref<ItemsPanelTemplate>::TryFromBorrowed(
                *const_cast<ItemsPanelTemplate*>(value));
            if (!retained) return;
        }
        SetItemsPanel(std::move(retained));
    }
    const Style* GetItemContainerStyle() const noexcept { return itemContainerStyle_; }
    void SetItemContainerStyle(Ref<Style> value) noexcept { SetValue(ItemContainerStyleProperty, std::move(value)); }
    void SetItemContainerStyle(const Style* value) noexcept {
        Ref<Style> retained;
        if (value != nullptr) {
            retained = Ref<Style>::TryFromBorrowed(
                *const_cast<Style*>(value));
            if (!retained) return;
        }
        SetItemContainerStyle(std::move(retained));
    }
    Panel* GetItemsHost() const noexcept { return itemsHost_; }
    std::uint32_t GetRealizedItemCount() const noexcept;
    std::uint32_t GetCreatedContainerCount() const noexcept;
    std::uint32_t GetRecycledContainerUseCount() const noexcept;
    ItemContainerGenerator* GetItemContainerGenerator() const noexcept { return generator_; }
    void SetItemsSourceCore(Collections::IItemsSource* source) noexcept;
    void SetItemTemplateCore(const DataTemplate* value) noexcept;
    void SetItemTemplateSelectorCore(const DataTemplateSelector* value) noexcept;
    void SetItemsPanelCore(const ItemsPanelTemplate* value) noexcept;
    void SetItemContainerStyleCore(const Style* value) noexcept;

    AERO_READONLY_PROPERTY(std::uint32_t, ItemCount);
    AERO_READONLY_PROPERTY(bool, HasItems);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ItemsSource);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, AlternationCount);
    AERO_DEPENDENCY_PROPERTY(String, DisplayMemberPath);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, ItemTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplateSelector>, ItemTemplateSelector);
    AERO_DEPENDENCY_PROPERTY(Ref<ItemsPanelTemplate>, ItemsPanel);
    AERO_DEPENDENCY_PROPERTY(Ref<Style>, ItemContainerStyle);

protected:
    explicit ItemsControl(TypeId runtimeType) noexcept;

    ItemContainerGenerator* AttachedGenerator() const noexcept { return generator_; }
    [[deprecated("Use PrepareContainerForItemOverride()")]]
    virtual Result<void> PrepareContainer(FrameworkElement& container, const Ref<Base::Object>& item,
        std::uint32_t index) noexcept;
    virtual void OnContainersChanged() noexcept {
        InvalidateMeasure();
        InvalidateArrange();
    }
    [[deprecated("Use OnItemsChanged()")]]
    virtual void OnItemsSourceCoreChanged() noexcept;
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    bool EnsureDefaultItemsPresenter() noexcept;

    [[deprecated("Use GetContainerForItemOverride()")]]
    virtual Result< Ref<FrameworkElement>> CreateContainer(const Ref<Base::Object>& item) noexcept;
    [[deprecated("Use ClearContainerForItemOverride()")]]
    virtual void ClearContainer(FrameworkElement& container) noexcept;
    Collections::IItemsSource* GetItemsSourceCore() const noexcept { return source_; }

private:
    friend class ItemContainerGenerator;

    void OnLocalChanged(const ItemsChangedEvent& event) noexcept;
    void OnSourceChanged(const ItemsChangedEvent& event) noexcept;
    void PublishItemCount() noexcept;

    ItemCollection items_;
    Collections::IItemsSource* source_ = nullptr;
    const DataTemplate* itemTemplate_ = nullptr;
    const DataTemplateSelector* itemTemplateSelector_ = nullptr;
    const ItemsPanelTemplate* itemsPanel_ = nullptr;
    const Style* itemContainerStyle_ = nullptr;
    ItemContainerGenerator* generator_ = nullptr;
    Panel* itemsHost_ = nullptr;
    Ref<Base::Object> defaultItemsPresenter_;
    ItemsChangedHandler changed_;
    ItemsChangedHandler localHandler_;
    ItemsChangedHandler sourceHandler_;
};

// ============================================================================
// ItemContainerGenerator
// ============================================================================

class AERO_GUI_API ItemContainerGenerator {
public:
    ~ItemContainerGenerator() noexcept;
    ItemContainerGenerator(const ItemContainerGenerator&) = delete;
    ItemContainerGenerator& operator=(const ItemContainerGenerator&) = delete;

    Result<void> Attach(ItemsControl& owner, Panel& itemsHost) noexcept;
    Result<void> AttachVirtualized(ItemsControl& owner, VirtualizingPanel& itemsHost) noexcept;
    Result<bool> Detach() noexcept;
    Result<void> Refresh() noexcept;
    void SetRealizationRange(std::uint32_t firstIndex, std::uint32_t count) noexcept;

    std::uint32_t GetGeneratedCount() const noexcept;
    std::uint32_t GetFirstGeneratedIndex() const noexcept;
    std::uint32_t GetCreatedContainerCount() const noexcept;
    std::uint32_t GetRecycledContainerUseCount() const noexcept;
    FrameworkElement* ContainerFromIndex(std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(const FrameworkElement& container) const noexcept;
    Ref<Base::Object> ItemFromContainer(const FrameworkElement& container) const noexcept;
    Base::Status LastError() const noexcept;

private:
    struct GeneratorState;
    friend struct GeneratorState;
    friend class ItemContainerGeneratorFactory;

    ItemContainerGenerator() noexcept = default;
    GeneratorState* state_ = nullptr;

    static bool OwnerHasGenerator(const ItemsControl& owner) noexcept;
    static void SetOwnerGenerator(ItemsControl& owner, ItemContainerGenerator* generator) noexcept;
    static void NotifyOwnerContainersChanged(ItemsControl& owner) noexcept;
    static Result<void> AttachHostGenerator(VirtualizingPanel& host, ItemContainerGenerator& generator,
        std::uint32_t itemCount) noexcept;
    static void DetachHostGenerator(VirtualizingPanel& host, ItemContainerGenerator& generator) noexcept;
    static void HostHandleItemsChanged(VirtualizingPanel& host, const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
};

// ============================================================================
// HeaderedItemsControl
// ============================================================================

class AERO_GUI_API HeaderedItemsControl : public ItemsControl {
    AERO_DECLARE_TYPE(HeaderedItemsControl, ItemsControl)

public:
    HeaderedItemsControl() noexcept : ItemsControl(StaticTypeId()) {}
    ~HeaderedItemsControl() override = default;

    Value GetHeader() const noexcept { return GetValue(HeaderProperty); }
    void SetHeader(Value value) noexcept { SetValue(HeaderProperty, std::move(value)); }
    void SetHeader(StringView value) noexcept {
        Result<Value> boxed = Value::TryFromString(Meta::TypeOf<String>(), value);
        if (!boxed) { AERO_ASSERT(false); return; }
        SetHeader(std::move(boxed).Value());
    }
    Ref<DataTemplate> GetHeaderTemplate() const noexcept { return GetValue(HeaderTemplateProperty); }
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept { SetValue(HeaderTemplateProperty, std::move(value)); }

    AERO_DEPENDENCY_PROPERTY(Value, Header);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, HeaderTemplate);

protected:
    explicit HeaderedItemsControl(Meta::TypeId runtimeType) noexcept : ItemsControl(runtimeType) {}

    virtual void OnHeaderChanged(const Value& oldHeader, const Value& newHeader) {
        (void)oldHeader;
        (void)newHeader;
    }
    virtual void OnHeaderTemplateChanged(const Ref<DataTemplate>& oldTemplate, const Ref<DataTemplate>& newTemplate) {
        (void)oldTemplate;
        (void)newTemplate;
    }
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override {
        ItemsControl::OnPropertyChanged(args);
        if (args.GetProperty() == HeaderProperty) {
            OnHeaderChanged(args.GetOldValue(), args.GetNewValue());
        } else if (args.GetProperty() == HeaderTemplateProperty) {
            const auto toTemplate = [](const Value& v) -> Ref<DataTemplate> {
                if (v.Kind() == Meta::ValueKind::Object && v.AsObject()) {
                    if (auto* dt = TryCast<DataTemplate>(v.AsObject().Get())) {
                        return Ref<DataTemplate>::FromBorrowed(*dt);
                    }
                }
                return {};
            };
            OnHeaderTemplateChanged(toTemplate(args.GetOldValue()), toTemplate(args.GetNewValue()));
        }
    }
};

} // namespace Aero::Controls
