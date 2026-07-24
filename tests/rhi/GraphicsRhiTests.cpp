#include <Aero/Rhi/Graphics.hpp>

#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Rhi;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

PipelineDescriptor MakePipeline() noexcept {
    static const std::uint8_t VertexCode[] = {1U, 2U, 3U, 4U};
    static const std::uint8_t FragmentCode[] = {5U, 6U, 7U, 8U};

    PipelineDescriptor descriptor;
    descriptor.vertexShader.stage = ShaderStage::Vertex;
    descriptor.vertexShader.language = ShaderLanguage::SpirV;
    descriptor.vertexShader.bytecode = VertexCode;
    descriptor.vertexShader.bytecodeSize =
        static_cast<std::uint32_t>(sizeof(VertexCode));
    descriptor.vertexShader.entryPoint = StringView("vs_main");
    descriptor.vertexShader.stableId = UINT64_C(0x1001);

    descriptor.fragmentShader.stage = ShaderStage::Fragment;
    descriptor.fragmentShader.language = ShaderLanguage::SpirV;
    descriptor.fragmentShader.bytecode = FragmentCode;
    descriptor.fragmentShader.bytecodeSize =
        static_cast<std::uint32_t>(sizeof(FragmentCode));
    descriptor.fragmentShader.entryPoint = StringView("fs_main");
    descriptor.fragmentShader.stableId = UINT64_C(0x1002);

    descriptor.vertexLayout.bufferCount = 1U;
    descriptor.vertexLayout.attributeCount = 2U;
    descriptor.vertexLayout.buffers[0].stride = 16U;
    descriptor.vertexLayout.attributes[0].location = 0U;
    descriptor.vertexLayout.attributes[0].bufferSlot = 0U;
    descriptor.vertexLayout.attributes[0].format = VertexFormat::Float2;
    descriptor.vertexLayout.attributes[0].offset = 0U;
    descriptor.vertexLayout.attributes[1].location = 1U;
    descriptor.vertexLayout.attributes[1].bufferSlot = 0U;
    descriptor.vertexLayout.attributes[1].format = VertexFormat::Float2;
    descriptor.vertexLayout.attributes[1].offset = 8U;
    descriptor.blend.enabled = true;
    descriptor.blend.color.source = BlendFactor::SourceAlpha;
    descriptor.blend.color.destination = BlendFactor::OneMinusSourceAlpha;
    descriptor.blend.alpha.source = BlendFactor::One;
    descriptor.blend.alpha.destination = BlendFactor::OneMinusSourceAlpha;
    return descriptor;
}

bool TestDescriptorValidationAndHashing() {
    NullGraphicsBackend backend;
    const GraphicsCapabilities capabilities =
        backend.QueryGraphicsCapabilities();

    PipelineDescriptor pipeline = MakePipeline();
    CHECK(ValidatePipelineDescriptor(pipeline, capabilities));
    const std::uint64_t firstHash = StablePipelineHash(pipeline);
    CHECK(firstHash != 0U);
    CHECK(StablePipelineHash(pipeline) == firstHash);

    pipeline.vertexLayout.attributes[1].location = 0U;
    Result<void> duplicate = ValidatePipelineDescriptor(
        pipeline, capabilities);
    CHECK(!duplicate);
    CHECK(duplicate.GetStatus().code == ErrorCode::InvalidArgument);

    TextureResourceDescriptor texture;
    texture.width = 32U;
    texture.height = 16U;
    texture.usage = TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination);
    CHECK(ValidateTextureDescriptor(texture, capabilities));

    SamplerDescriptor sampler;
    sampler.maxAnisotropy = 4U;
    CHECK(ValidateSamplerDescriptor(sampler, capabilities));
    return true;
}

struct FakeSokolState final {
    FenceValue completed = 0U;
    std::uint32_t created = 0U;
    std::uint32_t configured = 0U;
    std::uint32_t submitted = 0U;
    bool lost = false;
};

DeviceCapabilities FakeDeviceCapabilities(void*) noexcept {
    DeviceCapabilities capabilities;
    capabilities.maxFramesInFlight = 3U;
    return capabilities;
}

GraphicsCapabilities FakeGraphicsCapabilities(void*) noexcept {
    GraphicsCapabilities capabilities;
    capabilities.backendKind = GraphicsBackendKind::Sokol;
    capabilities.features =
        FeatureBit(GraphicsFeature::TextureSampling) |
        FeatureBit(GraphicsFeature::RenderTargets) |
        FeatureBit(GraphicsFeature::VertexIndexBuffers) |
        FeatureBit(GraphicsFeature::UniformBuffers);
    capabilities.shaderLanguages =
        ShaderLanguageBit(ShaderLanguage::Glsl330) |
        ShaderLanguageBit(ShaderLanguage::GlslEs300);
    return capabilities;
}

Result<void> FakeCreateResource(
    void* context,
    ResourceHandle,
    const ResourceDescriptor&) noexcept {
    auto* state = static_cast<FakeSokolState*>(context);
    ++state->created;
    return {};
}

void FakeDestroyResource(void*, ResourceHandle) noexcept {}

Result<void> FakeConfigureTexture(
    void* context,
    ResourceHandle,
    const TextureResourceDescriptor&) noexcept {
    auto* state = static_cast<FakeSokolState*>(context);
    ++state->configured;
    return {};
}

Result<void> FakeConfigureSampler(
    void* context,
    ResourceHandle,
    const SamplerDescriptor&) noexcept {
    auto* state = static_cast<FakeSokolState*>(context);
    ++state->configured;
    return {};
}

Result<void> FakeConfigurePipeline(
    void* context,
    ResourceHandle,
    const PipelineDescriptor&) noexcept {
    auto* state = static_cast<FakeSokolState*>(context);
    ++state->configured;
    return {};
}

Result<void> FakeSubmit(
    void* context,
    const CommandList&,
    FenceValue signalFence) noexcept {
    auto* state = static_cast<FakeSokolState*>(context);
    ++state->submitted;
    state->completed = signalFence;
    return {};
}

FenceValue FakeCompletedFence(void* context) noexcept {
    return static_cast<FakeSokolState*>(context)->completed;
}

bool FakeDeviceLost(void* context) noexcept {
    return static_cast<FakeSokolState*>(context)->lost;
}

bool TestBackendSelectionAndSokolAdapter() {
    FakeSokolState state;
    SokolBackendApi api;
    api.structSize = static_cast<std::uint32_t>(sizeof(SokolBackendApi));
    api.context = &state;
    api.deviceCapabilities = &FakeDeviceCapabilities;
    api.graphicsCapabilities = &FakeGraphicsCapabilities;
    api.createResource = &FakeCreateResource;
    api.destroyResource = &FakeDestroyResource;
    api.configureTexture = &FakeConfigureTexture;
    api.configureSampler = &FakeConfigureSampler;
    api.configurePipeline = &FakeConfigurePipeline;
    api.submit = &FakeSubmit;
    api.completedFence = &FakeCompletedFence;
    api.isDeviceLost = &FakeDeviceLost;

    SokolBackendAdapter sokol(api);
    CHECK(sokol.IsValid());
    NullGraphicsBackend nullBackend;
    IGraphicsBackend* candidates[] = {&nullBackend, &sokol};

    BackendRequest request;
    request.preferred = GraphicsBackendKind::Sokol;
    request.requiredFeatures = FeatureBit(GraphicsFeature::TextureSampling);
    request.requiredShaderLanguages =
        ShaderLanguageBit(ShaderLanguage::GlslEs300);
    Result<IGraphicsBackend*> selected = SelectGraphicsBackend(
        Span<IGraphicsBackend*>(candidates, 2U), request);
    CHECK(selected);
    CHECK(selected.Value() == &sokol);

    SokolBackendApi invalidApi;
    SokolBackendAdapter invalid(invalidApi);
    CHECK(!invalid.IsValid());
    return true;
}

bool TestResourcesCommandsAndSubmission() {
    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());

    BufferDescriptor vertexDescriptor;
    vertexDescriptor.sizeBytes = 256U;
    vertexDescriptor.usage = BufferUsage::Vertex;
    Result<ResourceHandle> vertex = device.CreateBuffer(vertexDescriptor);
    CHECK(vertex);

    BufferDescriptor indexDescriptor;
    indexDescriptor.sizeBytes = 128U;
    indexDescriptor.usage = BufferUsage::Index;
    Result<ResourceHandle> index = device.CreateBuffer(indexDescriptor);
    CHECK(index);

    BufferDescriptor uniformDescriptor;
    uniformDescriptor.sizeBytes = 256U;
    uniformDescriptor.usage = BufferUsage::Uniform;
    Result<ResourceHandle> uniform = device.CreateBuffer(uniformDescriptor);
    CHECK(uniform);

    TextureResourceDescriptor textureDescriptor;
    textureDescriptor.width = 4U;
    textureDescriptor.height = 4U;
    textureDescriptor.usage = TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination);
    Result<ResourceHandle> texture = device.CreateTexture(textureDescriptor);
    CHECK(texture);

    TextureResourceDescriptor targetDescriptor;
    targetDescriptor.width = 64U;
    targetDescriptor.height = 48U;
    targetDescriptor.usage = TextureUsageBit(TextureUsage::RenderTarget);
    Result<ResourceHandle> target = device.CreateRenderTarget(targetDescriptor);
    CHECK(target);

    SamplerDescriptor samplerDescriptor;
    samplerDescriptor.maxAnisotropy = 4U;
    Result<ResourceHandle> sampler = device.CreateSampler(samplerDescriptor);
    CHECK(sampler);

    PipelineDescriptor pipelineDescriptor = MakePipeline();
    Result<ResourceHandle> pipeline = device.CreatePipeline(pipelineDescriptor);
    CHECK(pipeline);
    CHECK(device.LiveResourceCount() == 7U);

    const std::uint8_t vertexBytes[32]{};
    const std::uint8_t indexBytes[12]{};
    const std::uint8_t uniformBytes[16]{};
    const std::uint8_t textureBytes[64]{};

    CommandEncoder encoder;
    CHECK(encoder.UploadBuffer(vertex.Value(), 0U, vertexBytes));
    CHECK(encoder.UploadBuffer(index.Value(), 0U, indexBytes));
    CHECK(encoder.UploadBuffer(uniform.Value(), 0U, uniformBytes));
    CHECK(encoder.UploadTexture(
        texture.Value(),
        {0U, 0U, 4U, 4U, 0U, 0U, 16U},
        textureBytes));

    RenderPassDescriptor pass;
    pass.renderArea = {0.0, 0.0, 64.0, 48.0};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0].target = target.Value();
    pass.colorAttachments[0].load = LoadOperation::Clear;
    pass.colorAttachments[0].clearColor = {0.1F, 0.2F, 0.3F, 1.0F};
    CHECK(encoder.BeginRenderPass(pass));
    CHECK(encoder.BindPipeline(pipeline.Value()));
    CHECK(encoder.BindVertexBuffer(0U, vertex.Value()));
    CHECK(encoder.BindIndexBuffer(index.Value(), IndexType::UInt16));
    CHECK(encoder.BindUniformBuffer(0U, uniform.Value(), 0U, 16U));
    CHECK(encoder.BindTextureSampler(0U, texture.Value(), sampler.Value()));
    CHECK(encoder.SetScissor({0.0, 0.0, 64.0, 48.0}));
    CHECK(encoder.DrawIndexed(6U));
    CHECK(encoder.EndRenderPass());

    Result<CommandList> commandBuffer = encoder.Finish();
    CHECK(commandBuffer);
    CHECK(commandBuffer.Value().CommandCount() == 13U);
    CHECK(commandBuffer.Value().UploadByteCount() == 124U);
    const std::uint64_t commandHash = commandBuffer.Value().StableHash();
    CHECK(commandHash != 0U);

    Result<FenceValue> fence = device.Submit(commandBuffer.Value());
    CHECK(fence);
    CHECK(fence.Value() == 1U);
    CHECK(backend.SubmissionCount() == 1U);
    CHECK(backend.LastGraphicsHash() == commandHash);

    backend.CompleteThrough(fence.Value());
    const ResourceHandle handles[] = {
        vertex.Value(), index.Value(), uniform.Value(), texture.Value(),
        target.Value(), sampler.Value(), pipeline.Value()};
    for (ResourceHandle handle : handles) {
        CHECK(device.DestroyResource(handle, fence.Value()));
    }
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected);
    CHECK(collected.Value() == 7U);
    CHECK(device.LiveResourceCount() == 0U);
    CHECK(backend.LiveBackendResourceCount() == 0U);
    return true;
}

bool TestInvalidEncodingAndResources() {
    CommandEncoder encoder;
    Result<void> drawOutsidePass = encoder.Draw(3U);
    CHECK(!drawOutsidePass);
    CHECK(drawOutsidePass.GetStatus().code == ErrorCode::InvalidState);

    NullGraphicsBackend backend;
    RhiDevice device(backend);
    CHECK(device.Initialize());
    TextureResourceDescriptor invalidTarget;
    invalidTarget.width = 16U;
    invalidTarget.height = 16U;
    Result<ResourceHandle> target = device.CreateRenderTarget(invalidTarget);
    CHECK(!target);
    CHECK(target.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

} // namespace

int main() {
    if (!TestDescriptorValidationAndHashing()) return 1;
    if (!TestBackendSelectionAndSokolAdapter()) return 1;
    if (!TestResourcesCommandsAndSubmission()) return 1;
    if (!TestInvalidEncodingAndResources()) return 1;
    std::puts("Aero graphics RHI tests passed");
    return 0;
}
