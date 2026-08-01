#pragma once

#include "DisplayList.hpp"

#include "Renderer.hpp"
#include "TextRuntimeService.hpp"

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

class TextRuntimeBackend final {
public:
    TextRuntimeBackend(
        Graphics::GraphicsDevice& device,
        Renderer& renderer,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : device_(&device),
          sink_(renderer),
          allocator_(&allocator) {
        services_.generation = generation;
        services_.context = this;
        services_.createService =
            [](void* context,
               Text::FontManager& fonts,
               const Aero::Detail::TextRuntimeConfig& config,
               Base::IAllocator&) noexcept
                -> Base::Result<
                    Controls::Detail::TextLayoutService*> {
                return static_cast<TextRuntimeBackend*>(
                    context)->Create(fonts, config);
            };
        services_.destroyService =
            [](void* context,
               Controls::Detail::TextLayoutService* service) noexcept {
                static_cast<TextRuntimeBackend*>(
                    context)->Destroy(service);
            };
        services_.collectGarbage =
            [](void* context,
               Controls::Detail::TextLayoutService* service) noexcept
                -> Base::Result<std::uint32_t> {
                return static_cast<TextRuntimeBackend*>(
                    context)->Collect(service);
            };
    }

    ~TextRuntimeBackend() noexcept {
        Shutdown();
    }

    Aero::Detail::TextBackendServices& Services() noexcept {
        return services_;
    }

    void Shutdown() noexcept {
        if (service_ == nullptr) return;
        service_->Shutdown();
        service_->~TextRuntimeService();
        allocator_->Deallocate(
            service_,
            sizeof(TextRuntimeService),
            alignof(TextRuntimeService),
            Base::MemoryTag::Render);
        service_ = nullptr;
    }

private:
    Base::Result<Controls::Detail::TextLayoutService*>
    Create(
        Text::FontManager& fonts,
        const Aero::Detail::TextRuntimeConfig& config) noexcept {
        if (service_ != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Endpoint text service is already bound");
        }
        void* memory = allocator_->Allocate({
            sizeof(TextRuntimeService),
            alignof(TextRuntimeService),
            Base::MemoryTag::Render});
        if (memory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Endpoint text service allocation failed");
        }
        service_ = new (memory)
            TextRuntimeService(
                fonts, *device_, sink_, allocator_);
        Base::Result<void> initialized =
            service_->Initialize(config);
        if (!initialized) {
            Base::Status failure =
                initialized.GetStatus();
            Shutdown();
            return failure;
        }
        return static_cast<
            Controls::Detail::TextLayoutService*>(
                service_);
    }

    void Destroy(
        Controls::Detail::TextLayoutService* service) noexcept {
        if (service == service_) Shutdown();
    }

    Base::Result<std::uint32_t> Collect(
        Controls::Detail::TextLayoutService* service) noexcept {
        if (service == nullptr || service != service_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Endpoint text service lease is stale");
        }
        return service_->CollectGarbage();
    }

    Graphics::GraphicsDevice* device_ = nullptr;
    RendererGlyphRunSink sink_;
    Base::IAllocator* allocator_ = nullptr;
    TextRuntimeService* service_ = nullptr;
    Aero::Detail::TextBackendServices services_;
};

} // namespace Aero::Render::Detail
