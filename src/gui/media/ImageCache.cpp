#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include "render/DisplayList.hpp"
#include "ImageCache.hpp"

#include "gui/media/MediaState.hpp"

#include <Aero/Controls.hpp>
#include <Aero/Shapes.hpp>

#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>

#include <algorithm>
#include <limits>
#include <utility>

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

#include "gui/core/facets/RenderFacet.hpp"

namespace Aero::Media {
using ImageResources = ::Aero::Render::ImageResources;

namespace {

struct StbiStreamContext {
    Base::Stream* stream = nullptr;
    bool failed = false;
    bool eof = false;
};

int ReadStbiStream(void* user, char* data, int size) {
    auto* context = static_cast<StbiStreamContext*>(user);
    if (context == nullptr || context->stream == nullptr ||
        size <= 0 || context->failed) {
        return 0;
    }
    Base::Result<std::uint32_t> read = context->stream->Read(
        {reinterpret_cast<std::uint8_t*>(data),
         static_cast<std::uint32_t>(size)});
    if (!read) {
        context->failed = true;
        return 0;
    }
    context->eof = read.Value() == 0U;
    return static_cast<int>(read.Value());
}

void SkipStbiStream(void* user, int count) {
    auto* context = static_cast<StbiStreamContext*>(user);
    if (context == nullptr || context->stream == nullptr ||
        context->failed || count == 0) {
        return;
    }
    if (context->stream->CanSeek()) {
        Base::Result<std::uint64_t> sought = context->stream->Seek(
            count, Base::SeekOrigin::Current);
        if (!sought) context->failed = true;
        return;
    }
    if (count < 0) {
        context->failed = true;
        return;
    }
    std::uint8_t discarded[256];
    std::uint32_t remaining = static_cast<std::uint32_t>(count);
    while (remaining != 0U) {
        const std::uint32_t request =
            std::min<std::uint32_t>(remaining, sizeof(discarded));
        Base::Result<std::uint32_t> read = context->stream->Read(
            {discarded, request});
        if (!read || read.Value() == 0U) {
            context->failed = true;
            context->eof = true;
            return;
        }
        remaining -= read.Value();
    }
}

int StbiStreamEof(void* user) {
    const auto* context = static_cast<const StbiStreamContext*>(user);
    return context == nullptr || context->failed || context->eof ? 1 : 0;
}

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

struct ImageCache::Record {
    Base::ResourceUri resolvedUri;
    Base::Vector<std::uint8_t> pixels;
    Render::RenderImageId renderImage =
        Render::InvalidRenderImageId;
    std::uint64_t backendGeneration = 0U;
    std::uint64_t seenEpoch = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    Base::Rect sourceRect;

    explicit Record(
        Base::IAllocator* allocator = nullptr) noexcept
        : pixels(allocator) {}
};

ImageCache::ImageCache(
    Base::IAllocator* allocator) noexcept
    : allocator_(
          allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      records_(allocator_) {}

ImageCache::~ImageCache() noexcept {
    Shutdown(nullptr);
}

bool ImageCache::Invalidate(
    const Base::ResourceUri& uri,
    ImageResources* backend) noexcept {
    bool changed = false;
    for (Record& record : records_) {
        if (!uri.Empty() && record.resolvedUri != uri) continue;
        if (record.renderImage != Render::InvalidRenderImageId &&
            backend != nullptr &&
            backend->generation == record.backendGeneration &&
            backend->release != nullptr) {
            backend->release(backend->context, record.renderImage);
        }
        record.renderImage = Render::InvalidRenderImageId;
        record.backendGeneration = 0U;
        record.pixels.Clear();
        record.width = 0U;
        record.height = 0U;
        changed = true;
    }
    return changed;
}

Base::Result<bool> ImageCache::Synchronize(
    Aero::Media::Visual* root,
    const Base::ResourceUri& documentUri,
    Markup::XamlProviderRegistry& sources,
    Media::TextureProvider* textureProvider,
    ImageResources* backend,
    bool backendGenerationChanged) noexcept {
    ++epoch_;
    if (epoch_ == 0U) ++epoch_;
    bool changed = false;

    if (backendGenerationChanged) {
        for (Record& record : records_) {
            record.renderImage =
                Render::InvalidRenderImageId;
            record.backendGeneration = 0U;
        }
    }

    Base::Vector<Aero::Media::Visual*> pending(
        allocator_);
    if (root != nullptr) {
        Base::Result<void> queued =
            pending.PushBack(root);
        if (!queued) return queued.GetStatus();
    }
    while (!pending.Empty()) {
        Aero::Media::Visual* visual =
            pending[pending.Size() - 1U];
        pending.PopBack();
        if (visual == nullptr) continue;
        for (Aero::Media::Visual* child :
             visual->GetVisualChildren()) {
            Base::Result<void> queued =
                pending.PushBack(child);
            if (!queued) return queued.GetStatus();
        }
        // A visual may reference one bitmap for its content/fill and another
        // for OpacityMask. Process both through the same device image table.
        for (std::uint32_t targetIndex = 0U;
             targetIndex < 2U; ++targetIndex) {
        Controls::Image* imageControl = nullptr;
        Media::ImageBrush* imageBrush =
            nullptr;
        Base::Ref<Media::ImageSource>
            source;
        if (targetIndex == 0U &&
            visual->RuntimeType() ==
                Controls::Image::StaticTypeId()) {
            imageControl =
                static_cast<Controls::Image*>(
                    visual);
            source = imageControl->GetSource();
        } else if (targetIndex == 0U) {
            Base::Ref<Media::Brush> fill;
            if (visual->PropertyRegistry().Types().IsDerivedFrom(
                    visual->RuntimeType(),
                    Shapes::Shape::StaticTypeId())) {
                fill = static_cast<Shapes::Shape*>(visual)->GetFill();
            } else if (visual->RuntimeType() ==
                       Controls::Border::StaticTypeId()) {
                fill = static_cast<Controls::Border*>(visual)->GetBackground();
            } else if (visual->PropertyRegistry().Types().IsDerivedFrom(
                           visual->RuntimeType(),
                           Controls::Panel::StaticTypeId())) {
                fill = static_cast<Controls::Panel*>(visual)->GetBackground();
            } else if (visual->PropertyRegistry().Types().IsDerivedFrom(
                           visual->RuntimeType(),
                           Controls::Control::StaticTypeId())) {
                fill = static_cast<Controls::Control*>(visual)->GetBackground();
            } else {
                continue;
            }
            if (!fill ||
                fill->RuntimeType() !=
                    Media::ImageBrush::
                        StaticTypeId()) {
                continue;
            }
            imageBrush =
                static_cast<
                    Media::ImageBrush*>(
                        fill.Get());
            source = imageBrush->GetSource();
        } else {
            UIElement* element = visual->AsUIElement();
            Base::Ref<Media::Brush> mask =
                element != nullptr
                ? element->GetOpacityMask()
                : Base::Ref<Media::Brush>{};
            if (!mask || mask->RuntimeType() !=
                    Media::ImageBrush::StaticTypeId()) {
                continue;
            }
            imageBrush = static_cast<Media::ImageBrush*>(mask.Get());
            source = imageBrush->GetSource();
        }
        if (!source) {
            Base::Result<void> cleared =
                imageControl != nullptr
                ? Core::RenderFacet::SetImageRuntimeData(
                    *imageControl,
                    Render::InvalidRenderImageId,
                    0U, 0U)
                : imageBrush->SetRuntimeImage(
                    Render::InvalidRenderImageId,
                    0U, 0U);
            if (!cleared) {
                return cleared.GetStatus();
            }
            continue;
        }
        if (source->RuntimeType() ==
                Media::BitmapImage::
                    StaticTypeId() ||
            source->RuntimeType() ==
                Media::CroppedBitmap::
                    StaticTypeId()) {
            // CroppedBitmap sources resolve through the wrapped bitmap and
            // produce a cropped sub-region of the decoded atlas.
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Image and ImageBrush currently require BitmapImage or CroppedBitmap sources");
        }
        Media::BitmapImage* bitmap =
            nullptr;
        Base::Rect sourceRect;
        if (source->RuntimeType() ==
            Media::CroppedBitmap::
                StaticTypeId()) {
            auto* cropped =
                static_cast<
                    Media::CroppedBitmap*>(
                        source.Get());
            Base::Ref<Media::ImageSource>
                inner = cropped->GetSource();
            if (!inner ||
                inner->RuntimeType() !=
                    Media::BitmapImage::
                        StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "CroppedBitmap source requires a BitmapImage source");
            }
            bitmap = static_cast<
                Media::BitmapImage*>(
                    inner.Get());
            sourceRect =
                cropped->GetSourceRect();
        } else {
            bitmap = static_cast<
                Media::BitmapImage*>(
                    source.Get());
        }
        Base::Result<Base::ResourceUri> resolved =
            ResolveImageUri(
                documentUri,
                bitmap->GetUriSource());
        if (!resolved) return resolved.GetStatus();

        Record* record = nullptr;
        for (Record& candidate : records_) {
            if (candidate.resolvedUri ==
                    resolved.Value() &&
                candidate.sourceRect.x ==
                    sourceRect.x &&
                candidate.sourceRect.y ==
                    sourceRect.y &&
                candidate.sourceRect.width ==
                    sourceRect.width &&
                candidate.sourceRect.height ==
                    sourceRect.height) {
                record = &candidate;
                break;
            }
        }
        if (record == nullptr) {
            Record created(allocator_);
            created.resolvedUri =
                resolved.Value();
            created.sourceRect =
                sourceRect;
            Base::Result<void> stored =
                records_.PushBack(
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
                    Render::InvalidRenderImageId &&
                backend != nullptr &&
                backend->generation ==
                    record->backendGeneration &&
                backend->release != nullptr) {
                backend->release(
                    backend->context,
                    record->renderImage);
            }
            record->renderImage =
                Render::InvalidRenderImageId;
            record->backendGeneration = 0U;
            record->pixels.Clear();
            record->width = 0U;
            record->height = 0U;

            Base::Ref<Base::Stream> imageStream;
            if (textureProvider != nullptr) {
                Base::Result<Media::TextureResourceInfo>
                    loadedTexture = textureProvider->Open(
                        resolved.Value());
                if (!loadedTexture) return loadedTexture.GetStatus();
                imageStream = std::move(
                    loadedTexture).Value().source.stream;
            } else {
                Base::Result<Markup::XamlProviderResolution>
                    provider = sources.ResolveDetailed(
                        resolved.Value());
                if (!provider ||
                    provider.Value().provider == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Image XAML provider was not found");
                }
                Base::Result<Markup::StreamResourceInfo>
                    loaded = provider.Value().provider->Open(
                        resolved.Value());
                if (!loaded) return loaded.GetStatus();
                imageStream = std::move(loaded).Value().stream;
            }
            if (!imageStream) {
                return InvalidImage(
                    "Image source stream is empty");
            }
            int width = 0;
            int height = 0;
            int channels = 0;
            StbiStreamContext streamContext{
                imageStream.Get(), false, false};
            const stbi_io_callbacks callbacks{
                &ReadStbiStream,
                &SkipStbiStream,
                &StbiStreamEof};
            stbi_uc* decoded =
                stbi_load_from_callbacks(
                    &callbacks,
                    &streamContext,
                    &width, &height, &channels, 4);
            if (streamContext.failed || decoded == nullptr ||
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
                record->pixels.Resize(
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

            if (sourceRect.width > 0.0 &&
                sourceRect.height > 0.0) {
                const std::uint32_t cropX =
                    static_cast<std::uint32_t>(
                        std::max(0.0, sourceRect.x));
                const std::uint32_t cropY =
                    static_cast<std::uint32_t>(
                        std::max(0.0, sourceRect.y));
                const std::uint32_t cropWidth =
                    static_cast<std::uint32_t>(
                        std::max(0.0, sourceRect.width));
                const std::uint32_t cropHeight =
                    static_cast<std::uint32_t>(
                        std::max(0.0, sourceRect.height));
                if (cropWidth == 0U ||
                    cropHeight == 0U ||
                    cropX >= record->width ||
                    cropY >= record->height ||
                    cropWidth >
                        record->width - cropX ||
                    cropHeight >
                        record->height - cropY) {
                    return InvalidImage(
                        "CroppedBitmap source rect is outside the decoded image");
                }
                const std::uint32_t cropBytes =
                    cropWidth * cropHeight * 4U;
                Base::Vector<std::uint8_t> cropped(
                    allocator_);
                Base::Result<void> reserved =
                    cropped.Resize(cropBytes);
                if (!reserved) return reserved.GetStatus();
                for (std::uint32_t row = 0U;
                     row < cropHeight; ++row) {
                    const std::uint32_t sourceOffset =
                        ((cropY + row) * record->width +
                            cropX) * 4U;
                    const std::uint32_t targetOffset =
                        row * cropWidth * 4U;
                    for (std::uint32_t pixel = 0U;
                         pixel < cropWidth * 4U;
                         ++pixel) {
                        cropped[targetOffset + pixel] =
                            record->pixels[
                                sourceOffset + pixel];
                    }
                }
                record->pixels = std::move(cropped);
                record->width = cropWidth;
                record->height = cropHeight;
            }
            changed = true;
        }

        if (backend != nullptr) {
            if (backend->create == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Image backend service is incomplete");
            }
            if (record->renderImage ==
                    Render::InvalidRenderImageId ||
                record->backendGeneration !=
                    backend->generation) {
                Base::Result<
                    Render::RenderImageId>
                    uploaded = backend->create(
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
                Render::InvalidRenderImageId) {
            if (nextHeadlessImage_ ==
                Render::InvalidRenderImageId) {
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
            ? Core::RenderFacet::SetImageRuntimeData(
                *imageControl,
                record->renderImage,
                record->width,
                record->height)
            : imageBrush->SetRuntimeImage(
                record->renderImage,
                record->width,
                record->height);
        if (!assigned) return assigned.GetStatus();
        }
    }

    for (std::uint32_t index =
             records_.Size();
         index > 0U; --index) {
        Record& record = records_[index - 1U];
        if (record.seenEpoch == epoch_) continue;
        if (backend != nullptr &&
            record.renderImage !=
                Render::InvalidRenderImageId &&
            record.backendGeneration ==
                backend->generation &&
            backend->release != nullptr) {
            backend->release(
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

void ImageCache::ReleaseBackendResources(
    ImageResources* backend) noexcept {
    for (Record& record : records_) {
        if (backend != nullptr &&
            backend->release != nullptr &&
            record.renderImage !=
                Render::InvalidRenderImageId &&
            record.backendGeneration ==
                backend->generation) {
            backend->release(
                backend->context,
                record.renderImage);
        }
        record.renderImage =
            Render::InvalidRenderImageId;
        record.backendGeneration = 0U;
    }
}

void ImageCache::Shutdown(
    ImageResources* backend) noexcept {
    ReleaseBackendResources(backend);
    records_.Clear();
}

} // namespace Aero::Media
