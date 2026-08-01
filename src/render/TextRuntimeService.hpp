#pragma once

#include "DisplayList.hpp"

#include "../controls/TextLayoutService.hpp"
#include "../runtime/TextResourceContract.hpp"

#include "graphics/Graphics.hpp"

#include <cstdint>

namespace Aero::Render::Detail {

using TextRuntimeConfig = Aero::Detail::TextRuntimeConfig;

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

class TextRuntimeService final
    : public Controls::Detail::TextLayoutService {
public:
    TextRuntimeService(
        Text::FontManager& fonts,
        Graphics::GraphicsDevice& device,
        GlyphRunResourceSink& sink,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextRuntimeService() override;

    TextRuntimeService(
        const TextRuntimeService&) = delete;
    TextRuntimeService& operator=(
        const TextRuntimeService&) = delete;

    Base::Result<void> Initialize(
        const TextRuntimeConfig& config) noexcept;
    Base::Result<void> RecoverDeviceResources(
        Graphics::GraphicsDevice& device,
        GlyphRunResourceSink& sink) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> ShapeAndPrepare(
        const Controls::Detail::TextLayoutRequest& request,
        Controls::Detail::TextLayoutResult& output) noexcept override;
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
