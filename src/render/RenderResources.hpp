#pragma once

#include "DisplayList.hpp"
#include "controls/TextBlockLayout.hpp"
#include "text/GlyphAtlas.hpp"
#include "text/TextLayout.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Text/TextTypes.hpp>

#include <cstdint>

namespace Aero::Text { class FontManager; }

namespace Aero::Detail {

struct ImageResources final {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Render::RenderImageId> (*create)(
        void*, std::uint32_t, std::uint32_t,
        Base::Span<const std::uint8_t>) noexcept = nullptr;
    void (*release)(void*, Render::RenderImageId) noexcept = nullptr;
};

struct MeshResources final {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Render::RenderMeshId> (*create)(
        void*, Base::Span<const Aero::Point>,
        Base::Span<const std::uint32_t>) noexcept = nullptr;
    void (*release)(void*, Render::RenderMeshId) noexcept = nullptr;
};

struct TextConfig final {
    Text::FontFace face;
    Base::Span<const Text::FontFace> fallbackFaces;
    float pixelSize = 16.0F;
    float lineHeight = 0.0F;
    Text::TextWrapping wrapping = Text::TextWrapping::NoWrap;
    Text::TextTrimming trimming = Text::TextTrimming::None;
    Text::TextAlignment alignment = Text::TextAlignment::Start;
    Text::GlyphAtlasConfig atlas;
    Render::RenderGlyphRunId firstGlyphRunId = UINT64_C(1) << 32U;
};

struct TextResources final {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Controls::Detail::TextBlockLayout*> (*create)(
        void*, Text::FontManager&, const TextConfig&,
        Base::IAllocator&) noexcept = nullptr;
    void (*destroy)(
        void*, Controls::Detail::TextBlockLayout*) noexcept = nullptr;
    Base::Result<std::uint32_t> (*collect)(
        void*, Controls::Detail::TextBlockLayout*) noexcept = nullptr;
};

// One resource seam between View and the selected native renderer. It replaces
// three parallel Contract/Service headers and one virtual dispatch per resource
// family with a single lightweight value.
struct RenderResources final {
    TextResources* text = nullptr;
    MeshResources* meshes = nullptr;
    ImageResources* images = nullptr;
};

} // namespace Aero::Detail
