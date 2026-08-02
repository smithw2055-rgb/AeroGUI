#pragma once

#include "DisplayList.hpp"
#include "controls/TextBlockLayout.hpp"
#include "text/GlyphAtlas.hpp"
#include "text/TextLayout.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdint>

namespace Aero::Text { class FontManager; }

namespace Aero::Internal {

struct ImageResources {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Render::RenderImageId> (*create)(
        void*, std::uint32_t, std::uint32_t,
        Base::Span<const std::uint8_t>) noexcept = nullptr;
    void (*release)(void*, Render::RenderImageId) noexcept = nullptr;
};

struct MeshResources {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Render::RenderMeshId> (*create)(
        void*, Base::Span<const Aero::Point>,
        Base::Span<const std::uint32_t>) noexcept = nullptr;
    void (*release)(void*, Render::RenderMeshId) noexcept = nullptr;
};

struct TextConfig {
    Text::FontFace face;
    Base::Span<const Text::FontFace> fallbackFaces;
    float pixelSize = 16.0F;
    float lineHeight = 0.0F;
    TextWrapping wrapping = TextWrapping::NoWrap;
    TextTrimming trimming = TextTrimming::None;
    TextAlignment alignment = TextAlignment::Start;
    Text::GlyphAtlasConfig atlas;
    Render::RenderGlyphRunId firstGlyphRunId = UINT64_C(1) << 32U;
};

struct TextResources {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Internal::TextBlockLayout*> (*create)(
        void*, Text::FontManager&, const TextConfig&,
        Base::IAllocator&) noexcept = nullptr;
    void (*destroy)(
        void*, Internal::TextBlockLayout*) noexcept = nullptr;
    Base::Result<std::uint32_t> (*collect)(
        void*, Internal::TextBlockLayout*) noexcept = nullptr;
};

// One resource seam between View and the selected native renderer. It replaces
// three parallel Contract/Service headers and one virtual dispatch per resource
// family with a single lightweight value.
struct RenderResources {
    TextResources* text = nullptr;
    MeshResources* meshes = nullptr;
    ImageResources* images = nullptr;
};

} // namespace Aero::Internal
