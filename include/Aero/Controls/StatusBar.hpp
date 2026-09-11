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
        return GetValue(IsSizingGripVisibleProperty);
    }
    void SetIsSizingGripVisible(bool value) noexcept {
        SetValue(IsSizingGripVisibleProperty, value);
    }
    AERO_DEPENDENCY_PROPERTY(bool, IsSizingGripVisible);

protected:
    Result<Ref<FrameworkElement>>
        GetContainerForItemOverride() const
            noexcept override;
};

} // namespace Aero::Controls
