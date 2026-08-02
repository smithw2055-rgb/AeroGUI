#include "markup/MarkupInternal.hpp"
#include "../render/DisplayList.hpp"
#include "ImageCache.hpp"

#include "media/BrushInternals.hpp"

#include <Aero/Controls/Common.hpp>
#include <Aero/Shapes.hpp>

#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>
#include "gui/ElementInternal.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#include "stb_image.h"

namespace Aero::Internal {

class ImageControlPrivate {
public:
    static Base::Result<void> SetRuntimeImage(
        Controls::Image& image,
        Render::RenderImageId renderImage,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight) noexcept {
        const bool measureChanged = image.pixelWidth_ != pixelWidth || image.pixelHeight_ != pixelHeight;
        const bool renderChanged = image.renderImage_ != renderImage;
        image.renderImage_ = renderImage;
        image.pixelWidth_ = pixelWidth;
        image.pixelHeight_ = pixelHeight;
        if (measureChanged) return image.InvalidateMeasure();
        return renderChanged ? image.InvalidateVisual() : Base::Result<void>();
    }
};

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

Base::Result<bool> ImageCache::Synchronize(
    Aero::Visual* root,
    const Base::ResourceUri& documentUri,
    Markup::XamlProviderRegistry& sources,
    Integration::TextureProvider* textureProvider,
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

    Base::Vector<Aero::Visual*> pending(
        allocator_);
    if (root != nullptr) {
        Base::Result<void> queued =
            pending.PushBack(root);
        if (!queued) return queued.GetStatus();
    }
    while (!pending.Empty()) {
        Aero::Visual* visual =
            pending[pending.Size() - 1U];
        pending.PopBack();
        if (visual == nullptr) continue;
        for (Aero::Visual* child :
             Aero::Internal::ElementPrivate::VisualChildren(*visual)) {
            Base::Result<void> queued =
                pending.PushBack(child);
            if (!queued) return queued.GetStatus();
        }
        Controls::Image* imageControl = nullptr;
        Media::ImageBrush* imageBrush =
            nullptr;
        Base::Ref<Media::ImageSource>
            source;
        if (visual->RuntimeType() ==
                Controls::Image::StaticTypeId()) {
            imageControl =
                static_cast<Controls::Image*>(
                    visual);
            source = imageControl->GetSource();
        } else {
            Shapes::Shape* shape = nullptr;
            if (visual->RuntimeType() ==
                Shapes::Rectangle::StaticTypeId()) {
                shape =
                    static_cast<
                        Shapes::Rectangle*>(
                            visual);
            } else if (
                visual->RuntimeType() ==
                Shapes::Ellipse::StaticTypeId()) {
                shape =
                    static_cast<
                        Shapes::Ellipse*>(
                            visual);
            }
            if (shape == nullptr) continue;
            Base::Ref<Media::Brush>
                fill = shape->GetFill();
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
        }
        if (!source) {
            Base::Result<void> cleared =
                imageControl != nullptr
                ? ImageControlPrivate::
                    SetRuntimeImage(
                    *imageControl,
                    Render::InvalidRenderImageId,
                    0U, 0U)
                : BrushPrivate::
                    SetRuntimeImage(
                    *imageBrush,
                    Render::InvalidRenderImageId,
                    0U, 0U);
            if (!cleared) {
                return cleared.GetStatus();
            }
            continue;
        }
        if (source->RuntimeType() !=
            Media::BitmapImage::
                StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Image and ImageBrush currently require BitmapImage sources");
        }
        auto* bitmap =
            static_cast<
                Media::BitmapImage*>(
                    source.Get());
        Base::Result<Base::ResourceUri> resolved =
            ResolveImageUri(
                documentUri,
                bitmap->GetUriSource());
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
                Base::Result<Integration::TextureResourceInfo>
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
                Base::Result<Integration::StreamResourceInfo>
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
            ? ImageControlPrivate::SetRuntimeImage(
                *imageControl,
                record->renderImage,
                record->width,
                record->height)
            : BrushPrivate::SetRuntimeImage(
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

} // namespace Aero::Internal
