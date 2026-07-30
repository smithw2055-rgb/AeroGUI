#pragma once

#include "Renderer.hpp"
#include "runtime/ImageResourceContract.hpp"

#include <limits>

namespace Aero::Render::Detail {

class ImageRuntimeBackend final {
public:
    ImageRuntimeBackend(
        Rhi::RhiDevice& device,
        Renderer& renderer,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : device_(&device),
          renderer_(&renderer),
          allocator_(&allocator),
          resources_(&allocator) {
        services_.generation = generation;
        services_.context = this;
        services_.createImage =
            [](void* context,
               std::uint32_t width,
               std::uint32_t height,
               Base::Span<const std::uint8_t>
                   pixels) noexcept
                -> Base::Result<
                    Presentation::RenderImageId> {
                return static_cast<
                    ImageRuntimeBackend*>(
                        context)->Create(
                            width, height, pixels);
            };
        services_.releaseImage =
            [](void* context,
               Presentation::RenderImageId image)
                noexcept {
                static_cast<ImageRuntimeBackend*>(
                    context)->Release(image);
            };
    }

    ~ImageRuntimeBackend() noexcept {
        Shutdown();
    }

    Aero::Detail::ImageBackendServices&
    Services() noexcept {
        return services_;
    }

    void Shutdown() noexcept {
        for (Resource& resource : resources_) {
            Destroy(resource);
        }
        resources_.Clear();
        if (device_ != nullptr &&
            device_->IsAlive(sampler_)) {
            static_cast<void>(
                device_->DestroyResource(
                    sampler_,
                    device_->LastSubmittedFence()));
        }
        sampler_ = {};
    }

private:
    struct Resource final {
        Presentation::RenderImageId id =
            Presentation::InvalidRenderImageId;
        Rhi::ResourceHandle texture;
    };

    Base::Result<void> EnsureSampler() noexcept {
        if (device_->IsAlive(sampler_)) return {};
        Rhi::SamplerDescriptor descriptor;
        descriptor.minFilter =
            Rhi::FilterMode::Linear;
        descriptor.magFilter =
            Rhi::FilterMode::Linear;
        descriptor.mipFilter =
            Rhi::FilterMode::Nearest;
        Base::Result<Rhi::ResourceHandle> made =
            device_->CreateSampler(descriptor);
        if (!made) return made.GetStatus();
        sampler_ = made.Value();
        return {};
    }

    Base::Result<Presentation::RenderImageId>
    Create(
        std::uint32_t width,
        std::uint32_t height,
        Base::Span<const std::uint8_t>
            pixels) noexcept {
        if (device_ == nullptr ||
            renderer_ == nullptr ||
            width == 0U || height == 0U ||
            width >
                std::numeric_limits<
                    std::uint32_t>::max() / 4U ||
            static_cast<std::uint64_t>(width) *
                    height * 4U !=
                pixels.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Image service requires tightly packed RGBA8 pixels");
        }
        Base::Result<void> sampler =
            EnsureSampler();
        if (!sampler) return sampler.GetStatus();

        Rhi::TextureResourceDescriptor descriptor;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.format =
            Rhi::GraphicsTextureFormat::Rgba8Unorm;
        descriptor.usage =
            Rhi::TextureUsageBit(
                Rhi::TextureUsage::Sampled) |
            Rhi::TextureUsageBit(
                Rhi::TextureUsage::CopyDestination);
        Base::Result<Rhi::ResourceHandle> texture =
            device_->CreateTexture(descriptor);
        if (!texture) return texture.GetStatus();

        Rhi::CommandEncoder encoder(allocator_);
        Rhi::TextureRegion region;
        region.width = width;
        region.height = height;
        region.bytesPerRow = width * 4U;
        Base::Result<void> uploaded =
            encoder.UploadTexture(
                texture.Value(), region, pixels);
        if (!uploaded) {
            static_cast<void>(
                device_->DestroyResource(
                    texture.Value()));
            return uploaded.GetStatus();
        }
        Base::Result<Rhi::CommandList> commands =
            encoder.Finish();
        if (!commands) {
            static_cast<void>(
                device_->DestroyResource(
                    texture.Value()));
            return commands.GetStatus();
        }
        Base::Result<Rhi::FenceValue> submitted =
            device_->Submit(commands.Value());
        if (!submitted) {
            static_cast<void>(
                device_->DestroyResource(
                    texture.Value()));
            return submitted.GetStatus();
        }
        if (nextImage_ ==
            Presentation::InvalidRenderImageId) {
            static_cast<void>(
                device_->DestroyResource(
                    texture.Value(),
                    submitted.Value()));
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Image service ID space is exhausted");
        }

        Resource resource;
        resource.id = nextImage_++;
        resource.texture = texture.Value();
        Base::Result<void> registered =
            renderer_->RegisterImage(
                resource.id,
                resource.texture,
                sampler_);
        if (!registered) {
            static_cast<void>(
                device_->DestroyResource(
                    resource.texture,
                    submitted.Value()));
            return registered.GetStatus();
        }
        Base::Result<void> stored =
            resources_.TryPushBack(resource);
        if (!stored) {
            static_cast<void>(
                renderer_->UnregisterImage(
                    resource.id));
            static_cast<void>(
                device_->DestroyResource(
                    resource.texture,
                    submitted.Value()));
            return stored.GetStatus();
        }
        return resource.id;
    }

    void Release(
        Presentation::RenderImageId image) noexcept {
        for (std::uint32_t index = 0U;
             index < resources_.Size(); ++index) {
            if (resources_[index].id != image) {
                continue;
            }
            Destroy(resources_[index]);
            for (std::uint32_t next = index + 1U;
                 next < resources_.Size(); ++next) {
                resources_[next - 1U] =
                    resources_[next];
            }
            resources_.PopBack();
            return;
        }
    }

    void Destroy(Resource& resource) noexcept {
        if (renderer_ != nullptr &&
            resource.id !=
                Presentation::InvalidRenderImageId) {
            static_cast<void>(
                renderer_->UnregisterImage(
                    resource.id));
        }
        if (device_ != nullptr &&
            device_->IsAlive(resource.texture)) {
            static_cast<void>(
                device_->DestroyResource(
                    resource.texture,
                    device_->LastSubmittedFence()));
        }
        resource = {};
    }

    Rhi::RhiDevice* device_ = nullptr;
    Renderer* renderer_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Resource> resources_;
    Rhi::ResourceHandle sampler_;
    Presentation::RenderImageId nextImage_ =
        UINT64_C(1) << 44U;
    Aero::Detail::ImageBackendServices services_;
};

} // namespace Aero::Render::Detail
