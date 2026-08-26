#pragma once

#include <Aero/Controls/ListBoxItem.hpp>

namespace Aero::Controls {

class AERO_GUI_API ListViewItem
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)
public:
    ListViewItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};
} // namespace Aero::Controls
