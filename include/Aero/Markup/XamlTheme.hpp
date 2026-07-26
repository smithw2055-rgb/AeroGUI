#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <memory>

namespace Aero::Core {
class MetadataRuntime;
}

namespace Aero::Markup {

enum class ThemeVariant : std::uint8_t {
    Light = 0U,
    Dark,
};

// Loads built-in theme documents through the ordinary metadata-backed
// XamlObjectWriter. Palette values, templates, visual trees, visual states and
// setters use the same object/value/resource pipeline as application XAML.
class AERO_API XamlTheme final {
public:
    static Base::Result<std::unique_ptr<XamlTheme>> Load(
        Base::StringView genericXaml,
        Base::StringView paletteXaml,
        Core::MetadataRuntime& runtime) noexcept;

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
