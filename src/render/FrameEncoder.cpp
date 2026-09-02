#include "FrameEncoder.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Aero::Render {

namespace {

struct Vertex2D {
    float x = 0.0f;
    float y = 0.0f;
    uint32_t color = 0xFFFFFFFF;
    float u = 0.0f;
    float v = 0.0f;
    float coverage = 1.0f;
};

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
inline void AssignColorEnable(RenderState& state, unsigned value) noexcept {
    state.f.colorEnable = static_cast<uint8_t>(value & 1U);
}
inline void AssignBlendMode(RenderState& state, unsigned value) noexcept {
    state.f.blendMode = static_cast<uint8_t>(value & 7U);
}
inline void AssignStencilMode(RenderState& state, unsigned value) noexcept {
    state.f.stencilMode = static_cast<uint8_t>(value & 7U);
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

inline uint32_t ColorToRGBA32(Color color, double opacity) noexcept {
    const float alpha = static_cast<float>(std::clamp(static_cast<double>(color.alpha) * opacity, 0.0, 1.0));
    const auto r = static_cast<uint8_t>(std::clamp(color.red * alpha * 255.0f + 0.5f, 0.0f, 255.0f));
    const auto g = static_cast<uint8_t>(std::clamp(color.green * alpha * 255.0f + 0.5f, 0.0f, 255.0f));
    const auto b = static_cast<uint8_t>(std::clamp(color.blue * alpha * 255.0f + 0.5f, 0.0f, 255.0f));
    const auto a = static_cast<uint8_t>(std::clamp(alpha * 255.0f + 0.5f, 0.0f, 255.0f));
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
}

inline Point TransformPoint(const ProjectiveTransform2D& t, double x, double y) noexcept {
    Point output{};
    if (!Base::TryTransformPoint(t, Point{x, y}, output)) {
        output.x = std::numeric_limits<double>::quiet_NaN();
        output.y = output.x;
    }
    return output;
}

ProjectiveTransform2D CombineTransform(
    const ProjectiveTransform2D& a,
    const ProjectiveTransform2D& b) noexcept {
    return Base::Compose(a, b);
}

ProjectiveTransform2D MakeTranslate(double x, double y) noexcept {
    return Base::ToProjective(
        Base::Transform2D{1.0, 0.0, 0.0, 1.0, x, y});
}

constexpr double kRoundedCornerSegments = 10.0;
constexpr std::uint32_t kRoundedRectContourPoints = 40U;

// Builds a closed convex contour approximating a rounded rectangle. The
// corners are sampled as `kRoundedCornerSegments` arcs each; a radius of zero
// yields four repeated corner points, so the same path also covers plain
// rectangles (degenerate fan/ring quads collapse harmlessly).
void BuildRoundedRectContour(
    double x, double y, double width, double height, double radius,
    Point* out) noexcept {
    const double pi = 3.14159265358979323846;
    const double centers[4][2] = {
        {x + radius, y + radius},
        {x + width - radius, y + radius},
        {x + width - radius, y + height - radius},
        {x + radius, y + height - radius}
    };
    const double start[4] = {pi, pi * 1.5, 0.0, pi * 0.5};

    std::uint32_t index = 0U;
    for (int corner = 0; corner < 4; ++corner) {
        for (std::uint32_t i = 0U; i < kRoundedCornerSegments; ++i) {
            const double angle = start[corner] +
                pi * 0.5 * (static_cast<double>(i) / kRoundedCornerSegments);
            out[index].x = centers[corner][0] + radius * std::cos(angle);
            out[index].y = centers[corner][1] + radius * std::sin(angle);
            ++index;
        }
    }
}

} // namespace

UiFrameEncoder::UiFrameEncoder(
    RenderDevice& device,
    Base::IAllocator* allocator) noexcept
    : device_(&device), allocator_(allocator) {}

UiFrameEncoder::~UiFrameEncoder() noexcept {
    Shutdown();
}

Base::Result<void> UiFrameEncoder::Initialize() noexcept {
    initialized_ = true;
    return {};
}

void UiFrameEncoder::Shutdown() noexcept {
    images_.Clear();
    atlases_.Clear();
    gradients_.Clear();
    glyphRuns_.Clear();
    meshes_.Clear();
    offscreenTargets_.Clear();
    initialized_ = false;
}

Base::Result<void> UiFrameEncoder::RegisterImage(
    RenderImageId imageId,
    Ref<Texture> texture) noexcept {
    for (auto& entry : images_) {
        if (entry.id == imageId) {
            entry.texture = std::move(texture);
            return {};
        }
    }
    return images_.PushBack(ImageEntry{imageId, std::move(texture)});
}

void UiFrameEncoder::UnregisterImage(RenderImageId imageId) noexcept {
    for (std::uint32_t i = 0; i < images_.Size(); ++i) {
        if (images_[i].id == imageId) {
            if (i != images_.Size() - 1) {
                images_[i] = std::move(images_[images_.Size() - 1]);
            }
            static_cast<void>(images_.Resize(images_.Size() - 1));
            return;
        }
    }
}

Base::Result<void> UiFrameEncoder::RegisterGlyphAtlas(
    std::uint32_t page,
    Ref<Texture> texture) noexcept {
    for (auto& entry : atlases_) {
        if (entry.page == page) {
            entry.texture = std::move(texture);
            return {};
        }
    }
    return atlases_.PushBack(AtlasEntry{page, std::move(texture)});
}

Base::Result<void> UiFrameEncoder::RegisterGlyphRun(
    RenderGlyphRunId glyphRun,
    Base::Span<const RenderGlyphQuad> quads) noexcept {
    for (auto& entry : glyphRuns_) {
        if (entry.glyphRun == glyphRun) {
            entry.quads.Clear();
            return entry.quads.Append(quads);
        }
    }
    GlyphRunEntry entry;
    entry.glyphRun = glyphRun;
    Base::Result<void> copied = entry.quads.Append(quads);
    if (!copied) return copied.GetStatus();
    return glyphRuns_.PushBack(std::move(entry));
}

void UiFrameEncoder::UnregisterGlyphRun(RenderGlyphRunId glyphRun) noexcept {
    for (std::uint32_t i = 0U; i < glyphRuns_.Size(); ++i) {
        if (glyphRuns_[i].glyphRun == glyphRun) {
            if (i != glyphRuns_.Size() - 1U) {
                glyphRuns_[i] = std::move(glyphRuns_[glyphRuns_.Size() - 1U]);
            }
            static_cast<void>(glyphRuns_.Resize(glyphRuns_.Size() - 1U));
            return;
        }
    }
}

Base::Result<void> UiFrameEncoder::RegisterMesh(
    RenderMeshId mesh,
    Base::Span<const Point> vertices,
    Base::Span<const std::uint32_t> indices) noexcept {
    for (auto& entry : meshes_) {
        if (entry.mesh == mesh) {
            entry.vertices.Clear();
            entry.indices.Clear();
            Base::Result<void> v = entry.vertices.Append(vertices);
            if (!v) return v.GetStatus();
            return entry.indices.Append(indices);
        }
    }
    MeshEntry entry;
    entry.mesh = mesh;
    Base::Result<void> v = entry.vertices.Append(vertices);
    if (!v) return v.GetStatus();
    Base::Result<void> i = entry.indices.Append(indices);
    if (!i) return i.GetStatus();
    return meshes_.PushBack(std::move(entry));
}

void UiFrameEncoder::UnregisterMesh(RenderMeshId mesh) noexcept {
    for (std::uint32_t i = 0U; i < meshes_.Size(); ++i) {
        if (meshes_[i].mesh == mesh) {
            if (i != meshes_.Size() - 1U) {
                meshes_[i] = std::move(meshes_[meshes_.Size() - 1U]);
            }
            static_cast<void>(meshes_.Resize(meshes_.Size() - 1U));
            return;
        }
    }
}

Texture* UiFrameEncoder::FindImage(RenderImageId id) const noexcept {
    for (const auto& entry : images_) {
        if (entry.id == id) return entry.texture.Get();
    }
    return nullptr;
}

Texture* UiFrameEncoder::FindAtlas(std::uint32_t page) const noexcept {
    for (const auto& entry : atlases_) {
        if (entry.page == page) return entry.texture.Get();
    }
    return nullptr;
}

const Base::Vector<RenderGlyphQuad>* UiFrameEncoder::FindGlyphRun(
    RenderGlyphRunId glyphRun) const noexcept {
    for (const auto& entry : glyphRuns_) {
        if (entry.glyphRun == glyphRun) return &entry.quads;
    }
    return nullptr;
}

const UiFrameEncoder::MeshEntry* UiFrameEncoder::FindMesh(
    RenderMeshId mesh) const noexcept {
    for (const auto& entry : meshes_) {
        if (entry.mesh == mesh) return &entry;
    }
    return nullptr;
}

Texture* UiFrameEncoder::GetOrCreateGradientRamp(
    const RenderGradientRampSnapshot& ramp) noexcept {
    for (auto& entry : gradients_) {
        if (entry.brushIdentity == ramp.brushIdentity) {
            if (entry.revision != ramp.revision) {
                entry.revision = ramp.revision;
                device_->UpdateTexture(
                    entry.texture.Get(), 0, 0, 0, GradientRampWidth, 1, ramp.pixels.data());
            }
            return entry.texture.Get();
        }
    }
    const void* data = ramp.pixels.data();
    Ref<Texture> tex = device_->CreateTexture(
        "GradientRamp", GradientRampWidth, 1, 1, TextureFormat::RGBA8, &data);
    if (!tex) return nullptr;
    Texture* result = tex.Get();
    static_cast<void>(gradients_.PushBack(
        GradientEntry{ramp.brushIdentity, ramp.revision, std::move(tex)}));
    return result;
}

RenderTarget* UiFrameEncoder::GetOrCreateOffscreenTarget(
    RenderNodeId nodeId, std::uint32_t width, std::uint32_t height,
    bool isMask) noexcept {
    const bool needsStencil = !isMask;
    for (auto& entry : offscreenTargets_) {
        if (entry.nodeId == nodeId && entry.isMask == isMask) {
            if (entry.width != width || entry.height != height) {
                entry.target = device_->CreateRenderTarget(
                    "Offscreen", width, height, 1, needsStencil);
                entry.width = width;
                entry.height = height;
            }
            return entry.target.Get();
        }
    }
    Ref<RenderTarget> target = device_->CreateRenderTarget(
        "Offscreen", width, height, 1, needsStencil);
    if (!target) return nullptr;
    RenderTarget* result = target.Get();
    static_cast<void>(offscreenTargets_.PushBack(
        OffscreenTargetEntry{nodeId, isMask, std::move(target), width, height}));
    return result;
}

void UiFrameEncoder::ResetFrame() noexcept {
    currentVertexOffset_ = 0U;
    currentVertexCount_ = 0U;
    currentIndexCount_ = 0U;
    currentBatch_ = Batch{};
    clipDepth_ = 0U;
    clipStack_.Clear();
    stats_ = FrameStatistics{};
}

void UiFrameEncoder::FlushBatch() noexcept {
    if (currentBatch_.numIndices > 0U && device_ != nullptr) {
        // Unmap the dynamic buffers before issuing the draw. D3D11 does not
        // permit drawing while a vertex/index buffer is mapped.
        if (mappedVertices_ != nullptr) {
            device_->UnmapVertices();
            mappedVertices_ = nullptr;
        }
        if (mappedIndices_ != nullptr) {
            device_->UnmapIndices();
            mappedIndices_ = nullptr;
        }

        device_->DrawBatch(currentBatch_);
        ++stats_.drawCallCount;
        ++stats_.batchCount;

        // Remap with WRITE_DISCARD to get a fresh buffer; recording restarts
        // from offset zero for the next batch.
        mappedVertices_ = static_cast<uint8_t*>(device_->MapVertices(DYNAMIC_VB_SIZE));
        mappedIndices_ = static_cast<uint16_t*>(device_->MapIndices(DYNAMIC_IB_SIZE));
        currentVertexOffset_ = 0U;
        currentVertexCount_ = 0U;
        currentIndexCount_ = 0U;
        currentBatch_.startIndex = 0U;
        currentBatch_.vertexOffset = 0U;
        currentBatch_.numIndices = 0U;
        currentBatch_.numVertices = 0U;
    }
}

void UiFrameEncoder::EmitQuad(
    const Point points[4],
    const Point uvs[4],
    Color color) noexcept {
    if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) return;
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) {
            return;
        }
    }

    if (currentVertexCount_ + 4U > (DYNAMIC_VB_SIZE / sizeof(Vertex2D)) ||
        currentIndexCount_ + 6U > (DYNAMIC_IB_SIZE / sizeof(uint16_t))) {
        // FlushBatch remaps the buffers (WRITE_DISCARD) and resets the offsets,
        // so recording can continue from a fresh buffer.
        FlushBatch();
        if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) {
            if (device_ != nullptr) {
                mappedVertices_ = static_cast<uint8_t*>(device_->MapVertices(DYNAMIC_VB_SIZE));
                mappedIndices_ = static_cast<uint16_t*>(device_->MapIndices(DYNAMIC_IB_SIZE));
            }
        }
        currentVertexOffset_ = 0U;
        currentVertexCount_ = 0U;
        currentIndexCount_ = 0U;
        currentBatch_.startIndex = 0U;
        currentBatch_.vertexOffset = 0U;
    }

    auto* v = reinterpret_cast<Vertex2D*>(mappedVertices_ + currentVertexOffset_);
    const uint32_t color32 = ColorToRGBA32(color, 1.0);

    for (int i = 0; i < 4; ++i) {
        v[i].x = static_cast<float>(points[i].x);
        v[i].y = static_cast<float>(points[i].y);
        v[i].color = color32;
        v[i].u = static_cast<float>(uvs[i].x);
        v[i].v = static_cast<float>(uvs[i].y);
        v[i].coverage = 1.0f;
    }

    const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_);
    mappedIndices_[currentIndexCount_ + 0] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 1] = baseVertex + 1;
    mappedIndices_[currentIndexCount_ + 2] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 3] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 4] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 5] = baseVertex + 3;

    currentVertexOffset_ += 4U * sizeof(Vertex2D);
    currentVertexCount_ += 4U;
    currentIndexCount_ += 6U;
    currentBatch_.numVertices += 4U;
    currentBatch_.numIndices += 6U;
    stats_.vertexCount += 4U;
    stats_.indexCount += 6U;
}

void UiFrameEncoder::EmitQuadWithColors(
    const Point points[4],
    const Point uvs[4],
    const Color colors[4],
    double opacity) noexcept {
    if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) return;
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) {
            return;
        }
    }

    if (currentVertexCount_ + 4U > (DYNAMIC_VB_SIZE / sizeof(Vertex2D)) ||
        currentIndexCount_ + 6U > (DYNAMIC_IB_SIZE / sizeof(uint16_t))) {
        FlushBatch();
        if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) {
            if (device_ != nullptr) {
                mappedVertices_ = static_cast<uint8_t*>(device_->MapVertices(DYNAMIC_VB_SIZE));
                mappedIndices_ = static_cast<uint16_t*>(device_->MapIndices(DYNAMIC_IB_SIZE));
            }
        }
        currentVertexOffset_ = 0U;
        currentVertexCount_ = 0U;
        currentIndexCount_ = 0U;
        currentBatch_.startIndex = 0U;
        currentBatch_.vertexOffset = 0U;
    }

    auto* v = reinterpret_cast<Vertex2D*>(mappedVertices_ + currentVertexOffset_);

    for (int i = 0; i < 4; ++i) {
        v[i].x = static_cast<float>(points[i].x);
        v[i].y = static_cast<float>(points[i].y);
        v[i].color = ColorToRGBA32(colors[i], opacity);
        v[i].u = static_cast<float>(uvs[i].x);
        v[i].v = static_cast<float>(uvs[i].y);
        v[i].coverage = 1.0f;
    }

    const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_);
    mappedIndices_[currentIndexCount_ + 0] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 1] = baseVertex + 1;
    mappedIndices_[currentIndexCount_ + 2] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 3] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 4] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 5] = baseVertex + 3;

    currentVertexOffset_ += 4U * sizeof(Vertex2D);
    currentVertexCount_ += 4U;
    currentIndexCount_ += 6U;
    currentBatch_.numVertices += 4U;
    currentBatch_.numIndices += 6U;
    stats_.vertexCount += 4U;
    stats_.indexCount += 6U;
}

void UiFrameEncoder::EmitTriangleFan(
    const Point* perimeter,
    std::uint32_t perimeterCount,
    Point centroid,
    Color color) noexcept {
    if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) return;
    if (perimeterCount < 3U) return;

    const std::uint32_t vertexCount = perimeterCount + 1U;
    const std::uint32_t indexCount = perimeterCount * 3U;

    if (currentVertexCount_ + vertexCount > (DYNAMIC_VB_SIZE / sizeof(Vertex2D)) ||
        currentIndexCount_ + indexCount > (DYNAMIC_IB_SIZE / sizeof(uint16_t))) {
        FlushBatch();
        if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) {
            if (device_ != nullptr) {
                mappedVertices_ = static_cast<uint8_t*>(device_->MapVertices(DYNAMIC_VB_SIZE));
                mappedIndices_ = static_cast<uint16_t*>(device_->MapIndices(DYNAMIC_IB_SIZE));
            }
        }
        currentVertexOffset_ = 0U;
        currentVertexCount_ = 0U;
        currentIndexCount_ = 0U;
        currentBatch_.startIndex = 0U;
        currentBatch_.vertexOffset = 0U;
    }

    auto* v = reinterpret_cast<Vertex2D*>(mappedVertices_ + currentVertexOffset_);
    const uint32_t color32 = ColorToRGBA32(color, 1.0);

    v[0].x = static_cast<float>(centroid.x);
    v[0].y = static_cast<float>(centroid.y);
    v[0].color = color32;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[0].coverage = 1.0f;

    for (std::uint32_t i = 0; i < perimeterCount; ++i) {
        v[i + 1].x = static_cast<float>(perimeter[i].x);
        v[i + 1].y = static_cast<float>(perimeter[i].y);
        v[i + 1].color = color32;
        v[i + 1].u = 0.0f;
        v[i + 1].v = 0.0f;
        v[i + 1].coverage = 1.0f;
    }

    const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_);
    std::uint32_t index = 0U;
    for (std::uint32_t i = 0; i < perimeterCount; ++i) {
        const uint16_t next = (i + 1U < perimeterCount)
            ? static_cast<uint16_t>(baseVertex + i + 2U)
            : static_cast<uint16_t>(baseVertex + 1U);
        mappedIndices_[currentIndexCount_ + index + 0] = static_cast<uint16_t>(baseVertex + 0U);
        mappedIndices_[currentIndexCount_ + index + 1] = static_cast<uint16_t>(baseVertex + i + 1U);
        mappedIndices_[currentIndexCount_ + index + 2] = next;
        index += 3U;
    }

    currentVertexOffset_ += vertexCount * static_cast<std::uint32_t>(sizeof(Vertex2D));
    currentVertexCount_ += vertexCount;
    currentIndexCount_ += indexCount;
    currentBatch_.numVertices += vertexCount;
    currentBatch_.numIndices += indexCount;
    stats_.vertexCount += vertexCount;
    stats_.indexCount += indexCount;
}

void UiFrameEncoder::EnsureBatchBlend(Shader::Enum shader) noexcept {
    if (currentBatch_.numIndices > 0U &&
        (currentBatch_.shader != shader ||
         currentBatch_.renderState.f.colorEnable != 1U ||
         currentBatch_.renderState.f.blendMode != static_cast<uint8_t>(currentBlendMode_))) {
        FlushBatch();
    }
    currentBatch_.shader = shader;
    AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
    AssignColorEnable(currentBatch_.renderState, 1U);
}

void UiFrameEncoder::SetBatchImage(
    Shader::Enum shader,
    Texture* texture,
    Texture* maskTexture) noexcept {
    const bool batchActive = currentBatch_.numIndices > 0U;
    if (batchActive &&
        (currentBatch_.shader != shader ||
         currentBatch_.image != texture ||
         currentBatch_.shadow != maskTexture ||
         currentBatch_.renderState.f.colorEnable != 1U ||
         currentBatch_.renderState.f.blendMode !=
             static_cast<uint8_t>(currentBlendMode_))) {
        FlushBatch();
    }
    currentBatch_.shader = shader;
    currentBatch_.image = texture;
    currentBatch_.shadow = maskTexture;
    currentBatch_.imageSampler.f.wrapMode = WrapMode::ClampToEdge;
    currentBatch_.imageSampler.f.minmagFilter = MinMagFilter::Linear;
    currentBatch_.shadowSampler.f.wrapMode = WrapMode::ClampToEdge;
    currentBatch_.shadowSampler.f.minmagFilter = MinMagFilter::Linear;
    AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
    AssignColorEnable(currentBatch_.renderState, 1U);
}

void UiFrameEncoder::SetBatchRamp(
    Shader::Enum shader,
    Texture* ramp,
    const float uniforms[8]) noexcept {
    const bool uniformsDiffer =
        std::memcmp(paintUniforms_, uniforms, sizeof(paintUniforms_)) != 0;
    const bool batchActive = currentBatch_.numIndices > 0U;
    if (batchActive &&
        (currentBatch_.shader != shader ||
         currentBatch_.ramps != ramp ||
         uniformsDiffer ||
         currentBatch_.renderState.f.colorEnable != 1U ||
         currentBatch_.renderState.f.blendMode !=
             static_cast<uint8_t>(currentBlendMode_))) {
        FlushBatch();
    }
    std::memcpy(paintUniforms_, uniforms, sizeof(paintUniforms_));
    currentBatch_.shader = shader;
    currentBatch_.ramps = ramp;
    currentBatch_.rampsSampler.f.wrapMode = WrapMode::ClampToEdge;
    currentBatch_.rampsSampler.f.minmagFilter = MinMagFilter::Linear;
    currentBatch_.pixelUniforms[0].values = paintUniforms_;
    currentBatch_.pixelUniforms[0].numDwords = 8U;
    AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
    AssignColorEnable(currentBatch_.renderState, 1U);
}

void UiFrameEncoder::EmitMaskBrush(
    const RenderMaskSnapshot& mask,
    double width,
    double height,
    const RenderFrame& frame) noexcept {
    RenderCommand cmd{};
    switch (mask.kind) {
    case RenderMaskKind::Solid:
        cmd.kind = RenderCommandKind::FillRect;
        cmd.rect = Rect{0.0, 0.0, width, height};
        cmd.color = mask.color;
        ProcessCommand(cmd, ProjectiveTransform2D{}, 1.0);
        break;
    case RenderMaskKind::Image:
        if (mask.image != InvalidRenderImageId) {
            cmd.kind = RenderCommandKind::DrawImage;
            cmd.rect = Rect{0.0, 0.0, width, height};
            cmd.image = mask.image;
            cmd.sourceUv = mask.sourceUv;
            cmd.color = Color{1.0F, 1.0F, 1.0F, 1.0F};
            ProcessCommand(cmd, ProjectiveTransform2D{}, 1.0);
        }
        break;
    case RenderMaskKind::LinearGradient:
    case RenderMaskKind::RadialGradient: {
        const Base::Span<const RenderGradientRampSnapshot> ramps =
            frame.GradientRamps();
        if (mask.gradientRamp >= ramps.Size()) break;
        const RenderGradientRampSnapshot& ramp =
            ramps[mask.gradientRamp];

        auto sampleAlpha = [&ramp](double t) noexcept {
            t = std::clamp(t, 0.0, 1.0);
            const std::uint32_t index =
                static_cast<std::uint32_t>(
                    t * static_cast<double>(GradientRampWidth - 1U));
            return static_cast<float>(ramp.pixels[index * 4U + 3U]) / 255.0F;
        };

        const Point corners[4] = {
            Point{0.0, 0.0},
            Point{width, 0.0},
            Point{width, height},
            Point{0.0, height}};
        Color colors[4]{};
        for (int i = 0; i < 4; ++i) {
            double t = 0.0;
            if (mask.kind == RenderMaskKind::LinearGradient) {
                const double dx = mask.endPoint.x - mask.startPoint.x;
                const double dy = mask.endPoint.y - mask.startPoint.y;
                const double lengthSq = dx * dx + dy * dy;
                if (lengthSq > 0.0) {
                    const double px =
                        corners[i].x / std::max(1.0, width);
                    const double py =
                        corners[i].y / std::max(1.0, height);
                    t = ((px - mask.startPoint.x) * dx +
                         (py - mask.startPoint.y) * dy) / lengthSq;
                }
            } else {
                const double rx = mask.radiusX > 0.0
                    ? mask.radiusX : 1.0;
                const double ry = mask.radiusY > 0.0
                    ? mask.radiusY : 1.0;
                const double px =
                    corners[i].x / std::max(1.0, width) - mask.center.x;
                const double py =
                    corners[i].y / std::max(1.0, height) - mask.center.y;
                t = std::sqrt(px * px / (rx * rx) + py * py / (ry * ry));
            }
            const float alpha = sampleAlpha(t);
            colors[i] = Color{1.0F, 1.0F, 1.0F, alpha};
        }
        cmd.kind = RenderCommandKind::FillGradientQuad;
        for (int i = 0; i < 4; ++i) {
            cmd.points[i] = corners[i];
            cmd.colors[i] = colors[i];
        }
        ProcessCommand(cmd, ProjectiveTransform2D{}, 1.0);
        break;
    }
    case RenderMaskKind::None:
    default:
        break;
    }
}

void UiFrameEncoder::CompositeOffscreen(
    const RenderNodeSnapshot& node,
    RenderTarget* offscreen,
    RenderTarget* maskTarget,
    const ProjectiveTransform2D& nodeTransform,
    double nodeOpacity,
    double dpi,
    const RenderFrame& frame) noexcept {
    if (offscreen == nullptr) return;
    Texture* texture = offscreen->GetTexture();
    if (texture == nullptr) return;

    const double w = node.renderSize.width;
    const double h = node.renderSize.height;
    if (w <= 0.0 || h <= 0.0) return;

    const Point uvs[4] = {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

    const bool hasMask =
        node.mask.kind != RenderMaskKind::None &&
        maskTarget != nullptr &&
        maskTarget->GetTexture() != nullptr;

    const double blurScale = std::max(1.0, node.effect.radius * dpi);
    offscreenSizeUniform_[0] =
        static_cast<float>(static_cast<double>(texture->GetWidth()) / blurScale);
    offscreenSizeUniform_[1] =
        static_cast<float>(static_cast<double>(texture->GetHeight()) / blurScale);

    // Offscreen targets pad by twice the blur radius so the 3x3 kernel, which
    // samples at ±radius, does not clamp against the texture edge. Composite
    // quads are expanded by the same local padding so the halo is visible.
    const double pad = (node.effect.kind == RenderEffectKind::DropShadow ||
                        node.effect.kind == RenderEffectKind::Blur ||
                        node.effect.kind == RenderEffectKind::DirectionalBlur)
        ? std::max(0.0, node.effect.radius) * 2.0
        : 0.0;
    const double quadW = w + pad * 2.0;
    const double quadH = h + pad * 2.0;

    // Drop shadow pass: blurred, tinted copy of the offscreen alpha, offset
    // along the effect direction, composited behind the source.
    // WPF Direction is degrees from +X, counter-clockwise in a Y-up space.
    // Default 315° therefore casts down-right in screen (Y-down) coordinates:
    //   offsetX =  cos(θ) * depth
    //   offsetY = -sin(θ) * depth
    if (node.effect.kind == RenderEffectKind::DropShadow) {
        const double radians =
            node.effect.direction * 0.017453292519943295;
        const double offsetX =
            std::cos(radians) * node.effect.depth;
        const double offsetY =
            -std::sin(radians) * node.effect.depth;
        const Point shadowPoints[4] = {
            TransformPoint(nodeTransform, -pad + offsetX, -pad + offsetY),
            TransformPoint(nodeTransform, -pad + quadW + offsetX, -pad + offsetY),
            TransformPoint(nodeTransform, -pad + quadW + offsetX, -pad + quadH + offsetY),
            TransformPoint(nodeTransform, -pad + offsetX, -pad + quadH + offsetY)};

        Color shadowColor = node.effect.color;
        shadowColor.alpha *= static_cast<float>(
            std::clamp(nodeOpacity * node.effect.opacity, 0.0, 1.0));

        SetBatchImage(Shader::Shadow, texture);
        currentBatch_.pixelUniforms[0] =
            {offscreenSizeUniform_, 2U, 0U};
        SetContentStencil();
        EmitQuad(shadowPoints, uvs, shadowColor);
    } else if (node.effect.kind == RenderEffectKind::DirectionalBlur &&
               node.effect.radius > 0.0) {
        const double radians =
            node.effect.direction * 0.017453292519943295;
        const double offsetX = std::cos(radians) * node.effect.radius;
        const double offsetY = std::sin(radians) * node.effect.radius;
        const Point blurPoints[4] = {
            TransformPoint(nodeTransform, -pad - offsetX, -pad - offsetY),
            TransformPoint(nodeTransform, -pad + quadW - offsetX, -pad - offsetY),
            TransformPoint(nodeTransform, -pad + quadW - offsetX, -pad + quadH - offsetY),
            TransformPoint(nodeTransform, -pad - offsetX, -pad + quadH - offsetY)};
        SetBatchImage(Shader::Blur, texture);
        currentBatch_.pixelUniforms[0] =
            {offscreenSizeUniform_, 2U, 0U};
        SetContentStencil();
        Color blurTint{1.0F, 1.0F, 1.0F, static_cast<float>(
            std::clamp(nodeOpacity * 0.5, 0.0, 1.0))};
        EmitQuad(blurPoints, uvs, blurTint);
    }

    // Source pass: the offscreen content composited into the parent context,
    // with group opacity, blur or opacity mask applied as required.
    Shader::Enum shader = Shader::Path_Pattern;
    if (hasMask) {
        shader = Shader::Mask;
    } else if ((node.effect.kind == RenderEffectKind::Blur ||
                node.effect.kind == RenderEffectKind::DirectionalBlur) &&
               node.effect.radius > 0.0) {
        shader = Shader::Blur;
    } else if (node.effect.kind == RenderEffectKind::Custom) {
        shader = Shader::Custom_Effect;
    }
    SetBatchImage(
        shader, texture,
        hasMask ? maskTarget->GetTexture() : nullptr);
    if (shader == Shader::Blur || shader == Shader::Custom_Effect) {
        currentBatch_.pixelUniforms[0] =
            {offscreenSizeUniform_, 2U, 0U};
        if (shader == Shader::Custom_Effect && node.effect.uniformCount > 0U) {
            customEffectUniforms_[0] = node.effect.uniforms[0];
            customEffectUniforms_[1] = node.effect.uniforms[1];
            customEffectUniforms_[2] = node.effect.uniforms[2];
            customEffectUniforms_[3] = node.effect.uniforms[3];
            currentBatch_.pixelUniforms[1] =
                {customEffectUniforms_, std::min(node.effect.uniformCount, 4U), 0U};
        }
        if (!node.effect.bytecode.Empty()) {
            currentBatch_.pixelShader = const_cast<void*>(
                static_cast<const void*>(node.effect.bytecode.Data()));
            currentBatch_.vertexUniforms[1].numDwords =
                node.effect.bytecode.Size();
        }
    }

    const Point points[4] = {
        TransformPoint(nodeTransform, -pad, -pad),
        TransformPoint(nodeTransform, -pad + quadW, -pad),
        TransformPoint(nodeTransform, -pad + quadW, -pad + quadH),
        TransformPoint(nodeTransform, -pad, -pad + quadH)};

    Color tint = Color{1.0F, 1.0F, 1.0F, 1.0F};
    tint.alpha = static_cast<float>(
        std::clamp(nodeOpacity, 0.0, 1.0));
    if (node.effect.kind == RenderEffectKind::Tint) {
        tint.red *= node.effect.color.red;
        tint.green *= node.effect.color.green;
        tint.blue *= node.effect.color.blue;
        tint.alpha *= node.effect.color.alpha;
    }
    if (node.mask.kind == RenderMaskKind::Solid && !hasMask) {
        tint.alpha *= node.mask.color.alpha;
    }

    SetContentStencil();
    if (node.effect.kind == RenderEffectKind::Pixelate &&
        node.effect.size > 1.0) {
        const double cell = std::max(1.0, node.effect.size);
        const std::uint32_t columns = std::max(
            1U, static_cast<std::uint32_t>(std::ceil(w / cell)));
        const std::uint32_t rows = std::max(
            1U, static_cast<std::uint32_t>(std::ceil(h / cell)));
        const std::uint32_t columnCap = std::min(columns, 64U);
        const std::uint32_t rowCap = std::min(rows, 64U);
        for (std::uint32_t row = 0U; row < rowCap; ++row) {
            for (std::uint32_t column = 0U; column < columnCap; ++column) {
                const double x0 = static_cast<double>(column) * w /
                    static_cast<double>(columnCap);
                const double y0 = static_cast<double>(row) * h /
                    static_cast<double>(rowCap);
                const double x1 = static_cast<double>(column + 1U) * w /
                    static_cast<double>(columnCap);
                const double y1 = static_cast<double>(row + 1U) * h /
                    static_cast<double>(rowCap);
                const double u = (static_cast<double>(column) + 0.5) /
                    static_cast<double>(columnCap);
                const double v = (static_cast<double>(row) + 0.5) /
                    static_cast<double>(rowCap);
                const Point cellPoints[4] = {
                    TransformPoint(nodeTransform, x0, y0),
                    TransformPoint(nodeTransform, x1, y0),
                    TransformPoint(nodeTransform, x1, y1),
                    TransformPoint(nodeTransform, x0, y1)};
                const Point cellUvs[4] = {
                    {u, v}, {u, v}, {u, v}, {u, v}};
                EmitQuad(cellPoints, cellUvs, tint);
            }
        }
    } else {
        EmitQuad(points, uvs, tint);
    }

    static_cast<void>(frame);
}

void UiFrameEncoder::SetContentStencil() noexcept {
    const std::uint8_t mode =
        clipDepth_ > 0U ? StencilMode::Equal_Keep : StencilMode::Disabled;
    const std::uint8_t ref = static_cast<std::uint8_t>(clipDepth_);
    if (currentBatch_.numIndices > 0U &&
        (currentBatch_.renderState.f.stencilMode != mode ||
         currentBatch_.stencilRef != ref)) {
        FlushBatch();
    }
    AssignStencilMode(currentBatch_.renderState, mode);
    currentBatch_.stencilRef = ref;
}

void UiFrameEncoder::EmitClipQuad(
    const Rect& rect,
    const ProjectiveTransform2D& transform,
    std::uint8_t stencilMode,
    std::uint8_t stencilRef) noexcept {
    if (currentBatch_.numIndices > 0U &&
        (currentBatch_.shader != Shader::Path_Solid ||
         currentBatch_.renderState.f.colorEnable != 0U ||
         currentBatch_.renderState.f.stencilMode != stencilMode ||
         currentBatch_.stencilRef != stencilRef)) {
        FlushBatch();
    }
    currentBatch_.shader = Shader::Path_Solid;
    AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(BlendMode::SrcOver));
    AssignColorEnable(currentBatch_.renderState, 0U);
    AssignStencilMode(currentBatch_.renderState, stencilMode);
    currentBatch_.stencilRef = stencilRef;

    const Point p0 = TransformPoint(transform, rect.x, rect.y);
    const Point p1 = TransformPoint(transform, rect.x + rect.width, rect.y);
    const Point p2 = TransformPoint(transform, rect.x + rect.width, rect.y + rect.height);
    const Point p3 = TransformPoint(transform, rect.x, rect.y + rect.height);

    const Point points[4] = {p0, p1, p2, p3};
    const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

    EmitQuad(points, uvs, {0.0F, 0.0F, 0.0F, 0.0F});
}

void UiFrameEncoder::EmitClipTriangles(
    Base::Span<const Point> vertices,
    Base::Span<const std::uint32_t> indices,
    std::uint32_t vertexOffset,
    std::uint32_t vertexCount,
    std::uint32_t indexOffset,
    std::uint32_t indexCount,
    const ProjectiveTransform2D& transform,
    std::uint8_t stencilMode,
    std::uint8_t stencilRef) noexcept {
    if (vertexCount == 0U || indexCount < 3U ||
        vertexOffset >= vertices.Size() ||
        indexOffset >= indices.Size() ||
        vertexCount > vertices.Size() - vertexOffset ||
        indexCount > indices.Size() - indexOffset) {
        return;
    }
    if (currentBatch_.numIndices > 0U &&
        (currentBatch_.shader != Shader::Path_Solid ||
         currentBatch_.renderState.f.colorEnable != 0U ||
         currentBatch_.renderState.f.stencilMode != stencilMode ||
         currentBatch_.stencilRef != stencilRef)) {
        FlushBatch();
    }
    currentBatch_.shader = Shader::Path_Solid;
    AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(BlendMode::SrcOver));
    AssignColorEnable(currentBatch_.renderState, 0U);
    AssignStencilMode(currentBatch_.renderState, stencilMode);
    currentBatch_.stencilRef = stencilRef;

    const Color transparent{0.0F, 0.0F, 0.0F, 0.0F};
    for (std::uint32_t triangle = 0U; triangle + 2U < indexCount; triangle += 3U) {
        const std::uint32_t i0 = indices[indexOffset + triangle];
        const std::uint32_t i1 = indices[indexOffset + triangle + 1U];
        const std::uint32_t i2 = indices[indexOffset + triangle + 2U];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
            continue;
        }
        const Point a = TransformPoint(
            transform,
            vertices[vertexOffset + i0].x,
            vertices[vertexOffset + i0].y);
        const Point b = TransformPoint(
            transform,
            vertices[vertexOffset + i1].x,
            vertices[vertexOffset + i1].y);
        const Point c = TransformPoint(
            transform,
            vertices[vertexOffset + i2].x,
            vertices[vertexOffset + i2].y);
        const Point points[4] = {a, b, c, c};
        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
        EmitQuad(points, uvs, transparent);
    }
}

void UiFrameEncoder::ProcessCommand(
    const RenderCommand& cmd,
    const ProjectiveTransform2D& currentTransform,
    double currentOpacity) noexcept {
    switch (cmd.kind) {
    case RenderCommandKind::PushClip: {
        EmitClipQuad(
            cmd.rect, currentTransform, StencilMode::Equal_Incr, clipDepth_);
        static_cast<void>(clipStack_.PushBack(
            ClipEntry{cmd.rect, currentTransform}));
        ++clipDepth_;
        break;
    }
    case RenderCommandKind::PopClip: {
        if (clipDepth_ > 0U && !clipStack_.Empty()) {
            const ClipEntry entry = clipStack_.Back();
            static_cast<void>(clipStack_.PopBack());
            --clipDepth_;
            EmitClipQuad(
                entry.rect, entry.transform, StencilMode::Equal_Decr,
                static_cast<std::uint8_t>(clipDepth_ + 1U));
        }
        break;
    }
    case RenderCommandKind::FillRect: {
        EnsureBatchBlend(Shader::Path_Solid);
        SetContentStencil();

        const Point p0 = TransformPoint(currentTransform, cmd.rect.x, cmd.rect.y);
        const Point p1 = TransformPoint(currentTransform, cmd.rect.x + cmd.rect.width, cmd.rect.y);
        const Point p2 = TransformPoint(currentTransform, cmd.rect.x + cmd.rect.width, cmd.rect.y + cmd.rect.height);
        const Point p3 = TransformPoint(currentTransform, cmd.rect.x, cmd.rect.y + cmd.rect.height);

        const Point points[4] = {p0, p1, p2, p3};
        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

        Color c = cmd.color;
        c.alpha = static_cast<float>(c.alpha * currentOpacity);
        EmitQuad(points, uvs, c);
        break;
    }
    case RenderCommandKind::FillGradientQuad: {
        const Point p0 = TransformPoint(currentTransform, cmd.points[0].x, cmd.points[0].y);
        const Point p1 = TransformPoint(currentTransform, cmd.points[1].x, cmd.points[1].y);
        const Point p2 = TransformPoint(currentTransform, cmd.points[2].x, cmd.points[2].y);
        const Point p3 = TransformPoint(currentTransform, cmd.points[3].x, cmd.points[3].y);

        const Point points[4] = {p0, p1, p2, p3};
        if (cmd.paintKind == 1U || cmd.paintKind == 2U) {
            const Shader::Enum shader = cmd.paintKind == 1U
                ? Shader::Path_Linear
                : Shader::Path_Radial;
            Texture* ramp = nullptr;
            if (currentFrame_ != nullptr) {
                const Base::Span<const RenderGradientRampSnapshot> ramps =
                    currentFrame_->GradientRamps();
                if (cmd.gradientRamp < ramps.Size()) {
                    ramp = GetOrCreateGradientRamp(ramps[cmd.gradientRamp]);
                }
            }
            float uniforms[8];
            std::memcpy(uniforms, cmd.paintUniforms, sizeof(uniforms));
            if (cmd.paintKind == 1U) {
                uniforms[0] *= static_cast<float>(currentOpacity);
            } else {
                uniforms[3] *= static_cast<float>(currentOpacity);
            }
            SetBatchRamp(shader, ramp, uniforms);
            SetContentStencil();
            const Color white{1.0F, 1.0F, 1.0F, 1.0F};
            EmitQuad(points, cmd.uvs, white);
            break;
        }
        EnsureBatchBlend(Shader::Path_Solid);
        SetContentStencil();

        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

        EmitQuadWithColors(points, uvs, cmd.colors, currentOpacity);
        break;
    }
    case RenderCommandKind::FillRoundedRect: {
        const double radius = std::min(
            std::max(0.0, cmd.scalar),
            std::fmin(cmd.rect.width, cmd.rect.height) * 0.5);

        EnsureBatchBlend(Shader::Path_Solid);
        SetContentStencil();

        Color c = cmd.color;
        c.alpha = static_cast<float>(c.alpha * currentOpacity);

        if (radius <= 0.0) {
            const Point p0 = TransformPoint(currentTransform, cmd.rect.x, cmd.rect.y);
            const Point p1 = TransformPoint(currentTransform, cmd.rect.x + cmd.rect.width, cmd.rect.y);
            const Point p2 = TransformPoint(currentTransform, cmd.rect.x + cmd.rect.width, cmd.rect.y + cmd.rect.height);
            const Point p3 = TransformPoint(currentTransform, cmd.rect.x, cmd.rect.y + cmd.rect.height);

            const Point points[4] = {p0, p1, p2, p3};
            const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
            EmitQuad(points, uvs, c);
            break;
        }

        Point contour[kRoundedRectContourPoints];
        BuildRoundedRectContour(
            cmd.rect.x, cmd.rect.y, cmd.rect.width, cmd.rect.height,
            radius, contour);

        const Point centroid{
            cmd.rect.x + cmd.rect.width * 0.5,
            cmd.rect.y + cmd.rect.height * 0.5};
        const Point center = TransformPoint(currentTransform, centroid.x, centroid.y);
        for (std::uint32_t i = 0U; i < kRoundedRectContourPoints; ++i) {
            contour[i] = TransformPoint(currentTransform, contour[i].x, contour[i].y);
        }
        EmitTriangleFan(contour, kRoundedRectContourPoints, center, c);
        break;
    }
    case RenderCommandKind::StrokeRect: {
        EnsureBatchBlend(Shader::Path_Solid);
        SetContentStencil();

        const double thickness = cmd.scalar > 0.0 ? cmd.scalar : 1.0;
        const double half = thickness * 0.5;
        const Rect& r = cmd.rect;
        const double maxRadius =
            std::fmin(r.width, r.height) * 0.5 - half;
        const double radius = std::clamp(
            std::max(0.0, cmd.cornerRadius), 0.0, maxRadius);

        Color c = cmd.color;
        c.alpha = static_cast<float>(c.alpha * currentOpacity);
        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

        Point outer[kRoundedRectContourPoints];
        Point inner[kRoundedRectContourPoints];
        BuildRoundedRectContour(
            r.x - half, r.y - half, r.width + 2.0 * half,
            r.height + 2.0 * half, radius + half, outer);
        BuildRoundedRectContour(
            r.x + half, r.y + half, r.width - 2.0 * half,
            r.height - 2.0 * half, std::max(0.0, radius - half), inner);

        for (std::uint32_t i = 0U; i < kRoundedRectContourPoints; ++i) {
            const std::uint32_t next = (i + 1U) % kRoundedRectContourPoints;
            const Point p0 = TransformPoint(currentTransform, outer[i].x, outer[i].y);
            const Point p1 = TransformPoint(currentTransform, outer[next].x, outer[next].y);
            const Point p2 = TransformPoint(currentTransform, inner[next].x, inner[next].y);
            const Point p3 = TransformPoint(currentTransform, inner[i].x, inner[i].y);
            const Point points[4] = {p0, p1, p2, p3};
            EmitQuad(points, uvs, c);
        }
        break;
    }
    case RenderCommandKind::DrawImage: {
        Texture* tex = FindImage(cmd.image);
        if (tex != nullptr) {
            if (currentBatch_.numIndices > 0U &&
                (currentBatch_.shader != Shader::Path_Pattern ||
                 currentBatch_.image != tex ||
                 currentBatch_.renderState.f.colorEnable != 1U ||
                 currentBatch_.renderState.f.blendMode != static_cast<uint8_t>(currentBlendMode_))) {
                FlushBatch();
            }
            currentBatch_.shader = Shader::Path_Pattern;
            currentBatch_.image = tex;
            currentBatch_.imageSampler.f.wrapMode = WrapMode::ClampToEdge;
            currentBatch_.imageSampler.f.minmagFilter = MinMagFilter::Linear;
            AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
            AssignColorEnable(currentBatch_.renderState, 1U);
            SetContentStencil();

            const Point p0 = TransformPoint(currentTransform, cmd.rect.x, cmd.rect.y);
            const Point p1 = TransformPoint(currentTransform, cmd.rect.x + cmd.rect.width, cmd.rect.y);
            const Point p2 = TransformPoint(currentTransform, cmd.rect.x + cmd.rect.width, cmd.rect.y + cmd.rect.height);
            const Point p3 = TransformPoint(currentTransform, cmd.rect.x, cmd.rect.y + cmd.rect.height);

            const Point points[4] = {p0, p1, p2, p3};
            const Point uvs[4] = {
                {cmd.sourceUv.x, cmd.sourceUv.y},
                {cmd.sourceUv.x + cmd.sourceUv.width, cmd.sourceUv.y},
                {cmd.sourceUv.x + cmd.sourceUv.width, cmd.sourceUv.y + cmd.sourceUv.height},
                {cmd.sourceUv.x, cmd.sourceUv.y + cmd.sourceUv.height}
            };

            Color c = cmd.color;
            c.alpha = static_cast<float>(c.alpha * currentOpacity);
            EmitQuad(points, uvs, c);
        }
        break;
    }
    case RenderCommandKind::DrawGlyphRun: {
        const Base::Vector<RenderGlyphQuad>* quads = FindGlyphRun(cmd.glyphRun);
        if (quads == nullptr) break;

        Color c = cmd.color;
        c.alpha = static_cast<float>(c.alpha * currentOpacity);
        for (const RenderGlyphQuad& quad : *quads) {
            Texture* atlas = FindAtlas(quad.page);
            if (atlas == nullptr) continue;
            const std::uint8_t targetStencilMode =
                clipDepth_ > 0U ? StencilMode::Equal_Keep : StencilMode::Disabled;
            const std::uint8_t targetStencilRef =
                static_cast<std::uint8_t>(clipDepth_);
            if (currentBatch_.numIndices > 0U &&
                (currentBatch_.shader != Shader::SDF_Solid ||
                 currentBatch_.glyphs != atlas ||
                 currentBatch_.renderState.f.colorEnable != 1U ||
                 currentBatch_.renderState.f.blendMode != static_cast<uint8_t>(currentBlendMode_) ||
                 currentBatch_.renderState.f.stencilMode != targetStencilMode ||
                 currentBatch_.stencilRef != targetStencilRef)) {
                FlushBatch();
            }
            currentBatch_.shader = Shader::SDF_Solid;
            currentBatch_.glyphs = atlas;
            currentBatch_.glyphsSampler.f.wrapMode = WrapMode::ClampToEdge;
            currentBatch_.glyphsSampler.f.minmagFilter = MinMagFilter::Linear;
            AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
            AssignColorEnable(currentBatch_.renderState, 1U);
            SetContentStencil();

            const Point p0 = TransformPoint(currentTransform, quad.x0, quad.y0);
            const Point p1 = TransformPoint(currentTransform, quad.x1, quad.y0);
            const Point p2 = TransformPoint(currentTransform, quad.x1, quad.y1);
            const Point p3 = TransformPoint(currentTransform, quad.x0, quad.y1);

            const Point points[4] = {p0, p1, p2, p3};
            const Point uvs[4] = {
                {quad.u0, quad.v0},
                {quad.u1, quad.v0},
                {quad.u1, quad.v1},
                {quad.u0, quad.v1}
            };

            EmitQuad(points, uvs, c);
        }
        break;
    }
    case RenderCommandKind::DrawMesh: {
        const MeshEntry* mesh = FindMesh(cmd.mesh);
        if (mesh == nullptr || mesh->indices.Empty()) {
            break;
        }

        const bool gpuPaint = cmd.paintKind == 1U || cmd.paintKind == 2U;
        if (gpuPaint) {
            Texture* ramp = nullptr;
            if (currentFrame_ != nullptr) {
                const Base::Span<const RenderGradientRampSnapshot> ramps =
                    currentFrame_->GradientRamps();
                if (cmd.gradientRamp < ramps.Size()) {
                    ramp = GetOrCreateGradientRamp(ramps[cmd.gradientRamp]);
                }
            }
            float uniforms[8];
            std::memcpy(uniforms, cmd.paintUniforms, sizeof(uniforms));
            if (cmd.paintKind == 1U) {
                uniforms[0] *= static_cast<float>(currentOpacity);
            } else {
                uniforms[3] *= static_cast<float>(currentOpacity);
            }
            SetBatchRamp(
                cmd.paintKind == 1U ? Shader::Path_Linear : Shader::Path_Radial,
                ramp,
                uniforms);
        } else {
            EnsureBatchBlend(Shader::Path_Solid);
        }
        SetContentStencil();

        Color c = cmd.color;
        if (!gpuPaint) {
            c.alpha = static_cast<float>(c.alpha * currentOpacity);
        }
        const uint32_t color32 = ColorToRGBA32(c, 1.0);

        if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) break;

        // Slabs must fit the mapped dynamic buffers, not just the 16-bit
        // index range. Scoreboard's emblem Paths tessellate far past 512KB
        // (DYNAMIC_VB_SIZE / sizeof(Vertex2D) ≈ 21k verts); splitting only
        // at 65535 still memcpy'd past MapVertices and SIGSEGV'd.
        constexpr std::uint32_t kMaxBatchVertices =
            static_cast<std::uint32_t>(DYNAMIC_VB_SIZE / sizeof(Vertex2D));
        constexpr std::uint32_t kMaxBatchIndices =
            static_cast<std::uint32_t>(DYNAMIC_IB_SIZE / sizeof(uint16_t));
        constexpr std::uint32_t kMaxMeshVertices =
            kMaxBatchVertices < 65535U ? kMaxBatchVertices : 65535U;

        auto remapDynamicBuffers = [&]() noexcept -> bool {
            if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) {
                if (device_ == nullptr) return false;
                mappedVertices_ = static_cast<uint8_t*>(
                    device_->MapVertices(DYNAMIC_VB_SIZE));
                mappedIndices_ = static_cast<uint16_t*>(
                    device_->MapIndices(DYNAMIC_IB_SIZE));
            }
            currentVertexOffset_ = 0U;
            currentVertexCount_ = 0U;
            currentIndexCount_ = 0U;
            currentBatch_.startIndex = 0U;
            currentBatch_.vertexOffset = 0U;
            return mappedVertices_ != nullptr && mappedIndices_ != nullptr;
        };

        auto writeMeshSlab = [&](
            std::uint32_t start,
            std::uint32_t end) noexcept {
            auto* v = reinterpret_cast<Vertex2D*>(
                mappedVertices_ + currentVertexOffset_);
            const bool hasInverse = cmd.uvs[0].x > 0.5;
            Base::Transform2D inverse{};
            if (hasInverse) {
                static_cast<void>(
                    Base::TryToTransform2D(cmd.transform, inverse));
            }
            for (std::uint32_t index = start; index < end; ++index) {
                const Point local = mesh->vertices[index];
                const Point pos = TransformPoint(
                    currentTransform, local.x, local.y);
                Point sample = local;
                if (hasInverse && cmd.rect.width > 1.0e-12 &&
                    cmd.rect.height > 1.0e-12) {
                    const Point uv{
                        (local.x - cmd.rect.x) / cmd.rect.width,
                        (local.y - cmd.rect.y) / cmd.rect.height};
                    const Point mapped{
                        uv.x * inverse.m11 + uv.y * inverse.m21 + inverse.dx,
                        uv.x * inverse.m12 + uv.y * inverse.m22 + inverse.dy};
                    sample = {
                        cmd.rect.x + mapped.x * cmd.rect.width,
                        cmd.rect.y + mapped.y * cmd.rect.height};
                }
                float u = 0.0F;
                float vCoord = 0.0F;
                if (cmd.paintKind == 1U) {
                    const double len2 =
                        cmd.scalar > 1.0e-12 ? cmd.scalar : 1.0;
                    const double t =
                        ((sample.x - cmd.points[0].x) * cmd.points[1].x +
                         (sample.y - cmd.points[0].y) * cmd.points[1].y) /
                        len2;
                    u = static_cast<float>(t);
                    vCoord = 0.5F;
                } else if (cmd.paintKind == 2U) {
                    const double rx =
                        std::fabs(cmd.points[2].x) > 1.0e-6
                            ? cmd.points[2].x : 1.0;
                    const double ry =
                        std::fabs(cmd.points[2].y) > 1.0e-6
                            ? cmd.points[2].y : 1.0;
                    u = static_cast<float>(
                        (sample.x - cmd.points[0].x) / rx);
                    vCoord = static_cast<float>(
                        (sample.y - cmd.points[0].y) / ry);
                }
                const std::uint32_t localIndex = index - start;
                v[localIndex].x = static_cast<float>(pos.x);
                v[localIndex].y = static_cast<float>(pos.y);
                v[localIndex].color = color32;
                v[localIndex].u = u;
                v[localIndex].v = vCoord;
                v[localIndex].coverage = 1.0F;
            }
        };

        if (currentVertexCount_ + std::min(mesh->vertices.Size(), kMaxMeshVertices) >
                kMaxBatchVertices ||
            currentIndexCount_ + 3U > kMaxBatchIndices) {
            FlushBatch();
            if (!remapDynamicBuffers()) break;
        }

        const std::uint32_t vertexCount = mesh->vertices.Size();
        const std::uint32_t splitCount =
            (vertexCount + kMaxMeshVertices - 1U) / kMaxMeshVertices;

        for (std::uint32_t split = 0U; split < splitCount; ++split) {
            const std::uint32_t start = split * kMaxMeshVertices;
            const std::uint32_t end = std::min(
                start + kMaxMeshVertices, vertexCount);
            const std::uint32_t count = end - start;

            if (currentVertexCount_ + count > kMaxBatchVertices) {
                FlushBatch();
                if (!remapDynamicBuffers()) break;
            }
            if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) break;

            writeMeshSlab(start, end);
            uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_);
            std::uint32_t emittedIndices = 0U;
            bool slabFailed = false;
            for (std::uint32_t index = 0U;
                 index + 2U < mesh->indices.Size();
                 index += 3U) {
                const std::uint32_t i0 = mesh->indices[index + 0U];
                const std::uint32_t i1 = mesh->indices[index + 1U];
                const std::uint32_t i2 = mesh->indices[index + 2U];
                if (i0 < start || i0 >= end ||
                    i1 < start || i1 >= end ||
                    i2 < start || i2 >= end) {
                    continue;
                }
                if (currentIndexCount_ + emittedIndices + 3U > kMaxBatchIndices) {
                    currentVertexOffset_ +=
                        count * static_cast<std::uint32_t>(sizeof(Vertex2D));
                    currentVertexCount_ += count;
                    currentIndexCount_ += emittedIndices;
                    currentBatch_.numVertices += count;
                    currentBatch_.numIndices += emittedIndices;
                    stats_.vertexCount += count;
                    stats_.indexCount += emittedIndices;
                    FlushBatch();
                    if (!remapDynamicBuffers()) {
                        slabFailed = true;
                        break;
                    }
                    writeMeshSlab(start, end);
                    baseVertex = 0U;
                    emittedIndices = 0U;
                }
                mappedIndices_[currentIndexCount_ + emittedIndices + 0U] =
                    static_cast<uint16_t>(baseVertex + i0 - start);
                mappedIndices_[currentIndexCount_ + emittedIndices + 1U] =
                    static_cast<uint16_t>(baseVertex + i1 - start);
                mappedIndices_[currentIndexCount_ + emittedIndices + 2U] =
                    static_cast<uint16_t>(baseVertex + i2 - start);
                emittedIndices += 3U;
            }
            if (slabFailed) break;

            currentVertexOffset_ +=
                count * static_cast<std::uint32_t>(sizeof(Vertex2D));
            currentVertexCount_ += count;
            currentIndexCount_ += emittedIndices;
            currentBatch_.numVertices += count;
            currentBatch_.numIndices += emittedIndices;
            stats_.vertexCount += count;
            stats_.indexCount += emittedIndices;
        }
        break;
    }
    default:
        break;
    }
}

Base::Result<void> UiFrameEncoder::RecordOffscreen(
    const RenderFrame& frame) noexcept {
    if (!initialized_ || device_ == nullptr) return {};

    for (const auto& ramp : frame.GradientRamps()) {
        GetOrCreateGradientRamp(ramp);
    }

    return {};
}

Base::Result<void> UiFrameEncoder::RecordOnscreen(
    const RenderFrame& frame,
    RenderTarget& target) noexcept {
    if (!initialized_ || device_ == nullptr) return {};

    ResetFrame();
    currentFrame_ = &frame;
    stats_.sourceCommandCount = frame.Commands().Size();

    for (const auto& ramp : frame.GradientRamps()) {
        GetOrCreateGradientRamp(ramp);
    }

    device_->SetRenderTarget(&target);
    device_->BeginOnscreenRender();

    Tile tile;
    tile.x = 0;
    tile.y = 0;
    tile.width = frame.PixelWidth();
    tile.height = frame.PixelHeight();
    device_->BeginTile(&target, tile);

    mappedVertices_ = static_cast<uint8_t*>(device_->MapVertices(DYNAMIC_VB_SIZE));
    mappedIndices_ = static_cast<uint16_t*>(device_->MapIndices(DYNAMIC_IB_SIZE));

    ProjectiveTransform2D rootTransform;
    const double dpi = (frame.DpiScale() > 0.0) ? frame.DpiScale() : 1.0;
    rootTransform.m11 = dpi;
    rootTransform.m22 = dpi;

    struct NodeState {
        RenderNodeId nodeId = InvalidRenderNodeId;
        ProjectiveTransform2D transform;
        double opacity = 1.0;
        bool pushedClip = false;
    };
    struct PushState {
        ProjectiveTransform2D transform;
        double opacity = 1.0;
    };

    const auto commands = frame.Commands();
    const auto nodes = frame.Nodes();
    const auto clipVertices = frame.GeometryClipVertices();
    const auto clipIndices = frame.GeometryClipIndices();
    const std::uint32_t nodeCount = nodes.Size();

    Base::Vector<NodeState> nodeStack(allocator_);
    auto emitClipEntry = [&](const ClipEntry& entry, std::uint8_t mode, std::uint8_t ref) noexcept {
        if (entry.geometry) {
            EmitClipTriangles(
                clipVertices,
                clipIndices,
                entry.vertexOffset,
                entry.vertexCount,
                entry.indexOffset,
                entry.indexCount,
                entry.transform,
                mode,
                ref);
            return;
        }
        EmitClipQuad(entry.rect, entry.transform, mode, ref);
    };
    auto popNodeClip = [&](Base::Vector<NodeState>& stack) noexcept {
        if (!stack.Empty() && stack.Back().pushedClip) {
            if (clipDepth_ > 0U && !clipStack_.Empty()) {
                const ClipEntry entry = clipStack_.Back();
                static_cast<void>(clipStack_.PopBack());
                --clipDepth_;
                emitClipEntry(
                    entry,
                    StencilMode::Equal_Decr,
                    static_cast<std::uint8_t>(clipDepth_ + 1U));
            }
        }
    };

    auto emitNodeCommands = [&](const RenderNodeSnapshot& node,
                                const ProjectiveTransform2D& nodeTransform,
                                double nodeOpacity) noexcept {
        switch (node.blendMode) {
        case ::Aero::BlendMode::Multiply:
            currentBlendMode_ = BlendMode::SrcOver_Multiply;
            break;
        case ::Aero::BlendMode::Screen:
            currentBlendMode_ = BlendMode::SrcOver_Screen;
            break;
        case ::Aero::BlendMode::Additive:
            currentBlendMode_ = BlendMode::SrcOver_Additive;
            break;
        case ::Aero::BlendMode::Normal:
        default:
            currentBlendMode_ = BlendMode::SrcOver;
            break;
        }
        if (currentBatch_.numIndices > 0U &&
            currentBatch_.renderState.f.blendMode != static_cast<uint8_t>(currentBlendMode_)) {
            FlushBatch();
        }

        bool nodePushedClip = false;
        if (node.geometryClipIndexCount >= 3U &&
            node.geometryClipVertexCount >= 3U) {
            ClipEntry entry;
            entry.geometry = true;
            entry.transform = nodeTransform;
            entry.vertexOffset = node.geometryClipVertexOffset;
            entry.vertexCount = node.geometryClipVertexCount;
            entry.indexOffset = node.geometryClipIndexOffset;
            entry.indexCount = node.geometryClipIndexCount;
            emitClipEntry(entry, StencilMode::Equal_Incr, clipDepth_);
            static_cast<void>(clipStack_.PushBack(entry));
            ++clipDepth_;
            nodePushedClip = true;
        } else if (node.clipsToBounds &&
                   node.renderSize.width > 0.0 &&
                   node.renderSize.height > 0.0) {
            // ClipToBounds is (0,0,RenderSize) in local space. nodeTransform
            // already includes layoutSlot, so a parent-space layoutClip
            // would be applied twice and can cull the entire subtree.
            const Rect localClip{
                0.0,
                0.0,
                node.renderSize.width,
                node.renderSize.height};
            EmitClipQuad(
                localClip, nodeTransform, StencilMode::Equal_Incr, clipDepth_);
            static_cast<void>(
                clipStack_.PushBack(ClipEntry{localClip, nodeTransform}));
            ++clipDepth_;
            nodePushedClip = true;
        }

        ProjectiveTransform2D currentTransform = nodeTransform;
        double currentOpacity = nodeOpacity;
        Base::Vector<PushState> pushStack(allocator_);

        const std::uint32_t commandEnd =
            node.commandOffset + node.commandCount;
        for (std::uint32_t index = node.commandOffset;
             index < commandEnd && index < commands.Size();
             ++index) {
            const RenderCommand& cmd = commands[index];
            switch (cmd.kind) {
            case RenderCommandKind::PushTransform:
                static_cast<void>(pushStack.PushBack(
                    PushState{currentTransform, currentOpacity}));
                currentTransform = CombineTransform(cmd.transform, currentTransform);
                break;
            case RenderCommandKind::PopTransform:
                if (!pushStack.Empty()) {
                    currentTransform = pushStack.Back().transform;
                    currentOpacity = pushStack.Back().opacity;
                    static_cast<void>(pushStack.PopBack());
                }
                break;
            case RenderCommandKind::PushOpacity:
                static_cast<void>(pushStack.PushBack(
                    PushState{currentTransform, currentOpacity}));
                currentOpacity = currentOpacity * cmd.scalar;
                break;
            case RenderCommandKind::PopOpacity:
                if (!pushStack.Empty()) {
                    currentTransform = pushStack.Back().transform;
                    currentOpacity = pushStack.Back().opacity;
                    static_cast<void>(pushStack.PopBack());
                }
                break;
            default:
                ProcessCommand(cmd, currentTransform, currentOpacity);
                break;
            }
        }
        return nodePushedClip;
    };

    // Parent index lookup used to compute contiguous subtree ranges.
    static constexpr std::uint32_t kNotFound = 0xFFFFFFFFU;
    Base::Vector<std::uint32_t> parentIndexes(allocator_);
    for (std::uint32_t i = 0U; i < nodeCount; ++i) {
        std::uint32_t parentIndex = kNotFound;
        const RenderNodeId parentId = nodes[i].parentId;
        for (std::uint32_t j = i; j > 0U; --j) {
            if (nodes[j - 1U].id == parentId) {
                parentIndex = j - 1U;
                break;
            }
        }
        static_cast<void>(parentIndexes.PushBack(parentIndex));
    }

    auto isInSubtree = [&](std::uint32_t index,
                           std::uint32_t rootIndex) noexcept {
        std::uint32_t cursor = index;
        while (cursor != kNotFound) {
            if (cursor == rootIndex) return true;
            if (cursor < rootIndex) return false;
            cursor = parentIndexes[cursor];
        }
        return false;
    };

    auto subtreeEndOf = [&](std::uint32_t rootIndex) noexcept {
        std::uint32_t end = rootIndex + 1U;
        while (end < nodeCount && isInSubtree(end, rootIndex)) {
            ++end;
        }
        return end;
    };

    auto subtreeCommandCount = [&](std::uint32_t rootIndex) noexcept {
        std::uint32_t count = nodes[rootIndex].commandCount;
        const std::uint32_t end = subtreeEndOf(rootIndex);
        for (std::uint32_t i = rootIndex + 1U; i < end; ++i) {
            count += nodes[i].commandCount;
        }
        return count;
    };

    for (std::uint32_t nodeIndex = 0U; nodeIndex < nodeCount; ) {
        const RenderNodeSnapshot& node = nodes[nodeIndex];
        while (!nodeStack.Empty() && nodeStack.Back().nodeId != node.parentId) {
            popNodeClip(nodeStack);
            static_cast<void>(nodeStack.PopBack());
        }
        const ProjectiveTransform2D baseTransform =
            nodeStack.Empty() ? rootTransform : nodeStack.Back().transform;
        const double baseOpacity =
            nodeStack.Empty() ? 1.0 : nodeStack.Back().opacity;

        ProjectiveTransform2D local = CombineTransform(node.renderTransform, MakeTranslate(node.layoutSlot.x, node.layoutSlot.y));
        const ProjectiveTransform2D nodeTransform = CombineTransform(local, baseTransform);
        const double nodeOpacity = baseOpacity * node.opacity;

        const bool hasEffect = node.effect.kind != RenderEffectKind::None;
        const bool hasMask = node.mask.kind != RenderMaskKind::None;
        const std::uint32_t subtreeCommands = subtreeCommandCount(nodeIndex);
        // ClipToBounds under a parent scale (outer Viewbox, Intro
        // ScaleTransform) cannot rely on window stencil: during Opacity<1
        // the subtree is baked into a texture whose size is the clip, so
        // items stay visible, then Opacity==1 switches to inline Equal_Keep
        // and the list vanishes. Rasterizing the clipped subtree into a
        // local-space layer matches the fade path and is resolution-correct
        // for axis-aligned clips.
        const bool needsClipLayer =
            node.clipsToBounds && subtreeCommands > 0U;
        // CompositeTransform3D (RotationY card-flip) stores a non-affine
        // homography on this node. Descendants keep 2D local visuals and
        // inherit via nodeTransform; DropShadow children each bake their
        // own axis-aligned layer, so the board never turns as one card.
        // Rasterize the subtree in local pixels, then warp that one quad.
        const bool needs3DLayer =
            subtreeCommands > 0U &&
            !Base::IsAffine(node.renderTransform);
        const bool needsOffscreen =
            hasEffect || hasMask ||
            (node.opacity < 1.0 && subtreeCommands > 1U) ||
            needsClipLayer ||
            needs3DLayer;

        // Opacity 0 still participates in hit-testing, but must not take the
        // offscreen compositing path. A hidden ScrollBar (17px, opacity 0)
        // was reallocating the default FBO and, before the SetRenderTarget
        // clear was removed, wiping the window/sidebar/welcome already drawn.
        if (node.opacity <= 0.0) {
            nodeIndex = subtreeEndOf(nodeIndex);
            continue;
        }

        if (!needsOffscreen) {
            const bool pushedClip =
                emitNodeCommands(node, nodeTransform, nodeOpacity);
            static_cast<void>(nodeStack.PushBack(
                NodeState{node.id, nodeTransform, nodeOpacity, pushedClip}));
            ++nodeIndex;
            continue;
        }

        // Offscreen compositing: record the subtree into an offscreen target,
        // then composite it back with the node's effect, mask and opacity.
        const std::uint32_t subtreeEnd = subtreeEndOf(nodeIndex);
        const double effectPadLocal =
            (node.effect.kind == RenderEffectKind::DropShadow ||
             node.effect.kind == RenderEffectKind::Blur ||
             node.effect.kind == RenderEffectKind::DirectionalBlur)
            ? std::max(0.0, node.effect.radius) * 2.0
            : 0.0;
        const std::uint32_t padPixels = static_cast<std::uint32_t>(
            std::ceil(effectPadLocal * dpi));
        const std::uint32_t offWidth = std::max(
            1U, static_cast<std::uint32_t>(
                std::ceil(node.renderSize.width * dpi)) + padPixels * 2U);
        const std::uint32_t offHeight = std::max(
            1U, static_cast<std::uint32_t>(
                std::ceil(node.renderSize.height * dpi)) + padPixels * 2U);

        FlushBatch();
        RenderTarget* offscreen =
            GetOrCreateOffscreenTarget(node.id, offWidth, offHeight, false);
        if (offscreen == nullptr) {
            // No offscreen target available; fold opacity per command instead.
            const bool pushedClip =
                emitNodeCommands(node, nodeTransform, nodeOpacity);
            static_cast<void>(nodeStack.PushBack(
                NodeState{node.id, nodeTransform, nodeOpacity, pushedClip}));
            ++nodeIndex;
            continue;
        }

        const std::uint8_t savedClipDepth = clipDepth_;
        clipDepth_ = 0U;

        device_->SetRenderTarget(offscreen);
        device_->BeginOffscreenRender();
        Tile offTile{0U, 0U, offWidth, offHeight};
        device_->BeginTile(offscreen, offTile);

        // Record the subtree in node-local space scaled by dpi. The root
        // node's slot offset, render transform and opacity are applied at
        // composite time rather than baked into the offscreen content —
        // except Viewbox stretch, which must be baked so unscaled children
        // (CheckNought Paths) fit a RenderSize-sized DropShadow target.
        ProjectiveTransform2D dpiPad;
        dpiPad.m11 = dpi;
        dpiPad.m22 = dpi;
        dpiPad.m31 = static_cast<double>(padPixels);
        dpiPad.m32 = static_cast<double>(padPixels);
        ProjectiveTransform2D offRoot = dpiPad;
        ProjectiveTransform2D compositeTransform = nodeTransform;
        if (node.hasViewboxTransform) {
            const ProjectiveTransform2D viewbox =
                Base::ToProjective(node.viewboxTransform);
            offRoot = CombineTransform(viewbox, dpiPad);
            ProjectiveTransform2D inverseViewbox;
            if (Base::Invert(viewbox, inverseViewbox)) {
                const ProjectiveTransform2D visualWithout = CombineTransform(
                    inverseViewbox, node.renderTransform);
                const ProjectiveTransform2D localWithout = CombineTransform(
                    visualWithout,
                    MakeTranslate(node.layoutSlot.x, node.layoutSlot.y));
                compositeTransform =
                    CombineTransform(localWithout, baseTransform);
            }
        }
        Base::Vector<NodeState> offStack(allocator_);
        for (std::uint32_t i = nodeIndex; i < subtreeEnd; ++i) {
            const RenderNodeSnapshot& sub = nodes[i];
            while (!offStack.Empty() && offStack.Back().nodeId != sub.parentId) {
                popNodeClip(offStack);
                static_cast<void>(offStack.PopBack());
            }
            const ProjectiveTransform2D subBase =
                offStack.Empty() ? offRoot : offStack.Back().transform;
            const double subBaseOpacity =
                offStack.Empty() ? 1.0 : offStack.Back().opacity;
            ProjectiveTransform2D subLocal;
            if (i == nodeIndex) {
                subLocal = ProjectiveTransform2D{};
            } else {
                subLocal = CombineTransform(sub.renderTransform, MakeTranslate(sub.layoutSlot.x, sub.layoutSlot.y));
            }
            const ProjectiveTransform2D subTransform = CombineTransform(subLocal, subBase);
            const double subOpacity =
                (i == nodeIndex) ? 1.0 : subBaseOpacity * sub.opacity;
            const bool pushedClip =
                emitNodeCommands(sub, subTransform, subOpacity);
            static_cast<void>(offStack.PushBack(
                NodeState{sub.id, subTransform, subOpacity, pushedClip}));
        }
        while (!offStack.Empty()) {
            popNodeClip(offStack);
            static_cast<void>(offStack.PopBack());
        }

        FlushBatch();
        device_->EndTile(offscreen);
        device_->EndOffscreenRender();
        device_->SetRenderTarget(&target);
        clipDepth_ = savedClipDepth;

        // Opacity mask: render the mask brush into a second offscreen target.
        RenderTarget* maskTarget = nullptr;
        if (hasMask) {
            maskTarget = GetOrCreateOffscreenTarget(
                node.id, offWidth, offHeight, true);
            if (maskTarget != nullptr) {
                device_->SetRenderTarget(maskTarget);
                device_->BeginOffscreenRender();
                Tile maskTile{0U, 0U, offWidth, offHeight};
                device_->BeginTile(maskTarget, maskTile);
                EmitMaskBrush(
                    node.mask,
                    node.renderSize.width * dpi,
                    node.renderSize.height * dpi,
                    frame);
                FlushBatch();
                device_->EndTile(maskTarget);
                device_->EndOffscreenRender();
                device_->SetRenderTarget(&target);
            }
        }

        Tile fullTile;
        fullTile.x = 0;
        fullTile.y = 0;
        fullTile.width = frame.PixelWidth();
        fullTile.height = frame.PixelHeight();
        device_->BeginTile(&target, fullTile);

        switch (node.blendMode) {
        case ::Aero::BlendMode::Multiply:
            currentBlendMode_ = BlendMode::SrcOver_Multiply;
            break;
        case ::Aero::BlendMode::Screen:
            currentBlendMode_ = BlendMode::SrcOver_Screen;
            break;
        case ::Aero::BlendMode::Additive:
            currentBlendMode_ = BlendMode::SrcOver_Additive;
            break;
        case ::Aero::BlendMode::Normal:
        default:
            currentBlendMode_ = BlendMode::SrcOver;
            break;
        }

        CompositeOffscreen(
            node, offscreen, maskTarget, compositeTransform, nodeOpacity, dpi,
            frame);
        FlushBatch();

        nodeIndex = subtreeEnd;
    }

    while (!nodeStack.Empty()) {
        popNodeClip(nodeStack);
        static_cast<void>(nodeStack.PopBack());
    }

    FlushBatch();

    if (mappedIndices_ != nullptr) {
        device_->UnmapIndices();
        mappedIndices_ = nullptr;
    }
    if (mappedVertices_ != nullptr) {
        device_->UnmapVertices();
        mappedVertices_ = nullptr;
    }

    device_->EndTile(&target);
    device_->EndOnscreenRender();
    currentFrame_ = nullptr;

    return {};
}

} // namespace Aero::Render
