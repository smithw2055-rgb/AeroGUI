#pragma once

#include "DisplayList.hpp"
#include "RenderResources.hpp"
#include "gui/controls/TextBlockLayout.hpp"
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/Texture.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Text { class FontManager; }

namespace Aero::Render {

class UiFrameEncoder;
struct TextRendererState;
using TextConfig = Aero::Render::TextConfig;

class TextRenderer : public ::Aero::Controls::TextBlockLayout {
public:
    TextRenderer(
        Text::FontManager& fonts,
        RenderDevice& device,
        UiFrameEncoder* encoder = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextRenderer() override;

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    Base::Result<void> Initialize(const TextConfig& config) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> ShapeAndPrepare(
        const ::Aero::Controls::TextLayoutRequest& request,
        ::Aero::Controls::TextLayoutResult& output) noexcept override;
    void ReleaseGlyphRun(Render::RenderGlyphRunId glyphRun) noexcept override;

    Base::Result<std::uint32_t> CollectGarbage() noexcept;

private:
    Text::FontManager* fonts_ = nullptr;
    RenderDevice* device_ = nullptr;
    UiFrameEncoder* encoder_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    alignas(std::max_align_t) std::uint8_t stateStorage_[16384]{};
    TextRendererState* state_ = nullptr;
};

} // namespace Aero::Render
