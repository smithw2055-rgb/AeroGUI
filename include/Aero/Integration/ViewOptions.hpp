#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Integration/PlatformServices.hpp>
#include <Aero/Integration/RenderEndpoint.hpp>

namespace Aero::Integration {

struct TextOptions final {
    Base::StringView primaryFamily;
    Base::Span<const Base::StringView> fallbackFamilies;
    Base::StringView language;
    // Relative FontFamily file values are resolved beneath this root.
    Base::StringView fontSearchRoot;
    float defaultPixelSize = 16.0F;
};

// Immutable creation options copied into a View. The caller may release the
// RenderEndpoint reference and all temporary spans after CreateView returns.
struct ViewOptions final {
    Base::Ref<RenderEndpoint> renderEndpoint;
    IClipboard* clipboard = nullptr;
    ITextInputMethodHost* textInputMethodHost = nullptr;
    TextOptions text;
    bool attachControlInteractions = true;
    bool attachTextEditing = true;
    bool automaticAnimationClock = true;
};

} // namespace Aero::Integration
