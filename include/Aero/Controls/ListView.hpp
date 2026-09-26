#pragma once

#include <Aero/Controls/Selectors.hpp>
#include <Aero/Controls/GridViews.hpp>
#include <Aero/Controls/TextBlock.hpp>


namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ListViewItem : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)
public:
    ListViewItem() noexcept : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};

class AERO_GUI_API ListView : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)
public:
    ListView() noexcept : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Ref<GridView> GetView() const noexcept;
    void SetView(Ref<GridView> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<GridView>, View);

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    void SynchronizeColumnHeaders() noexcept;
};
} // namespace Aero::Controls
