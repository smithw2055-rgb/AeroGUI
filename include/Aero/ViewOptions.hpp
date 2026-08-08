#pragma once

#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Input/Platform.hpp>

#include <cstdint>

namespace Aero {

class ResourceDictionary;

enum class BuiltInTheme : std::uint8_t { Light = 0U, Dark };
enum class ResourceLayer : std::uint8_t { Application = 0U, Theme, System };
enum class ResourceLoadMode : std::uint8_t { Replace = 0U, Merge };

struct TextOptions {
    Base::StringView primaryFamily;
    Base::Span<const Base::StringView> fallbackFamilies;
    Base::StringView language;
    // Relative FontFamily file values are resolved beneath this root.
    Base::StringView fontSearchRoot;
    float defaultPixelSize = 16.0F;
};

// Immutable per-View behavior and platform options. XAML, texture and font
// providers are process-level Gui configuration and are frozen by
// Gui::Initialize(); View creation cannot override their ownership.
struct ViewOptions {
    Input::IClipboard* clipboard = nullptr;
    Input::ITextInputMethodHost* textInputMethodHost = nullptr;
    TextOptions text;
    ResourceDictionary* applicationResources = nullptr;
    BuiltInTheme builtInTheme = BuiltInTheme::Light;
    bool attachControlInteractions = true;
    bool attachTextEditing = true;
    bool automaticAnimationClock = true;
    bool loadBuiltInTheme = false;
};

} // namespace Aero
