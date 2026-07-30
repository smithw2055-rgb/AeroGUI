#pragma once

#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Platform/Window.hpp>

namespace Aero {
class View;
}

namespace Aero::App {

namespace Detail {
class IWindowPeer {
public:
    virtual ~IWindowPeer() = default;
    virtual Base::Result<void> Show() noexcept = 0;
    virtual void Close() noexcept = 0;
    virtual bool IsOpen() const noexcept = 0;
    virtual Platform::IWindow* NativeWindow() noexcept = 0;
    virtual View* HostedView() noexcept = 0;
};
}

// A Window is both the XAML content root and the application/window boundary.
// Its visual behavior comes from ContentControl; its native lifetime is
// delegated to the cross-platform ApplicationHost peer.
class AERO_API Window final : public Controls::ContentControl {
    AERO_DECLARE_TYPE(Window, Controls::ContentControl)
public:
    Window() noexcept
        : ContentControl(StaticTypeId()) {}
    ~Window() noexcept override = default;

    Base::StringView Title() const noexcept {
        return GetValueOr(TitleProperty, Base::StringView{});
    }
    Base::Result<void> SetTitle(
        Base::StringView value) noexcept {
        return SetValue(TitleProperty, value);
    }
    Base::StringView FontFamily() const noexcept {
        return Presentation::FrameworkElement::
            FontFamily();
    }
    Base::Result<void> SetFontFamily(
        Base::StringView value) noexcept {
        return SetValue(FontFamilyProperty, value);
    }

    Base::Result<void> Show() noexcept;
    void Close() noexcept;
    bool IsOpen() const noexcept;
    Platform::IWindow* NativeWindow() noexcept;
    View* HostedView() noexcept;

    inline static constexpr Members::Property<Base::String>
        TitleProperty{"Title"};
    inline static constexpr auto FontFamilyProperty =
        Presentation::FrameworkElement::
            FontFamilyProperty;

private:
    friend class ApplicationHost;
    void Attach(Detail::IWindowPeer* peer) noexcept {
        peer_ = peer;
    }
    void Detach() noexcept { peer_ = nullptr; }

    Detail::IWindowPeer* peer_ = nullptr;
};

} // namespace Aero::App
