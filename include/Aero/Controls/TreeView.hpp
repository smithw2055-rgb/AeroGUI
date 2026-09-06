#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/TreeViewItem.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API TreeView
    : public ItemsControl {
    AERO_DECLARE_TYPE(TreeView, ItemsControl)
public:

    TreeView() noexcept
        : ItemsControl(StaticTypeId()) {}
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
};
} // namespace Aero::Controls
