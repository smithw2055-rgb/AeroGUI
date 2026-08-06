#pragma once

#include "DisplayList.hpp"

#include "FrameEncoder.hpp"
#include "TextRenderer.hpp"

#include <Aero/Base/Vector.hpp>

#include <new>

namespace Aero::Render::Detail {

class RendererGlyphRunSink
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

class TextGpuResources {
public:
    TextGpuResources(
        Graphics::GraphicsDevice& device,
        Renderer& renderer,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : device_(&device),
          sink_(renderer),
          allocator_(&allocator),
          renderers_(&allocator) {
        table_.generation = generation;
        table_.context = this;
        table_.create =
            [](void* context,
               Text::FontManager& fonts,
               const Aero::Render::Detail::TextConfig& config,
               Base::IAllocator&) noexcept
                -> Base::Result<
                    ::Aero::Controls::Detail::TextBlockLayout*> {
                return static_cast<TextGpuResources*>(
                    context)->Create(fonts, config);
            };
        table_.destroy =
            [](void* context,
               ::Aero::Controls::Detail::TextBlockLayout* layout) noexcept {
                static_cast<TextGpuResources*>(
                    context)->Destroy(layout);
            };
        table_.collect =
            [](void* context,
               ::Aero::Controls::Detail::TextBlockLayout* layout) noexcept
                -> Base::Result<std::uint32_t> {
                return static_cast<TextGpuResources*>(
                    context)->Collect(layout);
            };
    }

    ~TextGpuResources() noexcept {
        Shutdown();
    }

    Aero::Render::Detail::TextResources& Table() noexcept {
        return table_;
    }

    void Shutdown() noexcept {
        while (!renderers_.Empty()) {
            TextRenderer* renderer =
                renderers_.Back();
            renderers_.PopBack();
            DestroyRenderer(renderer);
        }
    }

private:
    static constexpr std::uint64_t GlyphRunNamespaceSize =
        UINT64_C(1) << 32U;

    void DestroyRenderer(
        TextRenderer* renderer) noexcept {
        if (renderer == nullptr) return;
        renderer->Shutdown();
        renderer->~TextRenderer();
        allocator_->Deallocate(
            renderer,
            sizeof(TextRenderer),
            alignof(TextRenderer),
            Base::MemoryTag::Render);
    }

    Base::Result<::Aero::Controls::Detail::TextBlockLayout*>
    Create(
        Text::FontManager& fonts,
        const Aero::Render::Detail::TextConfig& config) noexcept {
        if (nextGlyphRunBase_ >
            UINT64_MAX - GlyphRunNamespaceSize) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Render-device glyph-run namespace is exhausted");
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
        TextRenderer* renderer = new (memory)
            TextRenderer(
                fonts, *device_, sink_, allocator_);
        Aero::Render::Detail::TextConfig selected =
            config;
        selected.firstGlyphRunId =
            nextGlyphRunBase_;
        Base::Result<void> initialized =
            renderer->Initialize(selected);
        if (!initialized) {
            Base::Status failure =
                initialized.GetStatus();
            DestroyRenderer(renderer);
            return failure;
        }
        Base::Result<void> stored =
            renderers_.PushBack(renderer);
        if (!stored) {
            Base::Status failure =
                stored.GetStatus();
            DestroyRenderer(renderer);
            return failure;
        }
        nextGlyphRunBase_ +=
            GlyphRunNamespaceSize;
        return static_cast<
            ::Aero::Controls::Detail::TextBlockLayout*>(
                renderer);
    }

    void Destroy(
        ::Aero::Controls::Detail::TextBlockLayout* layout) noexcept {
        if (layout == nullptr) return;
        for (std::uint32_t index = 0U;
             index < renderers_.Size(); ++index) {
            if (renderers_[index] != layout) {
                continue;
            }
            TextRenderer* renderer =
                renderers_[index];
            for (std::uint32_t next = index + 1U;
                 next < renderers_.Size(); ++next) {
                renderers_[next - 1U] =
                    renderers_[next];
            }
            renderers_.PopBack();
            DestroyRenderer(renderer);
            return;
        }
    }

    Base::Result<std::uint32_t> Collect(
        ::Aero::Controls::Detail::TextBlockLayout* layout) noexcept {
        if (layout == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Render-device text layout is stale");
        }
        for (TextRenderer* renderer : renderers_) {
            if (renderer == layout) {
                return renderer->CollectGarbage();
            }
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Render-device text layout is stale");
    }

    Graphics::GraphicsDevice* device_ = nullptr;
    RendererGlyphRunSink sink_;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<TextRenderer*> renderers_;
    std::uint64_t nextGlyphRunBase_ =
        UINT64_C(1) << 32U;
    Aero::Render::Detail::TextResources table_;
};

} // namespace Aero::Render::Detail
