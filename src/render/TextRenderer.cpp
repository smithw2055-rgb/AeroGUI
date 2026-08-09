#include "TextRenderer.hpp"
#include "render/RenderDeviceState.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Render {
namespace {

// Text often sits below a Viewbox or another render transform.  Keep the
// layout in device-independent units, but rasterize glyph atlas entries at a
// higher density so an enlarged run is still sampled from sufficient detail.
constexpr float GlyphRasterScale = 4.0F;

struct GlyphVertex {
    float x = 0.0F;
    float y = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
};

struct BatchBuild {
    explicit BatchBuild(
        Base::IAllocator* allocator = nullptr) noexcept
        : vertices(allocator),
          indices(allocator),
          placements(allocator) {}

    std::uint32_t page = UINT32_MAX;
    Base::Vector<GlyphVertex> vertices;
    Base::Vector<std::uint32_t> indices;
    Base::Vector<Text::GlyphAtlasPlacement> placements;
    Graphics::ResourceHandle vertexBuffer;
    Graphics::ResourceHandle indexBuffer;
};

bool IsValidConfig(
    const TextConfig& config) noexcept {
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
            Render::InvalidRenderGlyphRunId) {
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

struct TextRendererState {
    struct PageResource {
        Graphics::ResourceHandle texture;
    };

    struct RunResource {
        Render::RenderGlyphRunId id =
            Render::InvalidRenderGlyphRunId;
        Graphics::ResourceHandle vertexBuffer;
        Graphics::ResourceHandle indexBuffer;
        Base::Vector<Text::GlyphAtlasPlacement> placements;
        Graphics::FenceValue retireFence = 0U;
        bool released = false;

        explicit RunResource(
            Base::IAllocator* allocator = nullptr) noexcept
            : placements(allocator) {}
    };

    explicit TextRendererState(
        Base::IAllocator* allocator) noexcept
        : atlas(allocator),
          fallbackFaces(allocator),
          pages(allocator),
          runs(allocator) {}

    TextConfig config;
    Text::GlyphAtlas atlas;
    Base::Vector<Text::FontFace> fallbackFaces;
    Base::Vector<PageResource> pages;
    Base::Vector<RunResource> runs;
    Graphics::ResourceHandle sampler;
    Render::RenderGlyphRunId nextGlyphRun = 1U;
    std::uint64_t useStamp = 1U;
    Graphics::FenceValue lastUploadFence = 0U;
    bool initialized = false;
};

static_assert(sizeof(TextRendererState) <= 16384U,
    "TextRenderer inline state storage is too small");
static_assert(alignof(TextRendererState) <= alignof(std::max_align_t),
    "TextRenderer inline state alignment is insufficient");

TextRenderer::TextRenderer(
    Text::FontManager& fonts,
    Aero::Render::RenderDeviceBase& device,
    GlyphRunResourceSink& sink,
    Base::IAllocator* allocator) noexcept
    : fonts_(&fonts),
      device_(&device),
      sink_(&sink),
      allocator_(
          allocator != nullptr
              ? allocator
              : &Base::GetDefaultAllocator()) {}

TextRenderer::~TextRenderer() {
    Shutdown();
}

Base::Result<void> TextRenderer::Initialize(
    const TextConfig& config) noexcept {
    if (state_ != nullptr) {
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

    state_ = new (stateStorage_) TextRendererState(allocator_);
    state_->config = config;
    state_->nextGlyphRun = config.firstGlyphRunId;
    Base::Result<void> fallbacksCopied =
        state_->fallbackFaces.Append(
            config.fallbackFaces);
    if (!fallbacksCopied) {
        Shutdown();
        return fallbacksCopied.GetStatus();
    }
    state_->config.fallbackFaces =
        state_->fallbackFaces.AsSpan();

    Base::Result<void> atlasReady =
        state_->atlas.Initialize(config.atlas);
    if (!atlasReady) {
        Shutdown();
        return atlasReady.GetStatus();
    }

    Graphics::SamplerDescriptor sampler;
    sampler.minFilter = Graphics::FilterMode::Linear;
    sampler.magFilter = Graphics::FilterMode::Linear;
    sampler.mipFilter = Graphics::FilterMode::Nearest;
    Base::Result<Graphics::ResourceHandle> createdSampler =
        device_->CreateSampler(sampler);
    if (!createdSampler) {
        Shutdown();
        return createdSampler.GetStatus();
    }
    state_->sampler = createdSampler.Value();
    state_->initialized = true;
    return {};
}

Base::Result<void>
TextRenderer::RecoverDeviceResources(
    Aero::Render::RenderDeviceBase& device,
    GlyphRunResourceSink& sink) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TextBlock render service is not initialized");
    }
    if (device.IsNativeDeviceLost()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Replacement text graphics device is lost");
    }

    TextConfig config = state_->config;
    Base::Vector<Text::FontFace> fallbackFaces(allocator_);
    Base::Result<void> copied =
        fallbackFaces.Append(
            state_->fallbackFaces.AsSpan());
    if (!copied) return copied.GetStatus();
    config.fallbackFaces = fallbackFaces.AsSpan();

    for (const TextRendererState::RunResource& run : state_->runs) {
        if (run.id !=
            Render::InvalidRenderGlyphRunId) {
            (void)sink_->UnregisterGlyphRun(run.id);
        }
    }
    state_->atlas.NotifyDeviceLost();
    state_->atlas.Shutdown();
    state_->~TextRendererState();
    state_ = nullptr;

    device_ = &device;
    sink_ = &sink;
    return Initialize(config);
}

void TextRenderer::Shutdown() noexcept {
    if (state_ == nullptr) return;
    const Graphics::FenceValue retireFence =
        device_ != nullptr
            ? device_->LastSubmittedFence()
            : 0U;
    for (TextRendererState::RunResource& run : state_->runs) {
        if (sink_ != nullptr &&
            run.id != Render::InvalidRenderGlyphRunId) {
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
        for (const TextRendererState::PageResource& page : state_->pages) {
            if (device_->IsAlive(page.texture)) {
                (void)device_->DestroyResource(
                    page.texture, retireFence);
            }
        }
        if (device_->IsAlive(state_->sampler)) {
            (void)device_->DestroyResource(
                state_->sampler, retireFence);
        }
    }
    state_->atlas.Shutdown();
    state_->~TextRendererState();
    state_ = nullptr;
}

bool TextRenderer::IsInitialized() const noexcept {
    return state_ != nullptr && state_->initialized;
}

Base::Result<std::uint32_t>
TextRenderer::CollectGarbage() noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TextBlock render service is not initialized");
    }
    const Graphics::FenceValue completed =
        device_->NativeCompletedFence();
    std::uint32_t releasedCount = 0U;
    std::uint32_t index = 0U;
    while (index < state_->runs.Size()) {
        TextRendererState::RunResource& run = state_->runs[index];
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
        const std::uint32_t last = state_->runs.Size() - 1U;
        if (index != last) {
            state_->runs[index] = std::move(state_->runs[last]);
        }
        state_->runs.PopBack();
        ++releasedCount;
    }
    Base::Result<std::uint32_t> deviceReleased =
        device_->CollectGarbage();
    if (!deviceReleased) return deviceReleased.GetStatus();
    return releasedCount;
}

Base::Result<void> TextRenderer::ShapeAndPrepare(
    const ::Aero::Controls::TextLayoutRequest& request,
    ::Aero::Controls::TextLayoutResult& output) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TextBlock render service is not initialized");
    }
    if (!Aero::IsValidLayoutSize(
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
    if (device_->IsNativeDeviceLost()) {
        state_->atlas.NotifyDeviceLost();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TextBlock render service detected device loss");
    }

    Base::Result<std::uint32_t> collected =
        CollectGarbage();
    if (!collected) return collected.GetStatus();

    const Graphics::FenceValue lastSubmitted =
        device_->LastSubmittedFence();
    if (lastSubmitted > 0U) {
        for (TextRendererState::RunResource& run : state_->runs) {
            if (run.released) continue;
            for (const Text::GlyphAtlasPlacement& placement :
                 run.placements) {
                Base::Result<void> marked =
                    state_->atlas.MarkSubmitted(
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
        : state_->config.face;
    layoutRequest.fallbackFaces =
        state_->fallbackFaces.AsSpan();
    layoutRequest.text = request.text;
    layoutRequest.pixelSize = request.pixelSize;
    layoutRequest.maxWidth =
        static_cast<float>(request.availableSize.width);
    layoutRequest.lineHeight =
        request.lineHeight > 0.0F
        ? request.lineHeight
        : state_->config.lineHeight;
    layoutRequest.wrapping = request.wrapping;
    layoutRequest.trimming = request.trimming;
    layoutRequest.alignment = request.alignment;
    layoutRequest.direction = request.direction;
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
                TextHitRegion region;
                region.textOffset = glyph.cluster;
                region.x = glyph.x;
                region.y = line.y;
                region.width = std::max(std::fabs(glyph.advanceX), 1.0F);
                region.height = lineHeight;
                bool merged = false;
                for (TextHitRegion& existing : output.hitRegions) {
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
                        output.hitRegions.PushBack(region);
                    if (!appended) return appended.GetStatus();
                }
            }
        }
        TextHitRegion endRegion;
        endRegion.textOffset = line.textStart + line.textLength;
        endRegion.x = line.x + line.width;
        endRegion.y = line.y;
        endRegion.width = 1.0F;
        endRegion.height = lineHeight;
        Base::Result<void> endAdded =
            output.hitRegions.PushBack(endRegion);
        if (!endAdded) return endAdded.GetStatus();
    }
    for (TextHitRegion& region : output.hitRegions) {
        std::uint32_t nextOffset = request.text.SizeBytes();
        for (const TextHitRegion& candidate : output.hitRegions) {
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
        state_->atlas.Config();
    const Graphics::FenceValue completedFence =
        device_->NativeCompletedFence();

    auto findBatch = [&](
        std::uint32_t page) noexcept -> Base::Result<BatchBuild*> {
        for (BatchBuild& batch : batches) {
            if (batch.page == page) return &batch;
        }
        Base::Result<BatchBuild*> added =
            batches.EmplaceBack(allocator_);
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
                state_->atlas.EnsureGlyph(
                    *fonts_, glyphRequest,
                    state_->useStamp++,
                    completedFence, placement);
            if (!ensured) return ensured.GetStatus();
            if (state_->useStamp == 0U) state_->useStamp = 1U;

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
                batch.vertices.Append(vertices);
            if (!appended) return appended.GetStatus();
            const std::uint32_t indices[] = {
                vertexBase, vertexBase + 1U,
                vertexBase + 2U, vertexBase,
                vertexBase + 2U, vertexBase + 3U};
            appended = batch.indices.Append(indices);
            if (!appended) return appended.GetStatus();
            appended = batch.placements.PushBack(placement);
            if (!appended) return appended.GetStatus();
        }
    }
    if (batches.Empty()) return {};

    Base::Result<void> outputReserved =
        output.glyphRuns.Reserve(batches.Size());
    if (!outputReserved) return outputReserved.GetStatus();
    Base::Result<void> runsReserved =
        state_->runs.Reserve(
            state_->runs.Size() + batches.Size());
    if (!runsReserved) return runsReserved.GetStatus();

    const std::uint32_t pageCount =
        state_->atlas.PageCount();
    Base::Result<void> pagesResized =
        state_->pages.Resize(pageCount);
    if (!pagesResized) return pagesResized.GetStatus();
    for (std::uint32_t page = 0U;
         page < pageCount; ++page) {
        if (device_->IsAlive(
                state_->pages[page].texture)) {
            continue;
        }
        Graphics::TextureResourceDescriptor descriptor;
        descriptor.width = atlasConfig.pageWidth;
        descriptor.height = atlasConfig.pageHeight;
        descriptor.format =
            Graphics::GraphicsTextureFormat::R8Unorm;
        descriptor.usage =
            Graphics::TextureUsageBit(
                Graphics::TextureUsage::Sampled) |
            Graphics::TextureUsageBit(
                Graphics::TextureUsage::CopyDestination);
        Base::Result<Graphics::ResourceHandle> texture =
            device_->CreateTexture(descriptor);
        if (!texture) return texture.GetStatus();
        state_->pages[page].texture = texture.Value();
    }

    ::Aero::Render::UiDrawContext encoder(*device_, allocator_);
    for (const Text::GlyphAtlasUpload& upload :
         state_->atlas.PendingUploads()) {
        if (upload.page >= state_->pages.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Glyph atlas upload references a missing page");
        }
        Graphics::TextureRegion region;
        region.x = upload.x;
        region.y = upload.y;
        region.width = upload.width;
        region.height = upload.height;
        region.bytesPerRow = upload.strideBytes;
        Base::Result<void> uploaded =
            encoder.UploadTexture(
                state_->pages[upload.page].texture,
                region, upload.pixels.AsSpan());
        if (!uploaded) return uploaded.GetStatus();
    }

    auto destroyBatchResources = [&](
        Graphics::FenceValue retireFence) noexcept {
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
        Graphics::BufferDescriptor vertexDescriptor;
        vertexDescriptor.sizeBytes = vertexBytes;
        vertexDescriptor.usage = Graphics::BufferUsage::Vertex;
        Base::Result<Graphics::ResourceHandle> vertex =
            device_->CreateBuffer(
                vertexDescriptor);
        if (!vertex) {
            destroyBatchResources(
                device_->LastSubmittedFence());
            return vertex.GetStatus();
        }
        batch.vertexBuffer = vertex.Value();

        Graphics::BufferDescriptor indexDescriptor;
        indexDescriptor.sizeBytes = indexBytes;
        indexDescriptor.usage = Graphics::BufferUsage::Index;
        Base::Result<Graphics::ResourceHandle> index =
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

    Base::Result<Graphics::FenceValue> submitted =
        encoder.Finish();
    if (!submitted) {
        destroyBatchResources(
            device_->LastSubmittedFence());
        return submitted.GetStatus();
    }
    state_->lastUploadFence = submitted.Value();
    state_->atlas.ClearPendingUploads();
    for (const BatchBuild& batch : batches) {
        for (const Text::GlyphAtlasPlacement& placement :
             batch.placements) {
            Base::Result<void> marked =
                state_->atlas.MarkSubmitted(
                    placement, submitted.Value());
            if (!marked) {
                destroyBatchResources(
                    submitted.Value());
                return marked.GetStatus();
            }
        }
    }

    Base::Vector<TextRendererState::RunResource> prepared(allocator_);
    Base::Result<void> preparedReserved =
        prepared.Reserve(batches.Size());
    if (!preparedReserved) {
        destroyBatchResources(submitted.Value());
        return preparedReserved.GetStatus();
    }
    std::uint32_t registeredCount = 0U;
    for (BatchBuild& batch : batches) {
        if (state_->nextGlyphRun ==
            Render::InvalidRenderGlyphRunId) {
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
        const Render::RenderGlyphRunId id =
            state_->nextGlyphRun++;
        Base::Result<void> registered =
            sink_->RegisterGlyphRun(
                id, batch.vertexBuffer,
                batch.indexBuffer,
                batch.indices.Size(),
                state_->pages[batch.page].texture,
                state_->sampler,
                Graphics::IndexType::UInt32);
        if (!registered) {
            for (std::uint32_t rollback = 0U;
                 rollback < registeredCount; ++rollback) {
                (void)sink_->UnregisterGlyphRun(
                    prepared[rollback].id);
            }
            destroyBatchResources(submitted.Value());
            return registered.GetStatus();
        }
        TextRendererState::RunResource run(allocator_);
        run.id = id;
        run.vertexBuffer = batch.vertexBuffer;
        run.indexBuffer = batch.indexBuffer;
        run.placements = std::move(batch.placements);
        Base::Result<void> appended =
            prepared.PushBack(std::move(run));
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

    for (TextRendererState::RunResource& run : prepared) {
        const Render::RenderGlyphRunId id = run.id;
        Base::Result<TextRendererState::RunResource*> stored =
            state_->runs.EmplaceBack(std::move(run));
        if (!stored) return stored.GetStatus();
        Base::Result<void> appended =
            output.glyphRuns.PushBack(id);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

void TextRenderer::ReleaseGlyphRun(
    Render::RenderGlyphRunId glyphRun) noexcept {
    if (!IsInitialized() ||
        glyphRun == Render::InvalidRenderGlyphRunId) {
        return;
    }
    for (TextRendererState::RunResource& run : state_->runs) {
        if (run.id != glyphRun || run.released) {
            continue;
        }
        run.retireFence =
            device_->LastSubmittedFence();
        if (run.retireFence > 0U) {
            for (const Text::GlyphAtlasPlacement& placement :
                 run.placements) {
                (void)state_->atlas.MarkSubmitted(
                    placement, run.retireFence);
            }
        }
        run.released = true;
        return;
    }
}

} // namespace Aero::Render
