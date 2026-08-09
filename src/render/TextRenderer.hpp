#pragma once

#include "DisplayList.hpp"

#include "gui/controls/TextBlockLayout.hpp"
#include "RenderResources.hpp"

#include "render/GraphicsTypes.hpp"
#include <AeroRender/RenderDevice.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Render {

struct TextRendererState;

using TextConfig = Aero::Render::TextConfig;

class GlyphRunResourceSink {
public:
    virtual ~GlyphRunResourceSink() = default;

    virtual Base::Result<void> RegisterGlyphRun(
        Render::RenderGlyphRunId glyphRun,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::ResourceHandle atlasTexture,
        Graphics::ResourceHandle sampler,
        Graphics::IndexType indexType) noexcept = 0;
    virtual Base::Result<void> UnregisterGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept = 0;
};

class TextRenderer
    : public ::Aero::Controls::TextBlockLayout {
public:
    TextRenderer(
        Text::FontManager& fonts,
        Aero::Render::RenderDeviceBase& device,
        GlyphRunResourceSink& sink,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextRenderer() override;

    TextRenderer(
        const TextRenderer&) = delete;
    TextRenderer& operator=(
        const TextRenderer&) = delete;

    Base::Result<void> Initialize(
        const TextConfig& config) noexcept;
    Base::Result<void> RecoverDeviceResources(
        Aero::Render::RenderDeviceBase& device,
        GlyphRunResourceSink& sink) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> ShapeAndPrepare(
        const ::Aero::Controls::TextLayoutRequest& request,
        ::Aero::Controls::TextLayoutResult& output) noexcept override;
    void ReleaseGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept override;

    Base::Result<std::uint32_t> CollectGarbage() noexcept;

private:
    Text::FontManager* fonts_ = nullptr;
    Aero::Render::RenderDeviceBase* device_ = nullptr;
    GlyphRunResourceSink* sink_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    alignas(std::max_align_t) std::uint8_t stateStorage_[16384]{};
    TextRendererState* state_ = nullptr;
};

} // namespace Aero::Render
