#include "TextRuntimeService.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Render::Detail {
namespace {

// Text often sits below a Viewbox or another render transform.  Keep the
// layout in device-independent units, but rasterize glyph atlas entries at a
// higher density so an enlarged run is still sampled from sufficient detail.
constexpr float GlyphRasterScale = 4.0F;

struct GlyphVertex final {
    float x = 0.0F;
    float y = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
};

struct BatchBuild final {
    explicit BatchBuild(
        Base::IAllocator* allocator = nullptr) noexcept
        : vertices(allocator),
          indices(allocator),
          placements(allocator) {}

    std::uint32_t page = UINT32_MAX;
    Base::Vector<GlyphVertex> vertices;
    Base::Vector<std::uint32_t> indices;
    Base::Vector<Text::GlyphAtlasPlacement> placements;
    Rhi::ResourceHandle vertexBuffer;
    Rhi::ResourceHandle indexBuffer;
};

bool IsValidConfig(
    const TextRuntimeConfig& config) noexcept {
    auto validFace = [](const Text::FontFace& face) noexcept {
        return face.handle.IsValid() &&
            std::isfinite(face.metrics.unitsPerEm) &&
            face.metrics.unitsPerEm > 0.0F &&
            std::isfinite(face.metrics.ascent) &&
            std::isfinite(face.metrics.descent) &&
            std::isfinite(face.metrics.lineGap);
    };
    if (!validFace(config.face) ||
        !std::isfinite(config.pixelSize) ||
        config.pixelSize <= 0.0F ||
        !std::isfinite(config.lineHeight) ||
        config.lineHeight < 0.0F ||
        config.firstGlyphRunId ==
            Presentation::InvalidRenderGlyphRunId) {
        return false;
    }
    for (const Text::FontFace& fallback :
         config.fallbackFaces) {
        if (!validFace(fallback)) return false;
    }
    return true;
}

Base::Span<const std::uint8_t> AsBytes(
    Base::Span<const GlyphVertex> values) noexcept {
    return {
        reinterpret_cast<const std::uint8_t*>(values.Data()),
        static_cast<std::uint32_t>(
            values.Size() * sizeof(GlyphVertex))};
}

Base::Span<const std::uint8_t> AsBytes(
    Base::Span<const std::uint32_t> values) noexcept {
    return {
        reinterpret_cast<const std::uint8_t*>(values.Data()),
        static_cast<std::uint32_t>(
            values.Size() * sizeof(std::uint32_t))};
}

} // namespace

struct TextRuntimeService::Impl final {
    struct PageResource final {
        Rhi::ResourceHandle texture;
    };

    struct RunResource final {
        Presentation::RenderGlyphRunId id =
            Presentation::InvalidRenderGlyphRunId;
        Rhi::ResourceHandle vertexBuffer;
        Rhi::ResourceHandle indexBuffer;
        Base::Vector<Text::GlyphAtlasPlacement> placements;
        Rhi::FenceValue retireFence = 0U;
        bool released = false;

        explicit RunResource(
            Base::IAllocator* allocator = nullptr) noexcept
            : placements(allocator) {}
    };

    explicit Impl(
        Base::IAllocator* allocator) noexcept
        : atlas(allocator),
          fallbackFaces(allocator),
          pages(allocator),
          runs(allocator) {}

    TextRuntimeConfig config;
    Text::GlyphAtlas atlas;
    Base::Vector<Text::FontFace> fallbackFaces;
    Base::Vector<PageResource> pages;
    Base::Vector<RunResource> runs;
    Rhi::ResourceHandle sampler;
    Presentation::RenderGlyphRunId nextGlyphRun = 1U;
    std::uint64_t useStamp = 1U;
    Rhi::FenceValue lastUploadFence = 0U;
    bool initialized = false;
};

TextRuntimeService::TextRuntimeService(
    Text::FontManager& fonts,
    Rhi::RhiDevice& device,
    GlyphRunResourceSink& sink,
    Base::IAllocator* allocator) noexcept
    : fonts_(&fonts),
      device_(&device),
      sink_(&sink),
      allocator_(
          allocator != nullptr
              ? allocator
              : &Base::GetDefaultAllocator()) {}

TextRuntimeService::~TextRuntimeService() {
    Shutdown();
}

Base::Result<void> TextRuntimeService::Initialize(
    const TextRuntimeConfig& config) noexcept {
    if (impl_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "TextBlock render service is already initialized");
    }
    if (!IsValidConfig(config) ||
        fonts_ == nullptr ||
        !fonts_->IsInitialized() ||
        device_ == nullptr ||
        sink_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock render service configuration is invalid");
    }

    void* memory = allocator_->Allocate(
        {sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "TextBlock render service allocation failed");
    }
    impl_ = new (memory) Impl(allocator_);
    impl_->config = config;
    impl_->nextGlyphRun = config.firstGlyphRunId;
    Base::Result<void> fallbacksCopied =
        impl_->fallbackFaces.TryAppend(
            config.fallbackFaces);
    if (!fallbacksCopied) {
        Shutdown();
        return fallbacksCopied.GetStatus();
    }
    impl_->config.fallbackFaces =
        impl_->fallbackFaces.AsSpan();

    Base::Result<void> atlasReady =
        impl_->atlas.Initialize(config.atlas);
    if (!atlasReady) {
        Shutdown();
        return atlasReady.GetStatus();
    }

    Rhi::SamplerDescriptor sampler;
    sampler.minFilter = Rhi::FilterMode::Linear;
    sampler.magFilter = Rhi::FilterMode::Linear;
    sampler.mipFilter = Rhi::FilterMode::Nearest;
    Base::Result<Rhi::ResourceHandle> createdSampler =
        device_->CreateSampler(sampler);
    if (!createdSampler) {
        Shutdown();
        return createdSampler.GetStatus();
    }
    impl_->sampler = createdSampler.Value();
    impl_->initialized = true;
    return {};
}

Base::Result<void>
TextRuntimeService::RecoverDeviceResources(
    Rhi::RhiDevice& device,
    GlyphRunResourceSink& sink) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TextBlock render service is not initialized");
    }
    if (device.Backend().IsDeviceLost()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Replacement text graphics device is lost");
    }

    TextRuntimeConfig config = impl_->config;
    Base::Vector<Text::FontFace> fallbackFaces(allocator_);
    Base::Result<void> copied =
        fallbackFaces.TryAppend(
            impl_->fallbackFaces.AsSpan());
    if (!copied) return copied.GetStatus();
    config.fallbackFaces = fallbackFaces.AsSpan();

    for (const Impl::RunResource& run : impl_->runs) {
        if (run.id !=
            Presentation::InvalidRenderGlyphRunId) {
            (void)sink_->UnregisterGlyphRun(run.id);
        }
    }
    impl_->atlas.NotifyDeviceLost();
    impl_->atlas.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;

    device_ = &device;
    sink_ = &sink;
    return Initialize(config);
}

void TextRuntimeService::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    const Rhi::FenceValue retireFence =
        device_ != nullptr
            ? device_->LastSubmittedFence()
            : 0U;
    for (Impl::RunResource& run : impl_->runs) {
        if (sink_ != nullptr &&
            run.id != Presentation::InvalidRenderGlyphRunId) {
            (void)sink_->UnregisterGlyphRun(run.id);
        }
        if (device_ != nullptr) {
            if (device_->IsAlive(run.vertexBuffer)) {
                (void)device_->DestroyResource(
                    run.vertexBuffer, retireFence);
            }
            if (device_->IsAlive(run.indexBuffer)) {
                (void)device_->DestroyResource(
                    run.indexBuffer, retireFence);
            }
        }
    }
    if (device_ != nullptr) {
        for (const Impl::PageResource& page : impl_->pages) {
            if (device_->IsAlive(page.texture)) {
                (void)device_->DestroyResource(
                    page.texture, retireFence);
            }
        }
        if (device_->IsAlive(impl_->sampler)) {
            (void)device_->DestroyResource(
                impl_->sampler, retireFence);
        }
    }
    impl_->atlas.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool TextRuntimeService::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

Base::Result<std::uint32_t>
TextRuntimeService::CollectGarbage() noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TextBlock render service is not initialized");
    }
    const Rhi::FenceValue completed =
        device_->Backend().CompletedFence();
    std::uint32_t releasedCount = 0U;
    std::uint32_t index = 0U;
    while (index < impl_->runs.Size()) {
        Impl::RunResource& run = impl_->runs[index];
        if (!run.released || run.retireFence > completed) {
            ++index;
            continue;
        }
        Base::Result<void> unregistered =
            sink_->UnregisterGlyphRun(run.id);
        if (!unregistered &&
            unregistered.GetStatus().code !=
                Base::ErrorCode::NotFound) {
            return unregistered.GetStatus();
        }
        if (device_->IsAlive(run.vertexBuffer)) {
            Base::Result<void> destroyed =
                device_->DestroyResource(
                    run.vertexBuffer, run.retireFence);
            if (!destroyed) return destroyed.GetStatus();
        }
        if (device_->IsAlive(run.indexBuffer)) {
            Base::Result<void> destroyed =
                device_->DestroyResource(
                    run.indexBuffer, run.retireFence);
            if (!destroyed) return destroyed.GetStatus();
        }
        const std::uint32_t last = impl_->runs.Size() - 1U;
        if (index != last) {
            impl_->runs[index] = std::move(impl_->runs[last]);
        }
        impl_->runs.PopBack();
        ++releasedCount;
    }
    Base::Result<std::uint32_t> deviceReleased =
        device_->CollectGarbage();
    if (!deviceReleased) return deviceReleased.GetStatus();
    return releasedCount;
}

Base::Result<void> TextRuntimeService::ShapeAndPrepare(
    const Controls::Detail::TextLayoutRequest& request,
    Controls::Detail::TextLayoutResult& output) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TextBlock render service is not initialized");
    }
    if (!Presentation::IsValidLayoutSize(
            request.availableSize) ||
        !std::isfinite(request.dpiScale) ||
        request.dpiScale <= 0.0 ||
        request.dpiScale >
            static_cast<double>(
                std::numeric_limits<float>::max()) ||
        !std::isfinite(request.pixelSize) ||
        request.pixelSize <= 0.0F ||
        !std::isfinite(request.lineHeight) ||
        request.lineHeight < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock layout request is invalid");
    }
    if (device_->Backend().IsDeviceLost()) {
        impl_->atlas.NotifyDeviceLost();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TextBlock render service detected device loss");
    }

    Base::Result<std::uint32_t> collected =
        CollectGarbage();
    if (!collected) return collected.GetStatus();

    const Rhi::FenceValue lastSubmitted =
        device_->LastSubmittedFence();
    if (lastSubmitted > 0U) {
        for (Impl::RunResource& run : impl_->runs) {
            if (run.released) continue;
            for (const Text::GlyphAtlasPlacement& placement :
                 run.placements) {
                Base::Result<void> marked =
                    impl_->atlas.MarkSubmitted(
                        placement, lastSubmitted);
                if (!marked &&
                    marked.GetStatus().code !=
                        Base::ErrorCode::InvalidArgument) {
                    return marked.GetStatus();
                }
            }
        }
    }

    Text::TextLayoutRequest layoutRequest;
    layoutRequest.face = request.face.handle.IsValid()
        ? request.face
        : impl_->config.face;
    layoutRequest.fallbackFaces =
        impl_->fallbackFaces.AsSpan();
    layoutRequest.text = request.text;
    layoutRequest.pixelSize = request.pixelSize;
    layoutRequest.maxWidth =
        static_cast<float>(request.availableSize.width);
    layoutRequest.lineHeight =
        request.lineHeight > 0.0F
        ? request.lineHeight
        : impl_->config.lineHeight;
    layoutRequest.wrapping = request.wrapping;
    layoutRequest.trimming = request.trimming;
    layoutRequest.alignment = request.alignment;
    Text::TextLayout layout(allocator_);
    Base::Result<void> laidOut =
        layout.ShapeAndMeasure(*fonts_, layoutRequest);
    if (!laidOut) return laidOut.GetStatus();

    output.glyphRuns.Clear();
    output.desiredSize = {
        static_cast<double>(layout.NaturalSize().width),
        static_cast<double>(layout.NaturalSize().height)};
    output.hitRegions.Clear();
    const Base::Span<const Text::TextLine> textLines = layout.Lines();
    const Base::Span<const Text::GlyphRun> textRuns = layout.Runs();
    for (const Text::TextLine& line : textLines) {
        const float lineHeight = std::max(
            line.ascent + line.descent, request.pixelSize);
        const std::uint32_t runEnd = std::min(
            line.firstRun + line.runCount, textRuns.Size());
        for (std::uint32_t runIndex = line.firstRun;
             runIndex < runEnd; ++runIndex) {
            const Text::GlyphRun& run = textRuns[runIndex];
            for (const Text::PositionedGlyph& glyph : run.glyphs) {
                Text::TextHitRegion region;
                region.textOffset = glyph.cluster;
                region.x = glyph.x;
                region.y = line.y;
                region.width = std::max(std::fabs(glyph.advanceX), 1.0F);
                region.height = lineHeight;
                bool merged = false;
                for (Text::TextHitRegion& existing : output.hitRegions) {
                    if (existing.textOffset != region.textOffset ||
                        existing.y != region.y) {
                        continue;
                    }
                    const float left = std::min(existing.x, region.x);
                    const float right = std::max(
                        existing.x + existing.width,
                        region.x + region.width);
                    existing.x = left;
                    existing.width = std::max(1.0F, right - left);
                    existing.height = std::max(existing.height, region.height);
                    merged = true;
                    break;
                }
                if (!merged) {
                    Base::Result<void> appended =
                        output.hitRegions.TryPushBack(region);
                    if (!appended) return appended.GetStatus();
                }
            }
        }
        Text::TextHitRegion endRegion;
        endRegion.textOffset = line.textStart + line.textLength;
        endRegion.x = line.x + line.width;
        endRegion.y = line.y;
        endRegion.width = 1.0F;
        endRegion.height = lineHeight;
        Base::Result<void> endAdded =
            output.hitRegions.TryPushBack(endRegion);
        if (!endAdded) return endAdded.GetStatus();
    }
    for (Text::TextHitRegion& region : output.hitRegions) {
        std::uint32_t nextOffset = request.text.SizeBytes();
        for (const Text::TextHitRegion& candidate : output.hitRegions) {
            if (candidate.textOffset > region.textOffset &&
                candidate.textOffset < nextOffset) {
                nextOffset = candidate.textOffset;
            }
        }
        region.textLength = nextOffset > region.textOffset
            ? nextOffset - region.textOffset
            : 0U;
    }
    if (layout.Runs().Empty()) return {};

    Base::Vector<BatchBuild> batches(allocator_);
    const float dpiScale =
        static_cast<float>(request.dpiScale);
    const float glyphRasterDpi = dpiScale * GlyphRasterScale;
    if (!std::isfinite(glyphRasterDpi) || glyphRasterDpi <= 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Text glyph raster scale exceeds supported precision");
    }
    const Text::GlyphAtlasConfig atlasConfig =
        impl_->atlas.Config();
    const Rhi::FenceValue completedFence =
        device_->Backend().CompletedFence();

    auto findBatch = [&](
        std::uint32_t page) noexcept -> Base::Result<BatchBuild*> {
        for (BatchBuild& batch : batches) {
            if (batch.page == page) return &batch;
        }
        Base::Result<BatchBuild*> added =
            batches.TryEmplaceBack(allocator_);
        if (!added) return added.GetStatus();
        added.Value()->page = page;
        return added.Value();
    };

    for (const Text::GlyphRun& run : layout.Runs()) {
        for (const Text::PositionedGlyph& glyph :
             run.glyphs) {
            Text::GlyphRequest glyphRequest;
            glyphRequest.face = run.face;
            glyphRequest.glyph = glyph.glyph;
            glyphRequest.pixelSize = run.pixelSize;
            glyphRequest.dpiScale = glyphRasterDpi;
            Text::GlyphMetrics metrics;
            Base::Result<void> measured =
                fonts_->GetGlyphMetrics(
                    glyphRequest, metrics);
            if (!measured) return measured.GetStatus();
            if (metrics.width <= 0.0F ||
                metrics.height <= 0.0F) {
                continue;
            }

            Text::GlyphAtlasPlacement placement;
            Base::Result<void> ensured =
                impl_->atlas.EnsureGlyph(
                    *fonts_, glyphRequest,
                    impl_->useStamp++,
                    completedFence, placement);
            if (!ensured) return ensured.GetStatus();
            if (impl_->useStamp == 0U) impl_->useStamp = 1U;

            Base::Result<BatchBuild*> found =
                findBatch(placement.page);
            if (!found) return found.GetStatus();
            BatchBuild& batch = *found.Value();
            if (batch.vertices.Size() >
                    UINT32_MAX - 4U ||
                batch.indices.Size() >
                    UINT32_MAX - 6U) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Text glyph geometry exceeds index limits");
            }
            const std::uint32_t vertexBase =
                batch.vertices.Size();
            const float inverseDpi = 1.0F / glyphRasterDpi;
            const float left = glyph.x +
                static_cast<float>(placement.bearingX) *
                    inverseDpi;
            const float top = glyph.y -
                static_cast<float>(placement.bearingY) *
                    inverseDpi;
            const float right = left +
                static_cast<float>(placement.width) *
                    inverseDpi;
            const float bottom = top +
                static_cast<float>(placement.height) *
                    inverseDpi;
            const float inverseWidth =
                1.0F /
                static_cast<float>(atlasConfig.pageWidth);
            const float inverseHeight =
                1.0F /
                static_cast<float>(atlasConfig.pageHeight);
            const float u0 =
                static_cast<float>(placement.x) *
                inverseWidth;
            const float v0 =
                static_cast<float>(placement.y) *
                inverseHeight;
            const float u1 =
                static_cast<float>(
                    placement.x + placement.width) *
                inverseWidth;
            const float v1 =
                static_cast<float>(
                    placement.y + placement.height) *
                inverseHeight;
            const GlyphVertex vertices[] = {
                {left, top, u0, v0},
                {right, top, u1, v0},
                {right, bottom, u1, v1},
                {left, bottom, u0, v1}};
            Base::Result<void> appended =
                batch.vertices.TryAppend(vertices);
            if (!appended) return appended.GetStatus();
            const std::uint32_t indices[] = {
                vertexBase, vertexBase + 1U,
                vertexBase + 2U, vertexBase,
                vertexBase + 2U, vertexBase + 3U};
            appended = batch.indices.TryAppend(indices);
            if (!appended) return appended.GetStatus();
            appended = batch.placements.TryPushBack(placement);
            if (!appended) return appended.GetStatus();
        }
    }
    if (batches.Empty()) return {};

    Base::Result<void> outputReserved =
        output.glyphRuns.TryReserve(batches.Size());
    if (!outputReserved) return outputReserved.GetStatus();
    Base::Result<void> runsReserved =
        impl_->runs.TryReserve(
            impl_->runs.Size() + batches.Size());
    if (!runsReserved) return runsReserved.GetStatus();

    const std::uint32_t pageCount =
        impl_->atlas.PageCount();
    Base::Result<void> pagesResized =
        impl_->pages.TryResize(pageCount);
    if (!pagesResized) return pagesResized.GetStatus();
    for (std::uint32_t page = 0U;
         page < pageCount; ++page) {
        if (device_->IsAlive(
                impl_->pages[page].texture)) {
            continue;
        }
        Rhi::TextureResourceDescriptor descriptor;
        descriptor.width = atlasConfig.pageWidth;
        descriptor.height = atlasConfig.pageHeight;
        descriptor.format =
            Rhi::GraphicsTextureFormat::R8Unorm;
        descriptor.usage =
            Rhi::TextureUsageBit(
                Rhi::TextureUsage::Sampled) |
            Rhi::TextureUsageBit(
                Rhi::TextureUsage::CopyDestination);
        Base::Result<Rhi::ResourceHandle> texture =
            device_->CreateTexture(descriptor);
        if (!texture) return texture.GetStatus();
        impl_->pages[page].texture = texture.Value();
    }

    Rhi::CommandEncoder encoder(allocator_);
    for (const Text::GlyphAtlasUpload& upload :
         impl_->atlas.PendingUploads()) {
        if (upload.page >= impl_->pages.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Glyph atlas upload references a missing page");
        }
        Rhi::TextureRegion region;
        region.x = upload.x;
        region.y = upload.y;
        region.width = upload.width;
        region.height = upload.height;
        region.bytesPerRow = upload.strideBytes;
        Base::Result<void> uploaded =
            encoder.UploadTexture(
                impl_->pages[upload.page].texture,
                region, upload.pixels.AsSpan());
        if (!uploaded) return uploaded.GetStatus();
    }

    auto destroyBatchResources = [&](
        Rhi::FenceValue retireFence) noexcept {
        for (BatchBuild& batch : batches) {
            if (device_->IsAlive(batch.vertexBuffer)) {
                (void)device_->DestroyResource(
                    batch.vertexBuffer, retireFence);
            }
            if (device_->IsAlive(batch.indexBuffer)) {
                (void)device_->DestroyResource(
                    batch.indexBuffer, retireFence);
            }
        }
    };

    for (BatchBuild& batch : batches) {
        if (batch.vertices.Size() >
                UINT32_MAX / sizeof(GlyphVertex) ||
            batch.indices.Size() >
                UINT32_MAX / sizeof(std::uint32_t)) {
            destroyBatchResources(
                device_->LastSubmittedFence());
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Text glyph buffer exceeds upload limits");
        }
        const std::uint64_t vertexBytes =
            static_cast<std::uint64_t>(
                batch.vertices.Size()) *
            sizeof(GlyphVertex);
        const std::uint64_t indexBytes =
            static_cast<std::uint64_t>(
                batch.indices.Size()) *
            sizeof(std::uint32_t);
        Rhi::BufferDescriptor vertexDescriptor;
        vertexDescriptor.sizeBytes = vertexBytes;
        vertexDescriptor.usage = Rhi::BufferUsage::Vertex;
        Base::Result<Rhi::ResourceHandle> vertex =
            device_->CreateBuffer(
                vertexDescriptor);
        if (!vertex) {
            destroyBatchResources(
                device_->LastSubmittedFence());
            return vertex.GetStatus();
        }
        batch.vertexBuffer = vertex.Value();

        Rhi::BufferDescriptor indexDescriptor;
        indexDescriptor.sizeBytes = indexBytes;
        indexDescriptor.usage = Rhi::BufferUsage::Index;
        Base::Result<Rhi::ResourceHandle> index =
            device_->CreateBuffer(
                indexDescriptor);
        if (!index) {
            destroyBatchResources(
                device_->LastSubmittedFence());
            return index.GetStatus();
        }
        batch.indexBuffer = index.Value();

        Base::Result<void> uploaded =
            encoder.UploadBuffer(
                batch.vertexBuffer, 0U,
                AsBytes(batch.vertices.AsSpan()));
        if (!uploaded) {
            destroyBatchResources(
                device_->LastSubmittedFence());
            return uploaded.GetStatus();
        }
        uploaded = encoder.UploadBuffer(
            batch.indexBuffer, 0U,
            AsBytes(batch.indices.AsSpan()));
        if (!uploaded) {
            destroyBatchResources(
                device_->LastSubmittedFence());
            return uploaded.GetStatus();
        }
    }

    Base::Result<Rhi::CommandList> commands =
        encoder.Finish();
    if (!commands) {
        destroyBatchResources(
            device_->LastSubmittedFence());
        return commands.GetStatus();
    }
    Base::Result<Rhi::FenceValue> submitted =
        device_->Submit(commands.Value());
    if (!submitted) {
        destroyBatchResources(
            device_->LastSubmittedFence());
        return submitted.GetStatus();
    }
    impl_->lastUploadFence = submitted.Value();
    impl_->atlas.ClearPendingUploads();
    for (const BatchBuild& batch : batches) {
        for (const Text::GlyphAtlasPlacement& placement :
             batch.placements) {
            Base::Result<void> marked =
                impl_->atlas.MarkSubmitted(
                    placement, submitted.Value());
            if (!marked) {
                destroyBatchResources(
                    submitted.Value());
                return marked.GetStatus();
            }
        }
    }

    Base::Vector<Impl::RunResource> prepared(allocator_);
    Base::Result<void> preparedReserved =
        prepared.TryReserve(batches.Size());
    if (!preparedReserved) {
        destroyBatchResources(submitted.Value());
        return preparedReserved.GetStatus();
    }
    std::uint32_t registeredCount = 0U;
    for (BatchBuild& batch : batches) {
        if (impl_->nextGlyphRun ==
            Presentation::InvalidRenderGlyphRunId) {
            for (std::uint32_t rollback = 0U;
                 rollback < registeredCount; ++rollback) {
                (void)sink_->UnregisterGlyphRun(
                    prepared[rollback].id);
            }
            destroyBatchResources(submitted.Value());
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Text glyph-run ID space is exhausted");
        }
        const Presentation::RenderGlyphRunId id =
            impl_->nextGlyphRun++;
        Base::Result<void> registered =
            sink_->RegisterGlyphRun(
                id, batch.vertexBuffer,
                batch.indexBuffer,
                batch.indices.Size(),
                impl_->pages[batch.page].texture,
                impl_->sampler,
                Rhi::IndexType::UInt32);
        if (!registered) {
            for (std::uint32_t rollback = 0U;
                 rollback < registeredCount; ++rollback) {
                (void)sink_->UnregisterGlyphRun(
                    prepared[rollback].id);
            }
            destroyBatchResources(submitted.Value());
            return registered.GetStatus();
        }
        Impl::RunResource run(allocator_);
        run.id = id;
        run.vertexBuffer = batch.vertexBuffer;
        run.indexBuffer = batch.indexBuffer;
        run.placements = std::move(batch.placements);
        Base::Result<void> appended =
            prepared.TryPushBack(std::move(run));
        if (!appended) {
            (void)sink_->UnregisterGlyphRun(id);
            for (std::uint32_t rollback = 0U;
                 rollback < registeredCount; ++rollback) {
                (void)sink_->UnregisterGlyphRun(
                    prepared[rollback].id);
            }
            destroyBatchResources(submitted.Value());
            return appended.GetStatus();
        }
        ++registeredCount;
    }

    for (Impl::RunResource& run : prepared) {
        const Presentation::RenderGlyphRunId id = run.id;
        Base::Result<Impl::RunResource*> stored =
            impl_->runs.TryEmplaceBack(std::move(run));
        if (!stored) return stored.GetStatus();
        Base::Result<void> appended =
            output.glyphRuns.TryPushBack(id);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

void TextRuntimeService::ReleaseGlyphRun(
    Presentation::RenderGlyphRunId glyphRun) noexcept {
    if (!IsInitialized() ||
        glyphRun == Presentation::InvalidRenderGlyphRunId) {
        return;
    }
    for (Impl::RunResource& run : impl_->runs) {
        if (run.id != glyphRun || run.released) {
            continue;
        }
        run.retireFence =
            device_->LastSubmittedFence();
        if (run.retireFence > 0U) {
            for (const Text::GlyphAtlasPlacement& placement :
                 run.placements) {
                (void)impl_->atlas.MarkSubmitted(
                    placement, run.retireFence);
            }
        }
        run.released = true;
        return;
    }
}

} // namespace Aero::Render::Detail
