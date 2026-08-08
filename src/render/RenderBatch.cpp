#include "render/GraphicsTypes.hpp"
#include "render/RenderBatch.hpp"

#include <cmath>
#include <cstring>
#include <utility>

namespace Aero::Graphics {
namespace {

constexpr std::uint64_t HashOffset = UINT64_C(1469598103934665603);
constexpr std::uint64_t HashPrime = UINT64_C(1099511628211);

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotFound, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

void HashBytes(std::uint64_t& hash, const void* data, std::size_t size) noexcept {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= HashPrime;
    }
}

template<class T>
void HashValue(std::uint64_t& hash, const T& value) noexcept {
    HashBytes(hash, &value, sizeof(T));
}

void HashStringView(std::uint64_t& hash, Base::StringView value) noexcept {
    HashValue(hash, value.SizeBytes());
    if (!value.Empty()) {
        HashBytes(hash, value.Data(), value.SizeBytes());
    }
}

void HashResource(std::uint64_t& hash, ResourceHandle handle) noexcept {
    HashValue(hash, handle.index);
    HashValue(hash, handle.generation);
    HashValue(hash, handle.type);
}

void HashColor(std::uint64_t& hash, Base::Color value) noexcept {
    HashValue(hash, value.red);
    HashValue(hash, value.green);
    HashValue(hash, value.blue);
    HashValue(hash, value.alpha);
}

void HashRect(std::uint64_t& hash, Base::Rect value) noexcept {
    HashValue(hash, value.x);
    HashValue(hash, value.y);
    HashValue(hash, value.width);
    HashValue(hash, value.height);
}

bool IsPowerOfTwo(std::uint32_t value) noexcept {
    return value != 0U && (value & (value - 1U)) == 0U;
}

bool IsValidColor(Base::Color value) noexcept {
    return Base::IsFiniteColor(value) &&
        value.red >= 0.0F && value.red <= 1.0F &&
        value.green >= 0.0F && value.green <= 1.0F &&
        value.blue >= 0.0F && value.blue <= 1.0F &&
        value.alpha >= 0.0F && value.alpha <= 1.0F;
}

bool IsValidGraphicsCapabilities(
    const GraphicsCapabilities& capabilities) noexcept {
    return capabilities.abiVersion == GraphicsAbiVersion &&
        capabilities.backendKind != NativeRenderBackendKind::Invalid &&
        capabilities.maxColorAttachments > 0U &&
        capabilities.maxColorAttachments <= MaxColorAttachments &&
        capabilities.maxVertexAttributes > 0U &&
        capabilities.maxVertexAttributes <= MaxVertexAttributes &&
        capabilities.maxSampledTextures > 0U &&
        capabilities.uniformBufferAlignment != 0U &&
        IsPowerOfTwo(capabilities.uniformBufferAlignment);
}

bool IsDepthFormat(GraphicsTextureFormat format) noexcept {
    return format == GraphicsTextureFormat::Depth24Stencil8;
}

std::uint32_t TextureBytesPerPixel(GraphicsTextureFormat format) noexcept {
    switch (format) {
    case GraphicsTextureFormat::Rgba8Unorm:
    case GraphicsTextureFormat::Bgra8Unorm:
    case GraphicsTextureFormat::Depth24Stencil8:
        return 4U;
    case GraphicsTextureFormat::R8Unorm:
        return 1U;
    }
    return 0U;
}

TextureFormat ToBaseTextureFormat(GraphicsTextureFormat format) noexcept {
    switch (format) {
    case GraphicsTextureFormat::Rgba8Unorm:
        return TextureFormat::Rgba8Unorm;
    case GraphicsTextureFormat::Bgra8Unorm:
        return TextureFormat::Bgra8Unorm;
    case GraphicsTextureFormat::R8Unorm:
    case GraphicsTextureFormat::Depth24Stencil8:
        return TextureFormat::R8Unorm;
    }
    return TextureFormat::Rgba8Unorm;
}

std::uint32_t VertexFormatSize(VertexFormat format) noexcept {
    switch (format) {
    case VertexFormat::Float:
        return 4U;
    case VertexFormat::Float2:
        return 8U;
    case VertexFormat::Float3:
        return 12U;
    case VertexFormat::Float4:
        return 16U;
    case VertexFormat::UByte4Normalized:
    case VertexFormat::UShort2:
        return 4U;
    case VertexFormat::UShort4:
        return 8U;
    }
    return 0U;
}

Base::Result<void> ValidateShader(
    const NativeShaderProgram& shader,
    ShaderStage expectedStage,
    const GraphicsCapabilities& capabilities) noexcept {
    if (shader.stage != expectedStage ||
        shader.language == ShaderLanguage::Invalid ||
        shader.bytecode == nullptr || shader.bytecodeSize == 0U ||
        shader.entryPoint.Empty() || shader.stableId == 0U) {
        return InvalidArgument("Shader descriptor is incomplete or has the wrong stage");
    }
    if ((capabilities.shaderLanguages & ShaderLanguageBit(shader.language)) == 0U) {
        return Unsupported("Shader language is unsupported by the selected backend");
    }
    return {};
}

void HashShader(std::uint64_t& hash, const NativeShaderProgram& shader) noexcept {
    HashValue(hash, shader.stage);
    HashValue(hash, shader.language);
    HashValue(hash, shader.stableId);
    HashStringView(hash, shader.entryPoint);
    HashValue(hash, shader.bytecodeSize);
    if (shader.bytecode != nullptr && shader.bytecodeSize != 0U) {
        HashBytes(hash, shader.bytecode, shader.bytecodeSize);
    }
}

void HashRenderPass(
    std::uint64_t& hash,
    const RenderPassDescriptor& descriptor) noexcept {
    HashRect(hash, descriptor.renderArea);
    HashValue(hash, descriptor.colorAttachmentCount);
    for (std::uint32_t index = 0U;
         index < descriptor.colorAttachmentCount && index < MaxColorAttachments;
         ++index) {
        const ColorAttachmentDescriptor& attachment =
            descriptor.colorAttachments[index];
        HashResource(hash, attachment.target);
        HashValue(hash, attachment.load);
        HashValue(hash, attachment.store);
        HashColor(hash, attachment.clearColor);
    }
    HashValue(hash, descriptor.hasDepthStencil);
    if (descriptor.hasDepthStencil) {
        HashResource(hash, descriptor.depthStencil.target);
        HashValue(hash, descriptor.depthStencil.depthLoad);
        HashValue(hash, descriptor.depthStencil.depthStore);
        HashValue(hash, descriptor.depthStencil.stencilLoad);
        HashValue(hash, descriptor.depthStencil.stencilStore);
        HashValue(hash, descriptor.depthStencil.clearDepth);
        HashValue(hash, descriptor.depthStencil.clearStencil);
    }
}

std::uint64_t StableTextureHash(
    const TextureResourceDescriptor& descriptor) noexcept {
    std::uint64_t hash = HashOffset;
    HashValue(hash, descriptor.width);
    HashValue(hash, descriptor.height);
    HashValue(hash, descriptor.mipLevels);
    HashValue(hash, descriptor.arrayLayers);
    HashValue(hash, descriptor.sampleCount);
    HashValue(hash, descriptor.format);
    HashValue(hash, descriptor.usage);
    return hash;
}

std::uint64_t StableSamplerHash(const SamplerDescriptor& descriptor) noexcept {
    std::uint64_t hash = HashOffset;
    HashValue(hash, descriptor.minFilter);
    HashValue(hash, descriptor.magFilter);
    HashValue(hash, descriptor.mipFilter);
    HashValue(hash, descriptor.addressU);
    HashValue(hash, descriptor.addressV);
    HashValue(hash, descriptor.addressW);
    HashValue(hash, descriptor.minLod);
    HashValue(hash, descriptor.maxLod);
    HashValue(hash, descriptor.maxAnisotropy);
    return hash;
}

Base::Result<void> ValidatePassDescriptorBasic(
    const RenderPassDescriptor& descriptor) noexcept {
    if (!Base::IsValidRect(descriptor.renderArea)) {
        return InvalidArgument("Render pass area is invalid");
    }
    if (descriptor.colorAttachmentCount > MaxColorAttachments ||
        (descriptor.colorAttachmentCount == 0U && !descriptor.hasDepthStencil)) {
        return InvalidArgument("Render pass attachment count is invalid");
    }
    for (std::uint32_t index = 0U;
         index < descriptor.colorAttachmentCount;
         ++index) {
        const ColorAttachmentDescriptor& attachment =
            descriptor.colorAttachments[index];
        if (!attachment.target.IsValid() ||
            (attachment.target.type != ResourceType::RenderTarget &&
             attachment.target.type != ResourceType::Texture) ||
            !IsValidColor(attachment.clearColor)) {
            return InvalidArgument("Color attachment descriptor is invalid");
        }
    }
    if (descriptor.hasDepthStencil) {
        if (!descriptor.depthStencil.target.IsValid() ||
            (descriptor.depthStencil.target.type != ResourceType::RenderTarget &&
             descriptor.depthStencil.target.type != ResourceType::Texture) ||
            !std::isfinite(descriptor.depthStencil.clearDepth) ||
            descriptor.depthStencil.clearDepth < 0.0F ||
            descriptor.depthStencil.clearDepth > 1.0F) {
            return InvalidArgument("Depth-stencil attachment descriptor is invalid");
        }
    }
    return {};
}

Base::Result<void> ValidateUploadRange(
    std::uint32_t offset,
    std::uint32_t size,
    std::uint32_t total) noexcept {
    if (size == 0U || offset > total || size > total - offset) {
        return InvalidArgument("Graphics command upload range is invalid");
    }
    return {};
}

} // namespace

Base::Result<void> ValidateTextureDescriptor(
    const TextureResourceDescriptor& descriptor,
    const GraphicsCapabilities& capabilities) noexcept {
    if (!IsValidGraphicsCapabilities(capabilities)) {
        return Unsupported("Graphics capabilities are invalid");
    }
    if (descriptor.width == 0U || descriptor.height == 0U ||
        descriptor.mipLevels == 0U || descriptor.arrayLayers == 0U ||
        descriptor.sampleCount == 0U ||
        !IsPowerOfTwo(descriptor.sampleCount) ||
        descriptor.usage == 0U || TextureBytesPerPixel(descriptor.format) == 0U) {
        return InvalidArgument("Texture descriptor is invalid");
    }
    if (descriptor.sampleCount > 1U && descriptor.mipLevels > 1U) {
        return InvalidArgument("Multisampled textures cannot have mip levels");
    }
    if (IsDepthFormat(descriptor.format) &&
        HasTextureUsage(descriptor.usage, TextureUsage::Sampled)) {
        return Unsupported("Depth sampling is not part of the initial graphics contract");
    }
    if (HasTextureUsage(descriptor.usage, TextureUsage::Sampled) &&
        !HasAllFeatures(capabilities.features,
            FeatureBit(GraphicsFeature::TextureSampling))) {
        return Unsupported("Selected backend does not support sampled textures");
    }
    if (HasTextureUsage(descriptor.usage, TextureUsage::RenderTarget) &&
        !HasAllFeatures(capabilities.features,
            FeatureBit(GraphicsFeature::RenderTargets))) {
        return Unsupported("Selected backend does not support render targets");
    }
    return {};
}

Base::Result<void> ValidateSamplerDescriptor(
    const SamplerDescriptor& descriptor,
    const GraphicsCapabilities& capabilities) noexcept {
    if (!IsValidGraphicsCapabilities(capabilities)) {
        return Unsupported("Graphics capabilities are invalid");
    }
    if (!std::isfinite(descriptor.minLod) ||
        !std::isfinite(descriptor.maxLod) ||
        descriptor.minLod < 0.0F || descriptor.maxLod < descriptor.minLod ||
        descriptor.maxAnisotropy == 0U) {
        return InvalidArgument("Sampler descriptor is invalid");
    }
    if (descriptor.maxAnisotropy > 1U &&
        !HasAllFeatures(capabilities.features,
            FeatureBit(GraphicsFeature::AnisotropicFiltering))) {
        return Unsupported("Selected backend does not support anisotropic filtering");
    }
    return {};
}

Base::Result<void> ValidateNativePipeline(
    const NativePipelineState& descriptor,
    const GraphicsCapabilities& capabilities) noexcept {
    if (!IsValidGraphicsCapabilities(capabilities)) {
        return Unsupported("Graphics capabilities are invalid");
    }
    Base::Result<void> vertex = ValidateShader(
        descriptor.vertexShader, ShaderStage::Vertex, capabilities);
    if (!vertex) {
        return vertex;
    }
    Base::Result<void> fragment = ValidateShader(
        descriptor.fragmentShader, ShaderStage::Fragment, capabilities);
    if (!fragment) {
        return fragment;
    }
    if (descriptor.vertexLayout.bufferCount > MaxVertexBuffers ||
        descriptor.vertexLayout.attributeCount > MaxVertexAttributes ||
        descriptor.vertexLayout.attributeCount > capabilities.maxVertexAttributes ||
        descriptor.sampleCount == 0U ||
        !IsPowerOfTwo(descriptor.sampleCount) ||
        IsDepthFormat(descriptor.colorFormat) ||
        descriptor.blend.writeMask > 0x0FU) {
        return InvalidArgument("Pipeline descriptor limits are invalid");
    }
    if (descriptor.depthStencil.depthTestEnabled ||
        descriptor.depthStencil.stencilEnabled) {
        if (!HasAllFeatures(capabilities.features,
                FeatureBit(GraphicsFeature::DepthStencil)) ||
            !IsDepthFormat(descriptor.depthStencilFormat)) {
            return Unsupported("Depth-stencil pipeline state is unsupported");
        }
    }

    bool locations[MaxVertexAttributes]{};
    for (std::uint32_t index = 0U;
         index < descriptor.vertexLayout.bufferCount;
         ++index) {
        if (descriptor.vertexLayout.buffers[index].stride == 0U) {
            return InvalidArgument("Vertex buffer stride must be non-zero");
        }
    }
    for (std::uint32_t index = 0U;
         index < descriptor.vertexLayout.attributeCount;
         ++index) {
        const VertexAttribute& attribute =
            descriptor.vertexLayout.attributes[index];
        if (attribute.location >= capabilities.maxVertexAttributes ||
            attribute.location >= MaxVertexAttributes ||
            attribute.bufferSlot >= descriptor.vertexLayout.bufferCount) {
            return InvalidArgument("Vertex attribute location or buffer slot is invalid");
        }
        if (locations[attribute.location]) {
            return InvalidArgument("Vertex attribute locations must be unique");
        }
        locations[attribute.location] = true;
        const std::uint32_t formatSize = VertexFormatSize(attribute.format);
        const std::uint32_t stride =
            descriptor.vertexLayout.buffers[attribute.bufferSlot].stride;
        if (formatSize == 0U || attribute.offset > stride ||
            formatSize > stride - attribute.offset) {
            return InvalidArgument("Vertex attribute exceeds its buffer stride");
        }
    }
    return {};
}

std::uint64_t StableNativePipelineHash(
    const NativePipelineState& descriptor) noexcept {
    std::uint64_t hash = HashOffset;
    HashShader(hash, descriptor.vertexShader);
    HashShader(hash, descriptor.fragmentShader);
    HashValue(hash, descriptor.vertexLayout.bufferCount);
    HashValue(hash, descriptor.vertexLayout.attributeCount);
    for (std::uint32_t index = 0U;
         index < descriptor.vertexLayout.bufferCount && index < MaxVertexBuffers;
         ++index) {
        HashValue(hash, descriptor.vertexLayout.buffers[index].stride);
        HashValue(hash, descriptor.vertexLayout.buffers[index].stepMode);
    }
    for (std::uint32_t index = 0U;
         index < descriptor.vertexLayout.attributeCount &&
             index < MaxVertexAttributes;
         ++index) {
        const VertexAttribute& attribute = descriptor.vertexLayout.attributes[index];
        HashValue(hash, attribute.location);
        HashValue(hash, attribute.bufferSlot);
        HashValue(hash, attribute.format);
        HashValue(hash, attribute.offset);
    }
    HashValue(hash, descriptor.topology);
    HashValue(hash, descriptor.blend.enabled);
    HashValue(hash, descriptor.blend.color.source);
    HashValue(hash, descriptor.blend.color.destination);
    HashValue(hash, descriptor.blend.color.operation);
    HashValue(hash, descriptor.blend.alpha.source);
    HashValue(hash, descriptor.blend.alpha.destination);
    HashValue(hash, descriptor.blend.alpha.operation);
    HashValue(hash, descriptor.blend.writeMask);
    HashValue(hash, descriptor.raster.cullMode);
    HashValue(hash, descriptor.raster.frontFace);
    HashValue(hash, descriptor.raster.fillMode);
    HashValue(hash, descriptor.raster.scissorEnabled);
    HashValue(hash, descriptor.depthStencil.depthTestEnabled);
    HashValue(hash, descriptor.depthStencil.depthWriteEnabled);
    HashValue(hash, descriptor.depthStencil.depthCompare);
    HashValue(hash, descriptor.depthStencil.stencilEnabled);
    HashValue(hash, descriptor.depthStencil.front.compare);
    HashValue(hash, descriptor.depthStencil.front.fail);
    HashValue(hash, descriptor.depthStencil.front.depthFail);
    HashValue(hash, descriptor.depthStencil.front.pass);
    HashValue(hash, descriptor.depthStencil.back.compare);
    HashValue(hash, descriptor.depthStencil.back.fail);
    HashValue(hash, descriptor.depthStencil.back.depthFail);
    HashValue(hash, descriptor.depthStencil.back.pass);
    HashValue(hash, descriptor.depthStencil.stencilReadMask);
    HashValue(hash, descriptor.depthStencil.stencilWriteMask);
    HashValue(hash, descriptor.colorFormat);
    HashValue(hash, descriptor.depthStencilFormat);
    HashValue(hash, descriptor.sampleCount);
    return hash;
}

} // namespace Aero::Graphics

namespace Aero::Render::Detail {
using namespace ::Aero::Graphics;

std::uint64_t RenderBatch::StableHash() const noexcept {
    std::uint64_t hash = HashOffset;
    HashValue(hash, steps_.Size());
    for (const RenderStep& step : steps_) {
        HashValue(hash, step.kind);
        HashResource(hash, step.resource);
        HashRenderPass(hash, step.pass);
        HashValue(hash, step.textureRegion.x);
        HashValue(hash, step.textureRegion.y);
        HashValue(hash, step.textureRegion.width);
        HashValue(hash, step.textureRegion.height);
        HashValue(hash, step.textureRegion.mipLevel);
        HashValue(hash, step.textureRegion.arrayLayer);
        HashValue(hash, step.textureRegion.bytesPerRow);
        HashResource(hash, step.drawState.pipeline);
        for (std::uint32_t slot = 0U; slot < MaxVertexBuffers; ++slot) {
            HashResource(hash, step.drawState.vertexBuffers[slot]);
            HashValue(hash, step.drawState.vertexOffsets[slot]);
        }
        HashResource(hash, step.drawState.indexBuffer);
        HashValue(hash, step.drawState.indexOffset);
        HashValue(hash, step.drawState.indexType);
        for (std::uint32_t slot = 0U; slot < 4U; ++slot) {
            HashResource(hash, step.drawState.uniformBuffers[slot]);
            HashValue(hash, step.drawState.uniformOffsets[slot]);
            HashValue(hash, step.drawState.uniformSizes[slot]);
        }
        for (std::uint32_t slot = 0U; slot < 8U; ++slot) {
            HashResource(hash, step.drawState.textures[slot]);
            HashResource(hash, step.drawState.samplers[slot]);
        }
        HashRect(hash, step.drawState.scissor);
        HashValue(hash, step.resourceOffset);
        HashValue(hash, step.resourceSize);
        HashValue(hash, step.uploadOffset);
        HashValue(hash, step.uploadSize);
        HashValue(hash, step.first);
        HashValue(hash, step.count);
        HashValue(hash, step.instanceCount);
        HashValue(hash, step.firstInstance);
        HashValue(hash, step.baseVertex);
        HashValue(hash, step.indexed);
    }
    HashValue(hash, uploadBytes_.Size());
    if (!uploadBytes_.Empty()) {
        HashBytes(hash, uploadBytes_.Data(), uploadBytes_.Size());
    }
    return hash;
}

Base::Result<void> RenderBatchBuilder::VerifyRecording() const noexcept {
    return finished_
        ? Base::Result<void>(InvalidState("Render batch builder is finished"))
        : Base::Result<void>();
}

Base::Result<void> RenderBatchBuilder::Append(
    const RenderStep& step) noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) return recording;
    return batch_.steps_.PushBack(step);
}

Base::Result<void> RenderBatchBuilder::AppendUpload(
    RenderStep& step,
    Base::Span<const std::uint8_t> data) noexcept {
    if (data.Empty()) return InvalidArgument("Upload payload must not be empty");
    if (batch_.uploadBytes_.Size() > UINT32_MAX - data.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render batch upload payload exceeds its size limit");
    }
    const std::uint32_t originalSize = batch_.uploadBytes_.Size();
    Base::Result<void> appendedBytes = batch_.uploadBytes_.Append(data);
    if (!appendedBytes) return appendedBytes;
    step.uploadOffset = originalSize;
    step.uploadSize = data.Size();
    Base::Result<void> appendedStep = Append(step);
    if (!appendedStep) {
        while (batch_.uploadBytes_.Size() > originalSize) {
            batch_.uploadBytes_.PopBack();
        }
    }
    return appendedStep;
}

Base::Result<void> RenderBatchBuilder::UploadBuffer(
    ResourceHandle buffer,
    std::uint64_t destinationOffset,
    Base::Span<const std::uint8_t> data) noexcept {
    if (!buffer.IsValid() || buffer.type != ResourceType::Buffer) {
        return InvalidState("Buffer uploads require a valid buffer");
    }
    RenderStep step;
    step.kind = RenderStepKind::UploadBuffer;
    step.resource = buffer;
    step.resourceOffset = destinationOffset;
    step.resourceSize = data.Size();
    return AppendUpload(step, data);
}

Base::Result<void> RenderBatchBuilder::UploadTexture(
    ResourceHandle texture,
    TextureRegion region,
    Base::Span<const std::uint8_t> data) noexcept {
    if (inRenderPass_ || !texture.IsValid() ||
        (texture.type != ResourceType::Texture &&
         texture.type != ResourceType::RenderTarget) ||
        region.width == 0U || region.height == 0U ||
        region.bytesPerRow == 0U) {
        return InvalidState("Texture uploads require a valid region outside a render pass");
    }
    RenderStep step;
    step.kind = RenderStepKind::UploadTexture;
    step.resource = texture;
    step.textureRegion = region;
    return AppendUpload(step, data);
}

Base::Result<void> RenderBatchBuilder::BeginRenderPass(
    const RenderPassDescriptor& descriptor) noexcept {
    if (inRenderPass_) return InvalidState("A render pass is already active");
    Base::Result<void> valid = ValidatePassDescriptorBasic(descriptor);
    if (!valid) return valid;
    RenderStep step;
    step.kind = RenderStepKind::BeginPass;
    step.pass = descriptor;
    Base::Result<void> appended = Append(step);
    if (appended) {
        inRenderPass_ = true;
        state_ = {};
    }
    return appended;
}

Base::Result<void> RenderBatchBuilder::EndRenderPass() noexcept {
    if (!inRenderPass_) return InvalidState("No render pass is active");
    RenderStep step;
    step.kind = RenderStepKind::EndPass;
    Base::Result<void> appended = Append(step);
    if (appended) inRenderPass_ = false;
    return appended;
}

Base::Result<void> RenderBatchBuilder::BindPipeline(ResourceHandle pipeline) noexcept {
    if (!inRenderPass_ || !pipeline.IsValid() ||
        pipeline.type != ResourceType::Pipeline) {
        return InvalidState("Pipeline binding requires an active render pass");
    }
    state_.pipeline = pipeline;
    return {};
}

Base::Result<void> RenderBatchBuilder::BindVertexBuffer(
    std::uint32_t slot, ResourceHandle buffer, std::uint64_t offset) noexcept {
    if (!inRenderPass_ || slot >= MaxVertexBuffers ||
        !buffer.IsValid() || buffer.type != ResourceType::Buffer) {
        return InvalidState("Vertex-buffer binding is invalid");
    }
    state_.vertexBuffers[slot] = buffer;
    state_.vertexOffsets[slot] = offset;
    return {};
}

Base::Result<void> RenderBatchBuilder::BindIndexBuffer(
    ResourceHandle buffer, IndexType type, std::uint64_t offset) noexcept {
    if (!inRenderPass_ || !buffer.IsValid() ||
        buffer.type != ResourceType::Buffer) {
        return InvalidState("Index-buffer binding is invalid");
    }
    state_.indexBuffer = buffer;
    state_.indexType = type;
    state_.indexOffset = offset;
    return {};
}

Base::Result<void> RenderBatchBuilder::BindUniformBuffer(
    std::uint32_t slot, ResourceHandle buffer,
    std::uint64_t offset, std::uint32_t size) noexcept {
    if (!inRenderPass_ || slot >= 4U || !buffer.IsValid() ||
        buffer.type != ResourceType::Buffer || size == 0U) {
        return InvalidState("Uniform-buffer binding is invalid");
    }
    state_.uniformBuffers[slot] = buffer;
    state_.uniformOffsets[slot] = offset;
    state_.uniformSizes[slot] = size;
    return {};
}

Base::Result<void> RenderBatchBuilder::BindTextureSampler(
    std::uint32_t slot, ResourceHandle texture,
    ResourceHandle sampler) noexcept {
    if (!inRenderPass_ || slot >= 8U || !texture.IsValid() ||
        !sampler.IsValid() ||
        (texture.type != ResourceType::Texture &&
         texture.type != ResourceType::RenderTarget) ||
        sampler.type != ResourceType::Sampler) {
        return InvalidState("Texture-sampler binding is invalid");
    }
    state_.textures[slot] = texture;
    state_.samplers[slot] = sampler;
    return {};
}

Base::Result<void> RenderBatchBuilder::SetScissor(Base::Rect rect) noexcept {
    if (!inRenderPass_ || !Base::IsValidRect(rect)) {
        return InvalidState("Scissor state requires a valid rectangle in a render pass");
    }
    state_.scissor = rect;
    return {};
}

Base::Result<void> RenderBatchBuilder::Draw(
    std::uint32_t vertexCount, std::uint32_t instanceCount,
    std::uint32_t firstVertex, std::uint32_t firstInstance) noexcept {
    if (!inRenderPass_ || vertexCount == 0U || instanceCount == 0U) {
        return InvalidState("Draw requires an active render pass and non-zero counts");
    }
    RenderStep step;
    step.kind = RenderStepKind::Draw;
    step.drawState = state_;
    step.first = firstVertex;
    step.count = vertexCount;
    step.instanceCount = instanceCount;
    step.firstInstance = firstInstance;
    return Append(step);
}

Base::Result<void> RenderBatchBuilder::DrawIndexed(
    std::uint32_t indexCount, std::uint32_t instanceCount,
    std::uint32_t firstIndex, std::int32_t baseVertex,
    std::uint32_t firstInstance) noexcept {
    if (!inRenderPass_ || indexCount == 0U || instanceCount == 0U) {
        return InvalidState("Indexed draw requires an active render pass and non-zero counts");
    }
    RenderStep step;
    step.kind = RenderStepKind::Draw;
    step.drawState = state_;
    step.first = firstIndex;
    step.count = indexCount;
    step.instanceCount = instanceCount;
    step.firstInstance = firstInstance;
    step.baseVertex = baseVertex;
    step.indexed = true;
    return Append(step);
}

Base::Result<RenderBatch> RenderBatchBuilder::Finish() noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) return recording.GetStatus();
    if (inRenderPass_) return InvalidState("Render batch has an unclosed pass");
    finished_ = true;
    return std::move(batch_);
}

} // namespace Aero::Render::Detail
