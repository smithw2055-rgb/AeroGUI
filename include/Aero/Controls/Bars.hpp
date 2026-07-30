#pragma once

#include <Aero/Controls/ContentControls.hpp>

namespace Aero::Controls {

class AERO_API Separator final
    : public Control {
    AERO_DECLARE_TYPE(Separator, Control)
public:
    Separator() noexcept
        : Control(StaticTypeId()) {}
    ~Separator() override = default;
};

class AERO_API ToolBar final
    : public ItemsControl {
    AERO_DECLARE_TYPE(ToolBar, ItemsControl)
public:
    ToolBar() noexcept;
    ~ToolBar() override;

    // Unlike the pre-gallery placeholder, WPF ToolBar.Header is content and
    // may therefore be an element, a scalar, or x:Null. Keep it as an
    // unboxed metadata value so template triggers can observe null directly.
    Core::Value Header() const noexcept;
    Base::Result<void> SetHeader(
        const Core::Value& value) noexcept;
    Base::Ref<DataTemplate> HeaderTemplate() const noexcept;
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    std::uint32_t OverflowCapacity()
        const noexcept;
    Base::Result<void> SetOverflowCapacity(
        std::uint32_t value) noexcept;
    bool IsOverflowOpen() const noexcept;
    Base::Result<void> SetIsOverflowOpen(
        bool value) noexcept;
    bool HasOverflowItems() const noexcept;
    std::uint32_t OverflowItemCount()
        const noexcept;

    inline static constexpr Members::Property<Core::Value>
        HeaderProperty{"Header"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<
        Orientation>
        OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<
        std::uint32_t>
        OverflowCapacityProperty{
            "OverflowCapacity"};
    inline static constexpr Members::Property<bool>
        IsOverflowOpenProperty{
            "IsOverflowOpen"};
    inline static constexpr Members::Property<
        bool>
        HasOverflowItemsProperty{
            "HasOverflowItems"};
    inline static constexpr Members::Property<
        std::uint32_t>
        OverflowItemCountProperty{
            "OverflowItemCount"};

protected:
    Base::Result<void>
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
class AERO_API ToolBarPanel final : public Panel {
    AERO_DECLARE_TYPE(ToolBarPanel, Panel)
public:
    ToolBarPanel() noexcept : Panel(StaticTypeId()) {}
    ~ToolBarPanel() override = default;
};

class AERO_API ToolBarOverflowPanel final : public Panel {
    AERO_DECLARE_TYPE(ToolBarOverflowPanel, Panel)
public:
    ToolBarOverflowPanel() noexcept : Panel(StaticTypeId()) {}
    ~ToolBarOverflowPanel() override = default;
};

// Owner for ToolBarTray attached properties. The current toolbar host does not
// support interactive band rearrangement yet, but authored IsLocked values
// must round-trip through the same dependency-property system as WPF.
class AERO_API ToolBarTray final : public Base::Object {
    AERO_DECLARE_TYPE(ToolBarTray, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    inline static constexpr Members::AttachedProperty<bool>
        IsLockedProperty{"IsLocked"};
};

class AERO_API StatusBarItem final
    : public ItemContainer {
    AERO_DECLARE_TYPE(StatusBarItem, ItemContainer)
public:
    StatusBarItem() noexcept
        : ItemContainer(StaticTypeId()) {}
    ~StatusBarItem() override = default;
};

class AERO_API StatusBar final
    : public ItemsControl {
    AERO_DECLARE_TYPE(StatusBar, ItemsControl)
public:
    StatusBar() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~StatusBar() override = default;
    bool IsSizingGripVisible() const noexcept {
        return GetValueOr(
            IsSizingGripVisibleProperty, true);
    }
    Base::Result<void> SetIsSizingGripVisible(
        bool value) noexcept {
        return SetValue(
            IsSizingGripVisibleProperty, value);
    }
    inline static constexpr Members::Property<bool>
        IsSizingGripVisibleProperty{
            "IsSizingGripVisible"};

protected:
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;
};

class AERO_API ToolTip final
    : public Popup {
    AERO_DECLARE_TYPE(ToolTip, Popup)
public:
    ToolTip() noexcept
        : Popup(StaticTypeId()) {}
    ~ToolTip() override = default;

    std::uint32_t InitialShowDelay()
        const noexcept;
    Base::Result<void> SetInitialShowDelay(
        std::uint32_t value) noexcept;
    std::uint32_t ShowDuration()
        const noexcept;
    Base::Result<void> SetShowDuration(
        std::uint32_t value) noexcept;

    inline static constexpr Members::Property<
        std::uint32_t>
        InitialShowDelayProperty{
            "InitialShowDelay"};
    inline static constexpr Members::Property<
        std::uint32_t>
        ShowDurationProperty{"ShowDuration"};
};

class AERO_API ToolTipService final
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ToolTipService, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Base::Ref<ToolTip> GetToolTip(
        const DependencyObject& target) noexcept;
    static Base::Result<void> SetToolTip(
        DependencyObject& target,
        Base::Ref<ToolTip> value) noexcept;
    static std::uint32_t InitialShowDelay(
        const DependencyObject& target) noexcept;
    static Base::Result<void> SetInitialShowDelay(
        DependencyObject& target,
        std::uint32_t value) noexcept;
    static std::uint32_t ShowDuration(
        const DependencyObject& target) noexcept;
    static Base::Result<void> SetShowDuration(
        DependencyObject& target,
        std::uint32_t value) noexcept;

    inline static constexpr Members::AttachedProperty<
        Base::Ref<ToolTip>>
        ToolTipProperty{"ToolTip"};
    inline static constexpr Members::AttachedProperty<
        std::uint32_t>
        InitialShowDelayProperty{
            "InitialShowDelay"};
    inline static constexpr Members::AttachedProperty<
        std::uint32_t>
        ShowDurationProperty{"ShowDuration"};
};

} // namespace Aero::Controls
