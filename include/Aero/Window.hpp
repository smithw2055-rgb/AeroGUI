#pragma once

#include <Aero/Controls/ContentControls.hpp>

namespace Aero {
class View;
}

namespace Aero::Platform {
class IWindow;
}

namespace Aero::App {
class ApplicationHost;
namespace Detail {
class IWindowPeer;
}
} // namespace Aero::App

namespace Aero::Integration {
class WindowInterop;
}

namespace Aero {

// WPF-aligned XAML content root for a native top-level window. Window keeps the
// ContentControl authoring model while delegating native lifetime and rendering
// integration to an internal App peer.
class AERO_API Window : public Controls::ContentControl {
    AERO_DECLARE_TYPE(Window, Controls::ContentControl)
public:
    Window() noexcept
        : Window(StaticTypeId()) {}
    ~Window() noexcept override = default;

    Base::StringView Title() const noexcept {
        return GetValueOr(TitleProperty, Base::StringView{});
    }
    Base::Result<void> SetTitle(
        Base::StringView value) noexcept {
        return SetValue(TitleProperty, value);
    }

    Base::Result<void> Show() noexcept;
    void Close() noexcept;
    bool IsOpen() const noexcept;

    inline static constexpr Members::Property<Base::String>
        TitleProperty{"Title"};

protected:
    explicit Window(Core::TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}

private:
    friend class App::ApplicationHost;
    friend class Integration::WindowInterop;

    Platform::IWindow* NativeWindow() noexcept;
    View* HostedView() noexcept;

    void Attach(App::Detail::IWindowPeer* peer) noexcept {
        peer_ = peer;
    }
    void Detach() noexcept { peer_ = nullptr; }

    App::Detail::IWindowPeer* peer_ = nullptr;
};

} // namespace Aero
