#pragma once

#include <Aero/Controls/ItemCollection.hpp>
#include <Aero/Controls/ItemsPanelTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/DataTemplateSelector.hpp>
#include <Aero/Style.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <utility>


namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;
class VirtualizingStackPanel;
class ItemContainerGenerator;
struct ItemContainerGeneratorRuntime;

class AERO_GUI_API ItemsControl : public Control {
    AERO_DECLARE_TYPE(ItemsControl, Control)
public:

    ItemsControl() noexcept;
    ~ItemsControl() override;

    ItemCollection& GetItems() noexcept {
        return items_;
    }
    const ItemCollection& GetItems() const noexcept {
        return items_;
    }
    Ref<Base::Object> GetItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Ref<Base::Object>{});
    }
    bool GetHasItems() const noexcept {
        return GetValueOr(HasItemsProperty, false);
    }
    std::uint32_t GetCount() const noexcept;
    Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept;
    void SetItemsSource(
        Ref<Base::Object> source) noexcept {
        SetValue(ItemsSourceProperty, std::move(source));
    }
    std::uint32_t GetAlternationCount() const noexcept {
        return GetValueOr(AlternationCountProperty, 0U);
    }
    void SetAlternationCount(
        std::uint32_t value) noexcept {
        SetValue(AlternationCountProperty, value);
    }
    StringView GetDisplayMemberPath() const noexcept {
        return GetValueOr(
            DisplayMemberPathProperty,
            StringView{});
    }
    void SetDisplayMemberPath(StringView value) noexcept {
        SetValue(DisplayMemberPathProperty, value);
    }

    const DataTemplate* GetItemTemplate() const noexcept {
        return itemTemplate_;
    }
    void SetItemTemplate(
        Ref<DataTemplate> value) noexcept {
        SetValue(ItemTemplateProperty, std::move(value));
    }
    void SetItemTemplate(
        const DataTemplate* value) noexcept {
        Ref<DataTemplate> retained;
        if (value != nullptr) {
            retained = Ref<DataTemplate>::TryFromBorrowed(
                *const_cast<DataTemplate*>(value));
            if (!retained) return;
        }
        SetItemTemplate(std::move(retained));
    }
    const DataTemplateSelector* GetItemTemplateSelector() const noexcept {
        return itemTemplateSelector_;
    }
    void SetItemTemplateSelector(
        Ref<DataTemplateSelector> value) noexcept {
        SetValue(ItemTemplateSelectorProperty, std::move(value));
    }
    virtual Ref<DataTemplate> ResolveItemTemplate(
        const Ref<Base::Object>& item,
        std::uint32_t index) const noexcept;
    const ItemsPanelTemplate* GetItemsPanel() const noexcept {
        return itemsPanel_;
    }
    void SetItemsPanel(
        Ref<ItemsPanelTemplate> value) noexcept {
        SetValue(ItemsPanelProperty, std::move(value));
    }
    void SetItemsPanel(
        const ItemsPanelTemplate* value) noexcept {
        Ref<ItemsPanelTemplate> retained;
        if (value != nullptr) {
            retained = Ref<ItemsPanelTemplate>::TryFromBorrowed(
                *const_cast<ItemsPanelTemplate*>(value));
            if (!retained) return;
        }
        SetItemsPanel(std::move(retained));
    }
    const Style* GetItemContainerStyle() const noexcept {
        return itemContainerStyle_;
    }
    void SetItemContainerStyle(
        Ref<Style> value) noexcept {
        SetValue(ItemContainerStyleProperty, std::move(value));
    }
    void SetItemContainerStyle(
        const Style* value) noexcept {
        Ref<Style> retained;
        if (value != nullptr) {
            retained = Ref<Style>::TryFromBorrowed(
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
    ItemContainerGenerator* GetItemContainerGenerator() const noexcept {
        return generator_;
    }

    inline static constexpr ReadOnlyDependencyProperty<std::uint32_t> ItemCountProperty{"ItemCount"};
    inline static constexpr ReadOnlyDependencyProperty<bool> HasItemsProperty{"HasItems"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ItemsSourceProperty{"ItemsSource"};
    inline static constexpr DependencyProperty<std::uint32_t> AlternationCountProperty{"AlternationCount"};
    inline static constexpr DependencyProperty<String> DisplayMemberPathProperty{"DisplayMemberPath"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr DependencyProperty<Ref<DataTemplateSelector>> ItemTemplateSelectorProperty{"ItemTemplateSelector"};
    inline static constexpr DependencyProperty<Ref<ItemsPanelTemplate>> ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr DependencyProperty<Ref<Style>> ItemContainerStyleProperty{"ItemContainerStyle"};

protected:
    explicit ItemsControl(TypeId runtimeType) noexcept;
    ItemContainerGenerator* AttachedGenerator() const noexcept {
        return generator_;
    }
    virtual Result<
        Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item) noexcept;
    virtual Result<void> PrepareContainer(
        FrameworkElement& container,
        const Ref<Base::Object>& item,
        std::uint32_t index) noexcept;
    virtual void ClearContainer(
        FrameworkElement& container) noexcept;
    virtual void OnContainersChanged() noexcept {}
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class ItemContainerGenerator;
    friend struct ItemContainerGeneratorRuntime;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif

    ItemCollection items_;
    Collections::IItemsSource* source_ = nullptr;
    const DataTemplate* itemTemplate_ = nullptr;
    const DataTemplateSelector* itemTemplateSelector_ = nullptr;
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
    void SetItemTemplateSelectorCore(
        const DataTemplateSelector* value) noexcept;
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
