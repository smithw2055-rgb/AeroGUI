#pragma once

#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Presentation/Input.hpp>

namespace Aero::Controls {

class VisualStateManager;
class Popup;
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

class Selector;

using SelectionChangedHandler =
    Base::Delegate<void(
        Selector&, const SelectionChangedEvent&)>;

class AERO_API ListBoxItem : public ItemContainer {
    AERO_DECLARE_TYPE(ListBoxItem, ItemContainer)
public:
    ListBoxItem() noexcept : ItemContainer(StaticTypeId()) {}
    ~ListBoxItem() override = default;

    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(bool value) noexcept;

    inline static constexpr Members::Property<bool>
        IsSelectedProperty{"IsSelected"};
protected:
    explicit ListBoxItem(TypeId runtimeType) noexcept
        : ItemContainer(runtimeType) {}
};

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

    inline static constexpr Members::Property<SelectionMode>
        SelectionModeProperty{"SelectionMode"};
    inline static constexpr Members::Property<std::uint32_t>
        SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        SelectedItemProperty{"SelectedItem"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        SelectedValueProperty{"SelectedValue"};
    inline static constexpr Members::Property<Base::String>
        SelectedValuePathProperty{"SelectedValuePath"};
    inline static constexpr Members::AttachedProperty<bool>
        IsSelectedProperty{"IsSelected"};
    // WPF Selector.SelectionChanged is a bubbling routed event. Keep the
    // strongly typed selection notification above for model-facing code while
    // also publishing the routed surface used by EventTrigger.
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        SelectionChangedRoutedEvent{
            "SelectionChanged"};
    UIElement::RoutedEvent_<RoutedEventHandler>
        SelectionChanged() noexcept {
        return Event(
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
    friend class ListBoxInteractionManager;
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

class ListBoxInteractionManager;

class AERO_API ListBox : public Selector {
    AERO_DECLARE_TYPE(ListBox, Selector)
public:
    ListBox() noexcept : Selector(StaticTypeId()) {}
    ~ListBox() override;

    Base::Result<bool> BringIntoView(
        std::uint32_t index) noexcept;

protected:
    explicit ListBox(TypeId runtimeType) noexcept
        : Selector(runtimeType) {}
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item) noexcept override;

private:
    friend class ListBoxInteractionManager;
    ListBoxInteractionManager* interactions_ = nullptr;
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

    inline static constexpr Members::Property<bool>
        IsSelectedProperty{"IsSelected"};
};

class ComboBoxInteractionManager;

class AERO_API ComboBox final : public Selector {
    AERO_DECLARE_TYPE(ComboBox, Selector)
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

    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        DropDownOpenedEvent{"DropDownOpened"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        DropDownClosedEvent{"DropDownClosed"};
    UIElement::RoutedEvent_<RoutedEventHandler>
        DropDownOpened() noexcept {
        return Event(DropDownOpenedEvent);
    }
    UIElement::RoutedEvent_<RoutedEventHandler>
        DropDownClosed() noexcept {
        return Event(DropDownClosedEvent);
    }

    inline static constexpr Members::Property<bool>
        IsDropDownOpenProperty{"IsDropDownOpen"};
    inline static constexpr Members::Property<double>
        MaxDropDownHeightProperty{"MaxDropDownHeight"};
    inline static constexpr Members::Property<bool>
        IsEditableProperty{"IsEditable"};
    inline static constexpr Members::Property<bool>
        IsReadOnlyProperty{"IsReadOnly"};
    inline static constexpr Members::Property<Base::String>
        TextProperty{"Text"};
    inline static constexpr Members::Property<Base::String>
        PlaceholderProperty{"Placeholder"};
    inline static constexpr Members::ReadOnlyProperty<
        Base::String>
        SelectionBoxTextProperty{
            "SelectionBoxText"};
    inline static constexpr Members::ReadOnlyProperty<
        Core::Value>
        SelectionBoxItemProperty{
            "SelectionBoxItem"};

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
    friend class ComboBoxInteractionManager;
    ComboBoxInteractionManager* interactions_ = nullptr;
    TextBlock* selectionBox_ = nullptr;
    ContentPresenter* selectionPresenter_ =
        nullptr;
    TextBox* editableTextBox_ = nullptr;
    Popup* popup_ = nullptr;
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
        UpdateEditablePresentation() noexcept;
    void SynchronizeContainers() noexcept;
    std::uint32_t FindContainerIndex(
        Base::Object* source) const noexcept;
};

class AERO_API ComboBoxInteractionManager final {
public:
    ComboBoxInteractionManager(
        ObjectTree& tree,
        RoutedEventManager& events,
        FocusManager& focus) noexcept;
    ~ComboBoxInteractionManager() noexcept;

    Base::Result<void> Attach(
        ComboBox& comboBox) noexcept;
    Base::Result<bool> Detach(
        ComboBox& comboBox) noexcept;

private:
    ObjectTree* tree_ = nullptr;
    [[maybe_unused]]
    RoutedEventManager* events_ = nullptr;
    FocusManager* focus_ = nullptr;
    Base::Vector<VisualHandle> records_;
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    std::uint32_t FindComboBox(
        const ComboBox& comboBox) const noexcept;
    ComboBox* ResolveComboBox(
        std::uint32_t index) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        const MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        const KeyEventArgs& args) noexcept;
};

class AERO_API ListBoxInteractionManager final {
public:
    ListBoxInteractionManager(
        ObjectTree& tree,
        RoutedEventManager& events,
        FocusManager& focus,
        VisualStateManager* states = nullptr) noexcept;
    ~ListBoxInteractionManager() noexcept;

    Base::Result<void> Attach(ListBox& listBox) noexcept;
    Base::Result<bool> Detach(ListBox& listBox) noexcept;

private:
    struct Record final {
        VisualHandle handle;
        std::uint32_t anchorIndex = UINT32_MAX;
    };

    ObjectTree* tree_ = nullptr;
    [[maybe_unused]] RoutedEventManager* events_ = nullptr;
    FocusManager* focus_ = nullptr;
    VisualStateManager* states_ = nullptr;
    Base::Vector<Record> records_;
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    std::uint32_t FindListBox(
        const ListBox& listBox) const noexcept;
    ListBox* ResolveListBox(
        std::uint32_t index) noexcept;
    std::uint32_t FindContainerIndex(
        ListBox& listBox,
        Base::Object* source) const noexcept;
    Base::Result<bool> ApplyUserSelection(
        ListBox& listBox,
        Record& record,
        std::uint32_t index,
        std::uint32_t modifiers) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        const MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        const KeyEventArgs& args) noexcept;
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
