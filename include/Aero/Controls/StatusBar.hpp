#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/ItemsControl.hpp>


namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API StatusBarItem : public ContentControl {
    AERO_DECLARE_TYPE(StatusBarItem, ContentControl)
public:
    StatusBarItem() noexcept : ContentControl(StaticTypeId()) {}
    ~StatusBarItem() override = default;
};

class AERO_GUI_API StatusBar : public ItemsControl {
    AERO_DECLARE_TYPE(StatusBar, ItemsControl)
public:
    StatusBar() noexcept : ItemsControl(StaticTypeId()) {}
    ~StatusBar() override = default;
    bool GetIsSizingGripVisible() const noexcept { return GetValue(IsSizingGripVisibleProperty); }
    void SetIsSizingGripVisible(bool value) noexcept { SetValue(IsSizingGripVisibleProperty, value); }
    AERO_DEPENDENCY_PROPERTY(bool, IsSizingGripVisible);

protected:
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;
};

} // namespace Aero::Controls
