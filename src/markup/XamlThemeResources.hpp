#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Markup/XamlTheme.hpp>

#include <cstdint>

namespace Aero::Markup {

// Temporary private bootstrap representation for the built-in theme catalog.
// Normal application and module resources use ResourceDictionary/Core::Value;
// this DTO must disappear when Generic.xaml is loaded by XamlObjectWriter.
enum class ThemeVisualKind : std::uint8_t {
    Grid = 0U,
    StackPanel,
    Border,
    ContentPresenter,
};

struct ThemeColorResource final {
    Base::String key;
    Presentation::Color value;
};

struct ThemeVisualNode final {
    ThemeVisualKind kind = ThemeVisualKind::Grid;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Presentation::Color background;
    Presentation::Color borderBrush;
    Presentation::Thickness padding;
    double borderThickness = 0.0;
    Controls::Orientation orientation = Controls::Orientation::Vertical;
    bool hasBackground = false;
    bool hasBorderBrush = false;
    bool hasPadding = false;
    bool hasBorderThickness = false;
    bool hasOrientation = false;
};

struct ThemeSetterResource final {
    Base::String targetName;
    Base::String property;
    Base::String resource;
};

struct ThemeVisualStateResource final {
    Base::String name;
    Base::Vector<ThemeSetterResource> setters;
};

struct ThemeVisualStateGroupResource final {
    Base::String name;
    Base::Vector<ThemeVisualStateResource> states;
};

struct ThemeControlTemplateResource final {
    Base::String targetType;
    Base::Vector<ThemeVisualNode> visualTree;
    Base::Vector<ThemeVisualStateGroupResource> visualStateGroups;
};

struct ThemeResourceDictionary final {
    ThemeVariant variant = ThemeVariant::Light;
    Base::Vector<ThemeColorResource> colors;
    Base::Vector<ThemeControlTemplateResource> templates;

    const ThemeColorResource* FindColor(
        Base::StringView key) const noexcept;
};

Base::Result<ThemeResourceDictionary> LoadThemeResourceDictionary(
    Base::StringView genericXaml,
    Base::StringView paletteXaml) noexcept;

} // namespace Aero::Markup
