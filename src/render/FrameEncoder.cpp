#include "DisplayList.hpp"
#include "FrameEncoder.hpp"
#include "render/RenderDeviceState.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Render {
using namespace Aero::Graphics;

// The frame encoder emits one device draw at a time through UiDrawContext.

namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

Media::Transform2D IdentityTransform() noexcept {
    return {};
}

Media::Transform2D Translation(double x, double y) noexcept {
    Media::Transform2D value;
    value.dx = x;
    value.dy = y;
    return value;
}

// Transforms use row-vector affine form: (x, y, 1) * M.
Media::Transform2D Compose(
    const Media::Transform2D& first,
    const Media::Transform2D& second) noexcept {
    Media::Transform2D output;
    output.m11 = first.m11 * second.m11 + first.m12 * second.m21;
    output.m12 = first.m11 * second.m12 + first.m12 * second.m22;
    output.m21 = first.m21 * second.m11 + first.m22 * second.m21;
    output.m22 = first.m21 * second.m12 + first.m22 * second.m22;
    output.dx = first.dx * second.m11 + first.dy * second.m21 + second.dx;
    output.dy = first.dx * second.m12 + first.dy * second.m22 + second.dy;
    return output;
}

bool InvertTransform(
    const Media::Transform2D& value,
    Media::Transform2D& inverse) noexcept {
    const double determinant =
        value.m11 * value.m22 - value.m12 * value.m21;
    if (!std::isfinite(determinant) ||
        std::fabs(determinant) < 1.0e-12) {
        return false;
    }
    inverse.m11 = value.m22 / determinant;
    inverse.m12 = -value.m12 / determinant;
    inverse.m21 = -value.m21 / determinant;
    inverse.m22 = value.m11 / determinant;
    inverse.dx = -(
        value.dx * inverse.m11 + value.dy * inverse.m21);
    inverse.dy = -(
        value.dx * inverse.m12 + value.dy * inverse.m22);
    return Base::IsFiniteTransform(inverse);
}

void TransformPoint(
    const Media::Transform2D& transform,
    double x,
    double y,
    double& outputX,
    double& outputY) noexcept {
    outputX = x * transform.m11 + y * transform.m21 + transform.dx;
    outputY = x * transform.m12 + y * transform.m22 + transform.dy;
}

Aero::Rect TransformBounds(
    const Media::Transform2D& transform,
    Aero::Rect rect) noexcept {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    double x3 = 0.0;
    double y3 = 0.0;
    TransformPoint(transform, rect.x, rect.y, x0, y0);
    TransformPoint(transform, rect.x + rect.width, rect.y, x1, y1);
    TransformPoint(transform, rect.x, rect.y + rect.height, x2, y2);
    TransformPoint(transform, rect.x + rect.width, rect.y + rect.height, x3, y3);
    const double minimumX = std::fmin(std::fmin(x0, x1), std::fmin(x2, x3));
    const double minimumY = std::fmin(std::fmin(y0, y1), std::fmin(y2, y3));
    const double maximumX = std::fmax(std::fmax(x0, x1), std::fmax(x2, x3));
    const double maximumY = std::fmax(std::fmax(y0, y1), std::fmax(y2, y3));
    return {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
}

Aero::Rect IntersectRect(Aero::Rect left, Aero::Rect right) noexcept {
    const double x0 = std::fmax(left.x, right.x);
    const double y0 = std::fmax(left.y, right.y);
    const double x1 = std::fmin(left.x + left.width, right.x + right.width);
    const double y1 = std::fmin(left.y + left.height, right.y + right.height);
    return {x0, y0, std::fmax(0.0, x1 - x0), std::fmax(0.0, y1 - y0)};
}

bool IsEmpty(Aero::Rect rect) noexcept {
    return rect.width <= 0.0 || rect.height <= 0.0;
}

Aero::Rect UnionRect(Aero::Rect left, Aero::Rect right) noexcept {
    if (left.width <= 0.0 || left.height <= 0.0) return right;
    if (right.width <= 0.0 || right.height <= 0.0) return left;
    const double x0 = std::fmin(left.x, right.x);
    const double y0 = std::fmin(left.y, right.y);
    const double x1 = std::fmax(
        left.x + left.width, right.x + right.width);
    const double y1 = std::fmax(
        left.y + left.height, right.y + right.height);
    return {x0, y0, x1 - x0, y1 - y0};
}

bool IsEmptyImageUv(Aero::Rect rect) noexcept {
    return rect.width == 0.0 || rect.height == 0.0;
}

bool IsValidImageUv(Aero::Rect value) noexcept {
    if (!Base::IsFiniteRect(value)) return false;
    const double endX = value.x + value.width;
    const double endY = value.y + value.height;
    return std::fmin(value.x, endX) >= 0.0 &&
        std::fmax(value.x, endX) <= 1.0 &&
        std::fmin(value.y, endY) >= 0.0 &&
        std::fmax(value.y, endY) <= 1.0;
}

bool FitsFloat(double value) noexcept {
    return std::isfinite(value) &&
        value >= -static_cast<double>(std::numeric_limits<float>::max()) &&
        value <= static_cast<double>(std::numeric_limits<float>::max());
}

constexpr std::uint32_t MaxShaderClips = 32U;
constexpr std::uint32_t MaxRectangleBatchInstances = 64U;

struct ShaderRectConstants  {
    float rects[MaxRectangleBatchInstances][4]{};
    float colors[MaxRectangleBatchInstances][4]{};
    float cornerRadii[MaxRectangleBatchInstances][4]{};
    float transform0[4]{};
    float transform1[4]{};
    float clipRect[MaxShaderClips][4]{};
    float clipInverse[MaxShaderClips][4]{};
    float clipTranslation[MaxShaderClips][4]{};
    std::uint32_t clipCount = 0U;
    std::uint32_t instanceMode = 0U;
    float strokeThickness = 0.0F;
    float padding = 0.0F;
};

struct ShaderImageConstants  {
    float rects[MaxRectangleBatchInstances][4]{};
    float sourceUvs[MaxRectangleBatchInstances][4]{};
    float tints[MaxRectangleBatchInstances][4]{};
    float transform0[4]{};
    float transform1[4]{};
    float clipRect[MaxShaderClips][4]{};
    float clipInverse[MaxShaderClips][4]{};
    float clipTranslation[MaxShaderClips][4]{};
    std::uint32_t clipCount = 0U;
    float padding[3]{};
};

struct ShaderMaskConstants  {
    float rect[4]{};
    float transform0[4]{};
    float transform1[4]{};
    float mask0[4]{};
    float mask1[4]{};
    float geometry0[4]{};
    float geometry1[4]{};
    float geometry2[4]{};
    float relativeInverse0[4]{};
    float relativeInverse1[4]{};
};

struct ShaderEffectConstants {
    float viewport[4]{};
    float filter0[4]{};
    float filter1[4]{};
    float tint[4]{1.0F, 1.0F, 1.0F, 1.0F};
};

struct ShaderMeshConstants  {
    float tints[MaxRectangleBatchInstances][4]{};
    float transform0[4]{};
    float transform1[4]{};
    float clipRect[MaxShaderClips][4]{};
    float clipInverse[MaxShaderClips][4]{};
    float clipTranslation[MaxShaderClips][4]{};
    std::uint32_t clipCount = 0U;
    float padding[3]{};
};

struct ShaderGlyphConstants  {
    float tints[MaxRectangleBatchInstances][4]{};
    float transform0[4]{};
    float transform1[4]{};
    float clipRect[MaxShaderClips][4]{};
    float clipInverse[MaxShaderClips][4]{};
    float clipTranslation[MaxShaderClips][4]{};
    std::uint32_t clipCount = 0U;
    float padding[3]{};
};

static_assert(sizeof(ShaderRectConstants) % 16U == 0U,
    "Renderer constant buffers must be float4 aligned");
static_assert(sizeof(ShaderRectConstants) <= 64U * 1024U,
    "Renderer constant buffers must not exceed 64 KiB");
static_assert(sizeof(ShaderImageConstants) % 16U == 0U,
    "Renderer constant buffers must be float4 aligned");
static_assert(sizeof(ShaderImageConstants) <= 64U * 1024U,
    "Renderer constant buffers must not exceed 64 KiB");
static_assert(sizeof(ShaderMaskConstants) % 16U == 0U,
    "Renderer mask constants must be float4 aligned");
static_assert(sizeof(ShaderEffectConstants) % 16U == 0U,
    "Renderer effect constants must be float4 aligned");
static_assert(sizeof(ShaderMeshConstants) % 16U == 0U,
    "Renderer constant buffers must be float4 aligned");
static_assert(sizeof(ShaderGlyphConstants) % 16U == 0U,
    "Renderer constant buffers must be float4 aligned");
static_assert(sizeof(ShaderGlyphConstants) <= 64U * 1024U,
    "Renderer constant buffers must not exceed 64 KiB");

Base::Result<void> PushClipState(
    Base::Vector<ClipState>& clips,
    Aero::Rect rect,
    const Media::Transform2D& transform) noexcept {
    if (clips.Size() >= MaxShaderClips) {
        return Unsupported("Renderer clip nesting exceeds shader capacity");
    }
    const double determinant = transform.m11 * transform.m22 -
        transform.m12 * transform.m21;
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-12) {
        return Unsupported("Renderer cannot clip through a singular transform");
    }
    Aero::Rect bounds = TransformBounds(transform, rect);
    if (!Aero::IsValidLayoutRect(bounds)) {
        return InvalidArgument("Renderer clip bounds are invalid");
    }
    if (!clips.Empty()) {
        bounds = IntersectRect(clips[clips.Size() - 1U].bounds, bounds);
    }
    return clips.PushBack({rect, transform, bounds});
}

template <typename Constants>
Base::Result<void> AppendDraw(
    ::Aero::Render::UiDrawContext& encoder,
    ResourceHandle uniformBuffer,
    const Constants& constants,
    Aero::Rect scissor,
    std::uint32_t instanceCount = 1U) noexcept {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&constants);
    Base::Result<void> uploaded = encoder.UploadBuffer(
        uniformBuffer, 0U, {bytes, static_cast<std::uint32_t>(sizeof(constants))});
    if (!uploaded) {
        return uploaded;
    }

    Base::Result<void> encoded = encoder.SetScissor(scissor);
    if (encoded) encoded = encoder.Draw(4U, instanceCount);
    return encoded;
}

struct FrameNodeGeometry {
    Render::RenderNodeId id = Render::InvalidRenderNodeId;
    Render::RenderNodeId parentId = Render::InvalidRenderNodeId;
    Media::Transform2D transform;
    Aero::Rect bounds;
};

bool RequiresNodeSurface(
    const Render::RenderNodeSnapshot& node) noexcept {
    return node.effect.kind != Render::RenderEffectKind::None ||
        node.mask.kind != Render::RenderMaskKind::None;
}

bool RequiresFrameSurface(
    const ::Aero::Render::RenderFrame& frame) noexcept {
    for (const Render::RenderNodeSnapshot& node : frame.Nodes()) {
        if (RequiresNodeSurface(node)) return true;
    }
    return false;
}

void AddStatistics(
    BatchStatistics& target,
    const BatchStatistics& source) noexcept {
    target.renderPassCount += source.renderPassCount;
    target.drawCallCount += source.drawCallCount;
    target.rectangleInstanceCount += source.rectangleInstanceCount;
    target.imageInstanceCount += source.imageInstanceCount;
    target.meshDrawCallCount += source.meshDrawCallCount;
    target.meshInstanceCount += source.meshInstanceCount;
    target.glyphDrawCallCount += source.glyphDrawCallCount;
    target.glyphInstanceCount += source.glyphInstanceCount;
    target.uniformBufferUploadCount += source.uniformBufferUploadCount;
    target.pipelineBindingCount += source.pipelineBindingCount;
    target.vertexBufferBindingCount += source.vertexBufferBindingCount;
    target.indexBufferBindingCount += source.indexBufferBindingCount;
    target.uniformBufferBindingCount += source.uniformBufferBindingCount;
    target.textureSamplerBindingCount += source.textureSamplerBindingCount;
    target.sourceCommandCount += source.sourceCommandCount;
    target.drawPacketCount += source.drawPacketCount;
    target.batchCount += source.batchCount;
    target.mergedPacketCount += source.mergedPacketCount;
    target.barrierCount += source.barrierCount;
    target.batchingEnabled = target.batchingEnabled && source.batchingEnabled;
}

} // namespace

UiFrameEncoder::UiFrameEncoder(
    Aero::RenderDevice::Access& device,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      state_(allocator_) {}

UiFrameEncoder::~UiFrameEncoder() noexcept {
    Shutdown();
}

Base::Result<void> UiFrameEncoder::Initialize() noexcept {
    if (state_.initialized) {
        return {};
    }
    if (device_ == nullptr || device_->IsNativeDeviceLost()) {
        return NotInitialized("Renderer requires a ready graphics device");
    }
    BufferDescriptor vertexDescriptor;
    vertexDescriptor.sizeBytes = 32U;
    vertexDescriptor.usage = BufferUsage::Vertex;
    Base::Result<ResourceHandle> vertex = device_->CreateBuffer(vertexDescriptor);
    if (!vertex) {
        Shutdown();
        return vertex.GetStatus();
    }
    state_.vertexBuffer = vertex.Value();

    SamplerDescriptor effectSamplerDescriptor;
    effectSamplerDescriptor.minFilter =
        FilterMode::Linear;
    effectSamplerDescriptor.magFilter =
        FilterMode::Linear;
    effectSamplerDescriptor.mipFilter =
        FilterMode::Linear;
    effectSamplerDescriptor.addressU =
        AddressMode::ClampToEdge;
    effectSamplerDescriptor.addressV =
        AddressMode::ClampToEdge;
    Base::Result<ResourceHandle> effectSampler =
        device_->CreateSampler(
            effectSamplerDescriptor);
    if (!effectSampler) {
        Shutdown();
        return effectSampler.GetStatus();
    }
    state_.effectSampler =
        effectSampler.Value();

    BufferDescriptor uniformDescriptor;
    uniformDescriptor.sizeBytes = sizeof(ShaderRectConstants);
    uniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> uniform = device_->CreateBuffer(uniformDescriptor);
    if (!uniform) {
        Shutdown();
        return uniform.GetStatus();
    }
    state_.uniformBuffer = uniform.Value();

    BufferDescriptor imageUniformDescriptor;
    imageUniformDescriptor.sizeBytes = sizeof(ShaderImageConstants);
    imageUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> imageUniform =
        device_->CreateBuffer(imageUniformDescriptor);
    if (!imageUniform) {
        Shutdown();
        return imageUniform.GetStatus();
    }
    state_.imageUniformBuffer = imageUniform.Value();

    BufferDescriptor maskUniformDescriptor;
    maskUniformDescriptor.sizeBytes = sizeof(ShaderMaskConstants);
    maskUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> maskUniform =
        device_->CreateBuffer(maskUniformDescriptor);
    if (!maskUniform) {
        Shutdown();
        return maskUniform.GetStatus();
    }
    state_.maskUniformBuffer = maskUniform.Value();

    BufferDescriptor effectUniformDescriptor;
    effectUniformDescriptor.sizeBytes = sizeof(ShaderEffectConstants);
    effectUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> effectUniform =
        device_->CreateBuffer(effectUniformDescriptor);
    if (!effectUniform) {
        Shutdown();
        return effectUniform.GetStatus();
    }
    state_.effectUniformBuffer = effectUniform.Value();

    BufferDescriptor meshUniformDescriptor;
    meshUniformDescriptor.sizeBytes = sizeof(ShaderMeshConstants);
    meshUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> meshUniform =
        device_->CreateBuffer(meshUniformDescriptor);
    if (!meshUniform) {
        Shutdown();
        return meshUniform.GetStatus();
    }
    state_.meshUniformBuffer = meshUniform.Value();

    BufferDescriptor glyphUniformDescriptor;
    glyphUniformDescriptor.sizeBytes = sizeof(ShaderGlyphConstants);
    glyphUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> glyphUniform =
        device_->CreateBuffer(glyphUniformDescriptor);
    if (!glyphUniform) {
        Shutdown();
        return glyphUniform.GetStatus();
    }
    state_.glyphUniformBuffer = glyphUniform.Value();

    state_.initialized = true;
    return {};
}

void UiFrameEncoder::Shutdown() noexcept {
    const FenceValue retireFence = device_ != nullptr
        ? device_->LastSubmittedFence()
        : 0U;
    if (device_ != nullptr) {
        for (const ViewSurface& surface :
             state_.viewSurfaces) {
            if (surface.target.IsValid()) {
                static_cast<void>(
                    device_->DestroyResource(
                        surface.target,
                        retireFence));
            }
        }
        for (const EffectSurface& surface :
             state_.effectSurfaces) {
            const ResourceHandle resources[] = {
                surface.content,
                surface.scratch,
                surface.result};
            for (ResourceHandle resource : resources) {
                if (resource.IsValid()) {
                    static_cast<void>(
                        device_->DestroyResource(
                            resource,
                            retireFence));
                }
            }
        }
        for (const GradientRampBinding& ramp :
             state_.gradientRamps) {
            if (ramp.texture.IsValid()) {
                static_cast<void>(
                    device_->DestroyResource(
                        ramp.texture,
                        retireFence));
            }
        }
        if (state_.effectSampler.IsValid()) {
            static_cast<void>(
                device_->DestroyResource(
                    state_.effectSampler,
                    retireFence));
        }
        if (state_.effectUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                state_.effectUniformBuffer, retireFence));
        }
        if (state_.maskUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                state_.maskUniformBuffer, retireFence));
        }
        if (state_.glyphUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                state_.glyphUniformBuffer, retireFence));
        }
        if (state_.meshUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                state_.meshUniformBuffer, retireFence));
        }
        if (state_.imageUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                state_.imageUniformBuffer, retireFence));
        }
        if (state_.uniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(state_.uniformBuffer, retireFence));
        }
        if (state_.vertexBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(state_.vertexBuffer, retireFence));
        }
    }
    state_.vertexBuffer = {};
    state_.uniformBuffer = {};
    state_.imageUniformBuffer = {};
    state_.maskUniformBuffer = {};
    state_.effectUniformBuffer = {};
    state_.meshUniformBuffer = {};
    state_.glyphUniformBuffer = {};
    state_.effectSampler = {};
    state_.nodes.Clear();
    state_.transforms.Clear();
    state_.clips.Clear();
    state_.opacities.Clear();
    state_.nodePath.Clear();
    state_.images.Clear();
    state_.gradientRamps.Clear();
    state_.meshes.Clear();
    state_.glyphRuns.Clear();
    state_.effectSurfaces.Clear();
    state_.viewSurfaces.Clear();
    state_.lastStatistics = {};
    state_.initialized = false;
}

Base::Result<void> UiFrameEncoder::RegisterImage(
    Render::RenderImageId image,
    ResourceHandle texture,
    ResourceHandle sampler) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    if (image == Render::InvalidRenderImageId ||
        texture.type != ResourceType::Texture ||
        sampler.type != ResourceType::Sampler || !device_->IsAlive(texture) ||
        !device_->IsAlive(sampler)) {
        return InvalidArgument(
            "Renderer image registration requires live texture and sampler resources");
    }
    for (const ImageBinding& binding : state_.images) {
        if (binding.id == image) {
            return InvalidState("Renderer image ID is already registered");
        }
    }
    return state_.images.PushBack({image, texture, sampler});
}

Base::Result<void> UiFrameEncoder::UnregisterImage(
    Render::RenderImageId image) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    for (std::uint32_t index = 0U; index < state_.images.Size(); ++index) {
        if (state_.images[index].id == image) {
            for (std::uint32_t next = index + 1U;
                 next < state_.images.Size(); ++next) {
                state_.images[next - 1U] = state_.images[next];
            }
            state_.images.PopBack();
            return {};
        }
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Renderer image ID is not registered");
}

Base::Result<void> UiFrameEncoder::RegisterMesh(
    Render::RenderMeshId mesh,
    ResourceHandle vertexBuffer,
    ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    IndexType indexType) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    if (mesh == Render::InvalidRenderMeshId || indexCount == 0U ||
        vertexBuffer.type != ResourceType::Buffer ||
        indexBuffer.type != ResourceType::Buffer || !device_->IsAlive(vertexBuffer) ||
        !device_->IsAlive(indexBuffer)) {
        return InvalidArgument(
            "Renderer mesh registration requires live vertex and index buffers");
    }
    for (const MeshBinding& binding : state_.meshes) {
        if (binding.id == mesh) {
            return InvalidState("Renderer mesh ID is already registered");
        }
    }
    return state_.meshes.PushBack(
        {mesh, vertexBuffer, indexBuffer, indexCount, indexType});
}

Base::Result<void> UiFrameEncoder::UnregisterMesh(
    Render::RenderMeshId mesh) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    for (std::uint32_t index = 0U; index < state_.meshes.Size(); ++index) {
        if (state_.meshes[index].id == mesh) {
            for (std::uint32_t next = index + 1U;
                 next < state_.meshes.Size(); ++next) {
                state_.meshes[next - 1U] = state_.meshes[next];
            }
            state_.meshes.PopBack();
            return {};
        }
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Renderer mesh ID is not registered");
}

Base::Result<void> UiFrameEncoder::RegisterGlyphRun(
    Render::RenderGlyphRunId glyphRun,
    ResourceHandle vertexBuffer,
    ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    ResourceHandle atlasTexture,
    ResourceHandle sampler,
    IndexType indexType) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    if (glyphRun == Render::InvalidRenderGlyphRunId || indexCount == 0U ||
        vertexBuffer.type != ResourceType::Buffer ||
        indexBuffer.type != ResourceType::Buffer ||
        atlasTexture.type != ResourceType::Texture ||
        sampler.type != ResourceType::Sampler || !device_->IsAlive(vertexBuffer) ||
        !device_->IsAlive(indexBuffer) || !device_->IsAlive(atlasTexture) ||
        !device_->IsAlive(sampler)) {
        return InvalidArgument(
            "Renderer glyph registration requires live buffers, atlas, and sampler");
    }
    for (const GlyphBinding& binding : state_.glyphRuns) {
        if (binding.id == glyphRun) {
            return InvalidState("Renderer glyph ID is already registered");
        }
    }
    return state_.glyphRuns.PushBack(
        {glyphRun, vertexBuffer, indexBuffer, indexCount, atlasTexture, sampler,
            indexType});
}

Base::Result<void> UiFrameEncoder::UnregisterGlyphRun(
    Render::RenderGlyphRunId glyphRun) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    for (std::uint32_t index = 0U; index < state_.glyphRuns.Size(); ++index) {
        if (state_.glyphRuns[index].id == glyphRun) {
            for (std::uint32_t next = index + 1U;
                 next < state_.glyphRuns.Size(); ++next) {
                state_.glyphRuns[next - 1U] = state_.glyphRuns[next];
            }
            state_.glyphRuns.PopBack();
            return {};
        }
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Renderer glyph ID is not registered");
}

bool UiFrameEncoder::IsInitialized() const noexcept {
    return state_.initialized;
}

BatchStatistics
UiFrameEncoder::LastStatistics() const noexcept {
    return state_.initialized
        ? state_.lastStatistics
        : BatchStatistics{};
}

void UiFrameEncoder::SetBatchingEnabled(
    bool enabled) noexcept {
    state_.batchingEnabled = enabled;
}

bool UiFrameEncoder::IsBatchingEnabled() const noexcept {
    return state_.batchingEnabled;
}

Base::Result<FenceValue> UiFrameEncoder::RecordOffscreen(
    const ::Aero::Render::RenderFrame& plan) noexcept {
    if (!IsInitialized()) {
        return NotInitialized(
            "Offscreen rendering requires an initialized batch composer");
    }
    ViewSurface* surface = state_.viewSurfaces.Empty()
        ? nullptr : &state_.viewSurfaces[0U];
    if (!RequiresFrameSurface(plan)) {
        if (surface != nullptr) {
            surface->version = plan.Version();
            surface->prepared = false;
            surface->statistics = {};
        }
        ::Aero::Render::UiDrawContext empty(*device_, allocator_);
        return empty.Finish();
    }

    const std::uint32_t width = plan.PixelWidth();
    const std::uint32_t height = plan.PixelHeight();
    if (width == 0U || height == 0U) {
        if (surface != nullptr) {
            surface->version = plan.Version();
            surface->prepared = false;
            surface->statistics = {};
        }
        ::Aero::Render::UiDrawContext empty(*device_, allocator_);
        return empty.Finish();
    }
    if (width > device_->Capabilities().maxTextureDimension ||
        height > device_->Capabilities().maxTextureDimension) {
        return InvalidArgument(
            "Offscreen View dimensions exceed device limits");
    }
    if (surface == nullptr) {
        Base::Result<ViewSurface*> added =
            state_.viewSurfaces.EmplaceBack();
        if (!added) return added.GetStatus();
        surface = added.Value();
    }
    if (surface->target.IsValid() &&
        (surface->width != width || surface->height != height ||
         !device_->IsAlive(surface->target))) {
        static_cast<void>(device_->DestroyResource(
            surface->target, device_->LastSubmittedFence()));
        surface->target = {};
    }
    if (!surface->target.IsValid()) {
        TextureResourceDescriptor descriptor;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.format = GraphicsTextureFormat::Bgra8Unorm;
        descriptor.usage =
            TextureUsageBit(TextureUsage::Sampled) |
            TextureUsageBit(TextureUsage::RenderTarget);
        Base::Result<ResourceHandle> created =
            device_->CreateRenderTarget(descriptor);
        if (!created) return created.GetStatus();
        surface->target = created.Value();
        surface->width = width;
        surface->height = height;
    }
    Base::Result<FenceValue> recorded =
        Record(plan, {
            surface->target, width, height,
            LoadOperation::Clear});
    if (!recorded) return recorded.GetStatus();
    surface->version = plan.Version();
    surface->statistics = state_.lastStatistics;
    surface->prepared = true;
    return std::move(recorded).Value();
}

Base::Result<FenceValue> UiFrameEncoder::RecordOnscreen(
    const ::Aero::Render::RenderFrame& plan,
    const FrameTarget& target) noexcept {
    if (!RequiresFrameSurface(plan)) {
        return Record(plan, target);
    }
    ViewSurface* surface = state_.viewSurfaces.Empty()
        ? nullptr : &state_.viewSurfaces[0U];
    if (surface == nullptr || !surface->prepared ||
        surface->version != plan.Version() ||
        !surface->target.IsValid() ||
        !device_->IsAlive(surface->target)) {
        return InvalidState(
            "RenderOffscreen must prepare the current View before Render");
    }
    if (!target.color.IsValid() || !device_->IsAlive(target.color) ||
        target.width == 0U || target.height == 0U) {
        return InvalidArgument("Onscreen render target is invalid");
    }

    ::Aero::Render::UiDrawContext encoder(*device_, allocator_);
    static constexpr float UnitQuad[] = {
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 1.0F, 1.0F, 1.0F};
    const auto* bytes =
        reinterpret_cast<const std::uint8_t*>(UnitQuad);
    Base::Result<void> encoded = encoder.UploadBuffer(
        state_.vertexBuffer, 0U,
        {bytes, static_cast<std::uint32_t>(sizeof(UnitQuad))});
    const std::uint32_t compositeWidth =
        (std::min)(surface->width, target.width);
    const std::uint32_t compositeHeight =
        (std::min)(surface->height, target.height);
    if (compositeWidth == 0U || compositeHeight == 0U) {
        ::Aero::Render::UiDrawContext empty(*device_, allocator_);
        return empty.Finish();
    }
    RenderPassDescriptor pass;
    pass.renderArea = {0.0, 0.0,
        static_cast<double>(compositeWidth),
        static_cast<double>(compositeHeight)};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0].target = target.color;
    pass.colorAttachments[0].load = target.load;
    pass.colorAttachments[0].store = StoreOperation::Store;
    pass.colorAttachments[0].clearColor = {0.0F, 0.0F, 0.0F, 0.0F};
    if (encoded) encoded = encoder.BeginRenderPass(pass);
    if (encoded) encoded = encoder.BindPipeline(
        {UiShader::Image, UiBlendMode::Normal});
    if (encoded) encoded = encoder.BindVertexBuffer(0U, state_.vertexBuffer);
    if (encoded) encoded = encoder.BindUniformBuffer(
        0U, state_.imageUniformBuffer, 0U,
        static_cast<std::uint32_t>(sizeof(ShaderImageConstants)));
    if (encoded) encoded = encoder.BindTextureSampler(
        0U, surface->target, state_.effectSampler);
    ShaderImageConstants constants;
    constants.rects[0][2] = static_cast<float>(compositeWidth);
    constants.rects[0][3] = static_cast<float>(compositeHeight);
    constants.sourceUvs[0][2] =
        static_cast<float>(compositeWidth) /
        static_cast<float>(surface->width);
    constants.sourceUvs[0][3] =
        static_cast<float>(compositeHeight) /
        static_cast<float>(surface->height);
    constants.tints[0][0] = 1.0F;
    constants.tints[0][1] = 1.0F;
    constants.tints[0][2] = 1.0F;
    constants.tints[0][3] = 1.0F;
    constants.transform0[0] = 1.0F;
    constants.transform0[3] = 1.0F;
    constants.transform1[2] = static_cast<float>(target.width);
    constants.transform1[3] = static_cast<float>(target.height);
    constants.clipCount = 1U;
    constants.clipRect[0][2] = static_cast<float>(compositeWidth);
    constants.clipRect[0][3] = static_cast<float>(compositeHeight);
    constants.clipInverse[0][0] = 1.0F;
    constants.clipInverse[0][3] = 1.0F;
    if (encoded) encoded = AppendDraw(
        encoder, state_.imageUniformBuffer, constants,
        pass.renderArea, 1U);
    if (encoded) encoded = encoder.EndRenderPass();
    Base::Result<FenceValue> finished = encoded
        ? encoder.Finish()
        : Base::Result<FenceValue>(encoded.GetStatus());
    if (!finished) return finished.GetStatus();

    BatchStatistics composite;
    composite.renderPassCount = 1U;
    composite.drawCallCount = 1U;
    composite.imageInstanceCount = 1U;
    composite.uniformBufferUploadCount = 1U;
    composite.pipelineBindingCount = 1U;
    composite.vertexBufferBindingCount = 1U;
    composite.uniformBufferBindingCount = 1U;
    composite.textureSamplerBindingCount = 1U;
    state_.lastStatistics = surface->statistics;
    AddStatistics(state_.lastStatistics, composite);
    return std::move(finished).Value();
}

Base::Result<FenceValue> UiFrameEncoder::Record(
    const ::Aero::Render::RenderFrame& plan,
    const FrameTarget& target) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer is not initialized");
    }
    if (device_->IsNativeDeviceLost()) {
        return InvalidState("Cannot record a RenderFrame for a lost graphics device");
    }
    if (!target.color.IsValid() ||
        (target.color.type != ResourceType::RenderTarget &&
         target.color.type != ResourceType::Texture) ||
        !device_->IsAlive(target.color) ||
        target.width == 0U || target.height == 0U) {
        return InvalidArgument("Renderer target is invalid or unavailable");
    }

    const std::uint32_t width = plan.PixelWidth() != 0U
        ? plan.PixelWidth()
        : target.width;
    const std::uint32_t height = plan.PixelHeight() != 0U
        ? plan.PixelHeight()
        : target.height;
    const std::uint32_t renderWidth =
        (std::min)(width, target.width);
    const std::uint32_t renderHeight =
        (std::min)(height, target.height);
    const Aero::Size logicalSize = plan.LogicalSize();
    const double logicalWidth = logicalSize.width > 0.0
        ? logicalSize.width
        : static_cast<double>(width);
    const double logicalHeight = logicalSize.height > 0.0
        ? logicalSize.height
        : static_cast<double>(height);
    const double pixelScaleX =
        static_cast<double>(width) / logicalWidth;
    const double pixelScaleY =
        static_cast<double>(height) / logicalHeight;
    const Aero::Rect frameClip = {
        0.0, 0.0,
        logicalWidth * static_cast<double>(renderWidth) /
            static_cast<double>(width),
        logicalHeight * static_cast<double>(renderHeight) /
            static_cast<double>(height)};
    Base::Vector<FrameNodeGeometry> frameGeometry(allocator_);
    Base::Result<void> geometryReserved =
        frameGeometry.Reserve(plan.Nodes().Size());
    if (!geometryReserved) return geometryReserved.GetStatus();
    for (const Render::RenderNodeSnapshot& node : plan.Nodes()) {
        Media::Transform2D parentTransform = IdentityTransform();
        if (node.parentId != Render::InvalidRenderNodeId) {
            const FrameNodeGeometry* parent = nullptr;
            for (const FrameNodeGeometry& candidate : frameGeometry) {
                if (candidate.id == node.parentId) {
                    parent = &candidate;
                    break;
                }
            }
            if (parent == nullptr) {
                return InvalidState(
                    "Renderer node parent geometry is unavailable");
            }
            parentTransform = parent->transform;
        }
        const Media::Transform2D transform = Compose(
            Compose(
                node.renderTransform,
                Translation(node.layoutSlot.x, node.layoutSlot.y)),
            parentTransform);
        Base::Result<void> appended = frameGeometry.PushBack({
            node.id,
            node.parentId,
            transform,
            TransformBounds(
                transform,
                {0.0, 0.0,
                 node.renderSize.width,
                 node.renderSize.height})});
        if (!appended) return appended.GetStatus();
    }
    std::uint32_t effectCount = 0U;
    for (const Render::RenderNodeSnapshot& node :
         plan.Nodes()) {
        if (RequiresNodeSurface(node)) {
            ++effectCount;
        }
    }
    for (std::uint32_t index = effectCount;
         index < state_.effectSurfaces.Size();
         ++index) {
        EffectSurface& surface = state_.effectSurfaces[index];
        const ResourceHandle resources[] = {
            surface.content, surface.scratch, surface.result};
        for (ResourceHandle resource : resources) {
            if (resource.IsValid()) {
                static_cast<void>(device_->DestroyResource(
                    resource, device_->LastSubmittedFence()));
            }
        }
    }
    Base::Result<void> resizedEffects =
        state_.effectSurfaces.Resize(
            effectCount);
    if (!resizedEffects) {
        return resizedEffects.GetStatus();
    }
    std::uint32_t surfaceOrdinal = 0U;
    for (const Render::RenderNodeSnapshot& node : plan.Nodes()) {
        if (!RequiresNodeSurface(node)) continue;
        EffectSurface& surface = state_.effectSurfaces[surfaceOrdinal++];
        Aero::Rect subtreeBounds;
        bool hasBounds = false;
        for (const FrameNodeGeometry& candidate : frameGeometry) {
            const FrameNodeGeometry* cursor = &candidate;
            bool belongs = false;
            while (cursor != nullptr) {
                if (cursor->id == node.id) {
                    belongs = true;
                    break;
                }
                if (cursor->parentId == Render::InvalidRenderNodeId) break;
                const FrameNodeGeometry* parent = nullptr;
                for (const FrameNodeGeometry& possible : frameGeometry) {
                    if (possible.id == cursor->parentId) {
                        parent = &possible;
                        break;
                    }
                }
                cursor = parent;
            }
            if (!belongs) continue;
            Aero::Rect candidateBounds = candidate.bounds;
            for (const Render::RenderNodeSnapshot& candidateNode :
                 plan.Nodes()) {
                if (candidateNode.id != candidate.id) continue;
                if (candidateNode.effect.kind !=
                        Render::RenderEffectKind::None) {
                    const double padding =
                        std::fmin(candidateNode.effect.radius, 50.0) +
                        (candidateNode.effect.kind ==
                             Render::RenderEffectKind::DropShadow
                         ? candidateNode.effect.depth
                         : 0.0);
                    candidateBounds.x -= padding;
                    candidateBounds.y -= padding;
                    candidateBounds.width += padding * 2.0;
                    candidateBounds.height += padding * 2.0;
                }
                break;
            }
            subtreeBounds = hasBounds
                ? UnionRect(subtreeBounds, candidateBounds)
                : candidateBounds;
            hasBounds = hasBounds || !IsEmpty(candidateBounds);
        }
        subtreeBounds = hasBounds
            ? IntersectRect(subtreeBounds, frameClip)
            : Aero::Rect{};
        const bool emptySurface = IsEmpty(subtreeBounds);
        const double pixelX0 = emptySurface
            ? 0.0
            : std::clamp(
                std::floor(subtreeBounds.x * pixelScaleX),
                0.0,
                static_cast<double>(renderWidth));
        const double pixelY0 = emptySurface
            ? 0.0
            : std::clamp(
                std::floor(subtreeBounds.y * pixelScaleY),
                0.0,
                static_cast<double>(renderHeight));
        const double pixelX1 = emptySurface
            ? 1.0
            : std::clamp(
                std::ceil(
                    (subtreeBounds.x + subtreeBounds.width) * pixelScaleX),
                pixelX0,
                static_cast<double>(renderWidth));
        const double pixelY1 = emptySurface
            ? 1.0
            : std::clamp(
                std::ceil(
                    (subtreeBounds.y + subtreeBounds.height) * pixelScaleY),
                pixelY0,
                static_cast<double>(renderHeight));
        const std::uint32_t surfaceWidth = (std::max)(
            1U,
            static_cast<std::uint32_t>(pixelX1 - pixelX0));
        const std::uint32_t surfaceHeight = (std::max)(
            1U,
            static_cast<std::uint32_t>(pixelY1 - pixelY0));
        const Aero::Rect alignedBounds = {
            pixelX0 / pixelScaleX,
            pixelY0 / pixelScaleY,
            static_cast<double>(surfaceWidth) / pixelScaleX,
            static_cast<double>(surfaceHeight) / pixelScaleY};
        const bool reusable =
            surface.width == surfaceWidth &&
            surface.height == surfaceHeight &&
            surface.content.IsValid() && surface.scratch.IsValid() &&
            surface.result.IsValid() &&
            device_->IsAlive(surface.content) &&
            device_->IsAlive(surface.scratch) &&
            device_->IsAlive(surface.result);
        if (!reusable) {
            const ResourceHandle resources[] = {
                surface.content, surface.scratch, surface.result};
            for (ResourceHandle resource : resources) {
                if (resource.IsValid()) {
                    static_cast<void>(device_->DestroyResource(
                        resource, device_->LastSubmittedFence()));
                }
            }
            surface = {};
        }
        if (!surface.content.IsValid()) {
            TextureResourceDescriptor descriptor;
            descriptor.width = surfaceWidth;
            descriptor.height = surfaceHeight;
            descriptor.format =
                GraphicsTextureFormat::Bgra8Unorm;
            descriptor.usage =
                TextureUsageBit(
                    TextureUsage::Sampled) |
                TextureUsageBit(
                    TextureUsage::RenderTarget);
            Base::Result<ResourceHandle> content =
                device_->CreateRenderTarget(descriptor);
            Base::Result<ResourceHandle> scratch = content
                ? device_->CreateRenderTarget(descriptor)
                : Base::Result<ResourceHandle>(content.GetStatus());
            Base::Result<ResourceHandle> result = scratch
                ? device_->CreateRenderTarget(descriptor)
                : Base::Result<ResourceHandle>(scratch.GetStatus());
            if (!content || !scratch || !result) {
                if (content) static_cast<void>(device_->DestroyResource(
                    content.Value(), device_->LastSubmittedFence()));
                if (scratch) static_cast<void>(device_->DestroyResource(
                    scratch.Value(), device_->LastSubmittedFence()));
                return !content
                    ? content.GetStatus()
                    : !scratch
                    ? scratch.GetStatus()
                    : result.GetStatus();
            }
            surface.content = content.Value();
            surface.scratch = scratch.Value();
            surface.result = result.Value();
            surface.width = surfaceWidth;
            surface.height = surfaceHeight;
        }
        surface.owner = node.id;
        surface.logicalBounds = alignedBounds;
        surface.empty = emptySurface;
    }

    ::Aero::Render::UiDrawContext encoder(*device_, allocator_);
    for (std::uint32_t index = 0U;
         index < state_.gradientRamps.Size();) {
        const GradientRampBinding& cached =
            state_.gradientRamps[index];
        bool used = false;
        for (const Render::RenderGradientRampSnapshot& ramp :
             plan.GradientRamps()) {
            if (ramp.brushIdentity == cached.key) {
                used = true;
                break;
            }
        }
        if (used) {
            ++index;
            continue;
        }
        if (cached.texture.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                cached.texture, device_->LastSubmittedFence()));
        }
        for (std::uint32_t next = index + 1U;
             next < state_.gradientRamps.Size(); ++next) {
            state_.gradientRamps[next - 1U] =
                std::move(state_.gradientRamps[next]);
        }
        state_.gradientRamps.PopBack();
    }
    for (const Render::RenderGradientRampSnapshot& ramp :
         plan.GradientRamps()) {
        GradientRampBinding* binding = nullptr;
        for (GradientRampBinding& candidate :
             state_.gradientRamps) {
            if (candidate.key == ramp.brushIdentity) {
                binding = &candidate;
                break;
            }
        }
        if (binding == nullptr) {
            Base::Result<GradientRampBinding*> appended =
                state_.gradientRamps.EmplaceBack();
            if (!appended) return appended.GetStatus();
            binding = appended.Value();
            binding->key = ramp.brushIdentity;
        }
        const bool current =
            binding->revision == ramp.revision &&
            binding->texture.IsValid() &&
            device_->IsAlive(binding->texture);
        if (current) continue;
        if (binding->texture.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                binding->texture, device_->LastSubmittedFence()));
            binding->texture = {};
        }
        TextureResourceDescriptor descriptor;
        descriptor.width = Render::GradientRampWidth;
        descriptor.height = 1U;
        descriptor.format = GraphicsTextureFormat::Rgba8Unorm;
        descriptor.usage =
            TextureUsageBit(TextureUsage::Sampled) |
            TextureUsageBit(TextureUsage::CopyDestination);
        Base::Result<ResourceHandle> created =
            device_->CreateTexture(descriptor);
        if (!created) return created.GetStatus();
        binding->texture = created.Value();
        TextureRegion region;
        region.width = Render::GradientRampWidth;
        region.height = 1U;
        region.bytesPerRow = Render::GradientRampWidth * 4U;
        Base::Result<void> uploaded = encoder.UploadTexture(
            binding->texture,
            region,
            {ramp.pixels.data(),
             static_cast<std::uint32_t>(ramp.pixels.size())});
        if (!uploaded) {
            static_cast<void>(device_->DestroyResource(binding->texture));
            binding->texture = {};
            return uploaded.GetStatus();
        }
        binding->revision = ramp.revision;
    }
    static constexpr float UnitQuad[] = {
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 1.0F, 1.0F, 1.0F};
    const auto* vertexBytes = reinterpret_cast<const std::uint8_t*>(UnitQuad);
    Base::Result<void> encoded = encoder.UploadBuffer(
        state_.vertexBuffer, 0U,
        {vertexBytes, static_cast<std::uint32_t>(sizeof(UnitQuad))});
    if (!encoded) {
        return encoded.GetStatus();
    }
    RenderPassDescriptor pass;
    pass.renderArea = {
        0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0].target = target.color;
    pass.colorAttachments[0].load = target.load;
    pass.colorAttachments[0].store = StoreOperation::Store;
    pass.colorAttachments[0].clearColor = {0.0F, 0.0F, 0.0F, 0.0F};
    BatchStatistics submissionStatistics;
    submissionStatistics.renderPassCount = 0U;
    enum class ActivePipeline : std::uint8_t {
        None = 0U,
        Rectangle,
        Image,
        Mesh,
        Glyph
    };
    ActivePipeline activePipeline = ActivePipeline::None;
    std::uint32_t activeBlendMode = UINT32_MAX;
    auto bindRectanglePipeline = [&](
        std::uint32_t blendMode) noexcept
        -> Base::Result<void> {
        if (activePipeline == ActivePipeline::Rectangle &&
            activeBlendMode == blendMode) {
            return {};
        }
        Base::Result<void> result =
            encoder.BindPipeline({
                UiShader::Rectangle,
                static_cast<UiBlendMode>(blendMode)});
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindVertexBuffer(0U, state_.vertexBuffer);
        }
        if (result) {
            ++submissionStatistics.vertexBufferBindingCount;
            result = encoder.BindUniformBuffer(
                0U, state_.uniformBuffer, 0U,
                static_cast<std::uint32_t>(sizeof(ShaderRectConstants)));
        }
        if (result) {
            ++submissionStatistics.uniformBufferBindingCount;
            activePipeline = ActivePipeline::Rectangle;
            activeBlendMode = blendMode;
        }
        return result;
    };
    auto bindImagePipeline = [&](
        std::uint32_t blendMode) noexcept
        -> Base::Result<void> {
        if (activePipeline == ActivePipeline::Image &&
            activeBlendMode == blendMode) {
            return {};
        }
        Base::Result<void> result =
            encoder.BindPipeline({
                UiShader::Image,
                static_cast<UiBlendMode>(blendMode)});
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindVertexBuffer(0U, state_.vertexBuffer);
        }
        if (result) {
            ++submissionStatistics.vertexBufferBindingCount;
            result = encoder.BindUniformBuffer(
                0U, state_.imageUniformBuffer, 0U,
                static_cast<std::uint32_t>(sizeof(ShaderImageConstants)));
        }
        if (result) {
            ++submissionStatistics.uniformBufferBindingCount;
            activePipeline = ActivePipeline::Image;
            activeBlendMode = blendMode;
        }
        return result;
    };
    auto bindMeshPipeline = [&](
        std::uint32_t blendMode) noexcept
        -> Base::Result<void> {
        if (activePipeline == ActivePipeline::Mesh &&
            activeBlendMode == blendMode) {
            return {};
        }
        Base::Result<void> result =
            encoder.BindPipeline({
                UiShader::Mesh,
                static_cast<UiBlendMode>(blendMode)});
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindUniformBuffer(
                0U, state_.meshUniformBuffer, 0U,
                static_cast<std::uint32_t>(sizeof(ShaderMeshConstants)));
        }
        if (result) {
            ++submissionStatistics.uniformBufferBindingCount;
            activePipeline = ActivePipeline::Mesh;
            activeBlendMode = blendMode;
        }
        return result;
    };
    auto bindGlyphPipeline = [&](
        std::uint32_t blendMode) noexcept
        -> Base::Result<void> {
        if (activePipeline == ActivePipeline::Glyph &&
            activeBlendMode == blendMode) {
            return {};
        }
        Base::Result<void> result =
            encoder.BindPipeline({
                UiShader::Glyph,
                static_cast<UiBlendMode>(blendMode)});
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindUniformBuffer(
                0U, state_.glyphUniformBuffer, 0U,
                static_cast<std::uint32_t>(sizeof(ShaderGlyphConstants)));
        }
        if (result) {
            ++submissionStatistics.uniformBufferBindingCount;
            activePipeline = ActivePipeline::Glyph;
            activeBlendMode = blendMode;
        }
        return result;
    };

    const Aero::Rect targetClip = frameClip;
    const Base::Span<const Render::RenderCommand> commands = plan.Commands();
    auto recordNodes = [&](
        Render::RenderNodeId effectRoot,
        bool mainPass,
        const EffectSurface* destinationSurface) noexcept
        -> Base::Result<void> {
    const double passLogicalWidth = destinationSurface != nullptr
        ? destinationSurface->logicalBounds.width
        : logicalWidth;
    const double passLogicalHeight = destinationSurface != nullptr
        ? destinationSurface->logicalBounds.height
        : logicalHeight;
    const double passPixelScaleX = destinationSurface != nullptr
        ? static_cast<double>(destinationSurface->width) /
            passLogicalWidth
        : pixelScaleX;
    const double passPixelScaleY = destinationSurface != nullptr
        ? static_cast<double>(destinationSurface->height) /
            passLogicalHeight
        : pixelScaleY;
    const Aero::Rect passTargetClip = destinationSurface != nullptr
        ? Aero::Rect{0.0, 0.0, passLogicalWidth, passLogicalHeight}
        : targetClip;
    const Media::Transform2D passOrigin = destinationSurface != nullptr
        ? Translation(
            -destinationSurface->logicalBounds.x,
            -destinationSurface->logicalBounds.y)
        : IdentityTransform();
    auto passPhysicalScissor = [passPixelScaleX, passPixelScaleY](
        Aero::Rect logical) noexcept {
        return Aero::Rect{
            logical.x * passPixelScaleX,
            logical.y * passPixelScaleY,
            logical.width * passPixelScaleX,
            logical.height * passPixelScaleY};
    };
    activePipeline = ActivePipeline::None;
    activeBlendMode = UINT32_MAX;
    state_.nodes.Clear();
    std::uint32_t effectOrdinal = 0U;
    for (const Render::RenderNodeSnapshot& node : plan.Nodes()) {
        const std::uint32_t nodeEffectSurfaceIndex =
            effectOrdinal;
        if (RequiresNodeSurface(node)) {
            ++effectOrdinal;
        }
        const std::uint32_t blendMode =
            static_cast<std::uint32_t>(
                node.blendMode);
        if (blendMode >= 4U) {
            encoded = InvalidArgument(
                "Renderer contains an invalid blend mode");
            break;
        }
        bool duplicateId = false;
        for (const NodeState& existing :
            state_.nodes) {
            duplicateId =
                duplicateId ||
                existing.id == node.id;
        }
        if (node.id == Render::InvalidRenderNodeId) {
            encoded = InvalidArgument(
                "Renderer node identity is invalid");
            break;
        }
        if (duplicateId) {
            encoded = InvalidArgument(
                "Renderer node identity is duplicated");
            break;
        }
        if (!Aero::IsValidLayoutRect(
                node.layoutSlot)) {
            encoded = InvalidArgument(
                "Renderer node layout slot is invalid");
            break;
        }
        if (!Aero::IsValidLayoutRect(node.clip)) {
            encoded = InvalidArgument(
                "Renderer node clip is invalid");
            break;
        }
        if (!Aero::IsValidLayoutSize(
                node.renderSize)) {
            encoded = InvalidArgument(
                "Renderer node render size is invalid");
            break;
        }
        if (!Base::IsFiniteTransform(node.renderTransform)) {
            encoded = InvalidArgument(
                "Renderer node transform is invalid");
            break;
        }
        if (!Render::IsValidOpacity(node.opacity)) {
            encoded = InvalidArgument(
                "Renderer node opacity is invalid");
            break;
        }
        if (static_cast<std::uint8_t>(node.mask.kind) >
                static_cast<std::uint8_t>(
                    Render::RenderMaskKind::RadialGradient) ||
            !Render::IsFinite(node.mask.color) ||
            (node.mask.kind == Render::RenderMaskKind::Image &&
             (node.mask.image == Render::InvalidRenderImageId ||
              !Aero::IsValidLayoutRect(node.mask.sourceUv)))) {
            encoded = InvalidArgument(
                "Renderer node opacity mask is invalid");
            break;
        }
        if (static_cast<std::uint8_t>(
                node.effect.kind) >
                static_cast<std::uint8_t>(
                    Render::RenderEffectKind::DropShadow) ||
            !std::isfinite(node.effect.radius) ||
            node.effect.radius < 0.0 ||
            !std::isfinite(node.effect.direction) ||
            !std::isfinite(node.effect.depth) ||
            node.effect.depth < 0.0 ||
            !Render::IsValidOpacity(
                node.effect.opacity) ||
            !Render::IsFinite(
                node.effect.color)) {
            encoded = InvalidArgument(
                "Renderer node effect is invalid");
            break;
        }
        if (node.commandOffset > commands.Size() ||
            node.commandCount >
                commands.Size() - node.commandOffset) {
            encoded = InvalidArgument(
                "Renderer node command range is invalid");
            break;
        }

        Media::Transform2D parentTransform = passOrigin;
        Aero::Rect parentClip = passTargetClip;
        double parentOpacity = 1.0;
        std::uint32_t parentIndex = UINT32_MAX;
        Render::RenderNodeId containingEffect =
            Render::InvalidRenderNodeId;
        std::uint32_t containingEffectCount = 0U;
        if (node.parentId != Render::InvalidRenderNodeId) {
            const NodeState* parent = nullptr;
            for (std::uint32_t index = state_.nodes.Size(); index > 0U; --index) {
                const NodeState& candidate = state_.nodes[index - 1U];
                if (candidate.id == node.parentId) {
                    parent = &candidate;
                    parentIndex = index - 1U;
                    break;
                }
            }
            if (parent == nullptr) {
                encoded = InvalidState("Renderer node parent is unavailable");
                break;
            }
            parentTransform = parent->transform;
            parentClip = parent->clip.bounds;
            parentOpacity = parent->opacity;
            containingEffect =
                parent->containingEffect;
            containingEffectCount =
                parent->containingEffectCount;
        }
        const Render::RenderNodeId parentContainingEffect =
            containingEffect;
        if (RequiresNodeSurface(node)) {
            containingEffect = node.id;
            ++containingEffectCount;
        }

        const Media::Transform2D nodeTransform = Compose(
            Compose(
                node.renderTransform,
                Translation(
                    node.layoutSlot.x,
                    node.layoutSlot.y)),
            parentTransform);
        ClipState nodeClip{node.clip, parentTransform, parentClip};
        if (node.clipsToBounds) {
            const Aero::Rect nodeBounds =
                TransformBounds(parentTransform, node.clip);
            if (!Aero::IsValidLayoutRect(nodeBounds)) {
                encoded = InvalidArgument("Renderer node clip bounds are invalid");
                break;
            }
            nodeClip.bounds = IntersectRect(parentClip, nodeBounds);
        }
        const double nodeOpacity =
            parentOpacity * node.opacity;
        Base::Result<void> appendedNode = state_.nodes.PushBack(
            {node.id, nodeTransform, nodeClip, node.clipsToBounds,
             nodeOpacity,
             parentIndex, containingEffect,
             containingEffectCount});
        if (!appendedNode) {
            encoded = appendedNode;
            break;
        }

        state_.transforms.Clear();
        state_.clips.Clear();
        state_.opacities.Clear();
        state_.nodePath.Clear();
        // Drawing code always has a current clip. Keep the render target as
        // that root clip, then add only ancestors that explicitly opt into
        // ClipToBounds.
        Base::Result<void> rootClip = state_.clips.PushBack(
            {passTargetClip, IdentityTransform(), passTargetClip});
        if (!rootClip) {
            encoded = rootClip;
            break;
        }
        std::uint32_t nodePathIndex = state_.nodes.Size() - 1U;
        while (true) {
            if (state_.nodePath.Size() >= MaxShaderClips) {
                encoded = Unsupported(
                    "Renderer layout clip nesting exceeds shader capacity");
                break;
            }
            Base::Result<void> pathAppended = state_.nodePath.PushBack(nodePathIndex);
            if (!pathAppended) {
                encoded = pathAppended;
                break;
            }
            const std::uint32_t nextParent =
                state_.nodes[nodePathIndex].parentIndex;
            if (nextParent == UINT32_MAX) {
                break;
            }
            nodePathIndex = nextParent;
        }
        if (!encoded) {
            break;
        }
        for (std::uint32_t index = state_.nodePath.Size(); index > 0U; --index) {
            const NodeState& pathNode =
                state_.nodes[state_.nodePath[index - 1U]];
            if (!pathNode.clipsToBounds) {
                continue;
            }
            Base::Result<void> pushed = state_.clips.PushBack(pathNode.clip);
            if (!pushed) {
                encoded = pushed;
                break;
            }
        }
        if (!encoded) {
            break;
        }
        if (!(state_.transforms.PushBack(nodeTransform)) ||
            !(state_.opacities.PushBack(nodeOpacity))) {
            encoded = OutOfMemory("Failed to allocate Renderer state stack");
            break;
        }
        const std::uint32_t baseClipCount = state_.clips.Size();

        const NodeState& currentNodeState =
            state_.nodes[
                state_.nodes.Size() - 1U];
        bool shouldDraw =
            mainPass
            ? currentNodeState.containingEffect ==
                Render::InvalidRenderNodeId
            : node.id == effectRoot ||
                currentNodeState.containingEffect == effectRoot;
        const bool compositeSurface =
            RequiresNodeSurface(node) && node.id != effectRoot &&
            (mainPass
             ? parentContainingEffect == Render::InvalidRenderNodeId
             : parentContainingEffect == effectRoot);
        if (compositeSurface) {
            if (nodeEffectSurfaceIndex >=
                    state_.effectSurfaces.Size() ||
                state_.effectSurfaces[nodeEffectSurfaceIndex].owner != node.id) {
                encoded = InvalidState(
                    "Renderer effect surface is unavailable");
                break;
            }
            const EffectSurface& effectSurface =
                state_.effectSurfaces[nodeEffectSurfaceIndex];
            const ResourceHandle source =
                node.effect.kind == Render::RenderEffectKind::None
                ? effectSurface.content
                : effectSurface.result;
            if (!source.IsValid() || !device_->IsAlive(source)) {
                encoded = InvalidState(
                    "Renderer effect surface output is unavailable");
                break;
            }
            Aero::Rect surfaceBounds = effectSurface.logicalBounds;
            if (destinationSurface != nullptr) {
                surfaceBounds.x -= destinationSurface->logicalBounds.x;
                surfaceBounds.y -= destinationSurface->logicalBounds.y;
            }
            const Aero::Rect effectBounds = IntersectRect(
                surfaceBounds, passTargetClip);
            if (!IsEmpty(effectBounds)) {
                ShaderImageConstants constants;
                constants.transform0[0] = 1.0F;
                constants.transform0[3] = 1.0F;
                constants.transform1[2] = static_cast<float>(passLogicalWidth);
                constants.transform1[3] = static_cast<float>(passLogicalHeight);
                constants.clipCount = 1U;
                constants.clipRect[0][2] = static_cast<float>(passLogicalWidth);
                constants.clipRect[0][3] = static_cast<float>(passLogicalHeight);
                constants.clipInverse[0][0] = 1.0F;
                constants.clipInverse[0][3] = 1.0F;
                constants.rects[0][0] = static_cast<float>(effectBounds.x);
                constants.rects[0][1] = static_cast<float>(effectBounds.y);
                constants.rects[0][2] = static_cast<float>(effectBounds.width);
                constants.rects[0][3] = static_cast<float>(effectBounds.height);
                constants.sourceUvs[0][0] = static_cast<float>(
                    (effectBounds.x - surfaceBounds.x) /
                    surfaceBounds.width);
                constants.sourceUvs[0][1] = static_cast<float>(
                    (effectBounds.y - surfaceBounds.y) /
                    surfaceBounds.height);
                constants.sourceUvs[0][2] = static_cast<float>(
                    effectBounds.width / surfaceBounds.width);
                constants.sourceUvs[0][3] = static_cast<float>(
                    effectBounds.height / surfaceBounds.height);
                constants.tints[0][0] = 1.0F;
                constants.tints[0][1] = 1.0F;
                constants.tints[0][2] = 1.0F;
                constants.tints[0][3] = 1.0F;
                encoded = bindImagePipeline(blendMode);
                if (encoded) {
                    encoded = encoder.BindTextureSampler(
                        0U, source, state_.effectSampler);
                }
                if (encoded) {
                    ++submissionStatistics.textureSamplerBindingCount;
                    encoded = AppendDraw(
                        encoder,
                        state_.imageUniformBuffer,
                        constants,
                        passPhysicalScissor(effectBounds),
                        1U);
                }
                if (encoded) {
                    ++submissionStatistics.drawCallCount;
                    ++submissionStatistics.imageInstanceCount;
                    ++submissionStatistics.uniformBufferUploadCount;
                }
            }
            shouldDraw = false;
        }

        for (std::uint32_t commandIndex = 0U;
             commandIndex <
                 (shouldDraw
                  ? node.commandCount
                  : 0U);
             ++commandIndex) {
            const Render::RenderCommand& command =
                commands[node.commandOffset + commandIndex];
            switch (command.kind) {
            case Render::RenderCommandKind::PushClip: {
                if (!Aero::IsValidLayoutRect(command.rect)) {
                    encoded = InvalidArgument("Renderer contains an invalid clip");
                    break;
                }
                Base::Result<void> pushed = PushClipState(
                    state_.clips, command.rect,
                    state_.transforms[state_.transforms.Size() - 1U]);
                if (!pushed) encoded = pushed;
                break;
            }
            case Render::RenderCommandKind::PopClip:
                if (state_.clips.Size() <= baseClipCount) {
                    encoded = InvalidState("Renderer clip stack underflow");
                } else {
                    state_.clips.PopBack();
                }
                break;
            case Render::RenderCommandKind::PushOpacity: {
                if (!Render::IsValidOpacity(command.scalar)) {
                    encoded = InvalidArgument("Renderer contains invalid opacity");
                    break;
                }
                Base::Result<void> pushed = state_.opacities.PushBack(
                    state_.opacities[state_.opacities.Size() - 1U] * command.scalar);
                if (!pushed) encoded = pushed;
                break;
            }
            case Render::RenderCommandKind::PopOpacity:
                if (state_.opacities.Size() <= 1U) {
                    encoded = InvalidState("Renderer opacity stack underflow");
                } else {
                    state_.opacities.PopBack();
                }
                break;
            case Render::RenderCommandKind::PushTransform: {
                if (!Render::IsFinite(command.transform)) {
                    encoded = InvalidArgument("Renderer contains an invalid transform");
                    break;
                }
                Base::Result<void> pushed = state_.transforms.PushBack(Compose(
                    command.transform,
                    state_.transforms[state_.transforms.Size() - 1U]));
                if (!pushed) encoded = pushed;
                break;
            }
            case Render::RenderCommandKind::PopTransform:
                if (state_.transforms.Size() <= 1U) {
                    encoded = InvalidState("Renderer transform stack underflow");
                } else {
                    state_.transforms.PopBack();
                }
                break;
            case Render::RenderCommandKind::FillRect:
            case Render::RenderCommandKind::FillRoundedRect:
            case Render::RenderCommandKind::StrokeRect: {
                encoded =
                    bindRectanglePipeline(
                        blendMode);
                if (!encoded) {
                    break;
                }
                if (!Aero::IsValidLayoutRect(command.rect) ||
                    !Render::IsFinite(command.color) ||
                    ((command.kind == Render::RenderCommandKind::FillRoundedRect ||
                      command.kind == Render::RenderCommandKind::StrokeRect) &&
                     (!std::isfinite(command.scalar) || command.scalar < 0.0))) {
                    encoded = InvalidArgument("Renderer contains invalid rectangle geometry");
                    break;
                }
                if (command.kind == Render::RenderCommandKind::FillRoundedRect &&
                    command.scalar * 2.0 >
                        std::fmin(command.rect.width, command.rect.height)) {
                    encoded = InvalidArgument("Renderer corner radius exceeds rectangle bounds");
                    break;
                }
                const Aero::Rect clip =
                    state_.clips[state_.clips.Size() - 1U].bounds;
                if (IsEmpty(clip) || IsEmpty(command.rect)) {
                    break;
                }
                const Media::Transform2D& transform =
                    state_.transforms[state_.transforms.Size() - 1U];
                const double opacity = state_.opacities[state_.opacities.Size() - 1U];
                if (!FitsFloat(command.rect.x) || !FitsFloat(command.rect.y) ||
                    !FitsFloat(command.rect.width) || !FitsFloat(command.rect.height) ||
                    (command.kind == Render::RenderCommandKind::FillRoundedRect &&
                     !FitsFloat(command.scalar)) ||
                    !FitsFloat(transform.m11) || !FitsFloat(transform.m12) ||
                    !FitsFloat(transform.m21) || !FitsFloat(transform.m22) ||
                    !FitsFloat(transform.dx) || !FitsFloat(transform.dy) ||
                    !FitsFloat(opacity)) {
                    encoded = InvalidArgument("Renderer values exceed shader precision");
                    break;
                }
                auto configureConstants = [&](ShaderRectConstants& constants)
                    noexcept -> Base::Result<void> {
                    constants.transform0[0] = static_cast<float>(transform.m11);
                    constants.transform0[1] = static_cast<float>(transform.m12);
                    constants.transform0[2] = static_cast<float>(transform.m21);
                    constants.transform0[3] = static_cast<float>(transform.m22);
                    constants.transform1[0] = static_cast<float>(transform.dx);
                    constants.transform1[1] = static_cast<float>(transform.dy);
                    constants.transform1[2] =
                        static_cast<float>(passLogicalWidth);
                    constants.transform1[3] =
                        static_cast<float>(passLogicalHeight);
                    constants.clipCount = state_.clips.Size();
                    for (std::uint32_t clipIndex = 0U;
                         clipIndex < state_.clips.Size();
                         ++clipIndex) {
                        const ClipState& clipState = state_.clips[clipIndex];
                        const Media::Transform2D& clipTransform =
                            clipState.transform;
                        const double determinant =
                            clipTransform.m11 * clipTransform.m22 -
                            clipTransform.m12 * clipTransform.m21;
                        const double inverseM11 = clipTransform.m22 / determinant;
                        const double inverseM12 = -clipTransform.m12 / determinant;
                        const double inverseM21 = -clipTransform.m21 / determinant;
                        const double inverseM22 = clipTransform.m11 / determinant;
                        if (!FitsFloat(clipState.rect.x) ||
                            !FitsFloat(clipState.rect.y) ||
                            !FitsFloat(clipState.rect.width) ||
                            !FitsFloat(clipState.rect.height) ||
                            !FitsFloat(inverseM11) || !FitsFloat(inverseM12) ||
                            !FitsFloat(inverseM21) || !FitsFloat(inverseM22) ||
                            !FitsFloat(clipTransform.dx) ||
                            !FitsFloat(clipTransform.dy)) {
                            return InvalidArgument(
                                "Renderer clip values exceed shader precision");
                        }
                        constants.clipRect[clipIndex][0] =
                            static_cast<float>(clipState.rect.x);
                        constants.clipRect[clipIndex][1] =
                            static_cast<float>(clipState.rect.y);
                        constants.clipRect[clipIndex][2] =
                            static_cast<float>(clipState.rect.width);
                        constants.clipRect[clipIndex][3] =
                            static_cast<float>(clipState.rect.height);
                        constants.clipInverse[clipIndex][0] =
                            static_cast<float>(inverseM11);
                        constants.clipInverse[clipIndex][1] =
                            static_cast<float>(inverseM12);
                        constants.clipInverse[clipIndex][2] =
                            static_cast<float>(inverseM21);
                        constants.clipInverse[clipIndex][3] =
                            static_cast<float>(inverseM22);
                        constants.clipTranslation[clipIndex][0] =
                            static_cast<float>(clipTransform.dx);
                        constants.clipTranslation[clipIndex][1] =
                            static_cast<float>(clipTransform.dy);
                    }
                    return {};
                };
                auto appendConstants = [&](const ShaderRectConstants& constants,
                    std::uint32_t instanceCount) noexcept -> Base::Result<void> {
                    Base::Result<void> result = AppendDraw(
                        encoder, state_.uniformBuffer, constants,
                        passPhysicalScissor(clip),
                        instanceCount);
                    if (result) {
                        ++submissionStatistics.drawCallCount;
                        submissionStatistics.rectangleInstanceCount += instanceCount;
                        ++submissionStatistics.uniformBufferUploadCount;
                    }
                    return result;
                };
                auto appendRectangle = [&](Aero::Rect rect, Render::Color color,
                    std::uint32_t instanceCount = 1U,
                    float strokeThickness = 0.0F,
                    float cornerRadius = 0.0F) noexcept -> Base::Result<void> {
                    ShaderRectConstants constants;
                    Base::Result<void> result = configureConstants(constants);
                    if (!result) {
                        return result;
                    }
                    constants.rects[0][0] = static_cast<float>(rect.x);
                    constants.rects[0][1] = static_cast<float>(rect.y);
                    constants.rects[0][2] = static_cast<float>(rect.width);
                    constants.rects[0][3] = static_cast<float>(rect.height);
                    constants.colors[0][0] = color.red;
                    constants.colors[0][1] = color.green;
                    constants.colors[0][2] = color.blue;
                    constants.colors[0][3] = static_cast<float>(color.alpha * opacity);
                    constants.cornerRadii[0][0] = cornerRadius;
                    constants.instanceMode = instanceCount == 4U ? 1U : 0U;
                    constants.strokeThickness = strokeThickness;
                    return appendConstants(constants, instanceCount);
                };

                if (command.kind == Render::RenderCommandKind::FillRect ||
                    command.kind == Render::RenderCommandKind::FillRoundedRect) {
                    ShaderRectConstants constants;
                    encoded = configureConstants(constants);
                    if (!encoded) {
                        break;
                    }
                    std::uint32_t batchCommandCount = 0U;
                    std::uint32_t instanceCount = 0U;
                    for (std::uint32_t batchIndex = commandIndex;
                         batchIndex < node.commandCount &&
                             instanceCount <
                                 (state_.batchingEnabled
                                  ? MaxRectangleBatchInstances
                                  : 1U);
                         ++batchIndex) {
                        const Render::RenderCommand& candidate =
                            commands[node.commandOffset + batchIndex];
                        if (candidate.kind != Render::RenderCommandKind::FillRect &&
                            candidate.kind != Render::RenderCommandKind::FillRoundedRect) {
                            break;
                        }
                        ++batchCommandCount;
                        if (!Aero::IsValidLayoutRect(candidate.rect) ||
                            !Render::IsFinite(candidate.color) ||
                            (candidate.kind == Render::RenderCommandKind::FillRoundedRect &&
                             (!std::isfinite(candidate.scalar) ||
                              candidate.scalar < 0.0 ||
                              candidate.scalar * 2.0 > std::fmin(
                                  candidate.rect.width, candidate.rect.height)))) {
                            encoded = InvalidArgument(
                                "Renderer contains invalid rectangle geometry");
                            break;
                        }
                        if (IsEmpty(candidate.rect)) {
                            continue;
                        }
                        if (!FitsFloat(candidate.rect.x) ||
                            !FitsFloat(candidate.rect.y) ||
                            !FitsFloat(candidate.rect.width) ||
                            !FitsFloat(candidate.rect.height) ||
                            (candidate.kind ==
                                Render::RenderCommandKind::FillRoundedRect &&
                             !FitsFloat(candidate.scalar))) {
                            encoded = InvalidArgument(
                                "Renderer values exceed shader precision");
                            break;
                        }
                        constants.rects[instanceCount][0] =
                            static_cast<float>(candidate.rect.x);
                        constants.rects[instanceCount][1] =
                            static_cast<float>(candidate.rect.y);
                        constants.rects[instanceCount][2] =
                            static_cast<float>(candidate.rect.width);
                        constants.rects[instanceCount][3] =
                            static_cast<float>(candidate.rect.height);
                        constants.colors[instanceCount][0] = candidate.color.red;
                        constants.colors[instanceCount][1] = candidate.color.green;
                        constants.colors[instanceCount][2] = candidate.color.blue;
                        constants.colors[instanceCount][3] =
                            static_cast<float>(candidate.color.alpha * opacity);
                        constants.cornerRadii[instanceCount][0] =
                            candidate.kind == Render::RenderCommandKind::FillRoundedRect
                            ? static_cast<float>(candidate.scalar)
                            : 0.0F;
                        ++instanceCount;
                    }
                    if (!encoded) {
                        break;
                    }
                    commandIndex += batchCommandCount - 1U;
                    if (instanceCount != 0U) {
                        constants.instanceMode = instanceCount > 1U ? 2U : 0U;
                        encoded = appendConstants(constants, instanceCount);
                    }
                } else if (command.scalar == 0.0 ||
                    command.scalar * 2.0 >= std::fmin(command.rect.width, command.rect.height)) {
                    encoded = appendRectangle(command.rect, command.color);
                } else {
                    // Border segments share transform, opacity, and clip state,
                    // so D3D11 emits them as one four-instance draw.
                    encoded = appendRectangle(
                        command.rect, command.color, 4U,
                        static_cast<float>(command.scalar));
                }
                break;
            }
            case Render::RenderCommandKind::DrawImage: {
                if (command.image == Render::InvalidRenderImageId ||
                    !Aero::IsValidLayoutRect(command.rect) ||
                    !IsValidImageUv(command.sourceUv) ||
                    !Render::IsFinite(command.color)) {
                    encoded = InvalidArgument(
                        "Renderer contains invalid image geometry");
                    break;
                }
                const ImageBinding* imageBinding = nullptr;
                for (const ImageBinding& candidate : state_.images) {
                    if (candidate.id == command.image) {
                        imageBinding = &candidate;
                        break;
                    }
                }
                if (imageBinding == nullptr) {
                    encoded = InvalidState(
                        "Renderer image is not registered");
                    break;
                }
                if (!device_->IsAlive(imageBinding->texture) ||
                    !device_->IsAlive(imageBinding->sampler)) {
                    encoded = InvalidState(
                        "Renderer image resources are no longer alive");
                    break;
                }
                const Aero::Rect clip =
                    state_.clips[state_.clips.Size() - 1U].bounds;
                if (IsEmpty(clip) || IsEmpty(command.rect) ||
                    IsEmptyImageUv(command.sourceUv)) {
                    break;
                }
                const Media::Transform2D& transform =
                    state_.transforms[state_.transforms.Size() - 1U];
                const double opacity =
                    state_.opacities[state_.opacities.Size() - 1U];
                if (!FitsFloat(command.rect.x) || !FitsFloat(command.rect.y) ||
                    !FitsFloat(command.rect.width) || !FitsFloat(command.rect.height) ||
                    !FitsFloat(command.sourceUv.x) || !FitsFloat(command.sourceUv.y) ||
                    !FitsFloat(command.sourceUv.width) ||
                    !FitsFloat(command.sourceUv.height) ||
                    !FitsFloat(transform.m11) || !FitsFloat(transform.m12) ||
                    !FitsFloat(transform.m21) || !FitsFloat(transform.m22) ||
                    !FitsFloat(transform.dx) || !FitsFloat(transform.dy) ||
                    !FitsFloat(opacity)) {
                    encoded = InvalidArgument(
                        "Renderer image values exceed shader precision");
                    break;
                }
                ShaderImageConstants constants;
                constants.transform0[0] = static_cast<float>(transform.m11);
                constants.transform0[1] = static_cast<float>(transform.m12);
                constants.transform0[2] = static_cast<float>(transform.m21);
                constants.transform0[3] = static_cast<float>(transform.m22);
                constants.transform1[0] = static_cast<float>(transform.dx);
                constants.transform1[1] = static_cast<float>(transform.dy);
                constants.transform1[2] =
                    static_cast<float>(passLogicalWidth);
                constants.transform1[3] =
                    static_cast<float>(passLogicalHeight);
                constants.clipCount = state_.clips.Size();
                for (std::uint32_t clipIndex = 0U;
                     clipIndex < state_.clips.Size();
                     ++clipIndex) {
                    const ClipState& clipState = state_.clips[clipIndex];
                    const Media::Transform2D& clipTransform = clipState.transform;
                    const double determinant =
                        clipTransform.m11 * clipTransform.m22 -
                        clipTransform.m12 * clipTransform.m21;
                    const double inverseM11 = clipTransform.m22 / determinant;
                    const double inverseM12 = -clipTransform.m12 / determinant;
                    const double inverseM21 = -clipTransform.m21 / determinant;
                    const double inverseM22 = clipTransform.m11 / determinant;
                    if (!FitsFloat(clipState.rect.x) ||
                        !FitsFloat(clipState.rect.y) ||
                        !FitsFloat(clipState.rect.width) ||
                        !FitsFloat(clipState.rect.height) ||
                        !FitsFloat(inverseM11) || !FitsFloat(inverseM12) ||
                        !FitsFloat(inverseM21) || !FitsFloat(inverseM22) ||
                        !FitsFloat(clipTransform.dx) ||
                        !FitsFloat(clipTransform.dy)) {
                        encoded = InvalidArgument(
                            "Renderer image clip values exceed shader precision");
                        break;
                    }
                    constants.clipRect[clipIndex][0] =
                        static_cast<float>(clipState.rect.x);
                    constants.clipRect[clipIndex][1] =
                        static_cast<float>(clipState.rect.y);
                    constants.clipRect[clipIndex][2] =
                        static_cast<float>(clipState.rect.width);
                    constants.clipRect[clipIndex][3] =
                        static_cast<float>(clipState.rect.height);
                    constants.clipInverse[clipIndex][0] =
                        static_cast<float>(inverseM11);
                    constants.clipInverse[clipIndex][1] =
                        static_cast<float>(inverseM12);
                    constants.clipInverse[clipIndex][2] =
                        static_cast<float>(inverseM21);
                    constants.clipInverse[clipIndex][3] =
                        static_cast<float>(inverseM22);
                    constants.clipTranslation[clipIndex][0] =
                        static_cast<float>(clipTransform.dx);
                    constants.clipTranslation[clipIndex][1] =
                        static_cast<float>(clipTransform.dy);
                }
                if (!encoded) {
                    break;
                }
                std::uint32_t batchCommandCount = 0U;
                std::uint32_t instanceCount = 0U;
                for (std::uint32_t batchIndex = commandIndex;
                     batchIndex < node.commandCount &&
                         instanceCount <
                             (state_.batchingEnabled
                              ? MaxRectangleBatchInstances
                              : 1U);
                     ++batchIndex) {
                    const Render::RenderCommand& candidate =
                        commands[node.commandOffset + batchIndex];
                    if (candidate.kind != Render::RenderCommandKind::DrawImage ||
                        candidate.image != command.image) {
                        break;
                    }
                    ++batchCommandCount;
                    if (!Aero::IsValidLayoutRect(candidate.rect) ||
                        !IsValidImageUv(candidate.sourceUv) ||
                        !Render::IsFinite(candidate.color)) {
                        encoded = InvalidArgument(
                            "Renderer contains invalid image geometry");
                        break;
                    }
                    if (IsEmpty(candidate.rect) ||
                        IsEmptyImageUv(candidate.sourceUv)) {
                        continue;
                    }
                    if (!FitsFloat(candidate.rect.x) ||
                        !FitsFloat(candidate.rect.y) ||
                        !FitsFloat(candidate.rect.width) ||
                        !FitsFloat(candidate.rect.height) ||
                        !FitsFloat(candidate.sourceUv.x) ||
                        !FitsFloat(candidate.sourceUv.y) ||
                        !FitsFloat(candidate.sourceUv.width) ||
                        !FitsFloat(candidate.sourceUv.height)) {
                        encoded = InvalidArgument(
                            "Renderer image values exceed shader precision");
                        break;
                    }
                    constants.rects[instanceCount][0] =
                        static_cast<float>(candidate.rect.x);
                    constants.rects[instanceCount][1] =
                        static_cast<float>(candidate.rect.y);
                    constants.rects[instanceCount][2] =
                        static_cast<float>(candidate.rect.width);
                    constants.rects[instanceCount][3] =
                        static_cast<float>(candidate.rect.height);
                    constants.sourceUvs[instanceCount][0] =
                        static_cast<float>(candidate.sourceUv.x);
                    constants.sourceUvs[instanceCount][1] =
                        static_cast<float>(candidate.sourceUv.y);
                    constants.sourceUvs[instanceCount][2] =
                        static_cast<float>(candidate.sourceUv.width);
                    constants.sourceUvs[instanceCount][3] =
                        static_cast<float>(candidate.sourceUv.height);
                    constants.tints[instanceCount][0] = candidate.color.red;
                    constants.tints[instanceCount][1] = candidate.color.green;
                    constants.tints[instanceCount][2] = candidate.color.blue;
                    constants.tints[instanceCount][3] =
                        static_cast<float>(candidate.color.alpha * opacity);
                    ++instanceCount;
                }
                if (!encoded) {
                    break;
                }
                commandIndex += batchCommandCount - 1U;
                if (instanceCount != 0U) {
                    encoded =
                        bindImagePipeline(
                            blendMode);
                    if (encoded) {
                        encoded = encoder.BindTextureSampler(
                            0U, imageBinding->texture, imageBinding->sampler);
                    }
                    if (encoded) {
                        ++submissionStatistics.textureSamplerBindingCount;
                        encoded = AppendDraw(encoder, state_.imageUniformBuffer,
                            constants, passPhysicalScissor(clip), instanceCount);
                    }
                    if (encoded) {
                        ++submissionStatistics.drawCallCount;
                        submissionStatistics.imageInstanceCount += instanceCount;
                        ++submissionStatistics.uniformBufferUploadCount;
                    }
                }
                break;
            }
            case Render::RenderCommandKind::DrawMesh: {
                if (command.mesh == Render::InvalidRenderMeshId ||
                    !Render::IsFinite(command.color)) {
                    encoded = InvalidArgument("Renderer contains invalid mesh draw");
                    break;
                }
                const MeshBinding* meshBinding = nullptr;
                for (const MeshBinding& candidate : state_.meshes) {
                    if (candidate.id == command.mesh) {
                        meshBinding = &candidate;
                        break;
                    }
                }
                if (meshBinding == nullptr ||
                    !device_->IsAlive(meshBinding->vertexBuffer) ||
                    !device_->IsAlive(meshBinding->indexBuffer)) {
                    encoded = InvalidState("Renderer mesh is not registered or alive");
                    break;
                }
                const Aero::Rect clip =
                    state_.clips[state_.clips.Size() - 1U].bounds;
                if (IsEmpty(clip)) {
                    break;
                }
                const Media::Transform2D& transform =
                    state_.transforms[state_.transforms.Size() - 1U];
                const double opacity =
                    state_.opacities[state_.opacities.Size() - 1U];
                if (!FitsFloat(transform.m11) || !FitsFloat(transform.m12) ||
                    !FitsFloat(transform.m21) || !FitsFloat(transform.m22) ||
                    !FitsFloat(transform.dx) || !FitsFloat(transform.dy) ||
                    !FitsFloat(opacity)) {
                    encoded = InvalidArgument("Renderer mesh values exceed shader precision");
                    break;
                }
                ShaderMeshConstants constants;
                constants.transform0[0] = static_cast<float>(transform.m11);
                constants.transform0[1] = static_cast<float>(transform.m12);
                constants.transform0[2] = static_cast<float>(transform.m21);
                constants.transform0[3] = static_cast<float>(transform.m22);
                constants.transform1[0] = static_cast<float>(transform.dx);
                constants.transform1[1] = static_cast<float>(transform.dy);
                constants.transform1[2] =
                    static_cast<float>(passLogicalWidth);
                constants.transform1[3] =
                    static_cast<float>(passLogicalHeight);
                constants.clipCount = state_.clips.Size();
                for (std::uint32_t clipIndex = 0U;
                     clipIndex < state_.clips.Size();
                     ++clipIndex) {
                    const ClipState& clipState = state_.clips[clipIndex];
                    const Media::Transform2D& clipTransform = clipState.transform;
                    const double determinant = clipTransform.m11 * clipTransform.m22 -
                        clipTransform.m12 * clipTransform.m21;
                    const double inverseM11 = clipTransform.m22 / determinant;
                    const double inverseM12 = -clipTransform.m12 / determinant;
                    const double inverseM21 = -clipTransform.m21 / determinant;
                    const double inverseM22 = clipTransform.m11 / determinant;
                    if (!FitsFloat(clipState.rect.x) || !FitsFloat(clipState.rect.y) ||
                        !FitsFloat(clipState.rect.width) || !FitsFloat(clipState.rect.height) ||
                        !FitsFloat(inverseM11) || !FitsFloat(inverseM12) ||
                        !FitsFloat(inverseM21) || !FitsFloat(inverseM22) ||
                        !FitsFloat(clipTransform.dx) || !FitsFloat(clipTransform.dy)) {
                        encoded = InvalidArgument("Renderer mesh clip values exceed shader precision");
                        break;
                    }
                    constants.clipRect[clipIndex][0] = static_cast<float>(clipState.rect.x);
                    constants.clipRect[clipIndex][1] = static_cast<float>(clipState.rect.y);
                    constants.clipRect[clipIndex][2] = static_cast<float>(clipState.rect.width);
                    constants.clipRect[clipIndex][3] = static_cast<float>(clipState.rect.height);
                    constants.clipInverse[clipIndex][0] = static_cast<float>(inverseM11);
                    constants.clipInverse[clipIndex][1] = static_cast<float>(inverseM12);
                    constants.clipInverse[clipIndex][2] = static_cast<float>(inverseM21);
                    constants.clipInverse[clipIndex][3] = static_cast<float>(inverseM22);
                    constants.clipTranslation[clipIndex][0] = static_cast<float>(clipTransform.dx);
                    constants.clipTranslation[clipIndex][1] = static_cast<float>(clipTransform.dy);
                }
                if (!encoded) {
                    break;
                }
                std::uint32_t batchCommandCount = 0U;
                std::uint32_t instanceCount = 0U;
                for (std::uint32_t batchIndex = commandIndex;
                     batchIndex < node.commandCount &&
                         instanceCount <
                             (state_.batchingEnabled
                              ? MaxRectangleBatchInstances
                              : 1U);
                     ++batchIndex) {
                    const Render::RenderCommand& candidate =
                        commands[node.commandOffset + batchIndex];
                    if (candidate.kind != Render::RenderCommandKind::DrawMesh ||
                        candidate.mesh != command.mesh) {
                        break;
                    }
                    ++batchCommandCount;
                    if (!Render::IsFinite(candidate.color)) {
                        encoded = InvalidArgument(
                            "Renderer contains invalid mesh tint");
                        break;
                    }
                    constants.tints[instanceCount][0] = candidate.color.red;
                    constants.tints[instanceCount][1] = candidate.color.green;
                    constants.tints[instanceCount][2] = candidate.color.blue;
                    constants.tints[instanceCount][3] =
                        static_cast<float>(candidate.color.alpha * opacity);
                    ++instanceCount;
                }
                if (!encoded) {
                    break;
                }
                commandIndex += batchCommandCount - 1U;
                encoded =
                    bindMeshPipeline(
                        blendMode);
                if (encoded) {
                    encoded = encoder.BindVertexBuffer(0U, meshBinding->vertexBuffer);
                }
                if (encoded) {
                    ++submissionStatistics.vertexBufferBindingCount;
                    encoded = encoder.BindIndexBuffer(
                        meshBinding->indexBuffer, meshBinding->indexType);
                }
                if (encoded) {
                    ++submissionStatistics.indexBufferBindingCount;
                    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&constants);
                    encoded = encoder.UploadBuffer(state_.meshUniformBuffer, 0U,
                        {bytes, static_cast<std::uint32_t>(sizeof(constants))});
                }
                if (encoded) {
                    encoded = encoder.SetScissor(
                        passPhysicalScissor(clip));
                }
                if (encoded) {
                    encoded = encoder.DrawIndexed(
                        meshBinding->indexCount, instanceCount);
                }
                if (encoded) {
                    ++submissionStatistics.drawCallCount;
                    ++submissionStatistics.meshDrawCallCount;
                    submissionStatistics.meshInstanceCount += instanceCount;
                    ++submissionStatistics.uniformBufferUploadCount;
                }
                break;
            }
            case Render::RenderCommandKind::DrawGlyphRun: {
                if (command.glyphRun == Render::InvalidRenderGlyphRunId ||
                    !Render::IsFinite(command.color)) {
                    encoded = InvalidArgument(
                        "Renderer contains invalid glyph draw");
                    break;
                }
                const GlyphBinding* glyphBinding = nullptr;
                for (const GlyphBinding& candidate : state_.glyphRuns) {
                    if (candidate.id == command.glyphRun) {
                        glyphBinding = &candidate;
                        break;
                    }
                }
                if (glyphBinding == nullptr ||
                    !device_->IsAlive(glyphBinding->vertexBuffer) ||
                    !device_->IsAlive(glyphBinding->indexBuffer) ||
                    !device_->IsAlive(glyphBinding->atlasTexture) ||
                    !device_->IsAlive(glyphBinding->sampler)) {
                    encoded = InvalidState(
                        "Renderer glyph is not registered or alive");
                    break;
                }
                const Aero::Rect clip =
                    state_.clips[state_.clips.Size() - 1U].bounds;
                if (IsEmpty(clip)) {
                    break;
                }
                const Media::Transform2D& transform =
                    state_.transforms[state_.transforms.Size() - 1U];
                const double opacity =
                    state_.opacities[state_.opacities.Size() - 1U];
                if (!FitsFloat(transform.m11) || !FitsFloat(transform.m12) ||
                    !FitsFloat(transform.m21) || !FitsFloat(transform.m22) ||
                    !FitsFloat(transform.dx) || !FitsFloat(transform.dy) ||
                    !FitsFloat(opacity)) {
                    encoded = InvalidArgument(
                        "Renderer glyph values exceed shader precision");
                    break;
                }
                ShaderGlyphConstants constants;
                constants.transform0[0] = static_cast<float>(transform.m11);
                constants.transform0[1] = static_cast<float>(transform.m12);
                constants.transform0[2] = static_cast<float>(transform.m21);
                constants.transform0[3] = static_cast<float>(transform.m22);
                constants.transform1[0] = static_cast<float>(transform.dx);
                constants.transform1[1] = static_cast<float>(transform.dy);
                constants.transform1[2] =
                    static_cast<float>(passLogicalWidth);
                constants.transform1[3] =
                    static_cast<float>(passLogicalHeight);
                constants.clipCount = state_.clips.Size();
                for (std::uint32_t clipIndex = 0U;
                     clipIndex < state_.clips.Size();
                     ++clipIndex) {
                    const ClipState& clipState = state_.clips[clipIndex];
                    const Media::Transform2D& clipTransform = clipState.transform;
                    const double determinant = clipTransform.m11 * clipTransform.m22 -
                        clipTransform.m12 * clipTransform.m21;
                    const double inverseM11 = clipTransform.m22 / determinant;
                    const double inverseM12 = -clipTransform.m12 / determinant;
                    const double inverseM21 = -clipTransform.m21 / determinant;
                    const double inverseM22 = clipTransform.m11 / determinant;
                    if (!FitsFloat(clipState.rect.x) || !FitsFloat(clipState.rect.y) ||
                        !FitsFloat(clipState.rect.width) || !FitsFloat(clipState.rect.height) ||
                        !FitsFloat(inverseM11) || !FitsFloat(inverseM12) ||
                        !FitsFloat(inverseM21) || !FitsFloat(inverseM22) ||
                        !FitsFloat(clipTransform.dx) || !FitsFloat(clipTransform.dy)) {
                        encoded = InvalidArgument(
                            "Renderer glyph clip values exceed shader precision");
                        break;
                    }
                    constants.clipRect[clipIndex][0] =
                        static_cast<float>(clipState.rect.x);
                    constants.clipRect[clipIndex][1] =
                        static_cast<float>(clipState.rect.y);
                    constants.clipRect[clipIndex][2] =
                        static_cast<float>(clipState.rect.width);
                    constants.clipRect[clipIndex][3] =
                        static_cast<float>(clipState.rect.height);
                    constants.clipInverse[clipIndex][0] = static_cast<float>(inverseM11);
                    constants.clipInverse[clipIndex][1] = static_cast<float>(inverseM12);
                    constants.clipInverse[clipIndex][2] = static_cast<float>(inverseM21);
                    constants.clipInverse[clipIndex][3] = static_cast<float>(inverseM22);
                    constants.clipTranslation[clipIndex][0] =
                        static_cast<float>(clipTransform.dx);
                    constants.clipTranslation[clipIndex][1] =
                        static_cast<float>(clipTransform.dy);
                }
                if (!encoded) {
                    break;
                }
                std::uint32_t batchCommandCount = 0U;
                std::uint32_t instanceCount = 0U;
                for (std::uint32_t batchIndex = commandIndex;
                     batchIndex < node.commandCount &&
                         instanceCount <
                             (state_.batchingEnabled
                              ? MaxRectangleBatchInstances
                              : 1U);
                     ++batchIndex) {
                    const Render::RenderCommand& candidate =
                        commands[node.commandOffset + batchIndex];
                    if (candidate.kind != Render::RenderCommandKind::DrawGlyphRun ||
                        candidate.glyphRun != command.glyphRun) {
                        break;
                    }
                    ++batchCommandCount;
                    if (!Render::IsFinite(candidate.color)) {
                        encoded = InvalidArgument(
                            "Renderer contains invalid glyph tint");
                        break;
                    }
                    constants.tints[instanceCount][0] = candidate.color.red;
                    constants.tints[instanceCount][1] = candidate.color.green;
                    constants.tints[instanceCount][2] = candidate.color.blue;
                    constants.tints[instanceCount][3] =
                        static_cast<float>(candidate.color.alpha * opacity);
                    ++instanceCount;
                }
                if (!encoded) {
                    break;
                }
                commandIndex += batchCommandCount - 1U;
                encoded =
                    bindGlyphPipeline(
                        blendMode);
                if (encoded) {
                    encoded = encoder.BindVertexBuffer(
                        0U, glyphBinding->vertexBuffer);
                }
                if (encoded) {
                    ++submissionStatistics.vertexBufferBindingCount;
                    encoded = encoder.BindIndexBuffer(
                        glyphBinding->indexBuffer, glyphBinding->indexType);
                }
                if (encoded) {
                    ++submissionStatistics.indexBufferBindingCount;
                    encoded = encoder.BindTextureSampler(
                        0U, glyphBinding->atlasTexture, glyphBinding->sampler);
                }
                if (encoded) {
                    ++submissionStatistics.textureSamplerBindingCount;
                    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&constants);
                    encoded = encoder.UploadBuffer(state_.glyphUniformBuffer, 0U,
                        {bytes, static_cast<std::uint32_t>(sizeof(constants))});
                }
                if (encoded) {
                    encoded = encoder.SetScissor(
                        passPhysicalScissor(clip));
                }
                if (encoded) {
                    encoded = encoder.DrawIndexed(
                        glyphBinding->indexCount, instanceCount);
                }
                if (encoded) {
                    ++submissionStatistics.drawCallCount;
                    ++submissionStatistics.glyphDrawCallCount;
                    submissionStatistics.glyphInstanceCount += instanceCount;
                    ++submissionStatistics.uniformBufferUploadCount;
                }
                break;
            }
            }
            if (!encoded) {
                break;
            }
        }
        if (!encoded || state_.transforms.Size() != 1U ||
            state_.clips.Size() != baseClipCount || state_.opacities.Size() != 1U) {
            if (encoded) {
                encoded = InvalidState("Renderer node has unbalanced state stacks");
            }
            break;
        }
    }
    return encoded;
    };

    auto applyEffect = [&](
        const Render::RenderNodeSnapshot& node,
        EffectSurface& surface) noexcept -> Base::Result<void> {
        if (node.effect.kind == Render::RenderEffectKind::None) return {};

        auto runFilterPass = [&](ResourceHandle destination,
                                 ResourceHandle source,
                                 const ShaderEffectConstants& constants)
            noexcept -> Base::Result<void> {
            pass.colorAttachments[0].target = destination;
            pass.colorAttachments[0].load = LoadOperation::Clear;
            pass.renderArea = {
                0.0, 0.0,
                static_cast<double>(surface.width),
                static_cast<double>(surface.height)};
            Base::Result<void> result = encoder.BeginRenderPass(pass);
            if (result) {
                ++submissionStatistics.renderPassCount;
                result = encoder.BindPipeline(
                    {UiShader::Effect, UiBlendMode::Opaque});
            }
            if (result) {
                ++submissionStatistics.pipelineBindingCount;
                result = encoder.BindVertexBuffer(0U, state_.vertexBuffer);
            }
            if (result) {
                ++submissionStatistics.vertexBufferBindingCount;
                result = encoder.BindUniformBuffer(
                    0U, state_.effectUniformBuffer, 0U,
                    static_cast<std::uint32_t>(sizeof(constants)));
            }
            if (result) {
                ++submissionStatistics.uniformBufferBindingCount;
                result = encoder.BindTextureSampler(
                    0U, source, state_.effectSampler);
            }
            if (result) {
                ++submissionStatistics.textureSamplerBindingCount;
                result = AppendDraw(
                    encoder,
                    state_.effectUniformBuffer,
                    constants,
                    pass.renderArea,
                    1U);
            }
            if (result) {
                ++submissionStatistics.drawCallCount;
                ++submissionStatistics.imageInstanceCount;
                ++submissionStatistics.uniformBufferUploadCount;
                result = encoder.EndRenderPass();
            }
            return result;
        };

        const double radius = std::fmin(node.effect.radius, 50.0);
        const double surfaceScaleX =
            static_cast<double>(surface.width) /
            surface.logicalBounds.width;
        const double surfaceScaleY =
            static_cast<double>(surface.height) /
            surface.logicalBounds.height;
        ShaderEffectConstants horizontal;
        horizontal.viewport[0] = static_cast<float>(surface.width);
        horizontal.viewport[1] = static_cast<float>(surface.height);
        horizontal.filter0[0] = static_cast<float>(
            radius * surfaceScaleX /
            (4.0 * static_cast<double>(surface.width)));
        Base::Result<void> result = runFilterPass(
            surface.scratch, surface.content, horizontal);
        if (!result) return result;

        ShaderEffectConstants vertical;
        vertical.viewport[0] = static_cast<float>(surface.width);
        vertical.viewport[1] = static_cast<float>(surface.height);
        vertical.filter0[1] = static_cast<float>(
            radius * surfaceScaleY /
            (4.0 * static_cast<double>(surface.height)));
        if (node.effect.kind == Render::RenderEffectKind::DropShadow) {
            constexpr double DegreesToRadians =
                0.017453292519943295769;
            const double radians = node.effect.direction * DegreesToRadians;
            const double shadowX =
                std::cos(radians) * node.effect.depth;
            const double shadowY =
                -std::sin(radians) * node.effect.depth;
            vertical.filter0[2] = static_cast<float>(
                shadowX / surface.logicalBounds.width);
            vertical.filter0[3] = static_cast<float>(
                shadowY / surface.logicalBounds.height);
            vertical.filter1[0] = 1.0F;
            vertical.tint[0] = node.effect.color.red;
            vertical.tint[1] = node.effect.color.green;
            vertical.tint[2] = node.effect.color.blue;
            vertical.tint[3] = node.effect.color.alpha *
                static_cast<float>(node.effect.opacity);
        }
        result = runFilterPass(
            surface.result, surface.scratch, vertical);
        if (!result || node.effect.kind !=
                Render::RenderEffectKind::DropShadow) {
            return result;
        }

        pass.colorAttachments[0].target = surface.result;
        pass.colorAttachments[0].load = LoadOperation::Load;
        result = encoder.BeginRenderPass(pass);
        if (result) {
            ++submissionStatistics.renderPassCount;
            result = encoder.BindPipeline(
                {UiShader::Image, UiBlendMode::Normal});
        }
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindVertexBuffer(0U, state_.vertexBuffer);
        }
        if (result) {
            ++submissionStatistics.vertexBufferBindingCount;
            result = encoder.BindUniformBuffer(
                0U, state_.imageUniformBuffer, 0U,
                static_cast<std::uint32_t>(sizeof(ShaderImageConstants)));
        }
        if (result) {
            ++submissionStatistics.uniformBufferBindingCount;
            result = encoder.BindTextureSampler(
                0U, surface.content, state_.effectSampler);
        }
        ShaderImageConstants composite;
        composite.rects[0][2] =
            static_cast<float>(surface.logicalBounds.width);
        composite.rects[0][3] =
            static_cast<float>(surface.logicalBounds.height);
        composite.sourceUvs[0][2] = 1.0F;
        composite.sourceUvs[0][3] = 1.0F;
        composite.tints[0][0] = 1.0F;
        composite.tints[0][1] = 1.0F;
        composite.tints[0][2] = 1.0F;
        composite.tints[0][3] = 1.0F;
        composite.transform0[0] = 1.0F;
        composite.transform0[3] = 1.0F;
        composite.transform1[2] =
            static_cast<float>(surface.logicalBounds.width);
        composite.transform1[3] =
            static_cast<float>(surface.logicalBounds.height);
        if (result) {
            ++submissionStatistics.textureSamplerBindingCount;
            result = AppendDraw(
                encoder,
                state_.imageUniformBuffer,
                composite,
                pass.renderArea,
                1U);
        }
        if (result) {
            ++submissionStatistics.drawCallCount;
            ++submissionStatistics.imageInstanceCount;
            ++submissionStatistics.uniformBufferUploadCount;
            result = encoder.EndRenderPass();
        }
        return result;
    };

    auto applyMask = [&](const Render::RenderNodeSnapshot& node,
                         const EffectSurface& surface) noexcept
        -> Base::Result<void> {
        if (node.mask.kind == Render::RenderMaskKind::None) return {};
        const double maskLogicalWidth = surface.logicalBounds.width;
        const double maskLogicalHeight = surface.logicalBounds.height;
        const double maskScaleX =
            static_cast<double>(surface.width) / maskLogicalWidth;
        const double maskScaleY =
            static_cast<double>(surface.height) / maskLogicalHeight;
        const Aero::Rect maskTargetClip = {
            0.0, 0.0, maskLogicalWidth, maskLogicalHeight};
        auto maskPhysicalScissor = [maskScaleX, maskScaleY](
            Aero::Rect logical) noexcept {
            return Aero::Rect{
                logical.x * maskScaleX,
                logical.y * maskScaleY,
                logical.width * maskScaleX,
                logical.height * maskScaleY};
        };
        const NodeState* state = nullptr;
        for (const NodeState& candidate : state_.nodes) {
            if (candidate.id == node.id) {
                state = &candidate;
                break;
            }
        }
        if (state == nullptr) {
            return InvalidState("Renderer mask node state is unavailable");
        }
        const Aero::Rect scissor =
            IntersectRect(state->clip.bounds, maskTargetClip);
        if (IsEmpty(scissor) ||
            node.renderSize.width <= 0.0 ||
            node.renderSize.height <= 0.0) {
            return {};
        }
        if (node.mask.kind == Render::RenderMaskKind::Solid) {
            ShaderRectConstants constants;
            constants.rects[0][2] =
                static_cast<float>(node.renderSize.width);
            constants.rects[0][3] =
                static_cast<float>(node.renderSize.height);
            constants.colors[0][0] = 1.0F;
            constants.colors[0][1] = 1.0F;
            constants.colors[0][2] = 1.0F;
            constants.colors[0][3] = node.mask.color.alpha;
            constants.transform0[0] =
                static_cast<float>(state->transform.m11);
            constants.transform0[1] =
                static_cast<float>(state->transform.m12);
            constants.transform0[2] =
                static_cast<float>(state->transform.m21);
            constants.transform0[3] =
                static_cast<float>(state->transform.m22);
            constants.transform1[0] =
                static_cast<float>(state->transform.dx);
            constants.transform1[1] =
                static_cast<float>(state->transform.dy);
            constants.transform1[2] = static_cast<float>(maskLogicalWidth);
            constants.transform1[3] = static_cast<float>(maskLogicalHeight);
            Base::Result<void> result =
                encoder.BindPipeline(
                    {UiShader::Rectangle, UiBlendMode::Mask});
            if (result) {
                ++submissionStatistics.pipelineBindingCount;
                result = encoder.BindVertexBuffer(0U, state_.vertexBuffer);
            }
            if (result) {
                ++submissionStatistics.vertexBufferBindingCount;
                result = encoder.BindUniformBuffer(
                    0U, state_.uniformBuffer, 0U,
                    static_cast<std::uint32_t>(sizeof(constants)));
            }
            if (result) {
                ++submissionStatistics.uniformBufferBindingCount;
                result = AppendDraw(
                    encoder, state_.uniformBuffer,
                    constants, maskPhysicalScissor(scissor), 1U);
            }
            if (result) {
                ++submissionStatistics.drawCallCount;
                ++submissionStatistics.rectangleInstanceCount;
                ++submissionStatistics.uniformBufferUploadCount;
            }
            return result;
        }

        ResourceHandle maskTexture;
        ResourceHandle maskSampler;
        ShaderMaskConstants constants;
        constants.rect[2] =
            static_cast<float>(node.renderSize.width);
        constants.rect[3] =
            static_cast<float>(node.renderSize.height);
        constants.transform0[0] =
            static_cast<float>(state->transform.m11);
        constants.transform0[1] =
            static_cast<float>(state->transform.m12);
        constants.transform0[2] =
            static_cast<float>(state->transform.m21);
        constants.transform0[3] =
            static_cast<float>(state->transform.m22);
        constants.transform1[0] =
            static_cast<float>(state->transform.dx);
        constants.transform1[1] =
            static_cast<float>(state->transform.dy);
        constants.transform1[2] = static_cast<float>(maskLogicalWidth);
        constants.transform1[3] = static_cast<float>(maskLogicalHeight);
        constants.mask0[0] = static_cast<float>(
            static_cast<std::uint8_t>(node.mask.kind));
        constants.mask0[1] = static_cast<float>(node.mask.mappingMode);
        constants.mask0[3] = node.mask.kind == Render::RenderMaskKind::Image
            ? node.mask.color.alpha
            : 1.0F;
        constants.mask1[0] = static_cast<float>(node.mask.tileMode);
        constants.mask1[1] = static_cast<float>(node.mask.stretch);
        constants.mask1[2] = static_cast<float>(node.mask.alignmentX);
        constants.mask1[3] = static_cast<float>(node.mask.alignmentY);

        Media::Transform2D relativeInverse;
        if (!InvertTransform(
                node.mask.relativeTransform,
                relativeInverse)) {
            return Unsupported(
                "Renderer opacity mask RelativeTransform is singular");
        }
        constants.relativeInverse0[0] =
            static_cast<float>(relativeInverse.m11);
        constants.relativeInverse0[1] =
            static_cast<float>(relativeInverse.m12);
        constants.relativeInverse0[2] =
            static_cast<float>(relativeInverse.m21);
        constants.relativeInverse0[3] =
            static_cast<float>(relativeInverse.m22);
        constants.relativeInverse1[0] =
            static_cast<float>(relativeInverse.dx);
        constants.relativeInverse1[1] =
            static_cast<float>(relativeInverse.dy);

        if (node.mask.kind == Render::RenderMaskKind::Image) {
            const ImageBinding* binding = nullptr;
            for (const ImageBinding& candidate : state_.images) {
                if (candidate.id == node.mask.image) {
                    binding = &candidate;
                    break;
                }
            }
            if (binding == nullptr ||
                !device_->IsAlive(binding->texture) ||
                !device_->IsAlive(binding->sampler)) {
                return InvalidState(
                    "Renderer opacity mask image is unavailable");
            }
            maskTexture = binding->texture;
            maskSampler = binding->sampler;

            Aero::Rect sourceUv = node.mask.sourceUv;
            if (node.mask.viewboxUnits == 1U) {
                sourceUv.x /= static_cast<double>(node.mask.imageWidth);
                sourceUv.y /= static_cast<double>(node.mask.imageHeight);
                sourceUv.width /= static_cast<double>(node.mask.imageWidth);
                sourceUv.height /= static_cast<double>(node.mask.imageHeight);
            }
            sourceUv.x = std::clamp(sourceUv.x, 0.0, 1.0);
            sourceUv.y = std::clamp(sourceUv.y, 0.0, 1.0);
            sourceUv.width = std::min(
                std::clamp(sourceUv.width, 0.0, 1.0),
                1.0 - sourceUv.x);
            sourceUv.height = std::min(
                std::clamp(sourceUv.height, 0.0, 1.0),
                1.0 - sourceUv.y);
            constants.geometry0[0] = static_cast<float>(sourceUv.x);
            constants.geometry0[1] = static_cast<float>(sourceUv.y);
            constants.geometry0[2] = static_cast<float>(sourceUv.width);
            constants.geometry0[3] = static_cast<float>(sourceUv.height);

            Aero::Rect viewport = node.mask.viewport;
            if (node.mask.viewportUnits == 0U) {
                viewport.x *= node.renderSize.width;
                viewport.y *= node.renderSize.height;
                viewport.width *= node.renderSize.width;
                viewport.height *= node.renderSize.height;
            }
            constants.geometry1[0] = static_cast<float>(viewport.x);
            constants.geometry1[1] = static_cast<float>(viewport.y);
            constants.geometry1[2] = static_cast<float>(viewport.width);
            constants.geometry1[3] = static_cast<float>(viewport.height);
            constants.geometry2[2] = static_cast<float>(node.mask.imageWidth);
            constants.geometry2[3] = static_cast<float>(node.mask.imageHeight);
        } else {
            if (node.mask.gradientRamp >= plan.GradientRamps().Size()) {
                return InvalidState(
                    "Renderer opacity mask gradient ramp is unavailable");
            }
            const Render::RenderGradientRampSnapshot& ramp =
                plan.GradientRamps()[node.mask.gradientRamp];
            const GradientRampBinding* binding = nullptr;
            for (const GradientRampBinding& candidate :
                 state_.gradientRamps) {
                if (candidate.key == ramp.brushIdentity &&
                    candidate.revision == ramp.revision) {
                    binding = &candidate;
                    break;
                }
            }
            if (binding == nullptr ||
                !device_->IsAlive(binding->texture) ||
                !device_->IsAlive(state_.effectSampler)) {
                return InvalidState(
                    "Renderer opacity mask gradient texture is unavailable");
            }
            maskTexture = binding->texture;
            maskSampler = state_.effectSampler;
            if (node.mask.kind ==
                    Render::RenderMaskKind::LinearGradient) {
                constants.geometry0[0] =
                    static_cast<float>(node.mask.startPoint.x);
                constants.geometry0[1] =
                    static_cast<float>(node.mask.startPoint.y);
                constants.geometry0[2] =
                    static_cast<float>(node.mask.endPoint.x);
                constants.geometry0[3] =
                    static_cast<float>(node.mask.endPoint.y);
            } else {
                constants.geometry1[0] =
                    static_cast<float>(node.mask.center.x);
                constants.geometry1[1] =
                    static_cast<float>(node.mask.center.y);
                constants.geometry1[2] =
                    static_cast<float>(node.mask.gradientOrigin.x);
                constants.geometry1[3] =
                    static_cast<float>(node.mask.gradientOrigin.y);
                constants.geometry2[0] =
                    static_cast<float>(node.mask.radiusX);
                constants.geometry2[1] =
                    static_cast<float>(node.mask.radiusY);
            }
        }

        Base::Result<void> result =
            encoder.BindPipeline(
                {UiShader::Mask, UiBlendMode::Mask});
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindVertexBuffer(0U, state_.vertexBuffer);
        }
        if (result) {
            ++submissionStatistics.vertexBufferBindingCount;
            result = encoder.BindUniformBuffer(
                0U, state_.maskUniformBuffer, 0U,
                static_cast<std::uint32_t>(sizeof(constants)));
        }
        if (result) {
            ++submissionStatistics.uniformBufferBindingCount;
            result = encoder.BindTextureSampler(
                0U, maskTexture, maskSampler);
        }
        if (result) {
            ++submissionStatistics.textureSamplerBindingCount;
            result = AppendDraw(
                encoder, state_.maskUniformBuffer,
                constants, maskPhysicalScissor(scissor), 1U);
        }
        if (result) {
            ++submissionStatistics.drawCallCount;
            ++submissionStatistics.imageInstanceCount;
            ++submissionStatistics.uniformBufferUploadCount;
        }
        return result;
    };

    for (std::uint32_t ordinal = state_.effectSurfaces.Size();
         ordinal > 0U; --ordinal) {
        EffectSurface& surface = state_.effectSurfaces[ordinal - 1U];
        const Render::RenderNodeSnapshot* node = nullptr;
        for (const Render::RenderNodeSnapshot& candidate : plan.Nodes()) {
            if (candidate.id == surface.owner) {
                node = &candidate;
                break;
            }
        }
        if (node == nullptr) {
            encoded = InvalidState(
                "Renderer effect surface owner is unavailable");
            break;
        }
        pass.renderArea = {
            0.0, 0.0,
            static_cast<double>(surface.width),
            static_cast<double>(surface.height)};
        pass.colorAttachments[0].target = surface.content;
        pass.colorAttachments[0].load = LoadOperation::Clear;
        encoded = encoder.BeginRenderPass(pass);
        if (encoded && !surface.empty) {
            ++submissionStatistics.renderPassCount;
            encoded = recordNodes(
                node->id, false, &surface);
        }
        if (encoded) {
            encoded = encoder.EndRenderPass();
        }
        if (encoded) {
            encoded = applyEffect(*node, surface);
        }
        const ResourceHandle output =
            node->effect.kind == Render::RenderEffectKind::None
            ? surface.content
            : surface.result;
        if (encoded && !surface.empty && node->mask.kind !=
                Render::RenderMaskKind::None) {
            pass.colorAttachments[0].target = output;
            pass.colorAttachments[0].load = LoadOperation::Load;
            encoded = encoder.BeginRenderPass(pass);
            if (encoded) {
                ++submissionStatistics.renderPassCount;
                encoded = applyMask(*node, surface);
            }
            if (encoded) encoded = encoder.EndRenderPass();
        }
        if (!encoded) break;
    }
    if (encoded) {
        pass.colorAttachments[0].target =
            target.color;
        pass.renderArea = {
            0.0, 0.0,
            static_cast<double>(renderWidth),
            static_cast<double>(renderHeight)};
        pass.colorAttachments[0].load = target.load;
        encoded = encoder.BeginRenderPass(pass);
    }
    if (encoded) {
        ++submissionStatistics.renderPassCount;
        encoded = recordNodes(
            Render::InvalidRenderNodeId,
            true,
            nullptr);
    }
    if (encoded) {
        encoded = encoder.EndRenderPass();
    }
    Base::Result<FenceValue> finished = encoded
        ? encoder.Finish()
        : Base::Result<FenceValue>(encoded.GetStatus());
    if (!finished) {
        return finished.GetStatus();
    }
    submissionStatistics.sourceCommandCount = plan.Commands().Size();
    submissionStatistics.drawPacketCount =
        submissionStatistics.rectangleInstanceCount +
        submissionStatistics.imageInstanceCount +
        submissionStatistics.meshInstanceCount +
        submissionStatistics.glyphInstanceCount;
    submissionStatistics.batchCount = submissionStatistics.drawCallCount;
    submissionStatistics.mergedPacketCount =
        submissionStatistics.drawPacketCount > submissionStatistics.batchCount
        ? submissionStatistics.drawPacketCount - submissionStatistics.batchCount
        : 0U;
    submissionStatistics.barrierCount =
        submissionStatistics.renderPassCount > 0U
        ? submissionStatistics.renderPassCount - 1U
        : 0U;
    submissionStatistics.batchingEnabled = state_.batchingEnabled;
    state_.lastStatistics = submissionStatistics;
    return std::move(finished).Value();
}

} // namespace Aero::Render
