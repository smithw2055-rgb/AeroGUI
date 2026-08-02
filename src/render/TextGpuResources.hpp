#pragma once

#include "DisplayList.hpp"

#include "Renderer.hpp"
#include "TextRenderer.hpp"

#include <new>

namespace Aero::Render::Detail {

class RendererGlyphRunSink final
    : public GlyphRunResourceSink {
public:
    explicit RendererGlyphRunSink(
        Renderer& renderer) noexcept
        : renderer_(&renderer) {}

    Base::Result<void> RegisterGlyphRun(
        Render::RenderGlyphRunId glyphRun,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::ResourceHandle atlasTexture,
        Graphics::ResourceHandle sampler,
        Graphics::IndexType indexType) noexcept override {
        return renderer_->RegisterGlyphRun(
            glyphRun,
            vertexBuffer,
            indexBuffer,
            indexCount,
            atlasTexture,
            sampler,
            indexType);
    }

    Base::Result<void> UnregisterGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept override {
        return renderer_->UnregisterGlyphRun(glyphRun);
    }

private:
    Renderer* renderer_ = nullptr;
};

class TextGpuResources final {
public:
    TextGpuResources(
        Graphics::GraphicsDevice& device,
        Renderer& renderer,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : device_(&device),
          sink_(renderer),
          allocator_(&allocator) {
        table_.generation = generation;
        table_.context = this;
        table_.create =
            [](void* context,
               Text::FontManager& fonts,
               const Aero::Internal::TextConfig& config,
               Base::IAllocator&) noexcept
                -> Base::Result<
                    Internal::TextBlockLayout*> {
                return static_cast<TextGpuResources*>(
                    context)->Create(fonts, config);
            };
        table_.destroy =
            [](void* context,
               Internal::TextBlockLayout* layout) noexcept {
                static_cast<TextGpuResources*>(
                    context)->Destroy(layout);
            };
        table_.collect =
            [](void* context,
               Internal::TextBlockLayout* layout) noexcept
                -> Base::Result<std::uint32_t> {
                return static_cast<TextGpuResources*>(
                    context)->Collect(layout);
            };
    }

    ~TextGpuResources() noexcept {
        Shutdown();
    }

    Aero::Internal::TextResources& Table() noexcept {
        return table_;
    }

    void Shutdown() noexcept {
        if (renderer_ == nullptr) return;
        renderer_->Shutdown();
        renderer_->~TextRenderer();
        allocator_->Deallocate(
            renderer_,
            sizeof(TextRenderer),
            alignof(TextRenderer),
            Base::MemoryTag::Render);
        renderer_ = nullptr;
    }

private:
    Base::Result<Internal::TextBlockLayout*>
    Create(
        Text::FontManager& fonts,
        const Aero::Internal::TextConfig& config) noexcept {
        if (renderer_ != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Render-device text renderer is already bound");
        }
        void* memory = allocator_->Allocate({
            sizeof(TextRenderer),
            alignof(TextRenderer),
            Base::MemoryTag::Render});
        if (memory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Render-device text renderer allocation failed");
        }
        renderer_ = new (memory)
            TextRenderer(
                fonts, *device_, sink_, allocator_);
        Base::Result<void> initialized =
            renderer_->Initialize(config);
        if (!initialized) {
            Base::Status failure =
                initialized.GetStatus();
            Shutdown();
            return failure;
        }
        return static_cast<
            Internal::TextBlockLayout*>(
                renderer_);
    }

    void Destroy(
        Internal::TextBlockLayout* layout) noexcept {
        if (layout == renderer_) Shutdown();
    }

    Base::Result<std::uint32_t> Collect(
        Internal::TextBlockLayout* layout) noexcept {
        if (layout == nullptr || layout != renderer_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Render-device text layout is stale");
        }
        return renderer_->CollectGarbage();
    }

    Graphics::GraphicsDevice* device_ = nullptr;
    RendererGlyphRunSink sink_;
    Base::IAllocator* allocator_ = nullptr;
    TextRenderer* renderer_ = nullptr;
    Aero::Internal::TextResources table_;
};

} // namespace Aero::Render::Detail
