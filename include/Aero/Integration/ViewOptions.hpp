#pragma once

#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Integration/Platform.hpp>

namespace Aero::Integration {

class XamlProvider;
class TextureProvider;
class FontProvider;

struct XamlProviderRoute {
    XamlProvider* provider = nullptr;
    Base::StringView scheme;
    Base::StringView assembly;
};

struct TextOptions  {
    Base::StringView primaryFamily;
    Base::Span<const Base::StringView> fallbackFamilies;
    Base::StringView language;
    // Relative FontFamily file values are resolved beneath this root.
    Base::StringView fontSearchRoot;
    float defaultPixelSize = 16.0F;
};

// Immutable creation options copied into a View. Rendering devices are attached
// explicitly through View::GetRenderer().Init() after CreateView returns.
struct ViewOptions  {
    IClipboard* clipboard = nullptr;
    ITextInputMethodHost* textInputMethodHost = nullptr;
    TextOptions text;
    Base::Span<const XamlProviderRoute> xamlProviders;
    TextureProvider* textureProvider = nullptr;
    FontProvider* fontProvider = nullptr;
    bool attachControlInteractions = true;
    bool attachTextEditing = true;
    bool automaticAnimationClock = true;
};

} // namespace Aero::Integration
