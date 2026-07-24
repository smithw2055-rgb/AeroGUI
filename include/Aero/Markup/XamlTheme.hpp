#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <memory>

namespace Aero::Markup {

enum class ThemeVariant : std::uint8_t {
    Light = 0U,
    Dark,
};

// Loads the provider-owned default-theme dialect used by themes/Generic.xaml
// plus one palette document. Visual trees and setter-only visual states are
// data-driven; the loader intentionally accepts only the built-in controls
// and primitive visuals needed by the M3 default theme.
class AERO_API XamlTheme final {
public:
    static Base::Result<std::unique_ptr<XamlTheme>> Load(
        Base::StringView genericXaml,
        Base::StringView paletteXaml,
        Core::DependencyPropertyRegistry& properties) noexcept;

    ~XamlTheme();

    XamlTheme(const XamlTheme&) = delete;
    XamlTheme& operator=(const XamlTheme&) = delete;
    XamlTheme(XamlTheme&&) = delete;
    XamlTheme& operator=(XamlTheme&&) = delete;

    ThemeVariant Variant() const noexcept;
    const Controls::ControlTemplate* FindTemplate(
        Core::TypeId targetType) const noexcept;
    Base::Result<Controls::TemplateHandle> Apply(
        Controls::TemplateManager& templates,
        Controls::Control& control) const noexcept;
    Base::Result<Presentation::Color> ColorToken(
        Base::StringView key) const noexcept;

private:
    struct Impl;

    explicit XamlTheme(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Aero::Markup
