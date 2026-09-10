#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/TreeViewItem.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API TreeView
    : public ItemsControl {
    AERO_DECLARE_TYPE(TreeView, ItemsControl)
public:

    TreeView() noexcept;
    ~TreeView() override;

    Ref<Base::Object>
        GetSelectedItem() const noexcept;
    bool SelectItem(
        TreeViewItem* item) noexcept;
    AERO_READONLY_PROPERTY(Ref<Base::Object>, SelectedItem);
    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedItemChangedEvent{"SelectedItemChanged"};

protected:
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;

private:
    TreeViewItem* FindItem(Base::Object* source) const noexcept;
    static Result<void> CollectVisibleItems(
        ::Aero::Media::Visual& parent,
        Base::Vector<TreeViewItem*>& items) noexcept;
};
} // namespace Aero::Controls
