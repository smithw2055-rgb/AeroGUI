#pragma once

#include <Aero/Collections.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Style.hpp>
#include <Aero/Data.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <utility>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;
using ItemsChangeAction = Collections::ItemsChangeAction;
using ItemsChangedEvent = Collections::ItemsChangedEvent;
using ItemsChangedHandler = Collections::ItemsChangedHandler;

class AERO_API ItemCollection : public Collections::IItemsSource {
public:
    std::uint32_t GetCount() const noexcept override {
        return items_.Size();
    }
    Base::Ref<Base::Object> GetItem(
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
    void AddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        changed_.Add(handler);
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

// WPF AlternationConverter selects an authored value by alternation index.
// Keeping object values intact lets a binding later return brushes, strings,
// and other resources without lossy text conversion.
class AERO_API AlternationConverter : public Base::Object {
    AERO_DECLARE_TYPE(AlternationConverter, Base::Object)
public:
    AlternationConverter() noexcept
        : values_(&Base::GetDefaultAllocator()) {}
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<Base::Object>> GetValues() const noexcept {
        return values_.AsSpan();
    }
    Base::Result<void> AddValue(
        Base::Ref<Base::Object> value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AlternationConverter values cannot be null");
        }
        return values_.PushBack(std::move(value));
    }
    void ClearValues() noexcept { values_.Clear(); }
private:
    Base::Vector<Base::Ref<Base::Object>> values_;
};

AERO_API Base::Result<void> AddBoxedItem(
    Collections::ObservableCollection& source,
    Value value) noexcept;
AERO_API Base::Result<void> AddBoxedStringItem(
    Collections::ObservableCollection& source,
    Base::StringView value) noexcept;

class AERO_API ItemsPanelTemplate : public Base::Object {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, Base::Object)
public:
    struct Impl;

    ItemsPanelTemplate() noexcept;
    ~ItemsPanelTemplate() noexcept override;
    ItemsPanelTemplate(const ItemsPanelTemplate&) = delete;
    ItemsPanelTemplate& operator=(const ItemsPanelTemplate&) = delete;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct Impl;
    void* state_ = nullptr;
};

class AERO_API ItemsPresenter : public Decorator {
    AERO_DECLARE_TYPE(ItemsPresenter, Decorator)
public:
    ItemsPresenter() noexcept
        : Decorator(StaticTypeId()) {}
    ~ItemsPresenter() override = default;
    Panel* GetItemsHost() const noexcept;
    void SetItemsHost(
        const Base::Ref<Base::Object>& owner,
        Panel& panel) noexcept;
};

class VirtualizingStackPanel;

class AERO_API ItemsControl : public Control {
    AERO_DECLARE_TYPE(ItemsControl, Control)
public:
    struct Impl;

    ItemsControl() noexcept;
    ~ItemsControl() override;

    ItemCollection& GetItems() noexcept {
        return items_;
    }
    const ItemCollection& GetItems() const noexcept {
        return items_;
    }
    Base::Ref<Base::Object> GetItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Base::Ref<Base::Object>{});
    }
    bool GetHasItems() const noexcept {
        return GetValueOr(HasItemsProperty, false);
    }
    std::uint32_t GetCount() const noexcept;
    Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept;
    void SetItemsSource(
        Base::Ref<Base::Object> source) noexcept {
        SetValue(ItemsSourceProperty, std::move(source));
    }
    std::uint32_t GetAlternationCount() const noexcept {
        return GetValueOr(AlternationCountProperty, 0U);
    }
    void SetAlternationCount(
        std::uint32_t value) noexcept {
        SetValue(AlternationCountProperty, value);
    }

    const DataTemplate* GetItemTemplate() const noexcept {
        return itemTemplate_;
    }
    void SetItemTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        SetValue(ItemTemplateProperty, std::move(value));
    }
    void SetItemTemplate(
        const DataTemplate* value) noexcept {
        Base::Ref<DataTemplate> retained;
        if (value != nullptr) {
            retained = Base::Ref<DataTemplate>::TryFromBorrowed(
                *const_cast<DataTemplate*>(value));
            if (!retained) return;
        }
        SetItemTemplate(std::move(retained));
    }
    const ItemsPanelTemplate* GetItemsPanel() const noexcept {
        return itemsPanel_;
    }
    void SetItemsPanel(
        Base::Ref<ItemsPanelTemplate> value) noexcept {
        SetValue(ItemsPanelProperty, std::move(value));
    }
    void SetItemsPanel(
        const ItemsPanelTemplate* value) noexcept {
        Base::Ref<ItemsPanelTemplate> retained;
        if (value != nullptr) {
            retained = Base::Ref<ItemsPanelTemplate>::TryFromBorrowed(
                *const_cast<ItemsPanelTemplate*>(value));
            if (!retained) return;
        }
        SetItemsPanel(std::move(retained));
    }
    const Style* GetItemContainerStyle() const noexcept {
        return itemContainerStyle_;
    }
    void SetItemContainerStyle(
        Base::Ref<Style> value) noexcept {
        SetValue(ItemContainerStyleProperty, std::move(value));
    }
    void SetItemContainerStyle(
        const Style* value) noexcept {
        Base::Ref<Style> retained;
        if (value != nullptr) {
            retained = Base::Ref<Style>::TryFromBorrowed(
                *const_cast<Style*>(value));
            if (!retained) return;
        }
        SetItemContainerStyle(std::move(retained));
    }

    void AddItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        return changed_.Remove(handler);
    }
    Panel* GetItemsHost() const noexcept {
        return itemsHost_;
    }
    std::uint32_t GetRealizedItemCount() const noexcept;
    std::uint32_t GetCreatedContainerCount() const noexcept;
    std::uint32_t GetRecycledContainerUseCount() const noexcept;

    inline static constexpr ReadOnlyDependencyProperty<std::uint32_t> ItemCountProperty{"ItemCount"};
    inline static constexpr ReadOnlyDependencyProperty<bool> HasItemsProperty{"HasItems"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ItemsSourceProperty{"ItemsSource"};
    inline static constexpr DependencyProperty<std::uint32_t> AlternationCountProperty{"AlternationCount"};
    inline static constexpr DependencyProperty<Base::Ref<DataTemplate>> ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr DependencyProperty<Base::Ref<ItemsPanelTemplate>> ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr DependencyProperty<Base::Ref<Style>> ItemContainerStyleProperty{"ItemContainerStyle"};

protected:
    explicit ItemsControl(TypeId runtimeType) noexcept;
    ItemContainerGenerator* AttachedGenerator() const noexcept {
        return generator_;
    }
    virtual Base::Result<
        Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item) noexcept;
    virtual Base::Result<void> PrepareContainer(
        FrameworkElement& container,
        const Base::Ref<Base::Object>& item,
        std::uint32_t index) noexcept;
    virtual void ClearContainer(
        FrameworkElement& container) noexcept;
    virtual void OnContainersChanged() noexcept {}
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class ItemContainerGenerator;
    friend struct Impl;
    friend struct ::Aero::Visual::Impl;
    ItemCollection items_;
    Collections::IItemsSource* source_ = nullptr;
    const DataTemplate* itemTemplate_ = nullptr;
    const ItemsPanelTemplate* itemsPanel_ = nullptr;
    const Style* itemContainerStyle_ = nullptr;
    ItemContainerGenerator* generator_ = nullptr;
    Panel* itemsHost_ = nullptr;
    ItemsChangedHandler changed_;
    ItemsChangedHandler localHandler_;
    ItemsChangedHandler sourceHandler_;

    void SetItemsSourceCore(
        Collections::IItemsSource* source) noexcept;
    void SetItemsSourceBorrowed(
        Collections::IItemsSource* source) noexcept;
    void SetItemTemplateCore(
        const DataTemplate* value) noexcept;
    void SetItemsPanelCore(
        const ItemsPanelTemplate* value) noexcept;
    void SetItemContainerStyleCore(
        const Style* value) noexcept;
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

    Value GetHeader() const noexcept {
        return GetValueOr(
            HeaderProperty,
            Value::NullObject(Meta::TypeOf<Base::Object>()));
    }
    void SetHeader(Value value) noexcept {
        SetValue(HeaderProperty, std::move(value));
    }
    Base::Result<void> SetHeader(Base::StringView value) noexcept {
        Base::Result<Value> boxed = Value::TryFromString(
            Meta::TypeOf<Base::String>(), value);
        if (!boxed) return boxed.GetStatus();
        SetHeader(std::move(boxed).Value());
        return {};
    }
    Base::Ref<DataTemplate> GetHeaderTemplate() const noexcept {
        return GetValueOr(
            HeaderTemplateProperty, Base::Ref<DataTemplate>{});
    }
    void SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        SetValue(HeaderTemplateProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};

protected:
    explicit HeaderedItemsControl(TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
};

class AERO_API ItemContainerGenerator {
public:
    struct Impl;

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
    void SetRealizationRange(
        std::uint32_t firstIndex,
        std::uint32_t count) noexcept;

    std::uint32_t GetGeneratedCount() const noexcept;
    std::uint32_t GetFirstGeneratedIndex() const noexcept;
    std::uint32_t GetCreatedContainerCount() const noexcept;
    std::uint32_t GetRecycledContainerUseCount() const noexcept;
    FrameworkElement* ContainerFromIndex(
        std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Status LastError() const noexcept;

private:
    friend struct ::Aero::Controls::Control::Impl;
    friend struct Impl;

    ItemContainerGenerator() noexcept = default;

    void* impl_ = nullptr;
};
} // namespace Aero::Controls
