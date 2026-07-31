#pragma once

#include "../render/DisplayList.hpp"

#include "../controls/TextLayoutService.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Text/GlyphAtlas.hpp>
#include <Aero/Text/TextLayout.hpp>
#include <Aero/Text/TextTypes.hpp>

#include <cstdint>

namespace Aero::Text {
class FontManager;
}

namespace Aero::Detail {

struct TextRuntimeConfig final {
    Text::FontFace face;
    Base::Span<const Text::FontFace> fallbackFaces;
    float pixelSize = 16.0F;
    float lineHeight = 0.0F;
    Text::TextWrapping wrapping =
        Text::TextWrapping::NoWrap;
    Text::TextTrimming trimming =
        Text::TextTrimming::None;
    Text::TextAlignment alignment =
        Text::TextAlignment::Start;
    Text::GlyphAtlasConfig atlas;
    Render::RenderGlyphRunId firstGlyphRunId =
        UINT64_C(1) << 32U;
};

// Private bridge supplied by GPU endpoints. Runtime owns font resolution and
// shaping configuration; the endpoint owns every device resource and the
// concrete GPU-backed layout service.
struct TextBackendServices final {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Controls::Detail::TextLayoutService*> (
        *createService)(
            void* context,
            Text::FontManager& fonts,
            const TextRuntimeConfig& config,
            Base::IAllocator& allocator) noexcept = nullptr;
    void (*destroyService)(
        void* context,
        Controls::Detail::TextLayoutService* service) noexcept = nullptr;
    Base::Result<std::uint32_t> (*collectGarbage)(
        void* context,
        Controls::Detail::TextLayoutService* service) noexcept = nullptr;
};

} // namespace Aero::Detail
