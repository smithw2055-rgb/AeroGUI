#pragma once

#include "DisplayList.hpp"

#include "FrameEncoder.hpp"
#include "render/RenderResources.hpp"
#include "render/RenderDeviceInternal.hpp"

#include <limits>

namespace Aero::Render::Detail {

class ImageGpuResources {
public:
    ImageGpuResources(
        Aero::RenderDevice::Impl& device,
        BatchComposer& renderer,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : device_(&device),
          renderer_(&renderer),
          allocator_(&allocator),
          resources_(&allocator) {
        table_.generation = generation;
        table_.context = this;
        table_.create =
            [](void* context,
               std::uint32_t width,
               std::uint32_t height,
               Base::Span<const std::uint8_t>
                   pixels) noexcept
                -> Base::Result<
                    Render::RenderImageId> {
                return static_cast<
                    ImageGpuResources*>(
                        context)->Create(
                            width, height, pixels);
            };
        table_.release =
            [](void* context,
               Render::RenderImageId image)
                noexcept {
                static_cast<ImageGpuResources*>(
                    context)->Release(image);
            };
    }

    ~ImageGpuResources() noexcept {
        Shutdown();
    }

    Aero::Render::Detail::ImageResources&
    Table() noexcept {
        return table_;
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
    struct Resource {
        Render::RenderImageId id =
            Render::InvalidRenderImageId;
        Graphics::ResourceHandle texture;
    };

    Base::Result<void> EnsureSampler() noexcept {
        if (device_->IsAlive(sampler_)) return {};
        Graphics::SamplerDescriptor descriptor;
        descriptor.minFilter =
            Graphics::FilterMode::Linear;
        descriptor.magFilter =
            Graphics::FilterMode::Linear;
        descriptor.mipFilter =
            Graphics::FilterMode::Nearest;
        Base::Result<Graphics::ResourceHandle> made =
            device_->CreateSampler(descriptor);
        if (!made) return made.GetStatus();
        sampler_ = made.Value();
        return {};
    }

    Base::Result<Render::RenderImageId>
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

        Graphics::TextureResourceDescriptor descriptor;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.format =
            Graphics::GraphicsTextureFormat::Rgba8Unorm;
        descriptor.usage =
            Graphics::TextureUsageBit(
                Graphics::TextureUsage::Sampled) |
            Graphics::TextureUsageBit(
                Graphics::TextureUsage::CopyDestination);
        Base::Result<Graphics::ResourceHandle> texture =
            device_->CreateTexture(descriptor);
        if (!texture) return texture.GetStatus();

        ::Aero::Render::Detail::RenderBatchBuilder encoder(allocator_);
        Graphics::TextureRegion region;
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
        Base::Result<::Aero::Render::Detail::RenderBatch> commands =
            encoder.Finish();
        if (!commands) {
            static_cast<void>(
                device_->DestroyResource(
                    texture.Value()));
            return commands.GetStatus();
        }
        Base::Result<Graphics::FenceValue> submitted =
            device_->SubmitBatch(commands.Value());
        if (!submitted) {
            static_cast<void>(
                device_->DestroyResource(
                    texture.Value()));
            return submitted.GetStatus();
        }
        if (nextImage_ ==
            Render::InvalidRenderImageId) {
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
            resources_.PushBack(resource);
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
        Render::RenderImageId image) noexcept {
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
                Render::InvalidRenderImageId) {
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

    Aero::RenderDevice::Impl* device_ = nullptr;
    BatchComposer* renderer_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Resource> resources_;
    Graphics::ResourceHandle sampler_;
    Render::RenderImageId nextImage_ =
        UINT64_C(1) << 44U;
    Aero::Render::Detail::ImageResources table_;
};

} // namespace Aero::Render::Detail
