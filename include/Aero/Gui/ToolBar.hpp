#pragma once

#include <Aero/Gui/ItemsControl.hpp>
#include <Aero/Gui/Panel.hpp>
#include <Aero/Gui/TextBlock.hpp>
#include <Aero/Gui/DataTemplate.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Separator
    : public Control {
    AERO_DECLARE_TYPE(Separator, Control)
public:
    Separator() noexcept
        : Control(StaticTypeId()) {}
    ~Separator() override = default;
};

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
    void SetHeader(
        const Value& value) noexcept;
    Base::Result<void> SetHeader(Base::StringView value) noexcept;
    Base::Ref<DataTemplate> GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Orientation GetOrientation() const noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    std::uint32_t GetOverflowCapacity()
        const noexcept;
    void SetOverflowCapacity(
        std::uint32_t value) noexcept;
    bool GetIsOverflowOpen() const noexcept;
    void SetIsOverflowOpen(
        bool value) noexcept;
    bool GetHasOverflowItems() const noexcept;
    std::uint32_t GetOverflowItemCount()
        const noexcept;

    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<std::uint32_t> OverflowCapacityProperty{"OverflowCapacity"};
    inline static constexpr DependencyProperty<bool> IsOverflowOpenProperty{"IsOverflowOpen"};
    inline static constexpr DependencyProperty<bool> HasOverflowItemsProperty{"HasOverflowItems"};
    inline static constexpr DependencyProperty<std::uint32_t> OverflowItemCountProperty{"OverflowItemCount"};

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
    Base::Result<void>
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
    inline static constexpr AttachedProperty<bool> IsLockedProperty{"IsLocked"};
};

} // namespace Aero::Controls
