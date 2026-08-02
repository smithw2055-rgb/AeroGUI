#pragma once

#include "render/RenderResources.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Markup {
class SourceProviders;
}

namespace Aero { class Visual; }

namespace Aero::Internal {

class ImageCache final {
public:
    explicit ImageCache(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~ImageCache() noexcept;

    ImageCache(const ImageCache&) = delete;
    ImageCache& operator=(
        const ImageCache&) = delete;

    Base::Result<bool> Synchronize(
        Aero::Visual* root,
        const Base::ResourceUri& documentUri,
        Markup::SourceProviders& sources,
        ImageResources* backend,
        bool backendGenerationChanged) noexcept;
    void ReleaseBackendResources(
        ImageResources* backend) noexcept;
    void Shutdown(
        ImageResources* backend) noexcept;

private:
    struct Record;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Record> records_;
    std::uint64_t epoch_ = 0U;
    Render::RenderImageId
        nextHeadlessImage_ =
            UINT64_C(1) << 40U;
};

} // namespace Aero::Internal
