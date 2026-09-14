#pragma once

#include <Aero/Controls/ListBoxItem.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;

class AERO_GUI_API ComboBoxItem
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ComboBoxItem, ListBoxItem)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    ComboBoxItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ComboBoxItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;

    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);
};
} // namespace Aero::Controls
