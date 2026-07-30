#pragma once

#include "ImageResourceContract.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Markup {
class SourceProviderRegistry;
}

namespace Aero::Presentation {
class Visual;
}

namespace Aero::Detail {

class ImageRuntime final {
public:
    explicit ImageRuntime(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~ImageRuntime() noexcept;

    ImageRuntime(const ImageRuntime&) = delete;
    ImageRuntime& operator=(
        const ImageRuntime&) = delete;

    Base::Result<bool> Synchronize(
        Presentation::Visual* root,
        const Base::ResourceUri& documentUri,
        Markup::SourceProviderRegistry& sources,
        ImageBackendServices* backend,
        bool backendGenerationChanged) noexcept;
    void ReleaseBackendResources(
        ImageBackendServices* backend) noexcept;
    void Shutdown(
        ImageBackendServices* backend) noexcept;

private:
    struct Record;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Record> records_;
    std::uint64_t epoch_ = 0U;
    Presentation::RenderImageId
        nextHeadlessImage_ =
            UINT64_C(1) << 40U;
};

} // namespace Aero::Detail
