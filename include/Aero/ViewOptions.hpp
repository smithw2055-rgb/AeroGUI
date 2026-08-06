#pragma once

#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Input/Platform.hpp>

namespace Aero::Integration {

struct TextOptions  {
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
struct ViewOptions  {
    IClipboard* clipboard = nullptr;
    ITextInputMethodHost* textInputMethodHost = nullptr;
    TextOptions text;
    bool attachControlInteractions = true;
    bool attachTextEditing = true;
    bool automaticAnimationClock = true;
};

} // namespace Aero::Integration
