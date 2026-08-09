#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Events/WindowEventArgs.hpp>

namespace Aero {
namespace App {
class WindowInterop;
class DesktopHost;
}

enum class WindowState : std::uint8_t { Normal = 0U, Minimized, Maximized };
enum class WindowStyle : std::uint8_t { None = 0U, SingleBorderWindow, ThreeDBorderWindow, ToolWindow };
enum class ResizeMode : std::uint8_t { NoResize = 0U, CanMinimize, CanResize, CanResizeWithGrip };
enum class SizeToContent : std::uint8_t { Manual = 0U, Width, Height, WidthAndHeight };

class AERO_APP_API Window : public Controls::ContentControl {
    AERO_DECLARE_TYPE(Window, Controls::ContentControl)
public:
    Window() noexcept : Window(StaticTypeId()) {}
    ~Window() noexcept override;

    StringView GetTitle() const noexcept { return GetValueOr(TitleProperty, StringView{}); }
    void SetTitle(StringView value) noexcept { SetValue(TitleProperty, value); }
    WindowState GetWindowState() const noexcept { return GetValueOr(WindowStateProperty, WindowState::Normal); }
    void SetWindowState(WindowState value) noexcept;
    WindowStyle GetWindowStyle() const noexcept { return GetValueOr(WindowStyleProperty, WindowStyle::SingleBorderWindow); }
    void SetWindowStyle(WindowStyle value) noexcept { SetValue(WindowStyleProperty, value); }
    ResizeMode GetResizeMode() const noexcept { return GetValueOr(ResizeModeProperty, ResizeMode::CanResize); }
    void SetResizeMode(ResizeMode value) noexcept { SetValue(ResizeModeProperty, value); }
    SizeToContent GetSizeToContent() const noexcept { return GetValueOr(SizeToContentProperty, SizeToContent::Manual); }
    void SetSizeToContent(SizeToContent value) noexcept { SetValue(SizeToContentProperty, value); }
    bool GetShowInTaskbar() const noexcept { return GetValueOr(ShowInTaskbarProperty, true); }
    void SetShowInTaskbar(bool value) noexcept { SetValue(ShowInTaskbarProperty, value); }
    bool GetTopmost() const noexcept { return GetValueOr(TopmostProperty, false); }
    void SetTopmost(bool value) noexcept { SetValue(TopmostProperty, value); }

    Result<void> Show() noexcept;
    Result<void> Close() noexcept;
    bool GetIsOpen() const noexcept;

    inline static constexpr DependencyProperty<String> TitleProperty{"Title"};
    inline static constexpr DependencyProperty<WindowState> WindowStateProperty{"WindowState"};
    inline static constexpr DependencyProperty<WindowStyle> WindowStyleProperty{"WindowStyle"};
    inline static constexpr DependencyProperty<ResizeMode> ResizeModeProperty{"ResizeMode"};
    inline static constexpr DependencyProperty<SizeToContent> SizeToContentProperty{"SizeToContent"};
    inline static constexpr DependencyProperty<bool> ShowInTaskbarProperty{"ShowInTaskbar"};
    inline static constexpr DependencyProperty<bool> TopmostProperty{"Topmost"};
    inline static constexpr RoutedEvent<CancelEventArgs> ClosingEvent{"Closing"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ActivatedEvent{"Activated"};
    inline static constexpr RoutedEvent<RoutedEventArgs> DeactivatedEvent{"Deactivated"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ContentRenderedEvent{"ContentRendered"};
    inline static constexpr RoutedEvent<RoutedEventArgs> SourceInitializedEvent{"SourceInitialized"};
    inline static constexpr RoutedEvent<RoutedEventArgs> StateChangedEvent{"StateChanged"};

    UIElement::Event<CancelEventArgs> Closing() noexcept { return GetEvent(ClosingEvent); }
    UIElement::Event<RoutedEventArgs> Closed() noexcept { return GetEvent(ClosedEvent); }
    UIElement::Event<RoutedEventArgs> Activated() noexcept { return GetEvent(ActivatedEvent); }
    UIElement::Event<RoutedEventArgs> Deactivated() noexcept { return GetEvent(DeactivatedEvent); }
    UIElement::Event<RoutedEventArgs> ContentRendered() noexcept { return GetEvent(ContentRenderedEvent); }
    UIElement::Event<RoutedEventArgs> SourceInitialized() noexcept { return GetEvent(SourceInitializedEvent); }
    UIElement::Event<RoutedEventArgs> StateChanged() noexcept { return GetEvent(StateChangedEvent); }

protected:
    explicit Window(Meta::TypeId runtimeType) noexcept;
    // Marks this code-behind Window for conventional XAML initialization.
    // With no URI the desktop host resolves <registered-type-name>.xaml next
    // to App.xaml; generated code may pass an explicit component URI.
    void InitializeComponent() noexcept;
    void InitializeComponent(StringView componentUri) noexcept;
    virtual void OnClosing(CancelEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ClosingEvent, &args)); }
    virtual void OnClosed(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ClosedEvent, &args)); }
    virtual void OnActivated(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ActivatedEvent, &args)); }
    virtual void OnDeactivated(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(DeactivatedEvent, &args)); }
    virtual void OnContentRendered(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(ContentRenderedEvent, &args)); }
    virtual void OnSourceInitialized(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(SourceInitializedEvent, &args)); }
    virtual void OnStateChanged(RoutedEventArgs& args) noexcept { static_cast<void>(RaiseEvent(StateChangedEvent, &args)); }

private:
    friend class App::WindowInterop;
    friend class App::DesktopHost;

    void Attach(void* hostState) noexcept;
    void Detach() noexcept;
    void NotifySourceInitialized() noexcept;
    void NotifyActivated() noexcept;
    void NotifyDeactivated() noexcept;
    void NotifyContentRendered() noexcept;
    void NotifyClosed() noexcept;
    bool ComponentRequested() const noexcept;
    StringView ComponentUri() const noexcept;

    void* hostState_ = nullptr;
    String componentUri_;
    bool componentRequested_ = false;
    bool sourceInitialized_ = false;
    bool contentRendered_ = false;
    bool closed_ = false;
};

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::WindowState)
AERO_DECLARE_TYPE_ENUM(Aero::WindowStyle)
AERO_DECLARE_TYPE_ENUM(Aero::ResizeMode)
AERO_DECLARE_TYPE_ENUM(Aero::SizeToContent)
