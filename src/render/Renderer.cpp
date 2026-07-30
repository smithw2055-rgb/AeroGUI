#include "Renderer.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Render {
using namespace Aero::Rhi;
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

Presentation::Transform2D IdentityTransform() noexcept {
    return {};
}

Presentation::Transform2D Translation(double x, double y) noexcept {
    Presentation::Transform2D value;
    value.dx = x;
    value.dy = y;
    return value;
}

// Transforms use row-vector affine form: (x, y, 1) * M.
Presentation::Transform2D Compose(
    const Presentation::Transform2D& first,
    const Presentation::Transform2D& second) noexcept {
    Presentation::Transform2D output;
    output.m11 = first.m11 * second.m11 + first.m12 * second.m21;
    output.m12 = first.m11 * second.m12 + first.m12 * second.m22;
    output.m21 = first.m21 * second.m11 + first.m22 * second.m21;
    output.m22 = first.m21 * second.m12 + first.m22 * second.m22;
    output.dx = first.dx * second.m11 + first.dy * second.m21 + second.dx;
    output.dy = first.dx * second.m12 + first.dy * second.m22 + second.dy;
    return output;
}

void TransformPoint(
    const Presentation::Transform2D& transform,
    double x,
    double y,
    double& outputX,
    double& outputY) noexcept {
    outputX = x * transform.m11 + y * transform.m21 + transform.dx;
    outputY = x * transform.m12 + y * transform.m22 + transform.dy;
}

Presentation::Rect TransformBounds(
    const Presentation::Transform2D& transform,
    Presentation::Rect rect) noexcept {
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

Presentation::Rect IntersectRect(Presentation::Rect left, Presentation::Rect right) noexcept {
    const double x0 = std::fmax(left.x, right.x);
    const double y0 = std::fmax(left.y, right.y);
    const double x1 = std::fmin(left.x + left.width, right.x + right.width);
    const double y1 = std::fmin(left.y + left.height, right.y + right.height);
    return {x0, y0, std::fmax(0.0, x1 - x0), std::fmax(0.0, y1 - y0)};
}

bool IsEmpty(Presentation::Rect rect) noexcept {
    return rect.width <= 0.0 || rect.height <= 0.0;
}

bool FitsFloat(double value) noexcept {
    return std::isfinite(value) &&
        value >= -static_cast<double>(std::numeric_limits<float>::max()) &&
        value <= static_cast<double>(std::numeric_limits<float>::max());
}

constexpr std::uint32_t MaxShaderClips = 32U;
constexpr std::uint32_t MaxRectangleBatchInstances = 64U;

struct ClipState final {
    Presentation::Rect rect;
    Presentation::Transform2D transform;
    Presentation::Rect bounds;
};

struct ShaderRectConstants final {
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

struct ShaderImageConstants final {
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

struct ShaderMeshConstants final {
    float tints[MaxRectangleBatchInstances][4]{};
    float transform0[4]{};
    float transform1[4]{};
    float clipRect[MaxShaderClips][4]{};
    float clipInverse[MaxShaderClips][4]{};
    float clipTranslation[MaxShaderClips][4]{};
    std::uint32_t clipCount = 0U;
    float padding[3]{};
};

struct ShaderGlyphConstants final {
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
static_assert(sizeof(ShaderMeshConstants) % 16U == 0U,
    "Renderer constant buffers must be float4 aligned");
static_assert(sizeof(ShaderGlyphConstants) % 16U == 0U,
    "Renderer constant buffers must be float4 aligned");
static_assert(sizeof(ShaderGlyphConstants) <= 64U * 1024U,
    "Renderer constant buffers must not exceed 64 KiB");

Base::Result<void> PushClipState(
    Base::Vector<ClipState>& clips,
    Presentation::Rect rect,
    const Presentation::Transform2D& transform) noexcept {
    if (clips.Size() >= MaxShaderClips) {
        return Unsupported("Renderer clip nesting exceeds shader capacity");
    }
    const double determinant = transform.m11 * transform.m22 -
        transform.m12 * transform.m21;
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-12) {
        return Unsupported("Renderer cannot clip through a singular transform");
    }
    Presentation::Rect bounds = TransformBounds(transform, rect);
    if (!Presentation::IsValidLayoutRect(bounds)) {
        return InvalidArgument("Renderer clip bounds are invalid");
    }
    if (!clips.Empty()) {
        bounds = IntersectRect(clips[clips.Size() - 1U].bounds, bounds);
    }
    return clips.TryPushBack({rect, transform, bounds});
}

struct NodeState final {
    Presentation::RenderNodeId id = Presentation::InvalidRenderNodeId;
    Presentation::Transform2D transform;
    ClipState clip;
    bool clipsToBounds = false;
    std::uint32_t parentIndex = UINT32_MAX;
    Presentation::RenderNodeId containingEffect =
        Presentation::InvalidRenderNodeId;
    std::uint32_t containingEffectCount = 0U;
};

template <typename Constants>
Base::Result<void> AppendDraw(
    CommandEncoder& encoder,
    ResourceHandle uniformBuffer,
    const Constants& constants,
    Presentation::Rect scissor,
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

struct ImageBinding final {
    Presentation::RenderImageId id = Presentation::InvalidRenderImageId;
    ResourceHandle texture;
    ResourceHandle sampler;
};

struct MeshBinding final {
    Presentation::RenderMeshId id = Presentation::InvalidRenderMeshId;
    ResourceHandle vertexBuffer;
    ResourceHandle indexBuffer;
    std::uint32_t indexCount = 0U;
    IndexType indexType = IndexType::UInt16;
};

struct GlyphBinding final {
    Presentation::RenderGlyphRunId id = Presentation::InvalidRenderGlyphRunId;
    ResourceHandle vertexBuffer;
    ResourceHandle indexBuffer;
    std::uint32_t indexCount = 0U;
    ResourceHandle atlasTexture;
    ResourceHandle sampler;
    IndexType indexType = IndexType::UInt16;
};

struct EffectSurface final {
    ResourceHandle target;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

} // namespace

struct Renderer::Impl final {
    explicit Impl(
        RhiDevice& device,
        Base::IAllocator* allocator) noexcept
        : device(&device),
          nodes(allocator),
          transforms(allocator),
          clips(allocator),
          opacities(allocator),
          nodePath(allocator),
          images(allocator),
          meshes(allocator),
          glyphRuns(allocator),
          effectSurfaces(allocator) {}

    RhiDevice* device = nullptr;
    ResourceHandle vertexBuffer;
    ResourceHandle uniformBuffer;
    std::array<ResourceHandle, 4U>
        rectanglePipelines;
    ResourceHandle imageUniformBuffer;
    std::array<ResourceHandle, 4U>
        imagePipelines;
    ResourceHandle meshUniformBuffer;
    std::array<ResourceHandle, 4U>
        meshPipelines;
    ResourceHandle glyphUniformBuffer;
    std::array<ResourceHandle, 4U>
        glyphPipelines;
    Base::Vector<NodeState> nodes;
    Base::Vector<Presentation::Transform2D> transforms;
    Base::Vector<ClipState> clips;
    Base::Vector<double> opacities;
    Base::Vector<std::uint32_t> nodePath;
    Base::Vector<ImageBinding> images;
    Base::Vector<MeshBinding> meshes;
    Base::Vector<GlyphBinding> glyphRuns;
    Base::Vector<EffectSurface> effectSurfaces;
    ResourceHandle effectSampler;
    RendererStatistics lastStatistics;
    bool batchingEnabled = true;
    bool initialized = false;
};

Renderer::Renderer(
    RhiDevice& device,
    const RendererShaderSet& shaders,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      shaders_(shaders),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

Renderer::~Renderer() noexcept {
    Shutdown();
}

Base::Result<void> Renderer::Initialize() noexcept {
    if (impl_ != nullptr && impl_->initialized) {
        return {};
    }
    if (device_ == nullptr || device_->Backend().IsDeviceLost()) {
        return NotInitialized("Renderer requires a ready graphics device");
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory("Failed to allocate Renderer backend state");
        }
        impl_ = new (memory) Impl(*device_, allocator_);
    }

    BufferDescriptor vertexDescriptor;
    vertexDescriptor.sizeBytes = 32U;
    vertexDescriptor.usage = BufferUsage::Vertex;
    Base::Result<ResourceHandle> vertex = device_->CreateBuffer(vertexDescriptor);
    if (!vertex) {
        return vertex.GetStatus();
    }
    impl_->vertexBuffer = vertex.Value();

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
    impl_->effectSampler =
        effectSampler.Value();

    BufferDescriptor uniformDescriptor;
    uniformDescriptor.sizeBytes = sizeof(ShaderRectConstants);
    uniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> uniform = device_->CreateBuffer(uniformDescriptor);
    if (!uniform) {
        Shutdown();
        return uniform.GetStatus();
    }
    impl_->uniformBuffer = uniform.Value();

    BufferDescriptor imageUniformDescriptor;
    imageUniformDescriptor.sizeBytes = sizeof(ShaderImageConstants);
    imageUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> imageUniform =
        device_->CreateBuffer(imageUniformDescriptor);
    if (!imageUniform) {
        Shutdown();
        return imageUniform.GetStatus();
    }
    impl_->imageUniformBuffer = imageUniform.Value();

    BufferDescriptor meshUniformDescriptor;
    meshUniformDescriptor.sizeBytes = sizeof(ShaderMeshConstants);
    meshUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> meshUniform =
        device_->CreateBuffer(meshUniformDescriptor);
    if (!meshUniform) {
        Shutdown();
        return meshUniform.GetStatus();
    }
    impl_->meshUniformBuffer = meshUniform.Value();

    BufferDescriptor glyphUniformDescriptor;
    glyphUniformDescriptor.sizeBytes = sizeof(ShaderGlyphConstants);
    glyphUniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> glyphUniform =
        device_->CreateBuffer(glyphUniformDescriptor);
    if (!glyphUniform) {
        Shutdown();
        return glyphUniform.GetStatus();
    }
    impl_->glyphUniformBuffer = glyphUniform.Value();

    PipelineDescriptor pipelineDescriptor;
    pipelineDescriptor.vertexShader = shaders_.rectangleVertex;
    pipelineDescriptor.fragmentShader = shaders_.rectangleFragment;
    pipelineDescriptor.vertexLayout.bufferCount = 1U;
    pipelineDescriptor.vertexLayout.attributeCount = 1U;
    pipelineDescriptor.vertexLayout.buffers[0].stride = 8U;
    pipelineDescriptor.vertexLayout.attributes[0].location = 0U;
    pipelineDescriptor.vertexLayout.attributes[0].bufferSlot = 0U;
    pipelineDescriptor.vertexLayout.attributes[0].format = VertexFormat::Float2;
    pipelineDescriptor.vertexLayout.attributes[0].offset = 0U;
    pipelineDescriptor.topology = PrimitiveTopology::TriangleStrip;
    pipelineDescriptor.blend.enabled = true;
    pipelineDescriptor.blend.color.source = BlendFactor::SourceAlpha;
    pipelineDescriptor.blend.color.destination = BlendFactor::OneMinusSourceAlpha;
    pipelineDescriptor.blend.alpha.source = BlendFactor::One;
    pipelineDescriptor.blend.alpha.destination = BlendFactor::OneMinusSourceAlpha;
    pipelineDescriptor.colorFormat = shaders_.colorFormat;
    pipelineDescriptor.raster.scissorEnabled = true;
    PipelineDescriptor imagePipelineDescriptor = pipelineDescriptor;
    imagePipelineDescriptor.vertexShader = shaders_.imageVertex;
    imagePipelineDescriptor.fragmentShader = shaders_.imageFragment;
    PipelineDescriptor meshPipelineDescriptor = pipelineDescriptor;
    meshPipelineDescriptor.vertexShader = shaders_.meshVertex;
    meshPipelineDescriptor.fragmentShader = shaders_.meshFragment;
    meshPipelineDescriptor.vertexLayout.buffers[0].stride = 28U;
    meshPipelineDescriptor.vertexLayout.attributeCount = 3U;
    meshPipelineDescriptor.vertexLayout.attributes[1].location = 1U;
    meshPipelineDescriptor.vertexLayout.attributes[1].bufferSlot = 0U;
    meshPipelineDescriptor.vertexLayout.attributes[1].format = VertexFormat::Float4;
    meshPipelineDescriptor.vertexLayout.attributes[1].offset = 8U;
    meshPipelineDescriptor.vertexLayout.attributes[2].location = 2U;
    meshPipelineDescriptor.vertexLayout.attributes[2].bufferSlot = 0U;
    meshPipelineDescriptor.vertexLayout.attributes[2].format = VertexFormat::Float;
    meshPipelineDescriptor.vertexLayout.attributes[2].offset = 24U;
    meshPipelineDescriptor.topology = PrimitiveTopology::TriangleList;
    PipelineDescriptor glyphPipelineDescriptor = pipelineDescriptor;
    glyphPipelineDescriptor.vertexShader = shaders_.glyphVertex;
    glyphPipelineDescriptor.fragmentShader = shaders_.glyphFragment;
    glyphPipelineDescriptor.vertexLayout.buffers[0].stride = 16U;
    glyphPipelineDescriptor.vertexLayout.attributeCount = 2U;
    glyphPipelineDescriptor.vertexLayout.attributes[1].location = 1U;
    glyphPipelineDescriptor.vertexLayout.attributes[1].bufferSlot = 0U;
    glyphPipelineDescriptor.vertexLayout.attributes[1].format = VertexFormat::Float2;
    glyphPipelineDescriptor.vertexLayout.attributes[1].offset = 8U;
    glyphPipelineDescriptor.topology = PrimitiveTopology::TriangleList;
    auto configureBlend = [](
        PipelineDescriptor& descriptor,
        std::uint32_t mode) noexcept {
        descriptor.blend.color.operation =
            BlendOperation::Add;
        descriptor.blend.alpha.operation =
            BlendOperation::Add;
        descriptor.blend.alpha.source =
            BlendFactor::One;
        descriptor.blend.alpha.destination =
            BlendFactor::OneMinusSourceAlpha;
        switch (mode) {
        case 1U:
            descriptor.blend.color.source =
                BlendFactor::DestinationColor;
            descriptor.blend.color.destination =
                BlendFactor::Zero;
            break;
        case 2U:
            descriptor.blend.color.source =
                BlendFactor::One;
            descriptor.blend.color.destination =
                BlendFactor::OneMinusSourceColor;
            break;
        case 3U:
            descriptor.blend.color.source =
                BlendFactor::SourceAlpha;
            descriptor.blend.color.destination =
                BlendFactor::One;
            break;
        default:
            descriptor.blend.color.source =
                BlendFactor::SourceAlpha;
            descriptor.blend.color.destination =
                BlendFactor::OneMinusSourceAlpha;
            break;
        }
    };
    for (std::uint32_t mode = 0U;
         mode < 4U; ++mode) {
        configureBlend(pipelineDescriptor, mode);
        configureBlend(
            imagePipelineDescriptor, mode);
        configureBlend(
            meshPipelineDescriptor, mode);
        configureBlend(
            glyphPipelineDescriptor, mode);
        Base::Result<ResourceHandle> rectangle =
            device_->CreatePipeline(
                pipelineDescriptor);
        Base::Result<ResourceHandle> image =
            rectangle
            ? device_->CreatePipeline(
                imagePipelineDescriptor)
            : Base::Result<ResourceHandle>(
                rectangle.GetStatus());
        Base::Result<ResourceHandle> mesh =
            image
            ? device_->CreatePipeline(
                meshPipelineDescriptor)
            : Base::Result<ResourceHandle>(
                image.GetStatus());
        Base::Result<ResourceHandle> glyph =
            mesh
            ? device_->CreatePipeline(
                glyphPipelineDescriptor)
            : Base::Result<ResourceHandle>(
                mesh.GetStatus());
        if (!rectangle || !image ||
            !mesh || !glyph) {
            const Base::Status failure =
                !rectangle
                ? rectangle.GetStatus()
                : !image
                ? image.GetStatus()
                : !mesh
                ? mesh.GetStatus()
                : glyph.GetStatus();
            Shutdown();
            return failure;
        }
        impl_->rectanglePipelines[mode] =
            rectangle.Value();
        impl_->imagePipelines[mode] =
            image.Value();
        impl_->meshPipelines[mode] =
            mesh.Value();
        impl_->glyphPipelines[mode] =
            glyph.Value();
    }
    impl_->initialized = true;
    return {};
}

void Renderer::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    const FenceValue retireFence = device_ != nullptr
        ? device_->LastSubmittedFence()
        : 0U;
    if (device_ != nullptr) {
        for (const EffectSurface& surface :
             impl_->effectSurfaces) {
            if (surface.target.IsValid()) {
                static_cast<void>(
                    device_->DestroyResource(
                        surface.target,
                        retireFence));
            }
        }
        if (impl_->effectSampler.IsValid()) {
            static_cast<void>(
                device_->DestroyResource(
                    impl_->effectSampler,
                    retireFence));
        }
        for (ResourceHandle pipeline :
             impl_->glyphPipelines) {
            if (pipeline.IsValid()) {
                static_cast<void>(
                    device_->DestroyResource(
                        pipeline, retireFence));
            }
        }
        if (impl_->glyphUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                impl_->glyphUniformBuffer, retireFence));
        }
        for (ResourceHandle pipeline :
             impl_->meshPipelines) {
            if (pipeline.IsValid()) {
                static_cast<void>(
                    device_->DestroyResource(
                        pipeline, retireFence));
            }
        }
        if (impl_->meshUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                impl_->meshUniformBuffer, retireFence));
        }
        for (ResourceHandle pipeline :
             impl_->imagePipelines) {
            if (pipeline.IsValid()) {
                static_cast<void>(
                    device_->DestroyResource(
                        pipeline, retireFence));
            }
        }
        if (impl_->imageUniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(
                impl_->imageUniformBuffer, retireFence));
        }
        for (ResourceHandle pipeline :
             impl_->rectanglePipelines) {
            if (pipeline.IsValid()) {
                static_cast<void>(
                    device_->DestroyResource(
                        pipeline, retireFence));
            }
        }
        if (impl_->uniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(impl_->uniformBuffer, retireFence));
        }
        if (impl_->vertexBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(impl_->vertexBuffer, retireFence));
        }
    }
    impl_->~Impl();
    allocator_->Deallocate(impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

Base::Result<void> Renderer::RegisterImage(
    Presentation::RenderImageId image,
    ResourceHandle texture,
    ResourceHandle sampler) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    if (image == Presentation::InvalidRenderImageId ||
        texture.type != ResourceType::Texture ||
        sampler.type != ResourceType::Sampler || !device_->IsAlive(texture) ||
        !device_->IsAlive(sampler)) {
        return InvalidArgument(
            "Renderer image registration requires live texture and sampler resources");
    }
    for (const ImageBinding& binding : impl_->images) {
        if (binding.id == image) {
            return InvalidState("Renderer image ID is already registered");
        }
    }
    return impl_->images.TryPushBack({image, texture, sampler});
}

Base::Result<void> Renderer::UnregisterImage(
    Presentation::RenderImageId image) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    for (std::uint32_t index = 0U; index < impl_->images.Size(); ++index) {
        if (impl_->images[index].id == image) {
            for (std::uint32_t next = index + 1U;
                 next < impl_->images.Size(); ++next) {
                impl_->images[next - 1U] = impl_->images[next];
            }
            impl_->images.PopBack();
            return {};
        }
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Renderer image ID is not registered");
}

Base::Result<void> Renderer::RegisterMesh(
    Presentation::RenderMeshId mesh,
    ResourceHandle vertexBuffer,
    ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    IndexType indexType) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    if (mesh == Presentation::InvalidRenderMeshId || indexCount == 0U ||
        vertexBuffer.type != ResourceType::Buffer ||
        indexBuffer.type != ResourceType::Buffer || !device_->IsAlive(vertexBuffer) ||
        !device_->IsAlive(indexBuffer)) {
        return InvalidArgument(
            "Renderer mesh registration requires live vertex and index buffers");
    }
    for (const MeshBinding& binding : impl_->meshes) {
        if (binding.id == mesh) {
            return InvalidState("Renderer mesh ID is already registered");
        }
    }
    return impl_->meshes.TryPushBack(
        {mesh, vertexBuffer, indexBuffer, indexCount, indexType});
}

Base::Result<void> Renderer::UnregisterMesh(
    Presentation::RenderMeshId mesh) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    for (std::uint32_t index = 0U; index < impl_->meshes.Size(); ++index) {
        if (impl_->meshes[index].id == mesh) {
            for (std::uint32_t next = index + 1U;
                 next < impl_->meshes.Size(); ++next) {
                impl_->meshes[next - 1U] = impl_->meshes[next];
            }
            impl_->meshes.PopBack();
            return {};
        }
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Renderer mesh ID is not registered");
}

Base::Result<void> Renderer::RegisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun,
    ResourceHandle vertexBuffer,
    ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    ResourceHandle atlasTexture,
    ResourceHandle sampler,
    IndexType indexType) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    if (glyphRun == Presentation::InvalidRenderGlyphRunId || indexCount == 0U ||
        vertexBuffer.type != ResourceType::Buffer ||
        indexBuffer.type != ResourceType::Buffer ||
        atlasTexture.type != ResourceType::Texture ||
        sampler.type != ResourceType::Sampler || !device_->IsAlive(vertexBuffer) ||
        !device_->IsAlive(indexBuffer) || !device_->IsAlive(atlasTexture) ||
        !device_->IsAlive(sampler)) {
        return InvalidArgument(
            "Renderer glyph registration requires live buffers, atlas, and sampler");
    }
    for (const GlyphBinding& binding : impl_->glyphRuns) {
        if (binding.id == glyphRun) {
            return InvalidState("Renderer glyph ID is already registered");
        }
    }
    return impl_->glyphRuns.TryPushBack(
        {glyphRun, vertexBuffer, indexBuffer, indexCount, atlasTexture, sampler,
            indexType});
}

Base::Result<void> Renderer::UnregisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer backend is not initialized");
    }
    for (std::uint32_t index = 0U; index < impl_->glyphRuns.Size(); ++index) {
        if (impl_->glyphRuns[index].id == glyphRun) {
            for (std::uint32_t next = index + 1U;
                 next < impl_->glyphRuns.Size(); ++next) {
                impl_->glyphRuns[next - 1U] = impl_->glyphRuns[next];
            }
            impl_->glyphRuns.PopBack();
            return {};
        }
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Renderer glyph ID is not registered");
}

bool Renderer::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

RendererStatistics
Renderer::LastStatistics() const noexcept {
    return impl_ != nullptr ? impl_->lastStatistics
                            : RendererStatistics{};
}

void Renderer::SetBatchingEnabled(
    bool enabled) noexcept {
    if (impl_ != nullptr) {
        impl_->batchingEnabled = enabled;
    }
}

bool Renderer::IsBatchingEnabled() const noexcept {
    return impl_ == nullptr ||
        impl_->batchingEnabled;
}

Base::Result<CommandList> Renderer::Record(
    const Presentation::RenderPlan& plan,
    const RenderTarget& target) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("Renderer is not initialized");
    }
    if (device_->Backend().IsDeviceLost()) {
        return InvalidState("Cannot record a RenderPlan for a lost graphics device");
    }
    if (!target.color.IsValid() ||
        (target.color.type != ResourceType::RenderTarget &&
         target.color.type != ResourceType::Texture) ||
        !device_->IsAlive(target.color) ||
        target.width == 0U || target.height == 0U) {
        return InvalidArgument("Renderer target is invalid or unavailable");
    }

    const std::uint32_t width = target.width;
    const std::uint32_t height = target.height;
    std::uint32_t effectCount = 0U;
    for (const Presentation::RenderNodeSnapshot& node :
         plan.Nodes()) {
        if (node.effect.kind !=
            Presentation::RenderEffectKind::None) {
            ++effectCount;
        }
    }
    for (std::uint32_t index = effectCount;
         index < impl_->effectSurfaces.Size();
         ++index) {
        if (impl_->effectSurfaces[index].
                target.IsValid()) {
            static_cast<void>(
                device_->DestroyResource(
                    impl_->effectSurfaces[index].
                        target,
                    device_->LastSubmittedFence()));
        }
    }
    Base::Result<void> resizedEffects =
        impl_->effectSurfaces.TryResize(
            effectCount);
    if (!resizedEffects) {
        return resizedEffects.GetStatus();
    }
    for (EffectSurface& surface :
         impl_->effectSurfaces) {
        if (surface.target.IsValid() &&
            (surface.width != width ||
             surface.height != height ||
             !device_->IsAlive(
                 surface.target))) {
            static_cast<void>(
                device_->DestroyResource(
                    surface.target,
                    device_->LastSubmittedFence()));
            surface = {};
        }
        if (!surface.target.IsValid()) {
            TextureResourceDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.format =
                shaders_.colorFormat;
            descriptor.usage =
                TextureUsageBit(
                    TextureUsage::Sampled) |
                TextureUsageBit(
                    TextureUsage::RenderTarget);
            Base::Result<ResourceHandle> created =
                device_->CreateRenderTarget(
                    descriptor);
            if (!created) {
                return created.GetStatus();
            }
            surface.target = created.Value();
            surface.width = width;
            surface.height = height;
        }
    }

    CommandEncoder encoder(allocator_);
    static constexpr float UnitQuad[] = {
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 1.0F, 1.0F, 1.0F};
    const auto* vertexBytes = reinterpret_cast<const std::uint8_t*>(UnitQuad);
    Base::Result<void> encoded = encoder.UploadBuffer(
        impl_->vertexBuffer, 0U,
        {vertexBytes, static_cast<std::uint32_t>(sizeof(UnitQuad))});
    if (!encoded) {
        return encoded.GetStatus();
    }
    RenderPassDescriptor pass;
    pass.renderArea = {
        0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0].target = target.color;
    pass.colorAttachments[0].load = LoadOperation::Clear;
    pass.colorAttachments[0].store = StoreOperation::Store;
    pass.colorAttachments[0].clearColor = {0.0F, 0.0F, 0.0F, 0.0F};
    RendererStatistics submissionStatistics;
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
            encoder.BindPipeline(
                impl_->rectanglePipelines[
                    blendMode]);
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindVertexBuffer(0U, impl_->vertexBuffer);
        }
        if (result) {
            ++submissionStatistics.vertexBufferBindingCount;
            result = encoder.BindUniformBuffer(
                0U, impl_->uniformBuffer, 0U,
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
            encoder.BindPipeline(
                impl_->imagePipelines[blendMode]);
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindVertexBuffer(0U, impl_->vertexBuffer);
        }
        if (result) {
            ++submissionStatistics.vertexBufferBindingCount;
            result = encoder.BindUniformBuffer(
                0U, impl_->imageUniformBuffer, 0U,
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
            encoder.BindPipeline(
                impl_->meshPipelines[blendMode]);
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindUniformBuffer(
                0U, impl_->meshUniformBuffer, 0U,
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
            encoder.BindPipeline(
                impl_->glyphPipelines[blendMode]);
        if (result) {
            ++submissionStatistics.pipelineBindingCount;
            result = encoder.BindUniformBuffer(
                0U, impl_->glyphUniformBuffer, 0U,
                static_cast<std::uint32_t>(sizeof(ShaderGlyphConstants)));
        }
        if (result) {
            ++submissionStatistics.uniformBufferBindingCount;
            activePipeline = ActivePipeline::Glyph;
            activeBlendMode = blendMode;
        }
        return result;
    };

    const Presentation::Rect targetClip = {
        0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
    const Base::Span<const Presentation::RenderCommand> commands = plan.Commands();
    auto recordNodes = [&](
        Presentation::RenderNodeId effectRoot,
        bool mainPass) noexcept
        -> Base::Result<void> {
    activePipeline = ActivePipeline::None;
    activeBlendMode = UINT32_MAX;
    impl_->nodes.Clear();
    std::uint32_t effectOrdinal = 0U;
    for (const Presentation::RenderNodeSnapshot& node : plan.Nodes()) {
        const std::uint32_t nodeEffectSurfaceIndex =
            effectOrdinal;
        if (node.effect.kind !=
            Presentation::RenderEffectKind::None) {
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
            impl_->nodes) {
            duplicateId =
                duplicateId ||
                existing.id == node.id;
        }
        if (node.id == Presentation::InvalidRenderNodeId) {
            encoded = InvalidArgument(
                "Renderer node identity is invalid");
            break;
        }
        if (duplicateId) {
            encoded = InvalidArgument(
                "Renderer node identity is duplicated");
            break;
        }
        if (!Presentation::IsValidLayoutRect(
                node.layoutSlot)) {
            encoded = InvalidArgument(
                "Renderer node layout slot is invalid");
            break;
        }
        if (!Presentation::IsValidLayoutRect(node.clip)) {
            encoded = InvalidArgument(
                "Renderer node clip is invalid");
            break;
        }
        if (!Presentation::IsValidLayoutSize(
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
        if (static_cast<std::uint8_t>(
                node.effect.kind) >
                static_cast<std::uint8_t>(
                    Presentation::
                        RenderEffectKind::DropShadow) ||
            !std::isfinite(node.effect.radius) ||
            node.effect.radius < 0.0 ||
            !std::isfinite(node.effect.direction) ||
            !std::isfinite(node.effect.depth) ||
            node.effect.depth < 0.0 ||
            !Presentation::IsValidOpacity(
                node.effect.opacity) ||
            !Presentation::IsFinite(
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

        Presentation::Transform2D parentTransform = IdentityTransform();
        Presentation::Rect parentClip = targetClip;
        std::uint32_t parentIndex = UINT32_MAX;
        Presentation::RenderNodeId containingEffect =
            Presentation::InvalidRenderNodeId;
        std::uint32_t containingEffectCount = 0U;
        if (node.parentId != Presentation::InvalidRenderNodeId) {
            const NodeState* parent = nullptr;
            for (std::uint32_t index = impl_->nodes.Size(); index > 0U; --index) {
                const NodeState& candidate = impl_->nodes[index - 1U];
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
            containingEffect =
                parent->containingEffect;
            containingEffectCount =
                parent->containingEffectCount;
        }
        if (node.effect.kind !=
            Presentation::RenderEffectKind::None) {
            containingEffect = node.id;
            ++containingEffectCount;
        }

        const Presentation::Transform2D nodeTransform = Compose(
            Compose(
                node.renderTransform,
                Translation(
                    node.layoutSlot.x,
                    node.layoutSlot.y)),
            parentTransform);
        ClipState nodeClip{node.clip, parentTransform, parentClip};
        if (node.clipsToBounds) {
            const Presentation::Rect nodeBounds =
                TransformBounds(parentTransform, node.clip);
            if (!Presentation::IsValidLayoutRect(nodeBounds)) {
                encoded = InvalidArgument("Renderer node clip bounds are invalid");
                break;
            }
            nodeClip.bounds = IntersectRect(parentClip, nodeBounds);
        }
        Base::Result<void> appendedNode = impl_->nodes.TryPushBack(
            {node.id, nodeTransform, nodeClip, node.clipsToBounds,
             parentIndex, containingEffect,
             containingEffectCount});
        if (!appendedNode) {
            encoded = appendedNode;
            break;
        }

        impl_->transforms.Clear();
        impl_->clips.Clear();
        impl_->opacities.Clear();
        impl_->nodePath.Clear();
        // Drawing code always has a current clip. Keep the render target as
        // that root clip, then add only ancestors that explicitly opt into
        // ClipToBounds.
        Base::Result<void> rootClip = impl_->clips.TryPushBack(
            {targetClip, IdentityTransform(), targetClip});
        if (!rootClip) {
            encoded = rootClip;
            break;
        }
        std::uint32_t nodePathIndex = impl_->nodes.Size() - 1U;
        while (true) {
            if (impl_->nodePath.Size() >= MaxShaderClips) {
                encoded = Unsupported(
                    "Renderer layout clip nesting exceeds shader capacity");
                break;
            }
            Base::Result<void> pathAppended = impl_->nodePath.TryPushBack(nodePathIndex);
            if (!pathAppended) {
                encoded = pathAppended;
                break;
            }
            const std::uint32_t nextParent =
                impl_->nodes[nodePathIndex].parentIndex;
            if (nextParent == UINT32_MAX) {
                break;
            }
            nodePathIndex = nextParent;
        }
        if (!encoded) {
            break;
        }
        for (std::uint32_t index = impl_->nodePath.Size(); index > 0U; --index) {
            const NodeState& pathNode =
                impl_->nodes[impl_->nodePath[index - 1U]];
            if (!pathNode.clipsToBounds) {
                continue;
            }
            Base::Result<void> pushed = impl_->clips.TryPushBack(pathNode.clip);
            if (!pushed) {
                encoded = pushed;
                break;
            }
        }
        if (!encoded) {
            break;
        }
        if (!(impl_->transforms.TryPushBack(nodeTransform)) ||
            !(impl_->opacities.TryPushBack(1.0))) {
            encoded = OutOfMemory("Failed to allocate Renderer state stack");
            break;
        }
        const std::uint32_t baseClipCount = impl_->clips.Size();

        bool isInRequestedSubtree =
            effectRoot ==
                Presentation::InvalidRenderNodeId;
        for (std::uint32_t pathIndex = 0U;
             pathIndex < impl_->nodePath.Size();
             ++pathIndex) {
            const Presentation::RenderNodeId pathId =
                impl_->nodes[
                    impl_->nodePath[
                        pathIndex]].id;
            if (pathId == effectRoot) {
                isInRequestedSubtree = true;
            }
        }
        const NodeState& currentNodeState =
            impl_->nodes[
                impl_->nodes.Size() - 1U];
        if (containingEffectCount > 1U) {
            encoded = Unsupported(
                "Renderer does not support nested effects");
            break;
        }
        bool shouldDraw =
            mainPass
            ? currentNodeState.containingEffect ==
                Presentation::InvalidRenderNodeId
            : isInRequestedSubtree;
        if (mainPass &&
            currentNodeState.containingEffect ==
                node.id) {
            if (nodeEffectSurfaceIndex >=
                    impl_->effectSurfaces.Size() ||
                !impl_->effectSurfaces[
                    nodeEffectSurfaceIndex].
                    target.IsValid()) {
                encoded = InvalidState(
                    "Renderer effect surface is unavailable");
                break;
            }
            Presentation::Rect effectBounds =
                TransformBounds(
                    nodeTransform,
                    {0.0, 0.0,
                     node.renderSize.width,
                     node.renderSize.height});
            const double effectPadding =
                std::fmin(
                    node.effect.radius,
                    50.0) +
                (node.effect.kind ==
                     Presentation::
                         RenderEffectKind::DropShadow
                 ? node.effect.depth
                 : 0.0);
            effectBounds.x -= effectPadding;
            effectBounds.y -= effectPadding;
            effectBounds.width +=
                effectPadding * 2.0;
            effectBounds.height +=
                effectPadding * 2.0;
            effectBounds = IntersectRect(
                effectBounds, targetClip);
            auto appendEffectSample = [&](
                double offsetX,
                double offsetY,
                Base::Color tint) noexcept
                -> Base::Result<void> {
                ShaderImageConstants constants;
                constants.transform0[0] = 1.0F;
                constants.transform0[3] = 1.0F;
                constants.transform1[2] =
                    static_cast<float>(width);
                constants.transform1[3] =
                    static_cast<float>(height);
                constants.clipCount = 1U;
                constants.clipRect[0][2] =
                    static_cast<float>(width);
                constants.clipRect[0][3] =
                    static_cast<float>(height);
                constants.clipInverse[0][0] = 1.0F;
                constants.clipInverse[0][3] = 1.0F;
                constants.rects[0][0] =
                    static_cast<float>(
                        effectBounds.x +
                        offsetX);
                constants.rects[0][1] =
                    static_cast<float>(
                        effectBounds.y +
                        offsetY);
                constants.rects[0][2] =
                    static_cast<float>(
                        effectBounds.width);
                constants.rects[0][3] =
                    static_cast<float>(
                        effectBounds.height);
                constants.sourceUvs[0][0] =
                    static_cast<float>(
                        effectBounds.x /
                        static_cast<double>(width));
                constants.sourceUvs[0][1] =
                    static_cast<float>(
                        effectBounds.y /
                        static_cast<double>(height));
                constants.sourceUvs[0][2] =
                    static_cast<float>(
                        effectBounds.width /
                        static_cast<double>(width));
                constants.sourceUvs[0][3] =
                    static_cast<float>(
                        effectBounds.height /
                        static_cast<double>(height));
                constants.tints[0][0] = tint.red;
                constants.tints[0][1] = tint.green;
                constants.tints[0][2] = tint.blue;
                constants.tints[0][3] = tint.alpha;
                Base::Result<void> result =
                    bindImagePipeline(0U);
                if (result) {
                    result =
                        encoder.BindTextureSampler(
                            0U,
                            impl_->effectSurfaces[
                                nodeEffectSurfaceIndex].
                                target,
                            impl_->effectSampler);
                }
                if (result) {
                    ++submissionStatistics.
                        textureSamplerBindingCount;
                    result = AppendDraw(
                        encoder,
                        impl_->imageUniformBuffer,
                        constants,
                        targetClip,
                        1U);
                }
                if (result) {
                    ++submissionStatistics.drawCallCount;
                    ++submissionStatistics.
                        imageInstanceCount;
                    ++submissionStatistics.
                        uniformBufferUploadCount;
                }
                return result;
            };
            const double sampleRadius =
                std::fmin(
                    node.effect.radius,
                    50.0) * 0.5;
            if (node.effect.kind ==
                Presentation::
                    RenderEffectKind::DropShadow) {
                constexpr double DegreesToRadians =
                    0.017453292519943295769;
                const double radians =
                    node.effect.direction *
                    DegreesToRadians;
                const double shadowX =
                    std::cos(radians) *
                    node.effect.depth;
                const double shadowY =
                    -std::sin(radians) *
                    node.effect.depth;
                Base::Color shadowTint =
                    node.effect.color;
                shadowTint.alpha =
                    static_cast<float>(
                        node.effect.opacity / 9.0);
                for (std::int32_t y = -1;
                     y <= 1 && encoded; ++y) {
                    for (std::int32_t x = -1;
                         x <= 1 && encoded; ++x) {
                        encoded = appendEffectSample(
                            shadowX +
                                sampleRadius * x,
                            shadowY +
                                sampleRadius * y,
                            shadowTint);
                    }
                }
                if (encoded) {
                    encoded = appendEffectSample(
                        0.0, 0.0,
                        {1.0F, 1.0F, 1.0F,
                         1.0F});
                }
            } else {
                const Base::Color blurTint{
                    1.0F, 1.0F, 1.0F,
                    1.0F / 9.0F};
                for (std::int32_t y = -1;
                     y <= 1 && encoded; ++y) {
                    for (std::int32_t x = -1;
                         x <= 1 && encoded; ++x) {
                        encoded = appendEffectSample(
                            sampleRadius * x,
                            sampleRadius * y,
                            blurTint);
                    }
                }
            }
            if (!encoded) break;
            shouldDraw = false;
        }

        for (std::uint32_t commandIndex = 0U;
             commandIndex <
                 (shouldDraw
                  ? node.commandCount
                  : 0U);
             ++commandIndex) {
            const Presentation::RenderCommand& command =
                commands[node.commandOffset + commandIndex];
            switch (command.kind) {
            case Presentation::RenderCommandKind::PushClip: {
                if (!Presentation::IsValidLayoutRect(command.rect)) {
                    encoded = InvalidArgument("Renderer contains an invalid clip");
                    break;
                }
                Base::Result<void> pushed = PushClipState(
                    impl_->clips, command.rect,
                    impl_->transforms[impl_->transforms.Size() - 1U]);
                if (!pushed) encoded = pushed;
                break;
            }
            case Presentation::RenderCommandKind::PopClip:
                if (impl_->clips.Size() <= baseClipCount) {
                    encoded = InvalidState("Renderer clip stack underflow");
                } else {
                    impl_->clips.PopBack();
                }
                break;
            case Presentation::RenderCommandKind::PushOpacity: {
                if (!Presentation::IsValidOpacity(command.scalar)) {
                    encoded = InvalidArgument("Renderer contains invalid opacity");
                    break;
                }
                Base::Result<void> pushed = impl_->opacities.TryPushBack(
                    impl_->opacities[impl_->opacities.Size() - 1U] * command.scalar);
                if (!pushed) encoded = pushed;
                break;
            }
            case Presentation::RenderCommandKind::PopOpacity:
                if (impl_->opacities.Size() <= 1U) {
                    encoded = InvalidState("Renderer opacity stack underflow");
                } else {
                    impl_->opacities.PopBack();
                }
                break;
            case Presentation::RenderCommandKind::PushTransform: {
                if (!Presentation::IsFinite(command.transform)) {
                    encoded = InvalidArgument("Renderer contains an invalid transform");
                    break;
                }
                Base::Result<void> pushed = impl_->transforms.TryPushBack(Compose(
                    command.transform,
                    impl_->transforms[impl_->transforms.Size() - 1U]));
                if (!pushed) encoded = pushed;
                break;
            }
            case Presentation::RenderCommandKind::PopTransform:
                if (impl_->transforms.Size() <= 1U) {
                    encoded = InvalidState("Renderer transform stack underflow");
                } else {
                    impl_->transforms.PopBack();
                }
                break;
            case Presentation::RenderCommandKind::FillRect:
            case Presentation::RenderCommandKind::FillRoundedRect:
            case Presentation::RenderCommandKind::StrokeRect: {
                encoded =
                    bindRectanglePipeline(
                        blendMode);
                if (!encoded) {
                    break;
                }
                if (!Presentation::IsValidLayoutRect(command.rect) ||
                    !Presentation::IsFinite(command.color) ||
                    ((command.kind == Presentation::RenderCommandKind::FillRoundedRect ||
                      command.kind == Presentation::RenderCommandKind::StrokeRect) &&
                     (!std::isfinite(command.scalar) || command.scalar < 0.0))) {
                    encoded = InvalidArgument("Renderer contains invalid rectangle geometry");
                    break;
                }
                if (command.kind == Presentation::RenderCommandKind::FillRoundedRect &&
                    command.scalar * 2.0 >
                        std::fmin(command.rect.width, command.rect.height)) {
                    encoded = InvalidArgument("Renderer corner radius exceeds rectangle bounds");
                    break;
                }
                const Presentation::Rect clip =
                    impl_->clips[impl_->clips.Size() - 1U].bounds;
                if (IsEmpty(clip) || IsEmpty(command.rect)) {
                    break;
                }
                const Presentation::Transform2D& transform =
                    impl_->transforms[impl_->transforms.Size() - 1U];
                const double opacity = impl_->opacities[impl_->opacities.Size() - 1U];
                if (!FitsFloat(command.rect.x) || !FitsFloat(command.rect.y) ||
                    !FitsFloat(command.rect.width) || !FitsFloat(command.rect.height) ||
                    (command.kind == Presentation::RenderCommandKind::FillRoundedRect &&
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
                    constants.transform1[2] = static_cast<float>(width);
                    constants.transform1[3] = static_cast<float>(height);
                    constants.clipCount = impl_->clips.Size();
                    for (std::uint32_t clipIndex = 0U;
                         clipIndex < impl_->clips.Size();
                         ++clipIndex) {
                        const ClipState& clipState = impl_->clips[clipIndex];
                        const Presentation::Transform2D& clipTransform =
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
                        encoder, impl_->uniformBuffer, constants, clip,
                        instanceCount);
                    if (result) {
                        ++submissionStatistics.drawCallCount;
                        submissionStatistics.rectangleInstanceCount += instanceCount;
                        ++submissionStatistics.uniformBufferUploadCount;
                    }
                    return result;
                };
                auto appendRectangle = [&](Presentation::Rect rect, Presentation::Color color,
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

                if (command.kind == Presentation::RenderCommandKind::FillRect ||
                    command.kind == Presentation::RenderCommandKind::FillRoundedRect) {
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
                                 (impl_->batchingEnabled
                                  ? MaxRectangleBatchInstances
                                  : 1U);
                         ++batchIndex) {
                        const Presentation::RenderCommand& candidate =
                            commands[node.commandOffset + batchIndex];
                        if (candidate.kind != Presentation::RenderCommandKind::FillRect &&
                            candidate.kind != Presentation::RenderCommandKind::FillRoundedRect) {
                            break;
                        }
                        ++batchCommandCount;
                        if (!Presentation::IsValidLayoutRect(candidate.rect) ||
                            !Presentation::IsFinite(candidate.color) ||
                            (candidate.kind == Presentation::RenderCommandKind::FillRoundedRect &&
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
                                Presentation::RenderCommandKind::FillRoundedRect &&
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
                            candidate.kind == Presentation::RenderCommandKind::FillRoundedRect
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
            case Presentation::RenderCommandKind::DrawImage: {
                if (command.image == Presentation::InvalidRenderImageId ||
                    !Presentation::IsValidLayoutRect(command.rect) ||
                    !Presentation::IsValidLayoutRect(command.sourceUv) ||
                    command.sourceUv.x < 0.0 || command.sourceUv.y < 0.0 ||
                    command.sourceUv.x + command.sourceUv.width > 1.0 ||
                    command.sourceUv.y + command.sourceUv.height > 1.0 ||
                    !Presentation::IsFinite(command.color)) {
                    encoded = InvalidArgument(
                        "Renderer contains invalid image geometry");
                    break;
                }
                const ImageBinding* imageBinding = nullptr;
                for (const ImageBinding& candidate : impl_->images) {
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
                const Presentation::Rect clip =
                    impl_->clips[impl_->clips.Size() - 1U].bounds;
                if (IsEmpty(clip) || IsEmpty(command.rect) ||
                    IsEmpty(command.sourceUv)) {
                    break;
                }
                const Presentation::Transform2D& transform =
                    impl_->transforms[impl_->transforms.Size() - 1U];
                const double opacity =
                    impl_->opacities[impl_->opacities.Size() - 1U];
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
                constants.transform1[2] = static_cast<float>(width);
                constants.transform1[3] = static_cast<float>(height);
                constants.clipCount = impl_->clips.Size();
                for (std::uint32_t clipIndex = 0U;
                     clipIndex < impl_->clips.Size();
                     ++clipIndex) {
                    const ClipState& clipState = impl_->clips[clipIndex];
                    const Presentation::Transform2D& clipTransform = clipState.transform;
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
                             (impl_->batchingEnabled
                              ? MaxRectangleBatchInstances
                              : 1U);
                     ++batchIndex) {
                    const Presentation::RenderCommand& candidate =
                        commands[node.commandOffset + batchIndex];
                    if (candidate.kind != Presentation::RenderCommandKind::DrawImage ||
                        candidate.image != command.image) {
                        break;
                    }
                    ++batchCommandCount;
                    if (!Presentation::IsValidLayoutRect(candidate.rect) ||
                        !Presentation::IsValidLayoutRect(candidate.sourceUv) ||
                        candidate.sourceUv.x < 0.0 || candidate.sourceUv.y < 0.0 ||
                        candidate.sourceUv.x + candidate.sourceUv.width > 1.0 ||
                        candidate.sourceUv.y + candidate.sourceUv.height > 1.0 ||
                        !Presentation::IsFinite(candidate.color)) {
                        encoded = InvalidArgument(
                            "Renderer contains invalid image geometry");
                        break;
                    }
                    if (IsEmpty(candidate.rect) || IsEmpty(candidate.sourceUv)) {
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
                        encoded = AppendDraw(encoder, impl_->imageUniformBuffer,
                            constants, clip, instanceCount);
                    }
                    if (encoded) {
                        ++submissionStatistics.drawCallCount;
                        submissionStatistics.imageInstanceCount += instanceCount;
                        ++submissionStatistics.uniformBufferUploadCount;
                    }
                }
                break;
            }
            case Presentation::RenderCommandKind::DrawMesh: {
                if (command.mesh == Presentation::InvalidRenderMeshId ||
                    !Presentation::IsFinite(command.color)) {
                    encoded = InvalidArgument("Renderer contains invalid mesh draw");
                    break;
                }
                const MeshBinding* meshBinding = nullptr;
                for (const MeshBinding& candidate : impl_->meshes) {
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
                const Presentation::Rect clip =
                    impl_->clips[impl_->clips.Size() - 1U].bounds;
                if (IsEmpty(clip)) {
                    break;
                }
                const Presentation::Transform2D& transform =
                    impl_->transforms[impl_->transforms.Size() - 1U];
                const double opacity =
                    impl_->opacities[impl_->opacities.Size() - 1U];
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
                constants.transform1[2] = static_cast<float>(width);
                constants.transform1[3] = static_cast<float>(height);
                constants.clipCount = impl_->clips.Size();
                for (std::uint32_t clipIndex = 0U;
                     clipIndex < impl_->clips.Size();
                     ++clipIndex) {
                    const ClipState& clipState = impl_->clips[clipIndex];
                    const Presentation::Transform2D& clipTransform = clipState.transform;
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
                             (impl_->batchingEnabled
                              ? MaxRectangleBatchInstances
                              : 1U);
                     ++batchIndex) {
                    const Presentation::RenderCommand& candidate =
                        commands[node.commandOffset + batchIndex];
                    if (candidate.kind != Presentation::RenderCommandKind::DrawMesh ||
                        candidate.mesh != command.mesh) {
                        break;
                    }
                    ++batchCommandCount;
                    if (!Presentation::IsFinite(candidate.color)) {
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
                    encoded = encoder.UploadBuffer(impl_->meshUniformBuffer, 0U,
                        {bytes, static_cast<std::uint32_t>(sizeof(constants))});
                }
                if (encoded) encoded = encoder.SetScissor(clip);
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
            case Presentation::RenderCommandKind::DrawGlyphRun: {
                if (command.glyphRun == Presentation::InvalidRenderGlyphRunId ||
                    !Presentation::IsFinite(command.color)) {
                    encoded = InvalidArgument(
                        "Renderer contains invalid glyph draw");
                    break;
                }
                const GlyphBinding* glyphBinding = nullptr;
                for (const GlyphBinding& candidate : impl_->glyphRuns) {
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
                const Presentation::Rect clip =
                    impl_->clips[impl_->clips.Size() - 1U].bounds;
                if (IsEmpty(clip)) {
                    break;
                }
                const Presentation::Transform2D& transform =
                    impl_->transforms[impl_->transforms.Size() - 1U];
                const double opacity =
                    impl_->opacities[impl_->opacities.Size() - 1U];
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
                constants.transform1[2] = static_cast<float>(width);
                constants.transform1[3] = static_cast<float>(height);
                constants.clipCount = impl_->clips.Size();
                for (std::uint32_t clipIndex = 0U;
                     clipIndex < impl_->clips.Size();
                     ++clipIndex) {
                    const ClipState& clipState = impl_->clips[clipIndex];
                    const Presentation::Transform2D& clipTransform = clipState.transform;
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
                             (impl_->batchingEnabled
                              ? MaxRectangleBatchInstances
                              : 1U);
                     ++batchIndex) {
                    const Presentation::RenderCommand& candidate =
                        commands[node.commandOffset + batchIndex];
                    if (candidate.kind != Presentation::RenderCommandKind::DrawGlyphRun ||
                        candidate.glyphRun != command.glyphRun) {
                        break;
                    }
                    ++batchCommandCount;
                    if (!Presentation::IsFinite(candidate.color)) {
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
                    encoded = encoder.UploadBuffer(impl_->glyphUniformBuffer, 0U,
                        {bytes, static_cast<std::uint32_t>(sizeof(constants))});
                }
                if (encoded) {
                    encoded = encoder.SetScissor(clip);
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
        if (!encoded || impl_->transforms.Size() != 1U ||
            impl_->clips.Size() != baseClipCount || impl_->opacities.Size() != 1U) {
            if (encoded) {
                encoded = InvalidState("Renderer node has unbalanced state stacks");
            }
            break;
        }
    }
    return encoded;
    };

    std::uint32_t surfaceIndex = 0U;
    for (const Presentation::RenderNodeSnapshot& node :
         plan.Nodes()) {
        if (node.effect.kind ==
            Presentation::RenderEffectKind::None) {
            continue;
        }
        pass.colorAttachments[0].target =
            impl_->effectSurfaces[
                surfaceIndex].target;
        encoded = encoder.BeginRenderPass(pass);
        if (encoded) {
            ++submissionStatistics.renderPassCount;
            encoded = recordNodes(
                node.id, false);
        }
        if (encoded) {
            encoded = encoder.EndRenderPass();
        }
        if (!encoded) break;
        ++surfaceIndex;
    }
    if (encoded) {
        pass.colorAttachments[0].target =
            target.color;
        encoded = encoder.BeginRenderPass(pass);
    }
    if (encoded) {
        ++submissionStatistics.renderPassCount;
        encoded = recordNodes(
            Presentation::InvalidRenderNodeId,
            true);
    }
    if (encoded) {
        encoded = encoder.EndRenderPass();
    }
    Base::Result<CommandList> finished = encoded
        ? encoder.Finish()
        : Base::Result<CommandList>(encoded.GetStatus());
    if (!finished) {
        return finished.GetStatus();
    }
    impl_->lastStatistics = submissionStatistics;
    return std::move(finished).Value();
}

} // namespace Aero::Render
