#pragma once

#include <Aero/Controls/ListBoxItem.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;

class AERO_GUI_API ComboBoxItem
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ComboBoxItem, ListBoxItem)
public:
    ComboBoxItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ComboBoxItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(
        bool value) noexcept;

    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
};
} // namespace Aero::Controls
