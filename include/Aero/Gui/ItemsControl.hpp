#pragma once

#include <Aero/Gui/ItemCollection.hpp>
#include <Aero/Gui/ItemsPanelTemplate.hpp>
#include <Aero/Gui/DataTemplate.hpp>
#include <Aero/Gui/Style.hpp>
#include <Aero/Gui/BindingBase.hpp>
#include <Aero/Gui/Control.hpp>
#include <Aero/Gui/Panel.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <utility>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;
class VirtualizingStackPanel;
class ItemContainerGenerator;

class AERO_GUI_API ItemsControl : public Control {
    AERO_DECLARE_TYPE(ItemsControl, Control)
public:
    struct Access;

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
    friend struct Access;
    friend struct ::Aero::Media::Visual::Access;
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

} // namespace Aero::Controls
