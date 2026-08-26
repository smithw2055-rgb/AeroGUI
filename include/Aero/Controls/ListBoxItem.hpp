#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Controls {

using ::Aero::Meta::TypeId;

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

} // namespace Aero::Controls
