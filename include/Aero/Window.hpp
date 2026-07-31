#pragma once

#include <Aero/Controls/ContentControls.hpp>

namespace Aero {
namespace App { class Launcher; }
namespace Integration { class WindowInterop; }

// WPF-aligned XAML content root for a native top-level window. Native lifetime
// and rendering are supplied by an App or Integration host and are not part of
// the Window public object model.
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
    Base::StringView FontFamily() const noexcept {
        return Aero::FrameworkElement::FontFamily();
    }
    Base::Result<void> SetFontFamily(
        Base::StringView value) noexcept {
        return SetValue(FontFamilyProperty, value);
    }

    Base::Result<void> Show() noexcept;
    void Close() noexcept;
    bool IsOpen() const noexcept;

    inline static constexpr Members::Property<Base::String>
        TitleProperty{"Title"};
    inline static constexpr auto FontFamilyProperty =
        Aero::FrameworkElement::FontFamilyProperty;

protected:
    explicit Window(Core::TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}

private:
    friend class App::Launcher;
    friend class Integration::WindowInterop;

    void Attach(void* runtimeState) noexcept {
        runtimeState_ = runtimeState;
    }
    void Detach() noexcept { runtimeState_ = nullptr; }

    void* runtimeState_ = nullptr;
};

} // namespace Aero
