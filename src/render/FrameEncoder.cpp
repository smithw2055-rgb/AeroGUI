#include "FrameEncoder.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

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

inline uint32_t ColorToRGBA32(Color color, double opacity) noexcept {
    const float alpha = static_cast<float>(std::clamp(static_cast<double>(color.alpha) * opacity, 0.0, 1.0));
    const auto r = static_cast<uint8_t>(std::clamp(color.red * 255.0f + 0.5f, 0.0f, 255.0f));
    const auto g = static_cast<uint8_t>(std::clamp(color.green * 255.0f + 0.5f, 0.0f, 255.0f));
    const auto b = static_cast<uint8_t>(std::clamp(color.blue * 255.0f + 0.5f, 0.0f, 255.0f));
    const auto a = static_cast<uint8_t>(std::clamp(alpha * 255.0f + 0.5f, 0.0f, 255.0f));
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
}

inline Point TransformPoint(const Transform2D& t, double x, double y) noexcept {
    return Point{
        x * t.m11 + y * t.m21 + t.dx,
        x * t.m12 + y * t.m22 + t.dy
    };
}

Transform2D CombineTransform(const Transform2D& a, const Transform2D& b) noexcept {
    Transform2D result;
    result.m11 = a.m11 * b.m11 + a.m12 * b.m21;
    result.m12 = a.m11 * b.m12 + a.m12 * b.m22;
    result.m21 = a.m21 * b.m11 + a.m22 * b.m21;
    result.m22 = a.m21 * b.m12 + a.m22 * b.m22;
    result.dx = a.dx * b.m11 + a.dy * b.m21 + b.dx;
    result.dy = a.dx * b.m12 + a.dy * b.m22 + b.dy;
    return result;
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
    RenderNodeId nodeId, std::uint32_t width, std::uint32_t height) noexcept {
    for (auto& entry : offscreenTargets_) {
        if (entry.nodeId == nodeId) {
            if (entry.width != width || entry.height != height) {
                entry.target = device_->CreateRenderTarget(
                    "Offscreen", width, height, 1, false);
                entry.width = width;
                entry.height = height;
            }
            return entry.target.Get();
        }
    }
    Ref<RenderTarget> target = device_->CreateRenderTarget(
        "Offscreen", width, height, 1, false);
    if (!target) return nullptr;
    RenderTarget* result = target.Get();
    static_cast<void>(offscreenTargets_.PushBack(
        OffscreenTargetEntry{nodeId, std::move(target), width, height}));
    return result;
}

void UiFrameEncoder::ResetFrame() noexcept {
    currentVertexOffset_ = 0U;
    currentVertexCount_ = 0U;
    currentIndexCount_ = 0U;
    currentBatch_ = Batch{};
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

void UiFrameEncoder::ProcessCommand(
    const RenderCommand& cmd,
    const Transform2D& currentTransform,
    double currentOpacity) noexcept {
    switch (cmd.kind) {
    case RenderCommandKind::FillRect: {
        if (currentBatch_.shader != Shader::Path_Solid) {
            FlushBatch();
            currentBatch_.shader = Shader::Path_Solid;
            currentBatch_.renderState.f.blendMode = BlendMode::SrcOver;
            currentBatch_.renderState.f.colorEnable = 1;
        }

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
    case RenderCommandKind::FillRoundedRect: {
        if (currentBatch_.shader != Shader::Path_AA_Solid) {
            FlushBatch();
            currentBatch_.shader = Shader::Path_AA_Solid;
            currentBatch_.renderState.f.blendMode = BlendMode::SrcOver;
            currentBatch_.renderState.f.colorEnable = 1;
        }

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
    case RenderCommandKind::DrawImage: {
        Texture* tex = FindImage(cmd.image);
        if (tex != nullptr) {
            if (currentBatch_.shader != Shader::Path_Pattern || currentBatch_.image != tex) {
                FlushBatch();
                currentBatch_.shader = Shader::Path_Pattern;
                currentBatch_.image = tex;
                currentBatch_.imageSampler.f.wrapMode = WrapMode::ClampToEdge;
                currentBatch_.imageSampler.f.minmagFilter = MinMagFilter::Linear;
                currentBatch_.renderState.f.blendMode = BlendMode::SrcOver;
                currentBatch_.renderState.f.colorEnable = 1;
            }

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
        Texture* atlas = FindAtlas(0U);
        if (atlas != nullptr) {
            if (currentBatch_.shader != Shader::SDF_Solid || currentBatch_.glyphs != atlas) {
                FlushBatch();
                currentBatch_.shader = Shader::SDF_Solid;
                currentBatch_.glyphs = atlas;
                currentBatch_.glyphsSampler.f.wrapMode = WrapMode::ClampToEdge;
                currentBatch_.glyphsSampler.f.minmagFilter = MinMagFilter::Linear;
                currentBatch_.renderState.f.blendMode = BlendMode::SrcOver;
                currentBatch_.renderState.f.colorEnable = 1;
            }

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
    stats_.sourceCommandCount = frame.Commands().Size();

    device_->BeginOnscreenRender();
    device_->SetRenderTarget(&target);

    Tile tile;
    tile.x = 0;
    tile.y = 0;
    tile.width = frame.PixelWidth();
    tile.height = frame.PixelHeight();
    device_->BeginTile(&target, tile);

    mappedVertices_ = static_cast<uint8_t*>(device_->MapVertices(DYNAMIC_VB_SIZE));
    mappedIndices_ = static_cast<uint16_t*>(device_->MapIndices(DYNAMIC_IB_SIZE));

    Transform2D rootTransform;
    rootTransform.m11 = 1.0;
    rootTransform.m22 = 1.0;

    const auto commands = frame.Commands();
    for (const auto& cmd : commands) {
        ProcessCommand(cmd, rootTransform, 1.0);
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

    return {};
}

} // namespace Aero::Render
