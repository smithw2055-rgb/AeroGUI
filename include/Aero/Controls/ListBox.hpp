#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Events/ControlEventArgs.hpp>


namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::TypeId;
enum class SelectionMode : std::uint8_t {
    Single = 0U,
    Multiple,
    Extended,
};

class AERO_GUI_API ListBoxItem : public ContentControl {
    AERO_DECLARE_TYPE(ListBoxItem, ContentControl)
public:
    ListBoxItem() noexcept : ContentControl(StaticTypeId()) {}
    ~ListBoxItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;

    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
protected:
    explicit ListBoxItem(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
};

namespace Primitives {
class AERO_GUI_API Selector : public ItemsControl {
    AERO_DECLARE_TYPE(Selector, ItemsControl)
public:
    Selector() noexcept;
    ~Selector() override;

    SelectionMode GetSelectionMode() const noexcept;
    std::uint32_t GetSelectedIndex() const noexcept;
    Ref<Base::Object> GetSelectedItem() const noexcept;
    Ref<Base::Object> GetSelectedValue() const noexcept;
    StringView GetSelectedValuePath() const noexcept {
        return GetValueOr(
            SelectedValuePathProperty,
            StringView{});
    }
    Span<const std::uint32_t> GetSelectedIndices() const noexcept {
        return {selectedIndices_.Data(), selectedIndices_.Size()};
    }
    std::uint32_t GetSelectedCount() const noexcept {
        return selectedIndices_.Size();
    }
    bool GetIsSelected(std::uint32_t index) const noexcept;
    std::uint32_t GetIndexOfItem(
        const Base::Object* item) const noexcept;

    void SetSelectionMode(
        SelectionMode value) noexcept;
    void SetSelectedIndex(
        std::uint32_t index) noexcept;
    void SetSelectedItem(
        Ref<Base::Object> item) noexcept;
    void SetSelectedValue(
        Ref<Base::Object> value) noexcept;
    bool Select(
        std::uint32_t index) noexcept;
    bool Unselect(
        std::uint32_t index) noexcept;
    bool Toggle(
        std::uint32_t index) noexcept;
    bool SelectRange(
        std::uint32_t first,
        std::uint32_t last,
        bool preserveExisting = false) noexcept;
    void ClearSelection() noexcept;

    void AddSelectionChanged(
        const SelectionChangedHandler& handler) noexcept {
        selectionChanged_.Add(handler);
    }
    bool RemoveSelectionChanged(
        const SelectionChangedHandler& handler) noexcept {
        return selectionChanged_.Remove(handler);
    }
    Base::Status LastSelectionError() const noexcept {
        return lastSelectionError_;
    }

    inline static constexpr DependencyProperty<SelectionMode> SelectionModeProperty{"SelectionMode"};
    inline static constexpr DependencyProperty<std::uint32_t> SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> SelectedItemProperty{"SelectedItem"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> SelectedValueProperty{"SelectedValue"};
    inline static constexpr DependencyProperty<String> SelectedValuePathProperty{"SelectedValuePath"};
    inline static constexpr AttachedProperty<bool> IsSelectedProperty{"IsSelected"};
    // WPF Selector.SelectionChanged is a bubbling routed event. Keep the
    // strongly typed selection notification above for model-facing code while
    // also publishing the routed surface used by EventTrigger.
    inline static constexpr RoutedEvent<RoutedEventArgs> SelectionChangedRoutedEvent{"SelectionChanged"};
    UIElement::Event<RoutedEventArgs>
        SelectionChanged() noexcept {
        return GetEvent(
            SelectionChangedRoutedEvent);
    }

protected:
    explicit Selector(TypeId runtimeType) noexcept;
    Result<void> PrepareContainer(
        FrameworkElement& container,
        const Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainer(
        FrameworkElement& container) noexcept override;
    void OnContainersChanged() noexcept override;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
    Base::Vector<std::uint32_t> selectedIndices_;
    std::uint32_t primaryIndex_ = UINT32_MAX;
    std::uint32_t pendingIndex_ = UINT32_MAX;
    // A bound SelectedItem can arrive before a delayed ItemsSource. Retain it
    // until its matching item materializes instead of writing null back
    // through the TwoWay binding.
    Ref<Base::Object> pendingSelectedItem_;
    SelectionChangedHandler selectionChanged_;
    ItemsChangedHandler itemsChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Base::Status lastSelectionError_;
    DependencyPropertyHandle activeProperty_;
    bool synchronizingProperties_ = false;

    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    Result<bool> ApplySelection(
        Span<const std::uint32_t> indices,
        std::uint32_t primaryIndex) noexcept;
    Result<void> PublishProperties() noexcept;
    void SyncContainers() noexcept;
};
} // namespace Primitives

class AERO_GUI_API ListBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ListBox, Primitives::Selector)
public:

    ListBox() noexcept : Primitives::Selector(StaticTypeId()) {}
    ~ListBox() override;

    Result<bool> BringIntoView(
        std::uint32_t index) noexcept;

protected:
    explicit ListBox(TypeId runtimeType) noexcept
        : Primitives::Selector(runtimeType) {}
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item) noexcept override;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::SelectionMode)
