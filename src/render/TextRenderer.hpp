#pragma once

#include "DisplayList.hpp"

#include "../controls/TextBlockLayout.hpp"
#include "RenderResources.hpp"

#include "render/RenderDevice.hpp"

#include <cstdint>

namespace Aero::Render::Detail {

using TextConfig = Aero::Render::Detail::TextConfig;

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
    : public ::Aero::Controls::Detail::TextBlockLayout {
public:
    TextRenderer(
        Text::FontManager& fonts,
        Graphics::GraphicsDevice& device,
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
        Graphics::GraphicsDevice& device,
        GlyphRunResourceSink& sink) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> ShapeAndPrepare(
        const ::Aero::Controls::Detail::TextLayoutRequest& request,
        ::Aero::Controls::Detail::TextLayoutResult& output) noexcept override;
    void ReleaseGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept override;

    Base::Result<std::uint32_t> CollectGarbage() noexcept;

private:
    struct Impl;

    Text::FontManager* fonts_ = nullptr;
    Graphics::GraphicsDevice* device_ = nullptr;
    GlyphRunResourceSink* sink_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render::Detail
