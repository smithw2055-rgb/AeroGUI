#pragma once

#include <Aero/Input.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Controls/Panels.hpp>

namespace Aero::Controls {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
using ::Aero::Input::ICommand;
using ::Aero::Media::ImageSource;

enum class MenuItemRole : std::uint8_t {
    TopLevelItem = 0U,
    TopLevelHeader,
    SubmenuItem,
    SubmenuHeader
};


class AERO_API MenuItem
    : public HeaderedItemsControl {
    AERO_DECLARE_TYPE(MenuItem, HeaderedItemsControl)
public:
    MenuItem() noexcept;
    ~MenuItem() override;

    Base::StringView GetInputGestureText()
        const noexcept;
    void SetInputGestureText(
        Base::StringView value) noexcept;
    bool GetIsCheckable() const noexcept;
    void SetIsCheckable(
        bool value) noexcept;
    bool GetIsChecked() const noexcept;
    void SetIsChecked(
        bool value) noexcept;
    bool GetIsHighlighted() const noexcept;
    bool GetIsSubmenuOpen() const noexcept;
    void SetIsSubmenuOpen(
        bool value) noexcept;
    MenuItemRole GetRole() const noexcept;
    ICommand* GetCommand() const noexcept;
    void SetCommand(
        Base::Ref<ICommand> command) noexcept;
    Value GetCommandParameter() const noexcept;
    void SetCommandParameter(Value value) noexcept;

    inline static constexpr Members::Property<Base::String> InputGestureTextProperty{"InputGestureText"};
    inline static constexpr Members::Property<bool> IsCheckableProperty{"IsCheckable"};
    inline static constexpr Members::Property<bool> IsCheckedProperty{"IsChecked"};
    inline static constexpr Members::ReadOnlyProperty<bool> IsHighlightedProperty{"IsHighlighted"};
    inline static constexpr Members::Property<bool> IsSubmenuOpenProperty{"IsSubmenuOpen"};
    inline static constexpr Members::ReadOnlyProperty<MenuItemRole> RoleProperty{"Role"};
    inline static constexpr Members::Property<Base::Ref<ICommand>> CommandProperty{"Command"};
    inline static constexpr Members::Property<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend struct ::Aero::Visual::Impl;
    TextBlock* gestureText_ = nullptr;
    TextBlock* checkGlyph_ = nullptr;
    Primitives::Popup* submenuPopup_ = nullptr;
    DependencyPropertyChangedEventHandler
        menuPropertyChangedHandler_;
    void OnMenuPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Base::Result<void>
        SynchronizeMenuTemplate() noexcept;
    void SetHighlightedState(bool value) noexcept;
    void SetRoleState(MenuItemRole value) noexcept;
};

class AERO_API Menu : public ItemsControl {
    AERO_DECLARE_TYPE(Menu, ItemsControl)
public:
    struct Impl;

    Menu() noexcept
        : Menu(StaticTypeId()) {}
    ~Menu() override;

protected:
    explicit Menu(TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    friend struct Impl;
};

class AERO_API ContextMenu
    : public Menu {
    AERO_DECLARE_TYPE(ContextMenu, Menu)
public:
    ContextMenu() noexcept;
    ~ContextMenu() override;

    bool GetIsOpen() const noexcept;
    void SetIsOpen(
        bool value) noexcept;
    Base::Ref<UIElement>
        GetPlacementTarget() const noexcept;
    void SetPlacementTarget(
        Base::Ref<UIElement> value) noexcept;

    inline static constexpr Members::Property<bool> IsOpenProperty{"IsOpen"};
    inline static constexpr Members::Property<Base::Ref<UIElement>> PlacementTargetProperty{"PlacementTarget"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};

protected:
    void
        OnApplyTemplate() noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    void OnOpenChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API ContextMenuService
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ContextMenuService, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Base::Ref<ContextMenu> GetContextMenu(
        const DependencyObject& target) noexcept;
    static void SetContextMenu(
        DependencyObject& target,
        Base::Ref<ContextMenu> value) noexcept;

    inline static constexpr Members::AttachedProperty<Base::Ref<ContextMenu>> ContextMenuProperty{"ContextMenu"};
};


} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::MenuItemRole)

namespace Aero::Controls {

class AERO_API Separator
    : public Control {
    AERO_DECLARE_TYPE(Separator, Control)
public:
    Separator() noexcept
        : Control(StaticTypeId()) {}
    ~Separator() override = default;
};

class AERO_API ToolBar
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

    inline static constexpr Members::Property<Value> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<std::uint32_t> OverflowCapacityProperty{"OverflowCapacity"};
    inline static constexpr Members::Property<bool> IsOverflowOpenProperty{"IsOverflowOpen"};
    inline static constexpr Members::Property<bool> HasOverflowItemsProperty{"HasOverflowItems"};
    inline static constexpr Members::Property<std::uint32_t> OverflowItemCountProperty{"OverflowItemCount"};

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
class AERO_API ToolBarPanel : public Panel {
    AERO_DECLARE_TYPE(ToolBarPanel, Panel)
public:
    ToolBarPanel() noexcept : Panel(StaticTypeId()) {}
    ~ToolBarPanel() override = default;
};

class AERO_API ToolBarOverflowPanel : public Panel {
    AERO_DECLARE_TYPE(ToolBarOverflowPanel, Panel)
public:
    ToolBarOverflowPanel() noexcept : Panel(StaticTypeId()) {}
    ~ToolBarOverflowPanel() override = default;
};

// Owner for ToolBarTray attached properties. The current toolbar host does not
// support interactive band rearrangement yet, but authored IsLocked values
// must round-trip through the same dependency-property system as WPF.
class AERO_API ToolBarTray : public Base::Object {
    AERO_DECLARE_TYPE(ToolBarTray, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    inline static constexpr Members::AttachedProperty<bool> IsLockedProperty{"IsLocked"};
};

class AERO_API StatusBarItem
    : public ContentControl {
    AERO_DECLARE_TYPE(StatusBarItem, ContentControl)
public:
    StatusBarItem() noexcept
        : ContentControl(StaticTypeId()) {}
    ~StatusBarItem() override = default;
};

class AERO_API StatusBar
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
    inline static constexpr Members::Property<bool> IsSizingGripVisibleProperty{"IsSizingGripVisible"};

protected:
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;
};

class AERO_API ToolTip
    : public Primitives::Popup {
    AERO_DECLARE_TYPE(ToolTip, Primitives::Popup)
public:
    ToolTip() noexcept
        : Primitives::Popup(StaticTypeId()) {}
    ~ToolTip() override = default;

    std::uint32_t GetInitialShowDelay()
        const noexcept;
    void SetInitialShowDelay(
        std::uint32_t value) noexcept;
    std::uint32_t GetShowDuration()
        const noexcept;
    void SetShowDuration(
        std::uint32_t value) noexcept;

    inline static constexpr Members::Property<std::uint32_t> InitialShowDelayProperty{"InitialShowDelay"};
    inline static constexpr Members::Property<std::uint32_t> ShowDurationProperty{"ShowDuration"};
};

class AERO_API ToolTipService
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ToolTipService, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Base::Ref<ToolTip> GetToolTip(
        const DependencyObject& target) noexcept;
    static void SetToolTip(
        DependencyObject& target,
        Base::Ref<ToolTip> value) noexcept;
    static std::uint32_t GetInitialShowDelay(
        const DependencyObject& target) noexcept;
    static void SetInitialShowDelay(
        DependencyObject& target,
        std::uint32_t value) noexcept;
    static std::uint32_t GetShowDuration(
        const DependencyObject& target) noexcept;
    static void SetShowDuration(
        DependencyObject& target,
        std::uint32_t value) noexcept;

    inline static constexpr Members::AttachedProperty<Base::Ref<ToolTip>> ToolTipProperty{"ToolTip"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> InitialShowDelayProperty{"InitialShowDelay"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> ShowDurationProperty{"ShowDuration"};
};

} // namespace Aero::Controls

namespace Aero::Controls {


class AERO_API Image : public FrameworkElement {
    AERO_DECLARE_TYPE(Image, FrameworkElement)
public:
    struct Impl;

    Image() noexcept
        : FrameworkElement(StaticTypeId()) {}
    ~Image() override = default;

    Base::Ref<ImageSource> GetSource() const noexcept;
    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    void SetSource(
        Base::Ref<ImageSource> value) noexcept;
    void SetStretch(
        Stretch value) noexcept;
    void SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr Members::Property<Base::Ref<ImageSource>> SourceProperty{"Source"};
    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<StretchDirection> StretchDirectionProperty{"StretchDirection"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnRender(
        DrawingContext& context) noexcept override;

private:
    friend struct Impl;
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};

} // namespace Aero::Controls
