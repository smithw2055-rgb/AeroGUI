#pragma once

#include "render/RenderResources.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/TextureProvider.hpp>

namespace Aero::Markup {
class XamlProviderRegistry;
}

namespace Aero::Media { class Visual; }

namespace Aero::Media::Detail {

class ImageCache {
public:
    explicit ImageCache(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~ImageCache() noexcept;

    ImageCache(const ImageCache&) = delete;
    ImageCache& operator=(
        const ImageCache&) = delete;

    Base::Result<bool> Synchronize(
        Aero::Media::Visual* root,
        const Base::ResourceUri& documentUri,
        Markup::XamlProviderRegistry& sources,
        Media::TextureProvider* textureProvider,
        ::Aero::Render::Detail::ImageResources* backend,
        bool backendGenerationChanged) noexcept;
    void ReleaseBackendResources(
        ::Aero::Render::Detail::ImageResources* backend) noexcept;
    void Shutdown(
        ::Aero::Render::Detail::ImageResources* backend) noexcept;

private:
    struct Record;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Record> records_;
    std::uint64_t epoch_ = 0U;
    Render::RenderImageId
        nextHeadlessImage_ =
            UINT64_C(1) << 40U;
};

} // namespace Aero::Media::Detail
