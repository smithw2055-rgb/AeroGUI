#include "ImageRuntime.hpp"

#include "ImageControlAccess.hpp"
#include "presentation/ImageBrushAccess.hpp"

#include <Aero/Controls/Images.hpp>
#include <Aero/Controls/Shapes.hpp>
#include "markup/Loader.hpp"
#include <Aero/Presentation/Brushes.hpp>
#include <Aero/Presentation/Images.hpp>
#include <Aero/Presentation/ObjectTree.hpp>

#include <limits>
#include <utility>

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Aero::Detail {
namespace {

Base::Status InvalidImage(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Base::Result<Base::ResourceUri> ResolveImageUri(
    const Base::ResourceUri& document,
    const Base::ResourceUri& authored) noexcept {
    if (authored.Empty()) {
        return InvalidImage(
            "BitmapImage UriSource cannot be empty");
    }
    if (authored.IsAbsolute() ||
        document.Empty()) {
        return authored;
    }
    return Base::ResourceUri::Resolve(
        document, authored.Canonical());
}

} // namespace

struct ImageRuntime::Record final {
    Base::ResourceUri resolvedUri;
    Base::Vector<std::uint8_t> pixels;
    Presentation::RenderImageId renderImage =
        Presentation::InvalidRenderImageId;
    std::uint64_t backendGeneration = 0U;
    std::uint64_t seenEpoch = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;

    explicit Record(
        Base::IAllocator* allocator = nullptr) noexcept
        : pixels(allocator) {}
};

ImageRuntime::ImageRuntime(
    Base::IAllocator* allocator) noexcept
    : allocator_(
          allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      records_(allocator_) {}

ImageRuntime::~ImageRuntime() noexcept {
    Shutdown(nullptr);
}

Base::Result<bool> ImageRuntime::Synchronize(
    Presentation::Visual* root,
    const Base::ResourceUri& documentUri,
    Markup::SourceProviderRegistry& sources,
    ImageBackendServices* backend,
    bool backendGenerationChanged) noexcept {
    ++epoch_;
    if (epoch_ == 0U) ++epoch_;
    bool changed = false;

    if (backendGenerationChanged) {
        for (Record& record : records_) {
            record.renderImage =
                Presentation::InvalidRenderImageId;
            record.backendGeneration = 0U;
        }
    }

    Base::Vector<Presentation::Visual*> pending(
        allocator_);
    if (root != nullptr) {
        Base::Result<void> queued =
            pending.TryPushBack(root);
        if (!queued) return queued.GetStatus();
    }
    while (!pending.Empty()) {
        Presentation::Visual* visual =
            pending[pending.Size() - 1U];
        pending.PopBack();
        if (visual == nullptr) continue;
        for (Presentation::Visual* child :
             visual->VisualChildren()) {
            Base::Result<void> queued =
                pending.TryPushBack(child);
            if (!queued) return queued.GetStatus();
        }
        Controls::Image* imageControl = nullptr;
        Presentation::ImageBrush* imageBrush =
            nullptr;
        Base::Ref<Presentation::ImageSource>
            source;
        if (visual->RuntimeType() ==
                Controls::Image::StaticTypeId()) {
            imageControl =
                static_cast<Controls::Image*>(
                    visual);
            source = imageControl->Source();
        } else {
            Controls::Shape* shape = nullptr;
            if (visual->RuntimeType() ==
                Controls::Rectangle::StaticTypeId()) {
                shape =
                    static_cast<
                        Controls::Rectangle*>(
                            visual);
            } else if (
                visual->RuntimeType() ==
                Controls::Ellipse::StaticTypeId()) {
                shape =
                    static_cast<
                        Controls::Ellipse*>(
                            visual);
            }
            if (shape == nullptr) continue;
            Base::Ref<Presentation::Brush>
                fill = shape->FillBrush();
            if (!fill ||
                fill->RuntimeType() !=
                    Presentation::ImageBrush::
                        StaticTypeId()) {
                continue;
            }
            imageBrush =
                static_cast<
                    Presentation::ImageBrush*>(
                        fill.Get());
            source = imageBrush->Source();
        }
        if (!source) {
            Base::Result<void> cleared =
                imageControl != nullptr
                ? ImageControlAccess::
                    SetRuntimeImage(
                    *imageControl,
                    Presentation::
                        InvalidRenderImageId,
                    0U, 0U)
                : ImageBrushAccess::
                    SetRuntimeImage(
                    *imageBrush,
                    Presentation::
                        InvalidRenderImageId,
                    0U, 0U);
            if (!cleared) {
                return cleared.GetStatus();
            }
            continue;
        }
        if (source->RuntimeType() !=
            Presentation::BitmapImage::
                StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Image and ImageBrush currently require BitmapImage sources");
        }
        auto* bitmap =
            static_cast<
                Presentation::BitmapImage*>(
                    source.Get());
        Base::Result<Base::ResourceUri> resolved =
            ResolveImageUri(
                documentUri,
                bitmap->UriSource());
        if (!resolved) return resolved.GetStatus();

        Record* record = nullptr;
        for (Record& candidate : records_) {
            if (candidate.resolvedUri ==
                    resolved.Value()) {
                record = &candidate;
                break;
            }
        }
        if (record == nullptr) {
            Record created(allocator_);
            created.resolvedUri =
                resolved.Value();
            Base::Result<void> stored =
                records_.TryPushBack(
                    std::move(created));
            if (!stored) return stored.GetStatus();
            record =
                &records_[records_.Size() - 1U];
        }
        record->seenEpoch = epoch_;

        if (record->resolvedUri !=
                resolved.Value() ||
            record->pixels.Empty()) {
            if (record->renderImage !=
                    Presentation::
                        InvalidRenderImageId &&
                backend != nullptr &&
                backend->generation ==
                    record->backendGeneration &&
                backend->releaseImage != nullptr) {
                backend->releaseImage(
                    backend->context,
                    record->renderImage);
            }
            record->renderImage =
                Presentation::InvalidRenderImageId;
            record->backendGeneration = 0U;
            record->pixels.Clear();
            record->width = 0U;
            record->height = 0U;

            Base::Result<
                Markup::SourceProviderResolution>
                provider =
                    sources.ResolveDetailed(
                        resolved.Value());
            if (!provider ||
                provider.Value().provider ==
                    nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Image SourceProvider was not found");
            }
            Base::Result<Integration::Source>
                loaded =
                    provider.Value().provider->Load(
                        resolved.Value());
            if (!loaded) return loaded.GetStatus();
            if (loaded.Value().bytes.Empty() ||
                loaded.Value().bytes.Size() >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<int>::
                            max())) {
                return InvalidImage(
                    "Image source bytes are empty or too large");
            }
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* decoded =
                stbi_load_from_memory(
                    loaded.Value().bytes.Data(),
                    static_cast<int>(
                        loaded.Value().bytes.Size()),
                    &width, &height, &channels, 4);
            if (decoded == nullptr ||
                width <= 0 || height <= 0 ||
                width >
                    static_cast<int>(
                        std::numeric_limits<
                            std::uint32_t>::max() /
                        4U) ||
                static_cast<std::uint64_t>(width) *
                    static_cast<std::uint64_t>(
                        height) * 4U >
                    UINT32_MAX) {
                if (decoded != nullptr) {
                    stbi_image_free(decoded);
                }
                return InvalidImage(
                    "Image decode failed or dimensions exceed runtime limits");
            }
            const std::uint32_t byteCount =
                static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(
                        width) *
                    static_cast<std::uint64_t>(
                        height) * 4U);
            Base::Result<void> resized =
                record->pixels.TryResize(
                    byteCount);
            if (!resized) {
                stbi_image_free(decoded);
                return resized.GetStatus();
            }
            for (std::uint32_t index = 0U;
                 index < byteCount; ++index) {
                record->pixels[index] =
                    decoded[index];
            }
            stbi_image_free(decoded);
            record->width =
                static_cast<std::uint32_t>(width);
            record->height =
                static_cast<std::uint32_t>(height);
            record->resolvedUri =
                resolved.Value();
            changed = true;
        }

        if (backend != nullptr) {
            if (backend->createImage == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Image backend service is incomplete");
            }
            if (record->renderImage ==
                    Presentation::
                        InvalidRenderImageId ||
                record->backendGeneration !=
                    backend->generation) {
                Base::Result<
                    Presentation::RenderImageId>
                    uploaded = backend->createImage(
                        backend->context,
                        record->width,
                        record->height,
                        record->pixels.AsSpan());
                if (!uploaded) {
                    return uploaded.GetStatus();
                }
                record->renderImage =
                    uploaded.Value();
                record->backendGeneration =
                    backend->generation;
                changed = true;
            }
        } else if (
            record->renderImage ==
                Presentation::
                    InvalidRenderImageId) {
            if (nextHeadlessImage_ ==
                Presentation::
                    InvalidRenderImageId) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Headless image ID space is exhausted");
            }
            record->renderImage =
                nextHeadlessImage_++;
            changed = true;
        }
        Base::Result<void> assigned =
            imageControl != nullptr
            ? ImageControlAccess::SetRuntimeImage(
                *imageControl,
                record->renderImage,
                record->width,
                record->height)
            : ImageBrushAccess::SetRuntimeImage(
                *imageBrush,
                record->renderImage,
                record->width,
                record->height);
        if (!assigned) return assigned.GetStatus();
    }

    for (std::uint32_t index =
             records_.Size();
         index > 0U; --index) {
        Record& record = records_[index - 1U];
        if (record.seenEpoch == epoch_) continue;
        if (backend != nullptr &&
            record.renderImage !=
                Presentation::
                    InvalidRenderImageId &&
            record.backendGeneration ==
                backend->generation &&
            backend->releaseImage != nullptr) {
            backend->releaseImage(
                backend->context,
                record.renderImage);
        }
        for (std::uint32_t next = index;
             next < records_.Size(); ++next) {
            records_[next - 1U] =
                std::move(records_[next]);
        }
        records_.PopBack();
        changed = true;
    }
    return changed;
}

void ImageRuntime::ReleaseBackendResources(
    ImageBackendServices* backend) noexcept {
    for (Record& record : records_) {
        if (backend != nullptr &&
            backend->releaseImage != nullptr &&
            record.renderImage !=
                Presentation::InvalidRenderImageId &&
            record.backendGeneration ==
                backend->generation) {
            backend->releaseImage(
                backend->context,
                record.renderImage);
        }
        record.renderImage =
            Presentation::InvalidRenderImageId;
        record.backendGeneration = 0U;
    }
}

void ImageRuntime::Shutdown(
    ImageBackendServices* backend) noexcept {
    ReleaseBackendResources(backend);
    records_.Clear();
}

} // namespace Aero::Detail
