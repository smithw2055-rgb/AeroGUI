#pragma once

#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Presentation/Input.hpp>

namespace Aero::Controls {

class VisualStateManager;

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

class AERO_API ListBoxItem final : public ItemContainer {
    AERO_DECLARE_TYPE(ListBoxItem, ItemContainer)
public:
    ListBoxItem() noexcept : ItemContainer(StaticTypeId()) {}
    ~ListBoxItem() override = default;

    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(bool value) noexcept;

    inline static constexpr Members::Property<bool>
        IsSelectedProperty{"IsSelected"};
};

class AERO_API Selector : public ItemsControl {
    AERO_DECLARE_TYPE(Selector, ItemsControl)
public:
    Selector() noexcept;
    ~Selector() override;

    SelectionMode GetSelectionMode() const noexcept;
    std::uint32_t SelectedIndex() const noexcept;
    Base::Ref<Base::Object> SelectedItem() const noexcept;
    // SelectedValue aliases SelectedItem until a SelectedValuePath contract is
    // introduced.
    Base::Ref<Base::Object> SelectedValue() const noexcept;
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

class AERO_API ListBox final : public Selector {
    AERO_DECLARE_TYPE(ListBox, Selector)
public:
    ListBox() noexcept : Selector(StaticTypeId()) {}
    ~ListBox() override;

    Base::Result<bool> BringIntoView(
        std::uint32_t index) noexcept;

protected:
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item) noexcept override;

private:
    friend class ListBoxInteractionManager;
    ListBoxInteractionManager* interactions_ = nullptr;
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
