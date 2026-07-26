#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Markup/XamlTheme.hpp>

#include <cstdint>

namespace Aero::Markup {

// ResourceDictionary-shaped object model for the built-in theme pipeline.
// This is intentionally independent from the current ControlTemplate
// materializer so the dictionary can later be created by XamlObjectWriter.
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

// XamlObjectWriter integration target. The final writer slice can activate this
// object and fill its dictionary through normal member adapters, then pass the
// retained value to XamlTheme without re-entering theme-specific XML parsing.
class AERO_API ThemeResourceDictionaryObject final : public Base::Object {
public:
    explicit ThemeResourceDictionaryObject(
        Base::MetaTypeId runtimeType = Base::InvalidMetaTypeId) noexcept;

    Base::MetaTypeId RuntimeType() const noexcept override;

    ThemeResourceDictionary& Dictionary() noexcept { return dictionary_; }
    const ThemeResourceDictionary& Dictionary() const noexcept {
        return dictionary_;
    }

    Base::Result<void> AddColor(ThemeColorResource color) noexcept;
    Base::Result<void> AddTemplate(
        ThemeControlTemplateResource controlTemplate) noexcept;
    ThemeResourceDictionary TakeDictionary() noexcept;

private:
    Base::MetaTypeId runtimeType_ = Base::InvalidMetaTypeId;
    ThemeResourceDictionary dictionary_;
};

Base::Result<ThemeResourceDictionary> LoadThemeResourceDictionary(
    Base::StringView genericXaml,
    Base::StringView paletteXaml) noexcept;

} // namespace Aero::Markup
