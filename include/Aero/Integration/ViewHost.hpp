#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Integration/RenderEndpoint.hpp>
#include <Aero/RuntimeEnvironment.hpp>

namespace Aero::Platform {
class IClipboard;
class ITextInputMethodHost;
}

namespace Aero::Integration {

class ISourceProvider;

struct TextOptions final {
    Base::StringView primaryFamily;
    Base::Span<const Base::StringView> fallbackFamilies;
    Base::StringView language;
    // Relative FontFamily file values are resolved beneath this root.
    Base::StringView fontSearchRoot;
    float defaultPixelSize = 16.0F;
};

struct ViewHostOptions final {
    Base::Ref<RenderEndpoint> renderEndpoint;
    Platform::IClipboard* clipboard = nullptr;
    Platform::ITextInputMethodHost* textInputMethodHost = nullptr;
    TextOptions text;
    bool attachControlInteractions = true;
    bool attachTextEditing = true;
    bool automaticAnimationClock = true;
};

class AERO_API ViewHost final {
public:
    static Base::Result<Base::Ref<View>> CreateView(
        RuntimeEnvironment& environment,
        const ViewHostOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    explicit ViewHost(View& view) noexcept : view_(&view) {}

    Base::Result<void> RegisterSourceProvider(
        ISourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;

    View& GetView() const noexcept { return *view_; }

private:
    View* view_ = nullptr;
};

} // namespace Aero::Integration
