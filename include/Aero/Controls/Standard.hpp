#pragma once

#include <Aero/Input.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Controls/Panels.hpp>

namespace Aero::Detail { class ControlRuntimeAccess; }

namespace Aero::Controls {

enum class MenuItemRole : std::uint8_t {
    TopLevelItem = 0U,
    TopLevelHeader,
    SubmenuItem,
    SubmenuHeader
};


class AERO_API MenuItem final
    : public TreeViewItem {
    AERO_DECLARE_TYPE(MenuItem, TreeViewItem)
public:
    MenuItem() noexcept;
    ~MenuItem() override;

    Base::StringView InputGestureText()
        const noexcept;
    Base::Result<void> SetInputGestureText(
        Base::StringView value) noexcept;
    bool IsCheckable() const noexcept;
    Base::Result<void> SetIsCheckable(
        bool value) noexcept;
    bool IsChecked() const noexcept;
    Base::Result<void> SetIsChecked(
        bool value) noexcept;
    bool IsHighlighted() const noexcept;
    bool IsSubmenuOpen() const noexcept;
    Base::Result<void> SetIsSubmenuOpen(
        bool value) noexcept;
    MenuItemRole Role() const noexcept;
    ICommand* GetCommand() const noexcept;
    Base::Result<void> SetCommand(
        Base::Ref<ICommand> command) noexcept;
    Base::Ref<Base::Object>
        CommandParameter() const noexcept;
    Base::Result<void> SetCommandParameter(
        Base::Ref<Base::Object> value) noexcept;

    inline static constexpr Members::Property<Base::String> InputGestureTextProperty{"InputGestureText"};
    inline static constexpr Members::Property<bool> IsCheckableProperty{"IsCheckable"};
    inline static constexpr Members::Property<bool> IsCheckedProperty{"IsChecked"};
    inline static constexpr Members::ReadOnlyProperty<bool> IsHighlightedProperty{"IsHighlighted"};
    inline static constexpr Members::Property<bool> IsSubmenuOpenProperty{"IsSubmenuOpen"};
    inline static constexpr Members::ReadOnlyProperty<MenuItemRole> RoleProperty{"Role"};
    inline static constexpr Members::Property<Base::Ref<ICommand>> CommandProperty{"Command"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> CommandParameterProperty{"CommandParameter"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};

protected:
    Base::Result<void>
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
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
    Base::Result<void> SetHighlightedState(bool value) noexcept;
    Base::Result<void> SetRoleState(MenuItemRole value) noexcept;
};

class AERO_API Menu : public ItemsControl {
    AERO_DECLARE_TYPE(Menu, ItemsControl)
public:
    Menu() noexcept
        : Menu(StaticTypeId()) {}
    ~Menu() override;

protected:
    explicit Menu(TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    void* interactions_ =
        nullptr;
};

class AERO_API ContextMenu final
    : public Menu {
    AERO_DECLARE_TYPE(ContextMenu, Menu)
public:
    ContextMenu() noexcept;
    ~ContextMenu() override;

    bool IsOpen() const noexcept;
    Base::Result<void> SetIsOpen(
        bool value) noexcept;
    Base::Ref<UIElement>
        PlacementTarget() const noexcept;
    Base::Result<void> SetPlacementTarget(
        Base::Ref<UIElement> value) noexcept;

    inline static constexpr Members::Property<bool> IsOpenProperty{"IsOpen"};
    inline static constexpr Members::Property<Base::Ref<UIElement>> PlacementTargetProperty{"PlacementTarget"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};

protected:
    Base::Result<void>
        OnApplyTemplate() noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    void OnOpenChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API ContextMenuService final
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ContextMenuService, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Base::Ref<ContextMenu> GetContextMenu(
        const DependencyObject& target) noexcept;
    static Base::Result<void> SetContextMenu(
        DependencyObject& target,
        Base::Ref<ContextMenu> value) noexcept;

    inline static constexpr Members::AttachedProperty<Base::Ref<ContextMenu>> ContextMenuProperty{"ContextMenu"};
};


} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::MenuItemRole> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("MenuItemRole"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "MenuItemRole"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Core

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

    inline static constexpr Members::Property<Core::Value> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<std::uint32_t> OverflowCapacityProperty{"OverflowCapacity"};
    inline static constexpr Members::Property<bool> IsOverflowOpenProperty{"IsOverflowOpen"};
    inline static constexpr Members::Property<bool> HasOverflowItemsProperty{"HasOverflowItems"};
    inline static constexpr Members::Property<std::uint32_t> OverflowItemCountProperty{"OverflowItemCount"};

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
    inline static constexpr Members::AttachedProperty<bool> IsLockedProperty{"IsLocked"};
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
    inline static constexpr Members::Property<bool> IsSizingGripVisibleProperty{"IsSizingGripVisible"};

protected:
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;
};

class AERO_API ToolTip final
    : public Primitives::Popup {
    AERO_DECLARE_TYPE(ToolTip, Primitives::Popup)
public:
    ToolTip() noexcept
        : Primitives::Popup(StaticTypeId()) {}
    ~ToolTip() override = default;

    std::uint32_t InitialShowDelay()
        const noexcept;
    Base::Result<void> SetInitialShowDelay(
        std::uint32_t value) noexcept;
    std::uint32_t ShowDuration()
        const noexcept;
    Base::Result<void> SetShowDuration(
        std::uint32_t value) noexcept;

    inline static constexpr Members::Property<std::uint32_t> InitialShowDelayProperty{"InitialShowDelay"};
    inline static constexpr Members::Property<std::uint32_t> ShowDurationProperty{"ShowDuration"};
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

    inline static constexpr Members::AttachedProperty<Base::Ref<ToolTip>> ToolTipProperty{"ToolTip"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> InitialShowDelayProperty{"InitialShowDelay"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> ShowDurationProperty{"ShowDuration"};
};

} // namespace Aero::Controls

namespace Aero::Detail {
class ImageControlAccess;
}

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Render;

class AERO_API Image final : public FrameworkElement {
    AERO_DECLARE_TYPE(Image, FrameworkElement)
public:
    Image() noexcept
        : FrameworkElement(StaticTypeId()) {}
    ~Image() override = default;

    Base::Ref<ImageSource> Source() const noexcept;
    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    Base::Result<void> SetSource(
        Base::Ref<ImageSource> value) noexcept;
    Base::Result<void> SetStretch(
        Stretch value) noexcept;
    Base::Result<void> SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr Members::Property<Base::Ref<ImageSource>> SourceProperty{"Source"};
    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<StretchDirection> StretchDirectionProperty{"StretchDirection"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<void> OnRender(
        DrawingContext& context) noexcept override;

private:
    friend class Aero::Detail::ImageControlAccess;
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};

} // namespace Aero::Controls
