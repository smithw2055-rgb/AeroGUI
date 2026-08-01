#pragma once

#include <Aero/Controls/Items.hpp>

namespace Aero {
namespace App { namespace Detail { class DesktopPrivate; } }
namespace App { class WindowInterop; }

enum class WindowState : std::uint8_t { Normal = 0U, Minimized, Maximized };
enum class WindowStyle : std::uint8_t { None = 0U, SingleBorderWindow, ThreeDBorderWindow, ToolWindow };
enum class ResizeMode : std::uint8_t { NoResize = 0U, CanMinimize, CanResize, CanResizeWithGrip };
enum class SizeToContent : std::uint8_t { Manual = 0U, Width, Height, WidthAndHeight };

struct CancelEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(CancelEventArgs, RoutedEventArgs)
public:
    CancelEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}

    bool GetCancel() const noexcept { return cancel_; }
    void SetCancel(bool value) noexcept { cancel_ = value; }

private:
    bool cancel_ = false;
};

using CancelEventHandler = Base::Delegate<void(Base::Object*, CancelEventArgs&)>;

class AERO_API Window : public Controls::ContentControl {
    AERO_DECLARE_TYPE(Window, Controls::ContentControl)
public:
    Window() noexcept : Window(StaticTypeId()) {}
    ~Window() noexcept override = default;

    Base::StringView GetTitle() const noexcept { return GetValueOr(TitleProperty, Base::StringView{}); }
    Base::Result<void> SetTitle(Base::StringView value) noexcept { return SetValue(TitleProperty, value); }
    WindowState GetWindowState() const noexcept { return GetValueOr(WindowStateProperty, WindowState::Normal); }
    Base::Result<void> SetWindowState(WindowState value) noexcept;
    WindowStyle GetWindowStyle() const noexcept { return GetValueOr(WindowStyleProperty, WindowStyle::SingleBorderWindow); }
    Base::Result<void> SetWindowStyle(WindowStyle value) noexcept { return SetValue(WindowStyleProperty, value); }
    ResizeMode GetResizeMode() const noexcept { return GetValueOr(ResizeModeProperty, ResizeMode::CanResize); }
    Base::Result<void> SetResizeMode(ResizeMode value) noexcept { return SetValue(ResizeModeProperty, value); }
    SizeToContent GetSizeToContent() const noexcept { return GetValueOr(SizeToContentProperty, SizeToContent::Manual); }
    Base::Result<void> SetSizeToContent(SizeToContent value) noexcept { return SetValue(SizeToContentProperty, value); }
    bool GetShowInTaskbar() const noexcept { return GetValueOr(ShowInTaskbarProperty, true); }
    Base::Result<void> SetShowInTaskbar(bool value) noexcept { return SetValue(ShowInTaskbarProperty, value); }
    bool GetTopmost() const noexcept { return GetValueOr(TopmostProperty, false); }
    Base::Result<void> SetTopmost(bool value) noexcept { return SetValue(TopmostProperty, value); }

    Base::Result<void> Show() noexcept;
    void Close() noexcept;
    bool IsOpen() const noexcept;

    inline static constexpr Members::Property<Base::String> TitleProperty{"Title"};
    inline static constexpr Members::Property<WindowState> WindowStateProperty{"WindowState"};
    inline static constexpr Members::Property<WindowStyle> WindowStyleProperty{"WindowStyle"};
    inline static constexpr Members::Property<ResizeMode> ResizeModeProperty{"ResizeMode"};
    inline static constexpr Members::Property<SizeToContent> SizeToContentProperty{"SizeToContent"};
    inline static constexpr Members::Property<bool> ShowInTaskbarProperty{"ShowInTaskbar"};
    inline static constexpr Members::Property<bool> TopmostProperty{"Topmost"};
    inline static constexpr Members::RoutedEvent<CancelEventArgs> ClosingEvent{"Closing"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ActivatedEvent{"Activated"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> DeactivatedEvent{"Deactivated"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ContentRenderedEvent{"ContentRendered"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SourceInitializedEvent{"SourceInitialized"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> StateChangedEvent{"StateChanged"};

    UIElement::Event<CancelEventArgs> Closing() noexcept { return GetEvent(ClosingEvent); }
    UIElement::Event<RoutedEventArgs> Closed() noexcept { return GetEvent(ClosedEvent); }
    UIElement::Event<RoutedEventArgs> Activated() noexcept { return GetEvent(ActivatedEvent); }
    UIElement::Event<RoutedEventArgs> Deactivated() noexcept { return GetEvent(DeactivatedEvent); }
    UIElement::Event<RoutedEventArgs> ContentRendered() noexcept { return GetEvent(ContentRenderedEvent); }
    UIElement::Event<RoutedEventArgs> SourceInitialized() noexcept { return GetEvent(SourceInitializedEvent); }
    UIElement::Event<RoutedEventArgs> StateChanged() noexcept { return GetEvent(StateChangedEvent); }

protected:
    explicit Window(Core::TypeId runtimeType) noexcept : ContentControl(runtimeType) {}
    virtual void OnClosing(CancelEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ClosingEvent, &args)); }
    virtual void OnClosed(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ClosedEvent, &args)); }
    virtual void OnActivated(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ActivatedEvent, &args)); }
    virtual void OnDeactivated(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(DeactivatedEvent, &args)); }
    virtual void OnContentRendered(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ContentRenderedEvent, &args)); }
    virtual void OnSourceInitialized(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(SourceInitializedEvent, &args)); }
    virtual void OnStateChanged(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(StateChangedEvent, &args)); }

private:
    friend class App::Detail::DesktopPrivate;
    friend class App::WindowInterop;

    void Attach(void* hostState) noexcept { hostState_ = hostState; sourceInitialized_ = false; contentRendered_ = false; closed_ = false; }
    void Detach() noexcept { hostState_ = nullptr; }
    void NotifySourceInitialized() noexcept;
    void NotifyActivated() noexcept;
    void NotifyDeactivated() noexcept;
    void NotifyContentRendered() noexcept;
    void NotifyClosed() noexcept;

    void* hostState_ = nullptr;
    bool sourceInitialized_ = false;
    bool contentRendered_ = false;
    bool closed_ = false;
};

} // namespace Aero

namespace Aero::Core {
#define AERO_WINDOW_ENUM_TRAITS(TypeName) template<> struct MetaTypeTraits<Aero::TypeName> { static constexpr TypeId Id() noexcept { return MakeTypeId(#TypeName); } static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); } static constexpr Base::StringView Name() noexcept { return #TypeName; } static constexpr TypeId BaseType() noexcept { return InvalidTypeId; } };
AERO_WINDOW_ENUM_TRAITS(WindowState)
AERO_WINDOW_ENUM_TRAITS(WindowStyle)
AERO_WINDOW_ENUM_TRAITS(ResizeMode)
AERO_WINDOW_ENUM_TRAITS(SizeToContent)
#undef AERO_WINDOW_ENUM_TRAITS
} // namespace Aero::Core
