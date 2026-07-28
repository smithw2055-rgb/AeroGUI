#include "Graphics.hpp"

#include <cmath>
#include <cstring>
#include <utility>

namespace Aero::Rhi {
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
        capabilities.backendKind != GraphicsBackendKind::Invalid &&
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
    const ShaderDescriptor& shader,
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

void HashShader(std::uint64_t& hash, const ShaderDescriptor& shader) noexcept {
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

Base::Result<void> ValidatePipelineDescriptor(
    const PipelineDescriptor& descriptor,
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

std::uint64_t StablePipelineHash(
    const PipelineDescriptor& descriptor) noexcept {
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

std::uint64_t CommandList::StableHash() const noexcept {
    std::uint64_t hash = HashOffset;
    HashValue(hash, commands_.Size());
    for (const Command& command : commands_) {
        HashValue(hash, command.kind);
        HashResource(hash, command.resource0);
        HashResource(hash, command.resource1);
        HashRenderPass(hash, command.renderPass);
        HashValue(hash, command.textureRegion.x);
        HashValue(hash, command.textureRegion.y);
        HashValue(hash, command.textureRegion.width);
        HashValue(hash, command.textureRegion.height);
        HashValue(hash, command.textureRegion.mipLevel);
        HashValue(hash, command.textureRegion.arrayLayer);
        HashValue(hash, command.textureRegion.bytesPerRow);
        HashRect(hash, command.rect);
        HashValue(hash, command.resourceOffset);
        HashValue(hash, command.resourceSize);
        HashValue(hash, command.uploadOffset);
        HashValue(hash, command.uploadSize);
        HashValue(hash, command.slot);
        HashValue(hash, command.first);
        HashValue(hash, command.count);
        HashValue(hash, command.instanceCount);
        HashValue(hash, command.firstInstance);
        HashValue(hash, command.baseVertex);
        HashValue(hash, command.indexType);
    }
    HashValue(hash, uploadBytes_.Size());
    if (!uploadBytes_.Empty()) {
        HashBytes(hash, uploadBytes_.Data(), uploadBytes_.Size());
    }
    return hash;
}

Base::Result<void> CommandEncoder::VerifyRecording() const noexcept {
    return finished_
        ? Base::Result<void>(InvalidState("Graphics command encoder is finished"))
        : Base::Result<void>();
}

Base::Result<void> CommandEncoder::Append(
    const Command& command) noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) {
        return recording;
    }
    return buffer_.commands_.TryPushBack(command);
}

Base::Result<void> CommandEncoder::AppendUpload(
    Command& command,
    Base::Span<const std::uint8_t> data) noexcept {
    if (data.Empty()) {
        return InvalidArgument("Upload payload must not be empty");
    }
    if (buffer_.uploadBytes_.Size() > UINT32_MAX - data.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Graphics upload payload exceeds the command-buffer limit");
    }
    const std::uint32_t originalSize = buffer_.uploadBytes_.Size();
    Base::Result<void> appendedBytes = buffer_.uploadBytes_.TryAppend(data);
    if (!appendedBytes) {
        return appendedBytes;
    }
    command.uploadOffset = originalSize;
    command.uploadSize = data.Size();
    Base::Result<void> appendedCommand = Append(command);
    if (!appendedCommand) {
        while (buffer_.uploadBytes_.Size() > originalSize) {
            buffer_.uploadBytes_.PopBack();
        }
        return appendedCommand;
    }
    return {};
}

Base::Result<void> CommandEncoder::UploadBuffer(
    ResourceHandle buffer,
    std::uint64_t destinationOffset,
    Base::Span<const std::uint8_t> data) noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) {
        return recording;
    }
    if (!buffer.IsValid() || buffer.type != ResourceType::Buffer) {
        return InvalidState("Buffer uploads require a valid buffer");
    }
    Command command;
    command.kind = CommandKind::UploadBuffer;
    command.resource0 = buffer;
    command.resourceOffset = destinationOffset;
    command.resourceSize = data.Size();
    return AppendUpload(command, data);
}

Base::Result<void> CommandEncoder::UploadTexture(
    ResourceHandle texture,
    TextureRegion region,
    Base::Span<const std::uint8_t> data) noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) {
        return recording;
    }
    if (inRenderPass_ || !texture.IsValid() ||
        (texture.type != ResourceType::Texture &&
         texture.type != ResourceType::RenderTarget) ||
        region.width == 0U || region.height == 0U ||
        region.bytesPerRow == 0U) {
        return InvalidState("Texture uploads require a valid region outside a render pass");
    }
    Command command;
    command.kind = CommandKind::UploadTexture;
    command.resource0 = texture;
    command.textureRegion = region;
    return AppendUpload(command, data);
}

Base::Result<void> CommandEncoder::BeginRenderPass(
    const RenderPassDescriptor& descriptor) noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) {
        return recording;
    }
    if (inRenderPass_) {
        return InvalidState("A render pass is already active");
    }
    Base::Result<void> valid = ValidatePassDescriptorBasic(descriptor);
    if (!valid) {
        return valid;
    }
    Command command;
    command.kind = CommandKind::BeginRenderPass;
    command.renderPass = descriptor;
    Base::Result<void> appended = Append(command);
    if (appended) {
        inRenderPass_ = true;
    }
    return appended;
}

Base::Result<void> CommandEncoder::EndRenderPass() noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) {
        return recording;
    }
    if (!inRenderPass_) {
        return InvalidState("No render pass is active");
    }
    Command command;
    command.kind = CommandKind::EndRenderPass;
    Base::Result<void> appended = Append(command);
    if (appended) {
        inRenderPass_ = false;
    }
    return appended;
}

Base::Result<void> CommandEncoder::BindPipeline(
    ResourceHandle pipeline) noexcept {
    if (!inRenderPass_ || !pipeline.IsValid() ||
        pipeline.type != ResourceType::Pipeline) {
        return InvalidState("Pipeline binding requires an active render pass");
    }
    Command command;
    command.kind = CommandKind::BindPipeline;
    command.resource0 = pipeline;
    return Append(command);
}

Base::Result<void> CommandEncoder::BindVertexBuffer(
    std::uint32_t slot,
    ResourceHandle buffer,
    std::uint64_t offset) noexcept {
    if (!inRenderPass_ || slot >= MaxVertexBuffers ||
        !buffer.IsValid() || buffer.type != ResourceType::Buffer) {
        return InvalidState("Vertex-buffer binding is invalid");
    }
    Command command;
    command.kind = CommandKind::BindVertexBuffer;
    command.resource0 = buffer;
    command.resourceOffset = offset;
    command.slot = slot;
    return Append(command);
}

Base::Result<void> CommandEncoder::BindIndexBuffer(
    ResourceHandle buffer,
    IndexType type,
    std::uint64_t offset) noexcept {
    if (!inRenderPass_ || !buffer.IsValid() ||
        buffer.type != ResourceType::Buffer) {
        return InvalidState("Index-buffer binding is invalid");
    }
    Command command;
    command.kind = CommandKind::BindIndexBuffer;
    command.resource0 = buffer;
    command.resourceOffset = offset;
    command.indexType = type;
    return Append(command);
}

Base::Result<void> CommandEncoder::BindUniformBuffer(
    std::uint32_t slot,
    ResourceHandle buffer,
    std::uint64_t offset,
    std::uint32_t size) noexcept {
    if (!inRenderPass_ || !buffer.IsValid() ||
        buffer.type != ResourceType::Buffer || size == 0U) {
        return InvalidState("Uniform-buffer binding is invalid");
    }
    Command command;
    command.kind = CommandKind::BindUniformBuffer;
    command.resource0 = buffer;
    command.resourceOffset = offset;
    command.resourceSize = size;
    command.slot = slot;
    return Append(command);
}

Base::Result<void> CommandEncoder::BindTextureSampler(
    std::uint32_t slot,
    ResourceHandle texture,
    ResourceHandle sampler) noexcept {
    if (!inRenderPass_ || !texture.IsValid() || !sampler.IsValid() ||
        texture.type != ResourceType::Texture ||
        sampler.type != ResourceType::Sampler) {
        return InvalidState("Texture-sampler binding is invalid");
    }
    Command command;
    command.kind = CommandKind::BindTextureSampler;
    command.resource0 = texture;
    command.resource1 = sampler;
    command.slot = slot;
    return Append(command);
}

Base::Result<void> CommandEncoder::SetScissor(
    Base::Rect rect) noexcept {
    if (!inRenderPass_ || !Base::IsValidRect(rect)) {
        return InvalidState("Scissor state requires a valid rectangle in a render pass");
    }
    Command command;
    command.kind = CommandKind::SetScissor;
    command.rect = rect;
    return Append(command);
}

Base::Result<void> CommandEncoder::Draw(
    std::uint32_t vertexCount,
    std::uint32_t instanceCount,
    std::uint32_t firstVertex,
    std::uint32_t firstInstance) noexcept {
    if (!inRenderPass_ || vertexCount == 0U || instanceCount == 0U) {
        return InvalidState("Draw requires an active render pass and non-zero counts");
    }
    Command command;
    command.kind = CommandKind::Draw;
    command.first = firstVertex;
    command.count = vertexCount;
    command.instanceCount = instanceCount;
    command.firstInstance = firstInstance;
    return Append(command);
}

Base::Result<void> CommandEncoder::DrawIndexed(
    std::uint32_t indexCount,
    std::uint32_t instanceCount,
    std::uint32_t firstIndex,
    std::int32_t baseVertex,
    std::uint32_t firstInstance) noexcept {
    if (!inRenderPass_ || indexCount == 0U || instanceCount == 0U) {
        return InvalidState("Indexed draw requires an active render pass and non-zero counts");
    }
    Command command;
    command.kind = CommandKind::DrawIndexed;
    command.first = firstIndex;
    command.count = indexCount;
    command.instanceCount = instanceCount;
    command.firstInstance = firstInstance;
    command.baseVertex = baseVertex;
    return Append(command);
}

Base::Result<CommandList> CommandEncoder::Finish() noexcept {
    Base::Result<void> recording = VerifyRecording();
    if (!recording) {
        return recording.GetStatus();
    }
    if (inRenderPass_) {
        return InvalidState("Graphics command buffer has an unclosed render pass");
    }
    finished_ = true;
    return std::move(buffer_);
}

Base::Result<IGraphicsBackend*> SelectGraphicsBackend(
    Base::Span<IGraphicsBackend*> backends,
    const BackendRequest& request) noexcept {
    IGraphicsBackend* fallback = nullptr;
    for (IGraphicsBackend* backend : backends) {
        if (backend == nullptr || backend->IsDeviceLost()) {
            continue;
        }
        const DeviceCapabilities device = backend->Capabilities();
        const GraphicsCapabilities graphics =
            backend->QueryGraphicsCapabilities();
        if (device.abiVersion != RhiAbiVersion ||
            !IsValidGraphicsCapabilities(graphics) ||
            !HasAllFeatures(graphics.features, request.requiredFeatures) ||
            (graphics.shaderLanguages & request.requiredShaderLanguages) !=
                request.requiredShaderLanguages) {
            continue;
        }
        if (request.preferred != GraphicsBackendKind::Invalid &&
            backend->Kind() == request.preferred) {
            return backend;
        }
        if (fallback == nullptr) {
            fallback = backend;
        }
    }
    if (fallback != nullptr &&
        (request.preferred == GraphicsBackendKind::Invalid ||
         request.allowFallback)) {
        return fallback;
    }
    return Unsupported("No graphics backend satisfies the requested capabilities");
}

DeviceCapabilities NullGraphicsBackend::Capabilities() const noexcept {
    DeviceCapabilities capabilities;
    capabilities.abiVersion = RhiAbiVersion;
    capabilities.maxFramesInFlight = 3U;
    capabilities.maxTextureDimension = 8192U;
    capabilities.supportsTimestampQueries = false;
    return capabilities;
}

GraphicsCapabilities
NullGraphicsBackend::QueryGraphicsCapabilities() const noexcept {
    GraphicsCapabilities capabilities;
    capabilities.backendKind = GraphicsBackendKind::Null;
    capabilities.features =
        FeatureBit(GraphicsFeature::TextureSampling) |
        FeatureBit(GraphicsFeature::RenderTargets) |
        FeatureBit(GraphicsFeature::VertexIndexBuffers) |
        FeatureBit(GraphicsFeature::UniformBuffers) |
        FeatureBit(GraphicsFeature::DepthStencil) |
        FeatureBit(GraphicsFeature::Instancing) |
        FeatureBit(GraphicsFeature::Scissor) |
        FeatureBit(GraphicsFeature::AnisotropicFiltering);
    capabilities.shaderLanguages =
        ShaderLanguageBit(ShaderLanguage::Dxbc) |
        ShaderLanguageBit(ShaderLanguage::Dxil) |
        ShaderLanguageBit(ShaderLanguage::SpirV) |
        ShaderLanguageBit(ShaderLanguage::MetalLib) |
        ShaderLanguageBit(ShaderLanguage::Glsl330) |
        ShaderLanguageBit(ShaderLanguage::GlslEs300) |
        ShaderLanguageBit(ShaderLanguage::Wgsl);
    capabilities.maxColorAttachments = MaxColorAttachments;
    capabilities.maxVertexAttributes = MaxVertexAttributes;
    capabilities.maxSampledTextures = 16U;
    capabilities.uniformBufferAlignment = 16U;
    return capabilities;
}

Base::Result<void> NullGraphicsBackend::CreateResource(
    ResourceHandle handle,
    const ResourceDescriptor& descriptor) noexcept {
    if (deviceLost_) {
        return InvalidState("Null graphics device is lost");
    }
    if (!handle.IsValid() || handle.type != descriptor.type) {
        return InvalidArgument(
            "Backend resource handle does not match descriptor");
    }
    if (Find(handle) != nullptr) {
        return InvalidState("Backend resource handle already exists");
    }
    ResourceRecord record;
    record.handle = handle;
    record.descriptor = descriptor;
    return resources_.TryPushBack(record);
}

void NullGraphicsBackend::DestroyResource(ResourceHandle handle) noexcept {
    for (std::uint32_t index = 0U; index < resources_.Size(); ++index) {
        if (resources_[index].handle == handle) {
            for (std::uint32_t current = index + 1U;
                 current < resources_.Size();
                 ++current) {
                resources_[current - 1U] = resources_[current];
            }
            resources_.PopBack();
            break;
        }
    }
}

NullGraphicsBackend::ResourceRecord* NullGraphicsBackend::Find(
    ResourceHandle handle) noexcept {
    for (ResourceRecord& record : resources_) {
        if (record.handle == handle) {
            return &record;
        }
    }
    return nullptr;
}

const NullGraphicsBackend::ResourceRecord* NullGraphicsBackend::Find(
    ResourceHandle handle) const noexcept {
    for (const ResourceRecord& record : resources_) {
        if (record.handle == handle) {
            return &record;
        }
    }
    return nullptr;
}

bool NullGraphicsBackend::IsConfigured(
    ResourceHandle handle,
    ConfigurationKind configuration) const noexcept {
    const ResourceRecord* record = Find(handle);
    return record != nullptr && record->configuration == configuration;
}

Base::Result<void> NullGraphicsBackend::ConfigureTexture(
    ResourceHandle handle,
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }
    ResourceRecord* record = Find(handle);
    if (record == nullptr ||
        (record->descriptor.type != ResourceType::Texture &&
         record->descriptor.type != ResourceType::RenderTarget) ||
        record->descriptor.texture.width != descriptor.width ||
        record->descriptor.texture.height != descriptor.height ||
        record->descriptor.texture.format != ToBaseTextureFormat(descriptor.format)) {
        return NotFound("Texture resource does not match its graphics descriptor");
    }
    if (record->descriptor.type == ResourceType::RenderTarget &&
        !HasTextureUsage(descriptor.usage, TextureUsage::RenderTarget)) {
        return InvalidArgument("Render-target resource lacks RenderTarget usage");
    }
    record->texture = descriptor;
    record->configurationHash = StableTextureHash(descriptor);
    record->configuration = ConfigurationKind::Texture;
    return {};
}

Base::Result<void> NullGraphicsBackend::ConfigureSampler(
    ResourceHandle handle,
    const SamplerDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateSamplerDescriptor(
        descriptor, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }
    ResourceRecord* record = Find(handle);
    if (record == nullptr || record->descriptor.type != ResourceType::Sampler) {
        return NotFound("Sampler resource was not found");
    }
    record->configurationHash = StableSamplerHash(descriptor);
    record->configuration = ConfigurationKind::Sampler;
    return {};
}

Base::Result<void> NullGraphicsBackend::ConfigurePipeline(
    ResourceHandle handle,
    const PipelineDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidatePipelineDescriptor(
        descriptor, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }
    ResourceRecord* record = Find(handle);
    if (record == nullptr || record->descriptor.type != ResourceType::Pipeline) {
        return NotFound("Pipeline resource was not found");
    }
    record->configurationHash = StablePipelineHash(descriptor);
    record->configuration = ConfigurationKind::Pipeline;
    return {};
}

Base::Result<void> NullGraphicsBackend::ValidateCommands(
    const CommandList& commands) const noexcept {
    const GraphicsCapabilities capabilities = QueryGraphicsCapabilities();
    const Base::Span<const std::uint8_t> uploadBytes = commands.UploadBytes();
    bool inPass = false;
    bool pipelineBound = false;
    bool indexBufferBound = false;

    for (const Command& command : commands.Commands()) {
        switch (command.kind) {
        case CommandKind::UploadBuffer: {
            Base::Result<void> uploadRange = ValidateUploadRange(
                command.uploadOffset, command.uploadSize, uploadBytes.Size());
            if (!uploadRange) {
                return uploadRange;
            }
            const ResourceRecord* record = Find(command.resource0);
            if (record == nullptr ||
                record->descriptor.type != ResourceType::Buffer ||
                (inPass && record->descriptor.buffer.usage !=
                    BufferUsage::Uniform) ||
                command.resourceSize != command.uploadSize ||
                command.resourceOffset > record->descriptor.buffer.sizeBytes ||
                command.resourceSize >
                    record->descriptor.buffer.sizeBytes - command.resourceOffset) {
                return InvalidArgument("Buffer upload exceeds the target resource");
            }
            break;
        }
        case CommandKind::UploadTexture: {
            if (inPass) {
                return InvalidState("Texture upload is not allowed inside a render pass");
            }
            Base::Result<void> uploadRange = ValidateUploadRange(
                command.uploadOffset, command.uploadSize, uploadBytes.Size());
            if (!uploadRange) {
                return uploadRange;
            }
            const ResourceRecord* record = Find(command.resource0);
            if (record == nullptr ||
                record->configuration != ConfigurationKind::Texture ||
                !HasTextureUsage(record->texture.usage,
                    TextureUsage::CopyDestination)) {
                return InvalidArgument("Texture upload target is not configured for copies");
            }
            const TextureRegion& region = command.textureRegion;
            if (region.width == 0U || region.height == 0U ||
                region.mipLevel >= record->texture.mipLevels ||
                region.arrayLayer >= record->texture.arrayLayers ||
                region.x > record->texture.width ||
                region.width > record->texture.width - region.x ||
                region.y > record->texture.height ||
                region.height > record->texture.height - region.y) {
                return InvalidArgument("Texture upload region exceeds the resource");
            }
            const std::uint32_t bytesPerPixel =
                TextureBytesPerPixel(record->texture.format);
            if (region.width > UINT32_MAX / bytesPerPixel) {
                return InvalidArgument("Texture row size overflows");
            }
            const std::uint32_t minimumRow = region.width * bytesPerPixel;
            if (region.bytesPerRow < minimumRow ||
                region.height > UINT32_MAX / region.bytesPerRow ||
                command.uploadSize < region.height * region.bytesPerRow) {
                return InvalidArgument("Texture upload payload is too small");
            }
            break;
        }
        case CommandKind::BeginRenderPass: {
            if (inPass) {
                return InvalidState("Nested render passes are not allowed");
            }
            Base::Result<void> pass = ValidatePassDescriptorBasic(
                command.renderPass);
            if (!pass) {
                return pass;
            }
            if (command.renderPass.colorAttachmentCount >
                capabilities.maxColorAttachments) {
                return Unsupported("Render pass exceeds backend attachment limits");
            }
            for (std::uint32_t index = 0U;
                 index < command.renderPass.colorAttachmentCount;
                 ++index) {
                const ResourceRecord* record = Find(
                    command.renderPass.colorAttachments[index].target);
                if (record == nullptr ||
                    record->configuration != ConfigurationKind::Texture ||
                    !HasTextureUsage(record->texture.usage,
                        TextureUsage::RenderTarget) ||
                    IsDepthFormat(record->texture.format)) {
                    return InvalidArgument("Color attachment is not a configured render target");
                }
            }
            if (command.renderPass.hasDepthStencil) {
                const ResourceRecord* record = Find(
                    command.renderPass.depthStencil.target);
                if (record == nullptr ||
                    record->configuration != ConfigurationKind::Texture ||
                    !HasTextureUsage(record->texture.usage,
                        TextureUsage::RenderTarget) ||
                    !IsDepthFormat(record->texture.format)) {
                    return InvalidArgument("Depth-stencil attachment is invalid");
                }
            }
            inPass = true;
            pipelineBound = false;
            indexBufferBound = false;
            break;
        }
        case CommandKind::EndRenderPass:
            if (!inPass) {
                return InvalidState("Render pass end has no matching begin");
            }
            inPass = false;
            pipelineBound = false;
            indexBufferBound = false;
            break;
        case CommandKind::BindPipeline:
            if (!inPass || !IsConfigured(
                    command.resource0, ConfigurationKind::Pipeline)) {
                return InvalidState("Pipeline binding is invalid");
            }
            pipelineBound = true;
            break;
        case CommandKind::BindVertexBuffer: {
            const ResourceRecord* record = Find(command.resource0);
            if (!inPass || command.slot >= MaxVertexBuffers ||
                record == nullptr ||
                record->descriptor.type != ResourceType::Buffer ||
                record->descriptor.buffer.usage != BufferUsage::Vertex ||
                command.resourceOffset >= record->descriptor.buffer.sizeBytes) {
                return InvalidState("Vertex-buffer binding is invalid");
            }
            break;
        }
        case CommandKind::BindIndexBuffer: {
            const ResourceRecord* record = Find(command.resource0);
            if (!inPass || record == nullptr ||
                record->descriptor.type != ResourceType::Buffer ||
                record->descriptor.buffer.usage != BufferUsage::Index ||
                command.resourceOffset >= record->descriptor.buffer.sizeBytes) {
                return InvalidState("Index-buffer binding is invalid");
            }
            indexBufferBound = true;
            break;
        }
        case CommandKind::BindUniformBuffer: {
            const ResourceRecord* record = Find(command.resource0);
            if (!inPass || record == nullptr ||
                record->descriptor.type != ResourceType::Buffer ||
                record->descriptor.buffer.usage != BufferUsage::Uniform ||
                command.resourceSize == 0U ||
                command.resourceOffset % capabilities.uniformBufferAlignment != 0U ||
                command.resourceOffset > record->descriptor.buffer.sizeBytes ||
                command.resourceSize >
                    record->descriptor.buffer.sizeBytes - command.resourceOffset) {
                return InvalidState("Uniform-buffer binding is invalid");
            }
            break;
        }
        case CommandKind::BindTextureSampler: {
            const ResourceRecord* texture = Find(command.resource0);
            if (!inPass || command.slot >= capabilities.maxSampledTextures ||
                texture == nullptr ||
                texture->configuration != ConfigurationKind::Texture ||
                !HasTextureUsage(texture->texture.usage, TextureUsage::Sampled) ||
                !IsConfigured(command.resource1, ConfigurationKind::Sampler)) {
                return InvalidState("Texture-sampler binding is invalid");
            }
            break;
        }
        case CommandKind::SetScissor:
            if (!inPass || !Base::IsValidRect(command.rect) ||
                !HasAllFeatures(capabilities.features,
                    FeatureBit(GraphicsFeature::Scissor))) {
                return InvalidState("Scissor command is invalid");
            }
            break;
        case CommandKind::Draw:
            if (!inPass || !pipelineBound || command.count == 0U ||
                command.instanceCount == 0U) {
                return InvalidState("Draw command is missing required state");
            }
            break;
        case CommandKind::DrawIndexed:
            if (!inPass || !pipelineBound || !indexBufferBound ||
                command.count == 0U || command.instanceCount == 0U) {
                return InvalidState("Indexed draw command is missing required state");
            }
            break;
        }
    }
    if (inPass) {
        return InvalidState("Graphics command buffer ends inside a render pass");
    }
    return {};
}

Base::Result<void> NullGraphicsBackend::Submit(
    const CommandList& commands,
    FenceValue signalFence) noexcept {
    if (IsDeviceLost()) {
        return InvalidState("Null graphics device is lost");
    }
    if (signalFence <= lastSubmittedFence_) {
        return InvalidArgument("Submission fence must increase monotonically");
    }
    Base::Result<void> valid = ValidateCommands(commands);
    if (!valid) {
        return valid;
    }
    lastSubmittedFence_ = signalFence;
    lastGraphicsHash_ = commands.StableHash();
    ++submissionCount_;
    return {};
}

void NullGraphicsBackend::CompleteThrough(FenceValue fence) noexcept {
    if (fence > lastSubmittedFence_) {
        fence = lastSubmittedFence_;
    }
    if (fence > completedFence_) {
        completedFence_ = fence;
    }
}

void NullGraphicsBackend::SimulateDeviceLoss() noexcept {
    deviceLost_ = true;
}

bool SokolBackendAdapter::IsValid() const noexcept {
    return api_.structSize >= sizeof(SokolBackendApi) &&
        api_.abiVersion == GraphicsAbiVersion &&
        api_.deviceCapabilities != nullptr &&
        api_.graphicsCapabilities != nullptr &&
        api_.createResource != nullptr &&
        api_.destroyResource != nullptr &&
        api_.configureTexture != nullptr &&
        api_.configureSampler != nullptr &&
        api_.configurePipeline != nullptr &&
        api_.submit != nullptr &&
        api_.completedFence != nullptr &&
        api_.isDeviceLost != nullptr;
}

DeviceCapabilities SokolBackendAdapter::Capabilities() const noexcept {
    return IsValid()
        ? api_.deviceCapabilities(api_.context)
        : DeviceCapabilities{};
}

GraphicsCapabilities
SokolBackendAdapter::QueryGraphicsCapabilities() const noexcept {
    GraphicsCapabilities capabilities = IsValid()
        ? api_.graphicsCapabilities(api_.context)
        : GraphicsCapabilities{};
    if (capabilities.backendKind == GraphicsBackendKind::Invalid) {
        capabilities.backendKind = GraphicsBackendKind::Sokol;
    }
    return capabilities;
}

Base::Result<void> SokolBackendAdapter::CreateResource(
    ResourceHandle handle,
    const ResourceDescriptor& descriptor) noexcept {
    return IsValid()
        ? api_.createResource(api_.context, handle, descriptor)
        : Base::Result<void>(Unsupported("Sokol backend function table is invalid"));
}

void SokolBackendAdapter::DestroyResource(ResourceHandle handle) noexcept {
    if (IsValid()) {
        api_.destroyResource(api_.context, handle);
    }
}

Base::Result<void> SokolBackendAdapter::ConfigureTexture(
    ResourceHandle handle,
    const TextureResourceDescriptor& descriptor) noexcept {
    return IsValid()
        ? api_.configureTexture(api_.context, handle, descriptor)
        : Base::Result<void>(Unsupported("Sokol backend function table is invalid"));
}

Base::Result<void> SokolBackendAdapter::ConfigureSampler(
    ResourceHandle handle,
    const SamplerDescriptor& descriptor) noexcept {
    return IsValid()
        ? api_.configureSampler(api_.context, handle, descriptor)
        : Base::Result<void>(Unsupported("Sokol backend function table is invalid"));
}

Base::Result<void> SokolBackendAdapter::ConfigurePipeline(
    ResourceHandle handle,
    const PipelineDescriptor& descriptor) noexcept {
    return IsValid()
        ? api_.configurePipeline(api_.context, handle, descriptor)
        : Base::Result<void>(Unsupported("Sokol backend function table is invalid"));
}

Base::Result<void> SokolBackendAdapter::Submit(
    const CommandList& commands,
    FenceValue signalFence) noexcept {
    if (!IsValid()) {
        return Unsupported("Sokol backend function table is invalid");
    }
    Base::Result<void> submitted =
        api_.submit(api_.context, commands, signalFence);
    if (submitted) {
        lastSubmittedFence_ = signalFence;
    }
    return submitted;
}

FenceValue SokolBackendAdapter::CompletedFence() const noexcept {
    return IsValid() ? api_.completedFence(api_.context) : 0U;
}

bool SokolBackendAdapter::IsDeviceLost() const noexcept {
    return !IsValid() || api_.isDeviceLost(api_.context);
}

} // namespace Aero::Rhi
