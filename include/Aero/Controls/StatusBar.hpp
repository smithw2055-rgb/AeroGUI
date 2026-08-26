#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/StatusBarItem.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API StatusBar
    : public ItemsControl {
    AERO_DECLARE_TYPE(StatusBar, ItemsControl)
public:
    StatusBar() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~StatusBar() override = default;
    bool GetIsSizingGripVisible() const noexcept {
        return GetValueOr(
            IsSizingGripVisibleProperty, true);
    }
    void SetIsSizingGripVisible(
        bool value) noexcept {
        SetValue(IsSizingGripVisibleProperty, value);
    }
    inline static constexpr DependencyProperty<bool> IsSizingGripVisibleProperty{"IsSizingGripVisible"};

protected:
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;
};

} // namespace Aero::Controls
