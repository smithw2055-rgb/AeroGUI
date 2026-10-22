#include "FrameEncoder.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Aero::Render {

namespace {



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

inline Shader::Enum ToPPAAShader(Shader::Enum shader, bool ppaa = true) noexcept {
    if (!ppaa) return shader;
    switch (shader) {
    case Shader::Path_Solid: return Shader::Path_AA_Solid;
    case Shader::Path_Linear: return Shader::Path_AA_Linear;
    case Shader::Path_Radial: return Shader::Path_AA_Radial;
    case Shader::Path_Pattern: return Shader::Path_AA_Pattern;
    case Shader::Path_Pattern_Clamp: return Shader::Path_AA_Pattern_Clamp;
    case Shader::Path_Pattern_Repeat: return Shader::Path_AA_Pattern_Repeat;
    case Shader::Path_Pattern_MirrorU: return Shader::Path_AA_Pattern_MirrorU;
    case Shader::Path_Pattern_MirrorV: return Shader::Path_AA_Pattern_MirrorV;
    case Shader::Path_Pattern_Mirror: return Shader::Path_AA_Pattern_Mirror;
    default: return shader;
    }
}


constexpr VertexOffsets ComputeVertexOffsets(uint8_t format) noexcept {
    VertexOffsets offsets{};
    if (format >= Shader::Vertex::Format::Count) {
        // Vertex2D compatibility fallback: Pos(8), Color(4), Tex0(8), Coverage(4)
        offsets.colorOffset = 8;
        offsets.tex0Offset = 12;
        offsets.coverageOffset = 20;
        offsets.stride = 24;
        return offsets;
    }
    const uint8_t attrs = AttributesForFormat[format];
    uint8_t currentOffset = 8; // Pos is always 8 bytes (0..7)
    for (uint32_t i = 1; i < Shader::Vertex::Format::Attr::Count; ++i) {
        if (attrs & (1U << i)) {
            if (i == Shader::Vertex::Format::Attr::Color) {
                offsets.colorOffset = static_cast<int8_t>(currentOffset);
            } else if (i == Shader::Vertex::Format::Attr::Tex0) {
                offsets.tex0Offset = static_cast<int8_t>(currentOffset);
            } else if (i == Shader::Vertex::Format::Attr::Tex1) {
                offsets.tex1Offset = static_cast<int8_t>(currentOffset);
            } else if (i == Shader::Vertex::Format::Attr::Coverage) {
                offsets.coverageOffset = static_cast<int8_t>(currentOffset);
            } else if (i == Shader::Vertex::Format::Attr::Rect) {
                offsets.rectOffset = static_cast<int8_t>(currentOffset);
            } else if (i == Shader::Vertex::Format::Attr::Tile) {
                offsets.tileOffset = static_cast<int8_t>(currentOffset);
            } else if (i == Shader::Vertex::Format::Attr::ImagePos) {
                offsets.imagePosOffset = static_cast<int8_t>(currentOffset);
            }
            currentOffset += SizeForType[TypeForAttr[i]];
        }
    }
    offsets.stride = currentOffset;
    return offsets;
}

struct FormatOffsetsTable {
    VertexOffsets data[Shader::Vertex::Format::Count + 1];
    constexpr FormatOffsetsTable() : data{} {
        for (uint8_t i = 0; i <= Shader::Vertex::Format::Count; ++i) {
            data[i] = ComputeVertexOffsets(i);
        }
    }
};

static constexpr FormatOffsetsTable kFormatOffsetsTable{};

inline const VertexOffsets& GetVertexOffsets(uint8_t format) noexcept {
    const uint8_t idx = (format < Shader::Vertex::Format::Count && SizeForFormat[format] != 0U)
        ? format
        : static_cast<uint8_t>(Shader::Vertex::Format::Count);
    return kFormatOffsetsTable.data[idx];
}

inline void WriteVertex(
    uint8_t* dst,
    const VertexOffsets& off,
    float x, float y,
    uint32_t color32 = 0xFFFFFFFF,
    float u = 0.0f, float v = 0.0f,
    float coverage = 1.0f) noexcept {
    auto* pos = reinterpret_cast<float*>(dst);
    pos[0] = x;
    pos[1] = y;

    if (off.colorOffset >= 0) {
        *reinterpret_cast<uint32_t*>(dst + off.colorOffset) = color32;
    }
    if (off.tex0Offset >= 0 && off.tex1Offset >= 0) {
        auto* uv0 = reinterpret_cast<float*>(dst + off.tex0Offset);
        uv0[0] = u;
        uv0[1] = v;
        auto* uv1 = reinterpret_cast<float*>(dst + off.tex1Offset);
        uv1[0] = 0.0f;
        uv1[1] = 0.0f;
    } else {
        if (off.tex0Offset >= 0) {
            auto* uv = reinterpret_cast<float*>(dst + off.tex0Offset);
            uv[0] = u;
            uv[1] = v;
        }
        if (off.tex1Offset >= 0) {
            auto* uv = reinterpret_cast<float*>(dst + off.tex1Offset);
            uv[0] = u;
            uv[1] = v;
        }
    }
    if (off.coverageOffset >= 0) {
        *reinterpret_cast<float*>(dst + off.coverageOffset) = coverage;
    }
    if (off.rectOffset >= 0) {
        std::memset(dst + off.rectOffset, 0, 8);
    }
    if (off.tileOffset >= 0) {
        std::memset(dst + off.tileOffset, 0, 16);
    }
    if (off.imagePosOffset >= 0) {
        std::memset(dst + off.imagePosOffset, 0, 16);
    }
}


uint32_t HashUniformDwords(const float* values, uint32_t numDwords) noexcept {
    const auto* bytes = reinterpret_cast<const uint8_t*>(values);
    const size_t len = static_cast<size_t>(numDwords) * sizeof(float);
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash != 0U ? hash : 1U;
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

constexpr std::uint32_t kMaxRoundedRectContourPoints = 256U;

std::uint32_t CalculateRoundedCornerSegments(
    const ProjectiveTransform2D& currentTransform,
    double x, double y, double radius) noexcept {
    if (radius <= 0.0) return 1U;
    const Point p0 = TransformPoint(currentTransform, x, y);
    const Point p1 = TransformPoint(currentTransform, x + radius, y);
    const Point p2 = TransformPoint(currentTransform, x, y + radius);
    const double screenRadius = std::max(
        std::hypot(p1.x - p0.x, p1.y - p0.y),
        std::hypot(p2.x - p0.x, p2.y - p0.y));
    const double arcLength = screenRadius * 3.14159265358979323846 * 0.5;
    return std::clamp(
        static_cast<std::uint32_t>(std::ceil(arcLength / 2.5)),
        3U, 32U);
}

// Builds a closed convex contour approximating a rounded rectangle. The
// corners are sampled as `cornerSegments` arcs each; a radius of zero
// yields four repeated corner points, so the same path also covers plain
// rectangles (degenerate fan/ring quads collapse harmlessly).
void BuildRoundedRectContour(
    double x, double y, double width, double height, double radius,
    std::uint32_t cornerSegments,
    Point* out) noexcept {
    const double centers[4][2] = {
        {x + radius, y + radius},
        {x + width - radius, y + radius},
        {x + width - radius, y + height - radius},
        {x + radius, y + height - radius}
    };
    const double startDir[4][2] = {
        {-1.0, 0.0},
        {0.0, -1.0},
        {1.0, 0.0},
        {0.0, 1.0}
    };

    const double delta = cornerSegments > 1U
        ? (3.14159265358979323846 * 0.5) / static_cast<double>(cornerSegments - 1U)
        : 0.0;
    const double cosStep = std::cos(delta);
    const double sinStep = std::sin(delta);

    std::uint32_t index = 0U;
    for (int corner = 0; corner < 4; ++corner) {
        double dirX = startDir[corner][0];
        double dirY = startDir[corner][1];
        for (std::uint32_t i = 0U; i < cornerSegments; ++i) {
            out[index].x = centers[corner][0] + radius * dirX;
            out[index].y = centers[corner][1] + radius * dirY;
            ++index;
            const double nextX = dirX * cosStep - dirY * sinStep;
            const double nextY = dirX * sinStep + dirY * cosStep;
            dirX = nextX;
            dirY = nextY;
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
    resources_.Clear();
    queuedBatches_.Clear();
    cpuVertices_.Clear();
    cpuIndices_.Clear();
    mappedVertices_ = nullptr;
    mappedIndices_ = nullptr;
    initialized_ = false;
}

Base::Result<void> UiFrameEncoder::RegisterImage(
    RenderImageId imageId,
    Ref<Texture> texture) noexcept {
    resources_.RegisterImage(imageId, std::move(texture));
    return {};
}

void UiFrameEncoder::UnregisterImage(RenderImageId imageId) noexcept {
    resources_.UnregisterImage(imageId);
}

Base::Result<void> UiFrameEncoder::RegisterGlyphAtlas(
    std::uint32_t page,
    Ref<Texture> texture) noexcept {
    resources_.RegisterAtlas(page, std::move(texture));
    return {};
}

Base::Result<void> UiFrameEncoder::RegisterGlyphRun(
    RenderGlyphRunId glyphRun,
    Base::Span<const RenderGlyphQuad> quads) noexcept {
    resources_.RegisterGlyphRun(glyphRun, quads);
    return {};
}

void UiFrameEncoder::UnregisterGlyphRun(RenderGlyphRunId glyphRun) noexcept {
    resources_.UnregisterGlyphRun(glyphRun);
}

Base::Result<void> UiFrameEncoder::RegisterMesh(
    RenderMeshId mesh,
    Base::Span<const Point> vertices,
    Base::Span<const std::uint32_t> indices,
    Base::Span<const float> coverages) noexcept {
    resources_.RegisterMesh(mesh, vertices, indices, coverages);
    return {};
}

void UiFrameEncoder::UnregisterMesh(RenderMeshId mesh) noexcept {
    resources_.UnregisterMesh(mesh);
}

Texture* UiFrameEncoder::FindImage(RenderImageId id) const noexcept {
    return resources_.FindImage(id);
}

Texture* UiFrameEncoder::FindAtlas(std::uint32_t page) const noexcept {
    return resources_.FindAtlas(page);
}

const Base::Vector<RenderGlyphQuad>* UiFrameEncoder::FindGlyphRun(
    RenderGlyphRunId glyphRun) const noexcept {
    return resources_.FindGlyphRun(glyphRun);
}

const UiFrameEncoder::MeshEntry* UiFrameEncoder::FindMesh(
    RenderMeshId mesh) const noexcept {
    return resources_.FindMesh(mesh);
}

Texture* UiFrameEncoder::GetOrCreateGradientRamp(
    const RenderGradientRampSnapshot& ramp) noexcept {
    auto& ramps = resources_.GradientRamps();
    for (auto& entry : ramps) {
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
    static_cast<void>(ramps.PushBack(
        GradientEntry{ramp.brushIdentity, ramp.revision, std::move(tex)}));
    return result;
}

void UiFrameEncoder::BeginOffscreenTargetFrame() noexcept {
    // Mark the pool free for this encode pass so same-sized ClipToBounds /
    // opacity layers can reuse FBOs across different nodeIds instead of
    // growing one target per node forever.
    for (auto& entry : resources_.OffscreenTargets()) {
        entry.inUse = false;
    }
}

RenderTarget* UiFrameEncoder::GetOrCreateOffscreenTarget(
    RenderNodeId nodeId, std::uint32_t width, std::uint32_t height,
    bool isMask) noexcept {
    const bool needsStencil = !isMask;
    auto& targets = resources_.OffscreenTargets();

    auto adopt = [&](OffscreenTargetEntry& entry) noexcept -> RenderTarget* {
        entry.nodeId = nodeId;
        entry.isMask = isMask;
        entry.inUse = true;
        if (!entry.target || entry.width != width || entry.height != height) {
            entry.target = device_->CreateRenderTarget(
                "Offscreen", width, height, 1, needsStencil);
            entry.width = width;
            entry.height = height;
        }
        return entry.target.Get();
    };

    // 1) Sticky match: same node + mask role (keeps resize churn local).
    for (auto& entry : targets) {
        if (!entry.inUse && entry.nodeId == nodeId && entry.isMask == isMask) {
            return adopt(entry);
        }
    }

    // 2) Reuse any free pool entry with matching size + mask/stencil format.
    for (auto& entry : targets) {
        if (!entry.inUse && entry.isMask == isMask &&
            entry.width == width && entry.height == height && entry.target) {
            entry.nodeId = nodeId;
            entry.inUse = true;
            return entry.target.Get();
        }
    }

    // 3) Reuse any free entry (resize) before allocating a new GPU target.
    for (auto& entry : targets) {
        if (!entry.inUse && entry.isMask == isMask) {
            return adopt(entry);
        }
    }

    Ref<RenderTarget> target = device_->CreateRenderTarget(
        "Offscreen", width, height, 1, needsStencil);
    if (!target) return nullptr;
    RenderTarget* result = target.Get();
    static_cast<void>(targets.PushBack(
        OffscreenTargetEntry{
            nodeId, isMask, true, std::move(target), width, height}));
    return result;
}

void UiFrameEncoder::ResetFrame() noexcept {
    queuedBatches_.Clear();
    currentBatchVertexStart_ = 0U;
    currentVertexOffset_ = 0U;
    currentVertexCount_ = 0U;
    currentIndexCount_ = 0U;
    currentBatch_ = Batch{};
    clipDepth_ = 0U;
    clipStack_.Clear();
    stats_ = FrameStatistics{};
    EnsureCapacity(0U, 0U);
}

bool UiFrameEncoder::EnsureCapacity(std::uint32_t additionalBytes, std::uint32_t additionalIndices) noexcept {
    constexpr std::uint32_t kInitialVbCapacity = DYNAMIC_VB_SIZE;
    constexpr std::uint32_t kInitialIbCapacity = static_cast<std::uint32_t>(DYNAMIC_IB_SIZE / sizeof(uint16_t));

    const std::uint32_t neededBytes = currentVertexOffset_ + additionalBytes;
    if (cpuVertices_.Size() < neededBytes || cpuVertices_.Size() == 0U) {
        std::uint32_t newCap = std::max(neededBytes, kInitialVbCapacity);
        if (newCap < cpuVertices_.Size() * 2U) newCap = cpuVertices_.Size() * 2U;
        static_cast<void>(cpuVertices_.Resize(newCap));
    }
    mappedVertices_ = cpuVertices_.Data();

    const std::uint32_t neededIndices = currentIndexCount_ + additionalIndices;
    if (cpuIndices_.Size() < neededIndices || cpuIndices_.Size() == 0U) {
        std::uint32_t newCap = std::max(neededIndices, kInitialIbCapacity);
        if (newCap < cpuIndices_.Size() * 2U) newCap = cpuIndices_.Size() * 2U;
        static_cast<void>(cpuIndices_.Resize(newCap));
    }
    mappedIndices_ = cpuIndices_.Data();

    return mappedVertices_ != nullptr && mappedIndices_ != nullptr;
}

bool UiFrameEncoder::EnsureMappedBuffers() noexcept {
    return EnsureCapacity(0U, 0U);
}

bool UiFrameEncoder::PrepareGeometrySpace(
    std::uint32_t vertexCount,
    std::uint32_t vertexStride,
    std::uint32_t indexCount) noexcept {
    const std::uint32_t vertexBytes = vertexCount * vertexStride;
    if (currentVertexOffset_ + vertexBytes > DYNAMIC_VB_SIZE ||
        (currentIndexCount_ + indexCount) * sizeof(uint16_t) > DYNAMIC_IB_SIZE ||
        (currentVertexCount_ - currentBatchVertexStart_) + vertexCount > 65535U) {
        CommitBatches();
    }
    return EnsureCapacity(vertexBytes, indexCount);
}

void UiFrameEncoder::FlushBatch() noexcept {
    if (currentBatch_.numIndices > 0U) {
        QueuedBatch qb{};
        qb.batch = currentBatch_;
        if (currentBatch_.pixelUniforms[0].values != nullptr && currentBatch_.pixelUniforms[0].numDwords > 0U) {
            const uint32_t count = std::min(currentBatch_.pixelUniforms[0].numDwords, 8U);
            std::memcpy(qb.pixelUniforms0, currentBatch_.pixelUniforms[0].values, count * sizeof(float));
            qb.batch.pixelUniforms[0].hash = HashUniformDwords(qb.pixelUniforms0, count);
        }
        if (currentBatch_.pixelUniforms[1].values != nullptr && currentBatch_.pixelUniforms[1].numDwords > 0U) {
            const uint32_t count = std::min(currentBatch_.pixelUniforms[1].numDwords, 4U);
            std::memcpy(qb.pixelUniforms1, currentBatch_.pixelUniforms[1].values, count * sizeof(float));
            qb.batch.pixelUniforms[1].hash = HashUniformDwords(qb.pixelUniforms1, count);
        }
        if (currentBatch_.vertexUniforms[0].values != nullptr && currentBatch_.vertexUniforms[0].numDwords > 0U) {
            const uint32_t count = std::min(currentBatch_.vertexUniforms[0].numDwords, 4U);
            std::memcpy(qb.vertexUniforms0, currentBatch_.vertexUniforms[0].values, count * sizeof(float));
            qb.batch.vertexUniforms[0].hash = HashUniformDwords(qb.vertexUniforms0, count);
        }
        if (currentBatch_.vertexUniforms[1].values != nullptr && currentBatch_.vertexUniforms[1].numDwords > 0U) {
            const uint32_t count = std::min(currentBatch_.vertexUniforms[1].numDwords, 4U);
            std::memcpy(qb.vertexUniforms1, currentBatch_.vertexUniforms[1].values, count * sizeof(float));
            qb.batch.vertexUniforms[1].hash = HashUniformDwords(qb.vertexUniforms1, count);
        }
        static_cast<void>(queuedBatches_.PushBack(std::move(qb)));

        currentBatch_ = Batch{};
        currentBatch_.numIndices = 0U;
        currentBatch_.numVertices = 0U;
        currentBatch_.startIndex = currentIndexCount_;
        currentBatch_.vertexOffset = currentVertexOffset_;
        currentBatchVertexStart_ = currentVertexCount_;
    }
}

void UiFrameEncoder::CommitBatches() noexcept {
    if (currentBatch_.numIndices > 0U) {
        FlushBatch();
    }
    if (queuedBatches_.Empty() || device_ == nullptr) {
        return;
    }

    const uint32_t vbBytes = currentVertexOffset_;
    const uint32_t ibBytes = currentIndexCount_ * sizeof(uint16_t);
    if (vbBytes > 0U && ibBytes > 0U) {
        void* gpuVB = device_->MapVertices(vbBytes);
        void* gpuIB = device_->MapIndices(ibBytes);
        if (gpuVB != nullptr && gpuIB != nullptr) {
            std::memcpy(gpuVB, cpuVertices_.Data(), vbBytes);
            std::memcpy(gpuIB, cpuIndices_.Data(), ibBytes);
        }
        device_->UnmapVertices();
        device_->UnmapIndices();
    }

    for (auto& qb : queuedBatches_) {
        if (qb.batch.pixelUniforms[0].numDwords > 0U) {
            qb.batch.pixelUniforms[0].values = qb.pixelUniforms0;
        } else {
            qb.batch.pixelUniforms[0].values = nullptr;
        }
        if (qb.batch.pixelUniforms[1].numDwords > 0U) {
            qb.batch.pixelUniforms[1].values = qb.pixelUniforms1;
        } else {
            qb.batch.pixelUniforms[1].values = nullptr;
        }
        if (qb.batch.vertexUniforms[0].numDwords > 0U) {
            qb.batch.vertexUniforms[0].values = qb.vertexUniforms0;
        } else {
            qb.batch.vertexUniforms[0].values = nullptr;
        }
        if (qb.batch.vertexUniforms[1].numDwords > 0U) {
            qb.batch.vertexUniforms[1].values = qb.vertexUniforms1;
        } else {
            qb.batch.vertexUniforms[1].values = nullptr;
        }
        device_->DrawBatch(qb.batch);
        ++stats_.drawCallCount;
        ++stats_.batchCount;
    }
    queuedBatches_.Clear();

    currentBatch_ = Batch{};
    currentVertexOffset_ = 0U;
    currentVertexCount_ = 0U;
    currentIndexCount_ = 0U;
    currentBatchVertexStart_ = 0U;
    currentBatch_.startIndex = 0U;
    currentBatch_.vertexOffset = 0U;
    currentBatch_.numIndices = 0U;
    currentBatch_.numVertices = 0U;
    mappedVertices_ = cpuVertices_.Data();
    mappedIndices_ = cpuIndices_.Data();
}

const VertexOffsets& UiFrameEncoder::CurrentVertexFormat(uint32_t& outStride) const noexcept {
    const uint8_t vertexEnum = (currentBatch_.shader.v < Shader::Count)
        ? VertexForShader[currentBatch_.shader.v] : 0U;
    const uint8_t format = (vertexEnum < Shader::Vertex::Count)
        ? FormatForVertex[vertexEnum] : 0U;
    outStride = SizeForFormat[format] != 0U
        ? static_cast<uint32_t>(SizeForFormat[format])
        : static_cast<uint32_t>(sizeof(Vertex2D));
    return GetVertexOffsets(format);
}

void UiFrameEncoder::EmitQuad(
    const Point points[4],
    const Point uvs[4],
    Color color) noexcept {
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) {
            return;
        }
    }

    const Point pts[4] = {points[0], points[1], points[2], points[3]};
    const float coverage = 1.0f;

    uint32_t stride = 0U;
    const auto& offsets = CurrentVertexFormat(stride);
    if (!PrepareGeometrySpace(4U, stride, 6U)) return;

    const uint32_t color32 = ColorToRGBA32(color, 1.0);
    uint8_t* dst = mappedVertices_ + currentVertexOffset_;
    for (int i = 0; i < 4; ++i) {
        WriteVertex(
            dst + i * stride, offsets,
            static_cast<float>(pts[i].x),
            static_cast<float>(pts[i].y),
            color32,
            static_cast<float>(uvs[i].x),
            static_cast<float>(uvs[i].y),
            coverage);
    }
    currentVertexOffset_ += 4U * stride;

    const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_ - currentBatchVertexStart_);
    mappedIndices_[currentIndexCount_ + 0] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 1] = baseVertex + 1;
    mappedIndices_[currentIndexCount_ + 2] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 3] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 4] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 5] = baseVertex + 3;
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
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) {
            return;
        }
    }

    const Point pts[4] = {points[0], points[1], points[2], points[3]};
    const float coverage = 1.0f;

    uint32_t stride = 0U;
    const auto& offsets = CurrentVertexFormat(stride);
    if (!PrepareGeometrySpace(4U, stride, 6U)) return;

    uint8_t* dst = mappedVertices_ + currentVertexOffset_;
    for (int i = 0; i < 4; ++i) {
        const uint32_t color32 = ColorToRGBA32(colors[i], opacity);
        WriteVertex(
            dst + i * stride, offsets,
            static_cast<float>(pts[i].x),
            static_cast<float>(pts[i].y),
            color32,
            static_cast<float>(uvs[i].x),
            static_cast<float>(uvs[i].y),
            coverage);
    }
    currentVertexOffset_ += 4U * stride;

    const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_ - currentBatchVertexStart_);
    mappedIndices_[currentIndexCount_ + 0] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 1] = baseVertex + 1;
    mappedIndices_[currentIndexCount_ + 2] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 3] = baseVertex + 0;
    mappedIndices_[currentIndexCount_ + 4] = baseVertex + 2;
    mappedIndices_[currentIndexCount_ + 5] = baseVertex + 3;

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
    if (perimeterCount < 3U) return;

    const std::uint32_t vertexCount = perimeterCount + 1U;
    const std::uint32_t indexCount = perimeterCount * 3U;

    uint32_t stride = 0U;
    const auto& offsets = CurrentVertexFormat(stride);
    if (!PrepareGeometrySpace(vertexCount, stride, indexCount)) return;

    const uint32_t color32 = ColorToRGBA32(color, 1.0);
    uint8_t* dst = mappedVertices_ + currentVertexOffset_;

    WriteVertex(
        dst, offsets,
        static_cast<float>(centroid.x),
        static_cast<float>(centroid.y),
        color32,
        0.0f,
        0.0f,
        1.0f);

    for (std::uint32_t i = 0; i < perimeterCount; ++i) {
        WriteVertex(
            dst + (i + 1U) * stride, offsets,
            static_cast<float>(perimeter[i].x),
            static_cast<float>(perimeter[i].y),
            color32,
            0.0f,
            0.0f,
            1.0f);
    }
    currentVertexOffset_ += vertexCount * stride;

    const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_ - currentBatchVertexStart_);
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
    const float uniforms[8],
    std::uint8_t wrapMode) noexcept {
    const bool uniformsDiffer =
        std::memcmp(paintUniforms_, uniforms, sizeof(paintUniforms_)) != 0;
    const bool batchActive = currentBatch_.numIndices > 0U;
    if (batchActive &&
        (currentBatch_.shader != shader ||
         currentBatch_.ramps != ramp ||
         currentBatch_.rampsSampler.f.wrapMode != wrapMode ||
         uniformsDiffer ||
         currentBatch_.renderState.f.colorEnable != 1U ||
         currentBatch_.renderState.f.blendMode !=
             static_cast<uint8_t>(currentBlendMode_))) {
        FlushBatch();
    }
    std::memcpy(paintUniforms_, uniforms, sizeof(paintUniforms_));
    currentBatch_.shader = shader;
    currentBatch_.ramps = ramp;
    currentBatch_.rampsSampler.f.wrapMode = wrapMode;
    currentBatch_.rampsSampler.f.minmagFilter = MinMagFilter::Linear;
    currentBatch_.pixelUniforms[0].values = paintUniforms_;
    currentBatch_.pixelUniforms[0].numDwords = 8U;
    AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
    AssignColorEnable(currentBatch_.renderState, 1U);
}

void UiFrameEncoder::EmitMaskBrush(
    const RenderMaskSnapshot& mask,
    double padX,
    double padY,
    double width,
    double height,
    const RenderFrame& frame) noexcept {
    static_cast<void>(frame);
    switch (mask.kind) {
    case RenderMaskKind::Solid: {
        EnsureBatchBlend(Shader::Path_Solid);
        SetContentStencil();
        const Point corners[4] = {
            Point{padX, padY},
            Point{padX + width, padY},
            Point{padX + width, padY + height},
            Point{padX, padY + height}
        };
        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
        EmitQuad(corners, uvs, mask.color);
        break;
    }
    case RenderMaskKind::Image:
        if (mask.image != InvalidRenderImageId) {
            Texture* tex = FindImage(mask.image);
            if (tex != nullptr) {
                SetBatchImage(Shader::Path_Pattern, tex, nullptr);
                SetContentStencil();
                const Point corners[4] = {
                    Point{padX, padY},
                    Point{padX + width, padY},
                    Point{padX + width, padY + height},
                    Point{padX, padY + height}
                };
                const Point uvs[4] = {
                    Point{mask.sourceUv.x, mask.sourceUv.y},
                    Point{mask.sourceUv.x + mask.sourceUv.width, mask.sourceUv.y},
                    Point{mask.sourceUv.x + mask.sourceUv.width, mask.sourceUv.y + mask.sourceUv.height},
                    Point{mask.sourceUv.x, mask.sourceUv.y + mask.sourceUv.height}
                };
                EmitQuad(corners, uvs, Color{1.0F, 1.0F, 1.0F, 1.0F});
            }
        }
        break;
    case RenderMaskKind::LinearGradient: {
        const Base::Span<const RenderGradientRampSnapshot> ramps =
            frame.GradientRamps();
        if (mask.gradientRamp >= ramps.Size()) break;
        Texture* ramp = GetOrCreateGradientRamp(ramps[mask.gradientRamp]);
        if (ramp == nullptr) break;

        Point start = mask.startPoint;
        Point end = mask.endPoint;
        if (mask.mappingMode == static_cast<std::uint8_t>(::Aero::Media::BrushMappingMode::Absolute)) {
            start.x /= std::max(1.0, width);
            start.y /= std::max(1.0, height);
            end.x /= std::max(1.0, width);
            end.y /= std::max(1.0, height);
        }
        const double dx = end.x - start.x;
        const double dy = end.y - start.y;
        const double lengthSq = dx * dx + dy * dy;

        const Point corners[4] = {
            Point{padX, padY},
            Point{padX + width, padY},
            Point{padX + width, padY + height},
            Point{padX, padY + height}};

        Point uvs[4]{};
        for (int i = 0; i < 4; ++i) {
            double t = 0.0;
            if (lengthSq > 0.0) {
                const double px = (corners[i].x - padX) / std::max(1.0, width);
                const double py = (corners[i].y - padY) / std::max(1.0, height);
                t = ((px - start.x) * dx + (py - start.y) * dy) / lengthSq;
            }
            uvs[i] = Point{t, 0.5};
        }

        float uniforms[8]{};
        uniforms[0] = 1.0F;

        SetBatchRamp(Shader::Path_Linear, ramp, uniforms, mask.tileMode);
        SetContentStencil();
        EmitQuad(corners, uvs, Color{1.0F, 1.0F, 1.0F, 1.0F});
        break;
    }
    case RenderMaskKind::RadialGradient: {
        const Base::Span<const RenderGradientRampSnapshot> ramps =
            frame.GradientRamps();
        if (mask.gradientRamp >= ramps.Size()) break;
        Texture* ramp = GetOrCreateGradientRamp(ramps[mask.gradientRamp]);
        if (ramp == nullptr) break;

        Point center = mask.center;
        Point origin = mask.gradientOrigin;
        double radiusX = std::max(std::fabs(mask.radiusX), 1.0e-6);
        double radiusY = std::max(std::fabs(mask.radiusY), 1.0e-6);
        if (mask.mappingMode == static_cast<std::uint8_t>(::Aero::Media::BrushMappingMode::Absolute)) {
            center.x /= std::max(1.0, width);
            center.y /= std::max(1.0, height);
            origin.x /= std::max(1.0, width);
            origin.y /= std::max(1.0, height);
            radiusX /= std::max(1.0, width);
            radiusY /= std::max(1.0, height);
        }

        const Point c{
            (center.x - origin.x) / radiusX,
            (center.y - origin.y) / radiusY};
        const double d = 1.0 - c.x * c.x - c.y * c.y;
        if (d <= 1.0e-5 || !std::isfinite(d)) break;

        float uniforms[8]{};
        uniforms[0] = static_cast<float>(c.x / d);
        uniforms[1] = static_cast<float>(c.y / d);
        uniforms[2] = static_cast<float>(1.0 / d);
        uniforms[3] = 1.0F;
        uniforms[4] = static_cast<float>(c.y);
        uniforms[5] = static_cast<float>(c.x);
        uniforms[6] = 0.5F;
        uniforms[7] = 0.0F;

        const Point corners[4] = {
            Point{padX, padY},
            Point{padX + width, padY},
            Point{padX + width, padY + height},
            Point{padX, padY + height}};

        Point uvs[4]{};
        for (int i = 0; i < 4; ++i) {
            const double px = (corners[i].x - padX) / std::max(1.0, width);
            const double py = (corners[i].y - padY) / std::max(1.0, height);
            uvs[i] = Point{
                (px - origin.x) / radiusX,
                (py - origin.y) / radiusY};
        }

        SetBatchRamp(Shader::Path_Radial, ramp, uniforms, mask.tileMode);
        SetContentStencil();
        EmitQuad(corners, uvs, Color{1.0F, 1.0F, 1.0F, 1.0F});
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
    if (texture == nullptr) {
        return;
    }

    const double w = node.renderSize.width;
    const double h = node.renderSize.height;
    if (w <= 0.0 || h <= 0.0) {
        return;
    }

    const Point uvs[4] = {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

    const RenderMaskSnapshot* maskSnapshot = frame.MaskAt(node.maskIndex);
    const RenderEffectSnapshot* effectSnapshot = frame.EffectAt(node.effectIndex);
    static const RenderMaskSnapshot kDefaultMask{};
    static const RenderEffectSnapshot kDefaultEffect{};
    const RenderMaskSnapshot& mask = maskSnapshot ? *maskSnapshot : kDefaultMask;
    const RenderEffectSnapshot& effect = effectSnapshot ? *effectSnapshot : kDefaultEffect;

    const bool hasMask =
        mask.kind != RenderMaskKind::None &&
        maskTarget != nullptr &&
        maskTarget->GetTexture() != nullptr;

    const double blurScale = std::max(1.0, effect.radius * dpi);
    offscreenSizeUniform_[0] =
        static_cast<float>(static_cast<double>(texture->GetWidth()) / blurScale);
    offscreenSizeUniform_[1] =
        static_cast<float>(static_cast<double>(texture->GetHeight()) / blurScale);

    // Offscreen targets pad by twice the blur radius so the 3x3 kernel, which
    // samples at ±radius, does not clamp against the texture edge. Composite
    // quads are expanded by the same local padding so the halo is visible.
    double padX = 0.0;
    double padY = 0.0;
    if (effect.kind == RenderEffectKind::DropShadow) {
        const double radians =
            effect.direction * 0.017453292519943295;
        const double dx = std::cos(radians) * effect.depth;
        const double dy = -std::sin(radians) * effect.depth;
        const double blur = std::ceil(std::max(0.0, effect.radius) * 2.0);
        padX = blur + std::ceil(std::abs(dx));
        padY = blur + std::ceil(std::abs(dy));
    } else if (effect.kind == RenderEffectKind::Blur ||
               effect.kind == RenderEffectKind::DirectionalBlur) {
        const double blur = std::ceil(std::max(0.0, effect.radius) * 2.0);
        padX = blur;
        padY = blur;
    }
    const double quadW = w + padX * 2.0;
    const double quadH = h + padY * 2.0;

    // Drop shadow pass: blurred, tinted copy of the offscreen alpha, offset
    // along the effect direction, composited behind the source.
    // WPF Direction is degrees from +X, counter-clockwise in a Y-up space.
    // Default 315° therefore casts down-right in screen (Y-down) coordinates:
    //   offsetX =  cos(θ) * depth
    //   offsetY = -sin(θ) * depth
    if (effect.kind == RenderEffectKind::DropShadow) {
        const double radians =
            effect.direction * 0.017453292519943295;
        const double offsetX =
            std::cos(radians) * effect.depth;
        const double offsetY =
            -std::sin(radians) * effect.depth;
        const Point shadowPoints[4] = {
            TransformPoint(nodeTransform, -padX + offsetX, -padY + offsetY),
            TransformPoint(nodeTransform, -padX + quadW + offsetX, -padY + offsetY),
            TransformPoint(nodeTransform, -padX + quadW + offsetX, -padY + quadH + offsetY),
            TransformPoint(nodeTransform, -padX + offsetX, -padY + quadH + offsetY)};

        Color shadowColor = effect.color;
        shadowColor.alpha *= static_cast<float>(
            std::clamp(nodeOpacity * effect.opacity, 0.0, 1.0));

        SetBatchImage(Shader::Shadow, nullptr, texture);
        currentBatch_.pixelUniforms[0] =
            {offscreenSizeUniform_, 2U, 0U};
        SetContentStencil();
        EmitQuad(shadowPoints, uvs, shadowColor);
    } else if (effect.kind == RenderEffectKind::DirectionalBlur &&
               effect.radius > 0.0) {
        const double radians =
            effect.direction * 0.017453292519943295;
        const double offsetX = std::cos(radians) * effect.radius;
        const double offsetY = std::sin(radians) * effect.radius;
        const Point blurPoints[4] = {
            TransformPoint(nodeTransform, -padX - offsetX, -padY - offsetY),
            TransformPoint(nodeTransform, -padX + quadW - offsetX, -padY - offsetY),
            TransformPoint(nodeTransform, -padX + quadW - offsetX, -padY + quadH - offsetY),
            TransformPoint(nodeTransform, -padX - offsetX, -padY + quadH - offsetY)};
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
    Shader::Enum shader = Shader::Opacity_Solid;
    if (hasMask) {
        shader = Shader::Mask;
    } else if ((effect.kind == RenderEffectKind::Blur ||
                effect.kind == RenderEffectKind::DirectionalBlur) &&
               effect.radius > 0.0) {
        shader = Shader::Blur;
    } else if (effect.kind == RenderEffectKind::Custom) {
        shader = Shader::Custom_Effect;
    }
    SetBatchImage(
        shader, texture,
        hasMask ? maskTarget->GetTexture() : nullptr);
    if (shader == Shader::Blur || shader == Shader::Custom_Effect) {
        currentBatch_.pixelUniforms[0] =
            {offscreenSizeUniform_, 2U, 0U};
        if (shader == Shader::Custom_Effect && effect.uniformCount > 0U) {
            customEffectUniforms_[0] = effect.uniforms[0];
            customEffectUniforms_[1] = effect.uniforms[1];
            customEffectUniforms_[2] = effect.uniforms[2];
            customEffectUniforms_[3] = effect.uniforms[3];
            currentBatch_.pixelUniforms[1] =
                {customEffectUniforms_, std::min(effect.uniformCount, 4U), 0U};
        }
        if (!effect.bytecode.Empty()) {
            currentBatch_.pixelShader = const_cast<void*>(
                static_cast<const void*>(effect.bytecode.Data()));
            currentBatch_.pixelShaderSize =
                static_cast<uint32_t>(effect.bytecode.Size());
        }
    }

    const Point points[4] = {
        TransformPoint(nodeTransform, -padX, -padY),
        TransformPoint(nodeTransform, -padX + quadW, -padY),
        TransformPoint(nodeTransform, -padX + quadW, -padY + quadH),
        TransformPoint(nodeTransform, -padX, -padY + quadH)};

    Color tint = Color{1.0F, 1.0F, 1.0F, 1.0F};
    tint.alpha = static_cast<float>(
        std::clamp(nodeOpacity, 0.0, 1.0));
    if (effect.kind == RenderEffectKind::Tint) {
        tint.red *= effect.color.red;
        tint.green *= effect.color.green;
        tint.blue *= effect.color.blue;
        tint.alpha *= effect.color.alpha;
    }
    if (mask.kind == RenderMaskKind::Solid && !hasMask) {
        tint.alpha *= mask.color.alpha;
    }

    SetContentStencil();
    if (effect.kind == RenderEffectKind::Pixelate &&
        effect.size > 1.0) {
        const double cell = std::max(1.0, effect.size);
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
    } else if (!Base::IsAffine(nodeTransform)) {
        constexpr std::uint32_t kGridCols = 16U;
        constexpr std::uint32_t kGridRows = 16U;
        for (std::uint32_t row = 0U; row < kGridRows; ++row) {
            for (std::uint32_t col = 0U; col < kGridCols; ++col) {
                const double lx0 = -padX + quadW * static_cast<double>(col) / kGridCols;
                const double ly0 = -padY + quadH * static_cast<double>(row) / kGridRows;
                const double lx1 = -padX + quadW * static_cast<double>(col + 1U) / kGridCols;
                const double ly1 = -padY + quadH * static_cast<double>(row + 1U) / kGridRows;
                const double u0 = static_cast<double>(col) / kGridCols;
                const double v0 = static_cast<double>(row) / kGridRows;
                const double u1 = static_cast<double>(col + 1U) / kGridCols;
                const double v1 = static_cast<double>(row + 1U) / kGridRows;
                const Point cellPoints[4] = {
                    TransformPoint(nodeTransform, lx0, ly0),
                    TransformPoint(nodeTransform, lx1, ly0),
                    TransformPoint(nodeTransform, lx1, ly1),
                    TransformPoint(nodeTransform, lx0, ly1)};
                const Point cellUvs[4] = {
                    {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
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

    uint32_t stride = 0U;
    const auto& offsets = CurrentVertexFormat(stride);

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
        if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
            !std::isfinite(b.x) || !std::isfinite(b.y) ||
            !std::isfinite(c.x) || !std::isfinite(c.y)) {
            continue;
        }

        if (!PrepareGeometrySpace(3U, stride, 3U)) return;

        uint8_t* dst = mappedVertices_ + currentVertexOffset_;
        WriteVertex(dst + 0U * stride, offsets, static_cast<float>(a.x), static_cast<float>(a.y), 0U, 0.0f, 0.0f, 1.0f);
        WriteVertex(dst + 1U * stride, offsets, static_cast<float>(b.x), static_cast<float>(b.y), 0U, 0.0f, 0.0f, 1.0f);
        WriteVertex(dst + 2U * stride, offsets, static_cast<float>(c.x), static_cast<float>(c.y), 0U, 0.0f, 0.0f, 1.0f);
        currentVertexOffset_ += 3U * stride;

        const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_ - currentBatchVertexStart_);
        mappedIndices_[currentIndexCount_ + 0] = baseVertex + 0;
        mappedIndices_[currentIndexCount_ + 1] = baseVertex + 1;
        mappedIndices_[currentIndexCount_ + 2] = baseVertex + 2;
        currentVertexCount_ += 3U;
        currentIndexCount_ += 3U;
        currentBatch_.numVertices += 3U;
        currentBatch_.numIndices += 3U;
        stats_.vertexCount += 3U;
        stats_.indexCount += 3U;
    }
}

void UiFrameEncoder::EmitTriangles(
    const Point* vertices,
    std::uint32_t vertexCount,
    const std::uint32_t* indices,
    std::uint32_t indexCount,
    const ProjectiveTransform2D& transform,
    Color color,
    double opacity) noexcept {
    if (vertexCount == 0U || indexCount < 3U || vertices == nullptr || indices == nullptr) {
        return;
    }
    EnsureBatchBlend(ToPPAAShader(Shader::Path_Solid, enablePPAA_));
    SetContentStencil();
    const uint32_t color32 = ColorToRGBA32(color, opacity);

    uint32_t stride = 0U;
    const auto& offsets = CurrentVertexFormat(stride);

    for (std::uint32_t t = 0U; t + 2U < indexCount; t += 3U) {
        const std::uint32_t i0 = indices[t];
        const std::uint32_t i1 = indices[t + 1U];
        const std::uint32_t i2 = indices[t + 2U];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) continue;

        const Point a = TransformPoint(transform, vertices[i0].x, vertices[i0].y);
        const Point b = TransformPoint(transform, vertices[i1].x, vertices[i1].y);
        const Point c = TransformPoint(transform, vertices[i2].x, vertices[i2].y);
        if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
            !std::isfinite(b.x) || !std::isfinite(b.y) ||
            !std::isfinite(c.x) || !std::isfinite(c.y)) continue;

        if (!PrepareGeometrySpace(3U, stride, 3U)) return;

        uint8_t* dst = mappedVertices_ + currentVertexOffset_;
        WriteVertex(dst + 0U * stride, offsets, static_cast<float>(a.x), static_cast<float>(a.y), color32, 0.0f, 0.0f, 1.0f);
        WriteVertex(dst + 1U * stride, offsets, static_cast<float>(b.x), static_cast<float>(b.y), color32, 0.0f, 0.0f, 1.0f);
        WriteVertex(dst + 2U * stride, offsets, static_cast<float>(c.x), static_cast<float>(c.y), color32, 0.0f, 0.0f, 1.0f);
        currentVertexOffset_ += 3U * stride;

        const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_ - currentBatchVertexStart_);
        mappedIndices_[currentIndexCount_ + 0] = baseVertex + 0;
        mappedIndices_[currentIndexCount_ + 1] = baseVertex + 1;
        mappedIndices_[currentIndexCount_ + 2] = baseVertex + 2;
        currentVertexCount_ += 3U;
        currentIndexCount_ += 3U;
        currentBatch_.numVertices += 3U;
        currentBatch_.numIndices += 3U;
        stats_.vertexCount += 3U;
        stats_.indexCount += 3U;
    }
}

void UiFrameEncoder::EmitAnalyticalShadow(
    const CmdAnalyticalShadow& cmd,
    const ProjectiveTransform2D& transform,
    double opacity) noexcept {
    const double blur = std::max(0.0, static_cast<double>(cmd.blurRadius));
    const double rx = cmd.rect.x + cmd.offset.x;
    const double ry = cmd.rect.y + cmd.offset.y;
    const double rw = cmd.rect.width;
    const double rh = cmd.rect.height;
    if (rw <= 0.0 || rh <= 0.0) return;

    Color c = cmd.color;
    c.alpha = static_cast<float>(std::clamp(static_cast<double>(c.alpha) * opacity, 0.0, 1.0));
    if (c.alpha <= 0.0f) return;

    EnsureBatchBlend(Shader::Path_Solid);
    SetContentStencil();

    const double maxRadius = std::fmin(rw, rh) * 0.5;
    const double baseRadius = std::clamp(static_cast<double>(cmd.cornerRadius), 0.0, maxRadius);
    const double spread = blur * 2.0;
    const double outerRadius = baseRadius + spread;
    const double innerRadius = std::max(0.0, baseRadius - blur * 0.5);

    const std::uint32_t cornerSegments = (outerRadius > 0.0)
        ? CalculateRoundedCornerSegments(transform, rx, ry, outerRadius)
        : 1U;
    const std::uint32_t contourCount = cornerSegments * 4U;

    Point inner[kMaxRoundedRectContourPoints];
    BuildRoundedRectContour(
        rx, ry, rw, rh, innerRadius, cornerSegments, inner);

    const Point centroid = TransformPoint(transform, rx + rw * 0.5, ry + rh * 0.5);
    Point transformedInner[kMaxRoundedRectContourPoints];
    for (std::uint32_t i = 0U; i < contourCount; ++i) {
        transformedInner[i] = TransformPoint(transform, inner[i].x, inner[i].y);
    }

    // 1. 中心实心圆角核
    EmitTriangleFan(transformedInner, contourCount, centroid, c);
    if (blur <= 0.0) return;

    // 2. 外围圆角平滑衰减环 (Inner Alpha = c.alpha -> Outer Alpha = 0)
    Point outer[kMaxRoundedRectContourPoints];
    BuildRoundedRectContour(
        rx - spread, ry - spread, rw + spread * 2.0, rh + spread * 2.0,
        outerRadius, cornerSegments, outer);

    Color cZero = c;
    cZero.alpha = 0.0f;
    const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

    for (std::uint32_t i = 0U; i < contourCount; ++i) {
        const std::uint32_t next = (i + 1U) % contourCount;
        const Point pts[4] = {
            transformedInner[i],
            transformedInner[next],
            TransformPoint(transform, outer[next].x, outer[next].y),
            TransformPoint(transform, outer[i].x, outer[i].y)
        };
        const Color cols[4] = {c, c, cZero, cZero};
        EmitQuadWithColors(pts, uvs, cols, 1.0);
    }
}

void UiFrameEncoder::ProcessCommand(
    const RenderCommandHeader& h,
    const ProjectiveTransform2D& currentTransform,
    double currentOpacity) noexcept {
    if (currentFrame_ == nullptr) return;

    switch (h.kind) {
    case RenderCommandKind::PushClip: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdPushClip>(h.payloadOffset);
        if (cmd == nullptr) break;
        EmitClipQuad(
            cmd->rect, currentTransform, StencilMode::Equal_Incr, clipDepth_);
        static_cast<void>(clipStack_.PushBack(
            ClipEntry{cmd->rect, currentTransform}));
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
        const auto* cmd = currentFrame_->PayloadPtr<CmdFillRect>(h.payloadOffset);
        if (cmd == nullptr) break;
        EnsureBatchBlend(Shader::Path_Solid);
        SetContentStencil();

        const Point p0 = TransformPoint(currentTransform, cmd->rect.x, cmd->rect.y);
        const Point p1 = TransformPoint(currentTransform, cmd->rect.x + cmd->rect.width, cmd->rect.y);
        const Point p2 = TransformPoint(currentTransform, cmd->rect.x + cmd->rect.width, cmd->rect.y + cmd->rect.height);
        const Point p3 = TransformPoint(currentTransform, cmd->rect.x, cmd->rect.y + cmd->rect.height);

        const Point points[4] = {p0, p1, p2, p3};
        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

        Color c = cmd->color;
        c.alpha = static_cast<float>(c.alpha * currentOpacity);
        EmitQuad(points, uvs, c);
        break;
    }
    case RenderCommandKind::FillGradientQuad: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdGradientQuad>(h.payloadOffset);
        if (cmd == nullptr) break;
        const Point p0 = TransformPoint(currentTransform, cmd->points[0].x, cmd->points[0].y);
        const Point p1 = TransformPoint(currentTransform, cmd->points[1].x, cmd->points[1].y);
        const Point p2 = TransformPoint(currentTransform, cmd->points[2].x, cmd->points[2].y);
        const Point p3 = TransformPoint(currentTransform, cmd->points[3].x, cmd->points[3].y);

        const Point points[4] = {p0, p1, p2, p3};
        if (h.paintKind == 1U || h.paintKind == 2U) {
            const Shader::Enum shader = ToPPAAShader(h.paintKind == 1U
                ? Shader::Path_Linear
                : Shader::Path_Radial, enablePPAA_);
            Texture* ramp = nullptr;
            if (currentFrame_ != nullptr) {
                const Base::Span<const RenderGradientRampSnapshot> ramps =
                    currentFrame_->GradientRamps();
                if (cmd->gradientRamp < ramps.Size()) {
                    ramp = GetOrCreateGradientRamp(ramps[cmd->gradientRamp]);
                }
            }
            float uniforms[8];
            std::memcpy(uniforms, cmd->paintUniforms, sizeof(uniforms));
            if (h.paintKind == 1U) {
                uniforms[0] *= static_cast<float>(currentOpacity);
            } else {
                uniforms[3] *= static_cast<float>(currentOpacity);
            }
            SetBatchRamp(shader, ramp, uniforms, h.wrapMode);
            SetContentStencil();
            const Color white{1.0F, 1.0F, 1.0F, 1.0F};
            EmitQuad(points, cmd->uvs, white);
            break;
        }
        EnsureBatchBlend(ToPPAAShader(Shader::Path_Solid, enablePPAA_));
        SetContentStencil();

        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

        EmitQuadWithColors(points, uvs, cmd->colors, currentOpacity);
        break;
    }
    case RenderCommandKind::FillRoundedRect: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdRoundedRect>(h.payloadOffset);
        if (cmd == nullptr) break;
        const double radius = std::min(
            std::max(0.0, static_cast<double>(cmd->radius)),
            std::fmin(cmd->rect.width, cmd->rect.height) * 0.5);

        EnsureBatchBlend(ToPPAAShader(Shader::Path_Solid, enablePPAA_));
        SetContentStencil();

        Color c = cmd->color;
        c.alpha = static_cast<float>(c.alpha * currentOpacity);

        if (radius <= 0.0) {
            const Point p0 = TransformPoint(currentTransform, cmd->rect.x, cmd->rect.y);
            const Point p1 = TransformPoint(currentTransform, cmd->rect.x + cmd->rect.width, cmd->rect.y);
            const Point p2 = TransformPoint(currentTransform, cmd->rect.x + cmd->rect.width, cmd->rect.y + cmd->rect.height);
            const Point p3 = TransformPoint(currentTransform, cmd->rect.x, cmd->rect.y + cmd->rect.height);

            const Point points[4] = {p0, p1, p2, p3};
            const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
            EmitQuad(points, uvs, c);
            break;
        }

        const std::uint32_t cornerSegments = CalculateRoundedCornerSegments(
            currentTransform, cmd->rect.x, cmd->rect.y, radius);
        const std::uint32_t contourCount = cornerSegments * 4U;

        Point contour[kMaxRoundedRectContourPoints];
        BuildRoundedRectContour(
            cmd->rect.x, cmd->rect.y, cmd->rect.width, cmd->rect.height,
            radius, cornerSegments, contour);

        const Point centroid{
            cmd->rect.x + cmd->rect.width * 0.5,
            cmd->rect.y + cmd->rect.height * 0.5};
        const Point center = TransformPoint(currentTransform, centroid.x, centroid.y);
        Point transformedContour[kMaxRoundedRectContourPoints];
        for (std::uint32_t i = 0U; i < contourCount; ++i) {
            transformedContour[i] = TransformPoint(currentTransform, contour[i].x, contour[i].y);
        }
        EmitTriangleFan(transformedContour, contourCount, center, c);

        if (enablePPAA_) {
            Point outer[kMaxRoundedRectContourPoints];
            BuildRoundedRectContour(
                cmd->rect.x - 0.5, cmd->rect.y - 0.5,
                cmd->rect.width + 1.0, cmd->rect.height + 1.0,
                radius + 0.5, cornerSegments, outer);
            Color cZero = c;
            cZero.alpha = 0.0f;
            const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
            for (std::uint32_t i = 0U; i < contourCount; ++i) {
                const std::uint32_t next = (i + 1U) % contourCount;
                const Point pts[4] = {
                    transformedContour[i],
                    transformedContour[next],
                    TransformPoint(currentTransform, outer[next].x, outer[next].y),
                    TransformPoint(currentTransform, outer[i].x, outer[i].y)
                };
                const Color cols[4] = {c, c, cZero, cZero};
                EmitQuadWithColors(pts, uvs, cols, 1.0);
            }
        }
        break;
    }
    case RenderCommandKind::StrokeRect: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdStrokeRect>(h.payloadOffset);
        if (cmd == nullptr) break;
        EnsureBatchBlend(ToPPAAShader(Shader::Path_Solid, enablePPAA_));
        SetContentStencil();

        const double thickness = cmd->thickness > 0.0f ? static_cast<double>(cmd->thickness) : 1.0;
        const Rect& r = cmd->rect;
        const double maxRadius = std::fmin(r.width, r.height) * 0.5;
        const double radius = std::clamp(
            std::max(0.0, static_cast<double>(cmd->cornerRadius)), 0.0, maxRadius);

        Color c = cmd->color;
        c.alpha = static_cast<float>(c.alpha * currentOpacity);
        const Point uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

        const double outerRadius = radius;
        const double innerRadius = std::max(0.0, radius - thickness);
        const std::uint32_t cornerSegments = radius > 0.0
            ? CalculateRoundedCornerSegments(currentTransform, r.x, r.y, outerRadius)
            : 1U;
        const std::uint32_t contourCount = cornerSegments * 4U;

        const double innerW = std::max(0.0, r.width - 2.0 * thickness);
        const double innerH = std::max(0.0, r.height - 2.0 * thickness);

        Point outer[kMaxRoundedRectContourPoints];
        Point inner[kMaxRoundedRectContourPoints];
        BuildRoundedRectContour(
            r.x, r.y, r.width,
            r.height, outerRadius, cornerSegments, outer);
        BuildRoundedRectContour(
            r.x + thickness, r.y + thickness, innerW,
            innerH, innerRadius, cornerSegments, inner);

        for (std::uint32_t i = 0U; i < contourCount; ++i) {
            const std::uint32_t next = (i + 1U) % contourCount;
            const Point p0 = TransformPoint(currentTransform, outer[i].x, outer[i].y);
            const Point p1 = TransformPoint(currentTransform, outer[next].x, outer[next].y);
            const Point p2 = TransformPoint(currentTransform, inner[next].x, inner[next].y);
            const Point p3 = TransformPoint(currentTransform, inner[i].x, inner[i].y);
            const Point points[4] = {p0, p1, p2, p3};
            EmitQuad(points, uvs, c);
        }

        if (enablePPAA_) {
            Point outerAA[kMaxRoundedRectContourPoints];
            BuildRoundedRectContour(
                r.x - 0.5, r.y - 0.5,
                r.width + 1.0, r.height + 1.0,
                outerRadius + 0.5, cornerSegments, outerAA);
            Color cZero = c;
            cZero.alpha = 0.0f;
            for (std::uint32_t i = 0U; i < contourCount; ++i) {
                const std::uint32_t next = (i + 1U) % contourCount;
                const Point pts[4] = {
                    TransformPoint(currentTransform, outer[i].x, outer[i].y),
                    TransformPoint(currentTransform, outer[next].x, outer[next].y),
                    TransformPoint(currentTransform, outerAA[next].x, outerAA[next].y),
                    TransformPoint(currentTransform, outerAA[i].x, outerAA[i].y)
                };
                const Color cols[4] = {c, c, cZero, cZero};
                EmitQuadWithColors(pts, uvs, cols, 1.0);
            }
            if (innerRadius > 0.0 && innerW > 1.0 && innerH > 1.0) {
                Point innerAA[kMaxRoundedRectContourPoints];
                BuildRoundedRectContour(
                    r.x + thickness + 0.5, r.y + thickness + 0.5,
                    std::max(0.0, innerW - 1.0), std::max(0.0, innerH - 1.0),
                    std::max(0.0, innerRadius - 0.5), cornerSegments, innerAA);
                for (std::uint32_t i = 0U; i < contourCount; ++i) {
                    const std::uint32_t next = (i + 1U) % contourCount;
                    const Point pts[4] = {
                        TransformPoint(currentTransform, innerAA[i].x, innerAA[i].y),
                        TransformPoint(currentTransform, innerAA[next].x, innerAA[next].y),
                        TransformPoint(currentTransform, inner[next].x, inner[next].y),
                        TransformPoint(currentTransform, inner[i].x, inner[i].y)
                    };
                    const Color cols[4] = {cZero, cZero, c, c};
                    EmitQuadWithColors(pts, uvs, cols, 1.0);
                }
            }
        }
        break;
    }
    case RenderCommandKind::DrawAnalyticalShadow: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdAnalyticalShadow>(h.payloadOffset);
        if (cmd == nullptr) break;
        EmitAnalyticalShadow(*cmd, currentTransform, currentOpacity);
        break;
    }
    case RenderCommandKind::DrawImage: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdDrawImage>(h.payloadOffset);
        if (cmd == nullptr) break;
        Texture* tex = FindImage(cmd->image);
        if (tex != nullptr) {
            const std::uint8_t targetWrapMode = h.wrapMode;
            if (currentBatch_.numIndices > 0U &&
                (currentBatch_.shader != Shader::Path_Pattern ||
                 currentBatch_.pattern != tex ||
                 currentBatch_.image != tex ||
                 currentBatch_.patternSampler.f.wrapMode != targetWrapMode ||
                 currentBatch_.imageSampler.f.wrapMode != targetWrapMode ||
                 currentBatch_.renderState.f.colorEnable != 1U ||
                 currentBatch_.renderState.f.blendMode != static_cast<uint8_t>(currentBlendMode_))) {
                FlushBatch();
            }
            currentBatch_.shader = Shader::Path_Pattern;
            currentBatch_.pattern = tex;
            currentBatch_.patternSampler.f.wrapMode = targetWrapMode;
            currentBatch_.patternSampler.f.minmagFilter = MinMagFilter::Linear;
            currentBatch_.image = tex;
            currentBatch_.imageSampler.f.wrapMode = targetWrapMode;
            currentBatch_.imageSampler.f.minmagFilter = MinMagFilter::Linear;
            AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
            AssignColorEnable(currentBatch_.renderState, 1U);
            SetContentStencil();

            const Point p0 = TransformPoint(currentTransform, cmd->destination.x, cmd->destination.y);
            const Point p1 = TransformPoint(currentTransform, cmd->destination.x + cmd->destination.width, cmd->destination.y);
            const Point p2 = TransformPoint(currentTransform, cmd->destination.x + cmd->destination.width, cmd->destination.y + cmd->destination.height);
            const Point p3 = TransformPoint(currentTransform, cmd->destination.x, cmd->destination.y + cmd->destination.height);

            const Point points[4] = {p0, p1, p2, p3};
            const Point uvs[4] = {
                {cmd->sourceUv.x, cmd->sourceUv.y},
                {cmd->sourceUv.x + cmd->sourceUv.width, cmd->sourceUv.y},
                {cmd->sourceUv.x + cmd->sourceUv.width, cmd->sourceUv.y + cmd->sourceUv.height},
                {cmd->sourceUv.x, cmd->sourceUv.y + cmd->sourceUv.height}
            };

            Color c = cmd->tint;
            c.alpha = static_cast<float>(c.alpha * currentOpacity);
            EmitQuad(points, uvs, c);
        }
        break;
    }
    case RenderCommandKind::DrawGlyphRun: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdDrawGlyphRun>(h.payloadOffset);
        if (cmd == nullptr) break;
        const Base::Vector<RenderGlyphQuad>* quads = FindGlyphRun(cmd->glyphRun);
        if (quads == nullptr) break;

        const Point runOrigin = TransformPoint(currentTransform, 0.0, 0.0);
        const double snapDx = std::round(runOrigin.x) - runOrigin.x;
        const double snapDy = std::round(runOrigin.y) - runOrigin.y;

        Color c = cmd->tint;
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
            glyphAtlasSizeUniform_[0] = static_cast<float>(atlas->GetWidth());
            glyphAtlasSizeUniform_[1] = static_cast<float>(atlas->GetHeight());
            currentBatch_.shader = Shader::SDF_Solid;
            currentBatch_.glyphs = atlas;
            currentBatch_.vertexUniforms[1].values = glyphAtlasSizeUniform_;
            currentBatch_.vertexUniforms[1].numDwords = 2U;
            currentBatch_.glyphsSampler.f.wrapMode = WrapMode::ClampToEdge;
            currentBatch_.glyphsSampler.f.minmagFilter = MinMagFilter::Linear;
            AssignBlendMode(currentBatch_.renderState, static_cast<unsigned>(currentBlendMode_));
            AssignColorEnable(currentBatch_.renderState, 1U);
            SetContentStencil();

            Point p0 = TransformPoint(currentTransform, quad.x0, quad.y0);
            Point p1 = TransformPoint(currentTransform, quad.x1, quad.y0);
            Point p2 = TransformPoint(currentTransform, quad.x1, quad.y1);
            Point p3 = TransformPoint(currentTransform, quad.x0, quad.y1);
            p0.x += snapDx; p0.y += snapDy;
            p1.x += snapDx; p1.y += snapDy;
            p2.x += snapDx; p2.y += snapDy;
            p3.x += snapDx; p3.y += snapDy;

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
    case RenderCommandKind::DrawTriangles: {
        const auto* cmd = currentFrame_->PayloadPtr<CmdDrawTriangles>(h.payloadOffset);
        if (cmd == nullptr) break;
        const auto* vertices = currentFrame_->PayloadPtr<Point>(cmd->vertexOffset);
        const auto* indices = currentFrame_->PayloadPtr<std::uint32_t>(cmd->indexOffset);
        if (vertices != nullptr && indices != nullptr) {
            EmitTriangles(
                vertices, cmd->vertexCount,
                indices, cmd->indexCount,
                currentTransform, cmd->color, currentOpacity);
        }
        break;
    }
    case RenderCommandKind::DrawSolidMesh:
    case RenderCommandKind::DrawGradientMesh: {
        RenderMeshId meshId = InvalidRenderMeshId;
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F};
        bool isGradient = false;
        bool hasInverse = false;
        Base::Transform2D inverse{};
        Rect bounds{};
        Point startOrOrigin{};
        Point delta{};
        Point radii{};
        double len2 = 0.0;

        if (h.kind == RenderCommandKind::DrawSolidMesh) {
            const auto* cmd = currentFrame_->PayloadPtr<CmdDrawSolidMesh>(h.payloadOffset);
            if (cmd == nullptr) break;
            meshId = cmd->mesh;
            tint = cmd->tint;
        } else {
            const auto* cmd = currentFrame_->PayloadPtr<CmdDrawGradientMesh>(h.payloadOffset);
            if (cmd == nullptr) break;
            meshId = cmd->mesh;
            tint = cmd->tint;
            isGradient = (h.paintKind == 1U || h.paintKind == 2U);
            hasInverse = cmd->hasInverse;
            inverse = cmd->inverse;
            bounds = cmd->bounds;
            startOrOrigin = cmd->startOrOrigin;
            delta = cmd->delta;
            radii = cmd->radii;
            len2 = cmd->len2;

            if (isGradient) {
                Texture* ramp = nullptr;
                if (currentFrame_ != nullptr) {
                    const Base::Span<const RenderGradientRampSnapshot> ramps =
                        currentFrame_->GradientRamps();
                    if (cmd->gradientRamp < ramps.Size()) {
                        ramp = GetOrCreateGradientRamp(ramps[cmd->gradientRamp]);
                    }
                }
                float uniforms[8];
                std::memcpy(uniforms, cmd->paintUniforms, sizeof(uniforms));
                if (h.paintKind == 1U) {
                    uniforms[0] *= static_cast<float>(currentOpacity);
                } else {
                    uniforms[3] *= static_cast<float>(currentOpacity);
                }
                SetBatchRamp(
                    ToPPAAShader(h.paintKind == 1U ? Shader::Path_Linear : Shader::Path_Radial, enablePPAA_),
                    ramp,
                    uniforms,
                    h.wrapMode);
            }
        }

        const MeshEntry* mesh = FindMesh(meshId);
        if (mesh == nullptr || mesh->indices.Empty()) {
            break;
        }

        if (!isGradient) {
            EnsureBatchBlend(ToPPAAShader(Shader::Path_Solid, enablePPAA_));
        }
        SetContentStencil();

        Color c = tint;
        if (!isGradient) {
            c.alpha = static_cast<float>(c.alpha * currentOpacity);
        }
        const uint32_t color32 = ColorToRGBA32(c, 1.0);

        if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) break;

        std::uint32_t vertexStride = 0U;
        const auto& offsets = CurrentVertexFormat(vertexStride);

        const std::uint32_t kMaxBatchVertices =
            static_cast<std::uint32_t>(DYNAMIC_VB_SIZE / vertexStride);
        constexpr std::uint32_t kMaxBatchIndices =
            static_cast<std::uint32_t>(DYNAMIC_IB_SIZE / sizeof(uint16_t));
        const std::uint32_t kMaxMeshVertices =
            kMaxBatchVertices < 65535U ? kMaxBatchVertices : 65535U;

        auto remapDynamicBuffers = [&]() noexcept -> bool {
            CommitBatches();
            return EnsureMappedBuffers();
        };

        auto getVertexAttributes = [&](Point local, Point& outPos, float& outU, float& outV) noexcept {
            outPos = TransformPoint(currentTransform, local.x, local.y);
            outU = 0.0F;
            outV = 0.0F;
            if (isGradient) {
                Point sample = local;
                if (hasInverse && bounds.width > 1.0e-12 && bounds.height > 1.0e-12) {
                    const Point uv{
                        (local.x - bounds.x) / bounds.width,
                        (local.y - bounds.y) / bounds.height};
                    const Point mapped{
                        uv.x * inverse.m11 + uv.y * inverse.m21 + inverse.dx,
                        uv.x * inverse.m12 + uv.y * inverse.m22 + inverse.dy};
                    sample = {
                        bounds.x + mapped.x * bounds.width,
                        bounds.y + mapped.y * bounds.height};
                }
                if (h.paintKind == 1U) {
                    const double len2Val = len2 > 1.0e-12 ? len2 : 1.0;
                    const double t =
                        ((sample.x - startOrOrigin.x) * delta.x +
                         (sample.y - startOrOrigin.y) * delta.y) /
                        len2Val;
                    outU = static_cast<float>(t);
                    outV = 0.5F;
                } else if (h.paintKind == 2U) {
                    const double rx = std::fabs(radii.x) > 1.0e-6 ? radii.x : 1.0;
                    const double ry = std::fabs(radii.y) > 1.0e-6 ? radii.y : 1.0;
                    outU = static_cast<float>((sample.x - startOrOrigin.x) / rx);
                    outV = static_cast<float>((sample.y - startOrOrigin.y) / ry);
                }
            }
        };

        auto writeMeshVertex = [&](Point local, std::uint32_t vertexIdx, uint8_t* dst) noexcept {
            Point pos;
            float u = 0.0F;
            float vCoord = 0.0F;
            getVertexAttributes(local, pos, u, vCoord);
            const float coverage = (vertexIdx < mesh->coverages.Size())
                ? mesh->coverages[vertexIdx]
                : 1.0F;
            WriteVertex(
                dst,
                offsets,
                static_cast<float>(pos.x),
                static_cast<float>(pos.y),
                color32,
                u,
                vCoord,
                coverage);
        };

        auto writeMeshSlab = [&](
            std::uint32_t start,
            std::uint32_t end) noexcept {
            for (std::uint32_t index = start; index < end; ++index) {
                const std::uint32_t localIndex = index - start;
                writeMeshVertex(
                    mesh->vertices[index],
                    index,
                    mappedVertices_ + currentVertexOffset_ + localIndex * vertexStride);
            }
        };

        if (currentVertexOffset_ + std::min(mesh->vertices.Size(), kMaxMeshVertices) * vertexStride > DYNAMIC_VB_SIZE ||
            currentVertexCount_ + std::min(mesh->vertices.Size(), kMaxMeshVertices) > kMaxBatchVertices ||
            currentIndexCount_ + 3U > kMaxBatchIndices) {
            if (!remapDynamicBuffers()) break;
        }

        const std::uint32_t vertexCount = mesh->vertices.Size();
        if (vertexCount <= kMaxMeshVertices) {
            const std::uint32_t count = vertexCount;
            if ((currentVertexCount_ - currentBatchVertexStart_) + count > 65535U) {
                FlushBatch();
            }
            if (currentVertexOffset_ + count * vertexStride > DYNAMIC_VB_SIZE ||
                currentVertexCount_ + count > kMaxBatchVertices) {
                if (!remapDynamicBuffers()) break;
            }
            if (!EnsureCapacity(count * vertexStride, mesh->indices.Size())) break;
            if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) break;

            writeMeshSlab(0U, count);
            uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_ - currentBatchVertexStart_);
            std::uint32_t emittedIndices = 0U;
            for (std::uint32_t index = 0U; index + 2U < mesh->indices.Size(); index += 3U) {
                if (currentIndexCount_ + emittedIndices + 3U > kMaxBatchIndices) {
                    currentVertexOffset_ += count * vertexStride;
                    currentVertexCount_ += count;
                    currentIndexCount_ += emittedIndices;
                    currentBatch_.numVertices += count;
                    currentBatch_.numIndices += emittedIndices;
                    stats_.vertexCount += count;
                    stats_.indexCount += emittedIndices;
                    FlushBatch();
                    if (!remapDynamicBuffers() || !EnsureCapacity(count * vertexStride, mesh->indices.Size() - index)) {
                        break;
                    }
                    writeMeshSlab(0U, count);
                    baseVertex = 0U;
                    emittedIndices = 0U;
                }
                mappedIndices_[currentIndexCount_ + emittedIndices + 0U] =
                    static_cast<uint16_t>(baseVertex + mesh->indices[index + 0U]);
                mappedIndices_[currentIndexCount_ + emittedIndices + 1U] =
                    static_cast<uint16_t>(baseVertex + mesh->indices[index + 1U]);
                mappedIndices_[currentIndexCount_ + emittedIndices + 2U] =
                    static_cast<uint16_t>(baseVertex + mesh->indices[index + 2U]);
                emittedIndices += 3U;
            }
            currentVertexOffset_ += count * vertexStride;
            currentVertexCount_ += count;
            currentIndexCount_ += emittedIndices;
            currentBatch_.numVertices += count;
            currentBatch_.numIndices += emittedIndices;
            stats_.vertexCount += count;
            stats_.indexCount += emittedIndices;
        } else {
            // Large mesh fallback: partition by triangles without dropping cross-boundary faces
            const std::uint32_t totalTriangles = mesh->indices.Size() / 3U;
            const std::uint32_t maxTrianglesPerBatch = std::min(kMaxMeshVertices / 3U, kMaxBatchIndices / 3U);
            std::uint32_t triIndex = 0U;
            while (triIndex < totalTriangles) {
                const std::uint32_t chunkTriangles = std::min(maxTrianglesPerBatch, totalTriangles - triIndex);
                const std::uint32_t chunkVertices = chunkTriangles * 3U;
                const std::uint32_t chunkIndices = chunkTriangles * 3U;

                if ((currentVertexCount_ - currentBatchVertexStart_) + chunkVertices > 65535U) {
                    FlushBatch();
                }
                if (currentVertexOffset_ + chunkVertices * vertexStride > DYNAMIC_VB_SIZE ||
                    currentVertexCount_ + chunkVertices > kMaxBatchVertices ||
                    currentIndexCount_ + chunkIndices > kMaxBatchIndices) {
                    if (!remapDynamicBuffers()) break;
                }
                if (!EnsureCapacity(chunkVertices * vertexStride, chunkIndices)) break;
                if (mappedVertices_ == nullptr || mappedIndices_ == nullptr) break;

                const uint16_t baseVertex = static_cast<uint16_t>(currentVertexCount_ - currentBatchVertexStart_);
                for (std::uint32_t t = 0U; t < chunkTriangles; ++t) {
                    const std::uint32_t iStart = (triIndex + t) * 3U;
                    for (std::uint32_t v = 0U; v < 3U; ++v) {
                        const std::uint32_t origIdx = mesh->indices[iStart + v];
                        const Point local = origIdx < mesh->vertices.Size() ? mesh->vertices[origIdx] : Point{};
                        const std::uint32_t localVertexIdx = t * 3U + v;
                        writeMeshVertex(
                            local,
                            origIdx,
                            mappedVertices_ + currentVertexOffset_ + localVertexIdx * vertexStride);
                        mappedIndices_[currentIndexCount_ + localVertexIdx] = static_cast<uint16_t>(baseVertex + localVertexIdx);
                    }
                }
                currentVertexOffset_ += chunkVertices * vertexStride;
                currentVertexCount_ += chunkVertices;
                currentIndexCount_ += chunkIndices;
                currentBatch_.numVertices += chunkVertices;
                currentBatch_.numIndices += chunkIndices;
                stats_.vertexCount += chunkVertices;
                stats_.indexCount += chunkIndices;
                triIndex += chunkTriangles;
            }
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
    BeginOffscreenTargetFrame();
    currentFrame_ = &frame;
    stats_.sourceCommandCount = frame.CommandHeaders().Size();

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

    EnsureMappedBuffers();

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

    const auto commands = frame.CommandHeaders();
    const auto nodes = frame.Nodes();
    const auto clipVertices = frame.GeometryClipVertices();
    const auto clipIndices = frame.GeometryClipIndices();
    const std::uint32_t nodeCount = nodes.Size();

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
            if (!clipStack_.Empty()) {
                const ClipEntry entry = clipStack_.Back();
                static_cast<void>(clipStack_.PopBack());
                if (clipDepth_ > 0U) {
                    --clipDepth_;
                    emitClipEntry(
                        entry,
                        StencilMode::Equal_Decr,
                        static_cast<std::uint8_t>(clipDepth_ + 1U));
                }
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
        } else if ((node.clipsToBounds || node.hasLayoutClip) &&
                   (node.clip.width > 0.0 || node.renderSize.width > 0.0) &&
                   (node.clip.height > 0.0 || node.renderSize.height > 0.0)) {
            const Rect localClip = (node.clip.width > 0.0 && node.clip.height > 0.0)
                ? node.clip
                : Rect{0.0, 0.0, node.renderSize.width, node.renderSize.height};
            EmitClipQuad(
                localClip, nodeTransform, StencilMode::Equal_Incr, clipDepth_);
            static_cast<void>(
                clipStack_.PushBack(ClipEntry{localClip, nodeTransform}));
            ++clipDepth_;
            nodePushedClip = true;
        }

        ProjectiveTransform2D currentTransform = nodeTransform;
        double currentOpacity = nodeOpacity;
        pushStack_.Clear();

        const std::uint32_t commandEnd =
            node.commandOffset + node.commandCount;
        for (std::uint32_t index = node.commandOffset;
             index < commandEnd && index < commands.Size();
             ++index) {
            const RenderCommandHeader& h = commands[index];
            switch (h.kind) {
            case RenderCommandKind::PushTransform: {
                const auto* cmd = frame.PayloadPtr<CmdPushTransform>(h.payloadOffset);
                if (cmd != nullptr) {
                    static_cast<void>(pushStack_.PushBack(
                        PushState{currentTransform, currentOpacity}));
                    currentTransform = CombineTransform(cmd->transform, currentTransform);
                }
                break;
            }
            case RenderCommandKind::PopTransform:
                if (!pushStack_.Empty()) {
                    currentTransform = pushStack_.Back().transform;
                    currentOpacity = pushStack_.Back().opacity;
                    static_cast<void>(pushStack_.PopBack());
                }
                break;
            case RenderCommandKind::PushOpacity: {
                const auto* cmd = frame.PayloadPtr<CmdPushOpacity>(h.payloadOffset);
                if (cmd != nullptr) {
                    static_cast<void>(pushStack_.PushBack(
                        PushState{currentTransform, currentOpacity}));
                    currentOpacity = currentOpacity * cmd->opacity;
                }
                break;
            }
            case RenderCommandKind::PopOpacity:
                if (!pushStack_.Empty()) {
                    currentTransform = pushStack_.Back().transform;
                    currentOpacity = pushStack_.Back().opacity;
                    static_cast<void>(pushStack_.PopBack());
                }
                break;
            default:
                ProcessCommand(h, currentTransform, currentOpacity);
                break;
            }
        }
        return nodePushedClip;
    };

    // P4.1: O(n) subtree ranges. Nodes are emitted in DFS preorder with
    // parent preceding child (RenderTree::Commit via BuildSubtree; enforced
    // by ValidateRenderFrame), so every subtree is a contiguous index range.
    // The old parentIndexes reverse scan (O(n^2)) plus the isInSubtree chain
    // walk inside every subtreeEndOf call (O(n . depth) each, O(n^3) total
    // across the record loop) is replaced by:
    //  1. one preorder pass deriving depths with an explicit ancestor stack
    //     (each node pushed/popped once, amortized O(n)) — mirrors the
    //     nodeStack open/close logic in the record loop below;
    //  2. one reverse pass computing every subtree end with a monotonic
    //     scan (path compression, amortized O(n));
    //  3. prefix sums over commandCount for O(1) subtree command queries.
    recordDepths_.Clear();
    recordAncestorStack_.Clear();
    for (std::uint32_t i = 0U; i < nodeCount; ++i) {
        const RenderNodeId parentId = nodes[i].parentId;
        while (!recordAncestorStack_.Empty() && recordAncestorStack_.Back() != parentId) {
            static_cast<void>(recordAncestorStack_.PopBack());
        }
        // Orphan parent ids (invalid frames) bottom out at depth 0, matching
        // the old kNotFound behavior where isInSubtree() returned false.
        static_cast<void>(recordDepths_.PushBack(recordAncestorStack_.Size()));
        static_cast<void>(recordAncestorStack_.PushBack(nodes[i].id));
    }

    recordSubtreeEnds_.Clear();
    for (std::uint32_t i = 0U; i < nodeCount; ++i) {
        static_cast<void>(recordSubtreeEnds_.PushBack(i + 1U));
    }
    for (std::uint32_t i = nodeCount; i > 0U;) {
        --i;
        std::uint32_t end = i + 1U;
        while (end < nodeCount && recordDepths_[end] > recordDepths_[i]) {
            end = recordSubtreeEnds_[end];
        }
        recordSubtreeEnds_[i] = end;
    }

    recordCommandPrefix_.Clear();
    static_cast<void>(recordCommandPrefix_.PushBack(0U));
    for (std::uint32_t i = 0U; i < nodeCount; ++i) {
        static_cast<void>(recordCommandPrefix_.PushBack(
            recordCommandPrefix_[i] + nodes[i].commandCount));
    }

    recordEffectMaskPrefix_.Clear();
    static_cast<void>(recordEffectMaskPrefix_.PushBack(0U));
    for (std::uint32_t i = 0U; i < nodeCount; ++i) {
        const RenderEffectSnapshot* eff = frame.EffectAt(nodes[i].effectIndex);
        const RenderMaskSnapshot* msk = frame.MaskAt(nodes[i].maskIndex);
        const bool hasEff = (eff != nullptr && eff->kind != RenderEffectKind::None) ||
                            (msk != nullptr && msk->kind != RenderMaskKind::None);
        static_cast<void>(recordEffectMaskPrefix_.PushBack(
            recordEffectMaskPrefix_[i] + (hasEff ? 1U : 0U)));
    }

    auto subtreeEndOf = [&](std::uint32_t rootIndex) noexcept {
        return recordSubtreeEnds_[rootIndex];
    };

    auto subtreeCommandCount = [&](std::uint32_t rootIndex) noexcept {
        return recordCommandPrefix_[recordSubtreeEnds_[rootIndex]] -
            recordCommandPrefix_[rootIndex];
    };

    // 在 RecordOnscreen 内部定义统一的递归子树录制器，支持任意深度离屏嵌套：
    auto renderSubtree = [&](auto& self,
        std::uint32_t startIndex,
        std::uint32_t endIndex,
        RenderTarget& currentTarget,
        std::uint32_t targetWidth,
        std::uint32_t targetHeight,
        const ProjectiveTransform2D& rootSpaceTransform,
        bool isOffscreenRoot) noexcept -> void {
        Base::Vector<NodeState> stack(allocator_);
        for (std::uint32_t idx = startIndex; idx < endIndex; ) {
            const RenderNodeSnapshot& node = nodes[idx];
            while (!stack.Empty() && stack.Back().nodeId != node.parentId) {
                popNodeClip(stack);
                static_cast<void>(stack.PopBack());
            }

            const ProjectiveTransform2D baseTransform =
                stack.Empty() ? rootSpaceTransform : stack.Back().transform;
            const double baseOpacity =
                stack.Empty() ? 1.0 : stack.Back().opacity;

            const bool isCurrentPassRoot = isOffscreenRoot && (idx == startIndex);
            const ProjectiveTransform2D local = isCurrentPassRoot
                ? ProjectiveTransform2D{}
                : CombineTransform(node.renderTransform, MakeTranslate(node.layoutSlot.x, node.layoutSlot.y));
            const ProjectiveTransform2D nodeTransform = CombineTransform(local, baseTransform);
            const double nodeOpacity = isCurrentPassRoot ? 1.0 : (baseOpacity * node.opacity);

            const std::uint32_t subEnd = subtreeEndOf(idx);
            if (!isCurrentPassRoot && node.opacity <= 0.0) {
                idx = subEnd;
                continue;
            }

            // 判断当前节点是否需要开启新的子离屏 Pass（跳过当前离屏 Pass 自身的根节点）
            bool takeOffscreen = false;
            if (!isCurrentPassRoot) {
                const std::uint32_t subCmds = subtreeCommandCount(idx);
                const bool subHasEffect = (subEnd > idx + 1U) &&
                    (recordEffectMaskPrefix_[subEnd] - recordEffectMaskPrefix_[idx + 1U] > 0U);
                const bool needs3D = subCmds > 0U && !Base::IsAffine(node.renderTransform) && subHasEffect;
                takeOffscreen = RenderPassPlanner::NeedsOffscreenFBO(
                    node,
                    frame.MaskAt(node.maskIndex),
                    frame.EffectAt(node.effectIndex),
                    subCmds,
                    needs3D);
            }

            if (!takeOffscreen) {
                const bool pushedClip = emitNodeCommands(node, nodeTransform, nodeOpacity);
                static_cast<void>(stack.PushBack(NodeState{node.id, nodeTransform, nodeOpacity, pushedClip}));
                ++idx;
                continue;
            }

            // 计算离屏 FBO 边距与尺寸
            double padLocalX = 0.0, padLocalY = 0.0;
            const RenderEffectSnapshot* nodeEffect = frame.EffectAt(node.effectIndex);
            if (nodeEffect != nullptr) {
                if (nodeEffect->kind == RenderEffectKind::DropShadow) {
                    const double rad = nodeEffect->direction * 0.017453292519943295;
                    const double blur = std::ceil(std::max(0.0, nodeEffect->radius) * 2.0);
                    padLocalX = blur + std::ceil(std::abs(std::cos(rad) * nodeEffect->depth));
                    padLocalY = blur + std::ceil(std::abs(-std::sin(rad) * nodeEffect->depth));
                } else if (nodeEffect->kind == RenderEffectKind::Blur ||
                           nodeEffect->kind == RenderEffectKind::DirectionalBlur) {
                    padLocalX = padLocalY = std::ceil(std::max(0.0, nodeEffect->radius) * 2.0);
                }
            }

            const std::uint32_t padPxX = static_cast<std::uint32_t>(std::ceil(padLocalX * dpi));
            const std::uint32_t padPxY = static_cast<std::uint32_t>(std::ceil(padLocalY * dpi));
            const std::uint32_t offW = std::max(1U, static_cast<std::uint32_t>(std::ceil(node.renderSize.width * dpi)) + padPxX * 2U);
            const std::uint32_t offH = std::max(1U, static_cast<std::uint32_t>(std::ceil(node.renderSize.height * dpi)) + padPxY * 2U);

            CommitBatches();
            RenderTarget* offscreen = GetOrCreateOffscreenTarget(node.id, offW, offH, false);
            if (offscreen == nullptr) {
                const bool pushedClip = emitNodeCommands(node, nodeTransform, nodeOpacity);
                static_cast<void>(stack.PushBack(NodeState{node.id, nodeTransform, nodeOpacity, pushedClip}));
                ++idx;
                continue;
            }

            // 保存父 Pass 的 Stencil 深度并切入子离屏 FBO
            const std::uint8_t savedClipDepth = clipDepth_;
            clipDepth_ = 0U;

            device_->SetRenderTarget(offscreen);
            device_->BeginOffscreenRender();
            device_->BeginTile(offscreen, Tile{0U, 0U, offW, offH});

            ProjectiveTransform2D dpiPad{};
            dpiPad.m11 = dpi;
            dpiPad.m22 = dpi;
            dpiPad.m31 = static_cast<double>(padPxX);
            dpiPad.m32 = static_cast<double>(padPxY);
            ProjectiveTransform2D offRoot = dpiPad;
            ProjectiveTransform2D compositeTransform = nodeTransform;
            if (node.hasViewboxTransform) {
                const ProjectiveTransform2D viewbox = Base::ToProjective(node.viewboxTransform);
                offRoot = CombineTransform(viewbox, dpiPad);
                ProjectiveTransform2D invViewbox{};
                if (Base::Invert(viewbox, invViewbox)) {
                    const ProjectiveTransform2D visualWithout = CombineTransform(invViewbox, node.renderTransform);
                    const ProjectiveTransform2D localWithout = CombineTransform(visualWithout, MakeTranslate(node.layoutSlot.x, node.layoutSlot.y));
                    compositeTransform = CombineTransform(localWithout, baseTransform);
                }
            }

            // 【递归录制离屏子树】：天然支持 2 层、3 层及任意深度离屏嵌套！
            self(self, idx, subEnd, *offscreen, offW, offH, offRoot, true);
            CommitBatches();
            device_->EndTile(offscreen);
            device_->EndOffscreenRender();

            // 若存在 OpacityMask，渲染 Mask FBO
            RenderTarget* maskTarget = nullptr;
            const RenderMaskSnapshot* nodeMask = frame.MaskAt(node.maskIndex);
            if (nodeMask != nullptr && nodeMask->kind != RenderMaskKind::None) {
                maskTarget = GetOrCreateOffscreenTarget(node.id, offW, offH, true);
                if (maskTarget != nullptr) {
                    device_->SetRenderTarget(maskTarget);
                    device_->BeginOffscreenRender();
                    device_->BeginTile(maskTarget, Tile{0U, 0U, offW, offH});
                    EmitMaskBrush(
                        *nodeMask,
                        static_cast<double>(padPxX),
                        static_cast<double>(padPxY),
                        node.renderSize.width * dpi,
                        node.renderSize.height * dpi,
                        frame);
                    CommitBatches();
                    device_->EndTile(maskTarget);
                    device_->EndOffscreenRender();
                }
            }

            // 恢复父 RenderTarget 与父级 Stencil 深度（BeginTile 不再清空父级 Stencil！）
            device_->SetRenderTarget(&currentTarget);
            device_->BeginTile(&currentTarget, Tile{0U, 0U, targetWidth, targetHeight});
            clipDepth_ = savedClipDepth;

            switch (node.blendMode) {
            case ::Aero::BlendMode::Multiply: currentBlendMode_ = BlendMode::SrcOver_Multiply; break;
            case ::Aero::BlendMode::Screen: currentBlendMode_ = BlendMode::SrcOver_Screen; break;
            case ::Aero::BlendMode::Additive: currentBlendMode_ = BlendMode::SrcOver_Additive; break;
            case ::Aero::BlendMode::Normal:
            default: currentBlendMode_ = BlendMode::SrcOver; break;
            }

            CompositeOffscreen(node, offscreen, maskTarget, compositeTransform, nodeOpacity, dpi, frame);
            CommitBatches();
            for (auto& entry : resources_.OffscreenTargets()) {
                if (entry.target.Get() == offscreen || (maskTarget != nullptr && entry.target.Get() == maskTarget)) {
                    entry.inUse = false;
                }
            }

            idx = subEnd;
        }

        while (!stack.Empty()) {
            popNodeClip(stack);
            static_cast<void>(stack.PopBack());
        }
    };

    // 启动顶层 Onscreen 树录制：
    renderSubtree(
        renderSubtree, 0U, nodeCount, target,
        frame.PixelWidth(), frame.PixelHeight(), rootTransform, false);

    CommitBatches();

    mappedIndices_ = nullptr;
    mappedVertices_ = nullptr;

    device_->EndTile(&target);
    device_->EndOnscreenRender();
    currentFrame_ = nullptr;

    return {};
}

} // namespace Aero::Render
