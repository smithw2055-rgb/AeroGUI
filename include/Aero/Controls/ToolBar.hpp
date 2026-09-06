#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Separator.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/DataTemplate.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ToolBar
    : public ItemsControl {
    AERO_DECLARE_TYPE(ToolBar, ItemsControl)
public:
    ToolBar() noexcept;
    ~ToolBar() override;

    // Unlike the pre-gallery placeholder, WPF ToolBar.Header is content and
    // may therefore be an element, a scalar, or x:Null. Keep it as an
    // unboxed metadata value so template triggers can observe null directly.
    Value GetHeader() const noexcept;
    void SetHeader(const Value& value) noexcept;
    void SetHeader(StringView value) noexcept;
    Ref<DataTemplate> GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept;
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    std::uint32_t GetOverflowCapacity()
        const noexcept;
    void SetOverflowCapacity(std::uint32_t value) noexcept;
    bool GetIsOverflowOpen() const noexcept;
    void SetIsOverflowOpen(bool value) noexcept;
    bool GetHasOverflowItems() const noexcept;
    std::uint32_t GetOverflowItemCount()
        const noexcept;

    AERO_DEPENDENCY_PROPERTY(Value, Header);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, HeaderTemplate);
    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, OverflowCapacity);
    AERO_DEPENDENCY_PROPERTY(bool, IsOverflowOpen);
    AERO_DEPENDENCY_PROPERTY(bool, HasOverflowItems);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, OverflowItemCount);

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnContainersChanged() noexcept override;

private:
    TextBlock* headerText_ = nullptr;
    TextBlock* overflowGlyph_ = nullptr;
    DependencyPropertyChangedEventHandler
        headerChangedHandler_;
    void OnHeaderChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Result<void>
        SynchronizeToolBar() noexcept;
};

// Template item hosts for the primary and overflow regions of ToolBar.
class AERO_GUI_API ToolBarPanel : public Panel {
    AERO_DECLARE_TYPE(ToolBarPanel, Panel)
public:
    ToolBarPanel() noexcept : Panel(StaticTypeId()) {}
    ~ToolBarPanel() override = default;
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_GUI_API ToolBarOverflowPanel : public Panel {
    AERO_DECLARE_TYPE(ToolBarOverflowPanel, Panel)
public:
    ToolBarOverflowPanel() noexcept : Panel(StaticTypeId()) {}
    ~ToolBarOverflowPanel() override = default;
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

// Owner for ToolBarTray attached properties. The current toolbar host does not
// support interactive band rearrangement yet, but authored IsLocked values
// must round-trip through the same dependency-property system as WPF.
class AERO_GUI_API ToolBarTray : public Base::Object {
    AERO_DECLARE_TYPE(ToolBarTray, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    AERO_ATTACHED_PROPERTY(bool, IsLocked);
};

} // namespace Aero::Controls
