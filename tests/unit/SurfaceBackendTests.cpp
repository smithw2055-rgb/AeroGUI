#include <Aero/Rhi/Surface.hpp>

#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
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
    descriptor.vertexShader.stableId = UINT64_C(0x7001);

    descriptor.fragmentShader.stage = ShaderStage::Fragment;
    descriptor.fragmentShader.language = ShaderLanguage::SpirV;
    descriptor.fragmentShader.bytecode = FragmentCode;
    descriptor.fragmentShader.bytecodeSize =
        static_cast<std::uint32_t>(sizeof(FragmentCode));
    descriptor.fragmentShader.entryPoint = StringView("fs_main");
    descriptor.fragmentShader.stableId = UINT64_C(0x7002);

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
    descriptor.blend.color.destination =
        BlendFactor::OneMinusSourceAlpha;
    return descriptor;
}

SurfaceCapabilities MakeSurfaceCapabilities() noexcept {
    SurfaceCapabilities capabilities;
    capabilities.supportedKinds =
        SurfaceKindBit(SurfaceKind::Headless) |
        SurfaceKindBit(SurfaceKind::D3D11Window) |
        SurfaceKindBit(SurfaceKind::WglWindow) |
        SurfaceKindBit(SurfaceKind::GlxWindow) |
        SurfaceKindBit(SurfaceKind::EglWindow) |
        SurfaceKindBit(SurfaceKind::WebGL2Canvas) |
        SurfaceKindBit(SurfaceKind::ExternalRenderTarget);
    capabilities.maxWidth = 4096U;
    capabilities.maxHeight = 4096U;
    capabilities.supportsResize = true;
    capabilities.supportsPresent = true;
    capabilities.supportsContextLossRecovery = true;
    capabilities.supportsExternalRenderTargets = true;
    return capabilities;
}

NativeSurfaceDescriptor MakeHeadlessSurface(
    std::uint32_t width = 64U,
    std::uint32_t height = 48U) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::Headless;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.colorFormat = GraphicsTextureFormat::Rgba8Unorm;
    descriptor.stableId = UINT64_C(0xA001);
    return descriptor;
}

NativeSurfaceDescriptor MakeWebGlSurface(
    std::uint32_t width = 320U,
    std::uint32_t height = 200U) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::WebGL2Canvas;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.colorFormat = GraphicsTextureFormat::Rgba8Unorm;
    descriptor.webgl2.contextHandle = 17U;
    descriptor.webgl2.canvasId = UINT64_C(0xCAFE);
    descriptor.stableId = UINT64_C(0xA002);
    return descriptor;
}

struct FakeHost final {
    NativeSurfaceDescriptor surface;
    FenceValue completedFence = 0U;
    FenceValue lastSignaledFence = 0U;
    std::uint64_t activeFrameSerial = 0U;
    std::uint32_t resourcesCreated = 0U;
    std::uint32_t resourcesDestroyed = 0U;
    std::uint32_t resourcesConfigured = 0U;
    std::uint32_t uploadBufferCount = 0U;
    std::uint32_t uploadTextureCount = 0U;
    std::uint32_t beginPassCount = 0U;
    std::uint32_t endPassCount = 0U;
    std::uint32_t bindingCount = 0U;
    std::uint32_t drawCount = 0U;
    std::uint32_t signalCount = 0U;
    std::uint32_t surfaceCreateCount = 0U;
    std::uint32_t surfaceDestroyCount = 0U;
    std::uint32_t resizeCount = 0U;
    std::uint32_t acquireCount = 0U;
    std::uint32_t presentCount = 0U;
    std::uint32_t discardCount = 0U;
    std::uint32_t restoreCount = 0U;
    std::uint32_t uploadedBytes = 0U;
    bool surfaceCreated = false;
    bool surfaceLost = false;
    bool deviceLost = false;
    bool inRenderPass = false;
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
        FeatureBit(GraphicsFeature::UniformBuffers) |
        FeatureBit(GraphicsFeature::Scissor);
    capabilities.shaderLanguages =
        ShaderLanguageBit(ShaderLanguage::SpirV) |
        ShaderLanguageBit(ShaderLanguage::Glsl330) |
        ShaderLanguageBit(ShaderLanguage::GlslEs300);
    capabilities.maxColorAttachments = 4U;
    capabilities.maxVertexAttributes = 16U;
    capabilities.maxSampledTextures = 8U;
    capabilities.uniformBufferAlignment = 16U;
    return capabilities;
}

SurfaceCapabilities FakeSurfaceCapabilities(void*) noexcept {
    return MakeSurfaceCapabilities();
}

Result<void> FakeCreateResource(
    void* context,
    ResourceHandle,
    const ResourceDescriptor&) noexcept {
    ++static_cast<FakeHost*>(context)->resourcesCreated;
    return {};
}

void FakeDestroyResource(
    void* context,
    ResourceHandle) noexcept {
    ++static_cast<FakeHost*>(context)->resourcesDestroyed;
}

Result<void> FakeConfigureTexture(
    void* context,
    ResourceHandle,
    const TextureResourceDescriptor&) noexcept {
    ++static_cast<FakeHost*>(context)->resourcesConfigured;
    return {};
}

Result<void> FakeConfigureSampler(
    void* context,
    ResourceHandle,
    const SamplerDescriptor&) noexcept {
    ++static_cast<FakeHost*>(context)->resourcesConfigured;
    return {};
}

Result<void> FakeConfigurePipeline(
    void* context,
    ResourceHandle,
    const PipelineDescriptor&) noexcept {
    ++static_cast<FakeHost*>(context)->resourcesConfigured;
    return {};
}

Result<void> FakeLegacySubmit(
    void* context,
    const CommandBuffer&,
    FenceValue signalFence) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (signalFence <= host.lastSignaledFence) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Fence is not monotonic");
    }
    host.lastSignaledFence = signalFence;
    host.completedFence = signalFence;
    return {};
}

Result<void> FakeUploadBuffer(
    void* context,
    ResourceHandle,
    std::uint64_t,
    Span<const std::uint8_t> data) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    ++host.uploadBufferCount;
    host.uploadedBytes += data.Size();
    return {};
}

Result<void> FakeUploadTexture(
    void* context,
    ResourceHandle,
    const TextureRegion&,
    Span<const std::uint8_t> data) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    ++host.uploadTextureCount;
    host.uploadedBytes += data.Size();
    return {};
}

Result<void> FakeBeginRenderPass(
    void* context,
    const RenderPassDescriptor&) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (host.inRenderPass) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Render pass is already active");
    }
    host.inRenderPass = true;
    ++host.beginPassCount;
    return {};
}

Result<void> FakeEndRenderPass(void* context) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (!host.inRenderPass) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Render pass is not active");
    }
    host.inRenderPass = false;
    ++host.endPassCount;
    return {};
}

Result<void> FakeBindPipeline(
    void* context,
    ResourceHandle) noexcept {
    ++static_cast<FakeHost*>(context)->bindingCount;
    return {};
}

Result<void> FakeBindVertexBuffer(
    void* context,
    std::uint32_t,
    ResourceHandle,
    std::uint64_t) noexcept {
    ++static_cast<FakeHost*>(context)->bindingCount;
    return {};
}

Result<void> FakeBindIndexBuffer(
    void* context,
    ResourceHandle,
    IndexType,
    std::uint64_t) noexcept {
    ++static_cast<FakeHost*>(context)->bindingCount;
    return {};
}

Result<void> FakeBindUniformBuffer(
    void* context,
    std::uint32_t,
    ResourceHandle,
    std::uint64_t,
    std::uint32_t) noexcept {
    ++static_cast<FakeHost*>(context)->bindingCount;
    return {};
}

Result<void> FakeBindTextureSampler(
    void* context,
    std::uint32_t,
    ResourceHandle,
    ResourceHandle) noexcept {
    ++static_cast<FakeHost*>(context)->bindingCount;
    return {};
}

Result<void> FakeSetScissor(
    void* context,
    Rect) noexcept {
    ++static_cast<FakeHost*>(context)->bindingCount;
    return {};
}

Result<void> FakeDraw(
    void* context,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t) noexcept {
    ++static_cast<FakeHost*>(context)->drawCount;
    return {};
}

Result<void> FakeDrawIndexed(
    void* context,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::int32_t,
    std::uint32_t) noexcept {
    ++static_cast<FakeHost*>(context)->drawCount;
    return {};
}

Result<void> FakeSignalFence(
    void* context,
    FenceValue signalFence) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (signalFence <= host.lastSignaledFence) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Fence is not monotonic");
    }
    host.lastSignaledFence = signalFence;
    host.completedFence = signalFence;
    ++host.signalCount;
    return {};
}

FenceValue FakeCompletedFence(void* context) noexcept {
    return static_cast<FakeHost*>(context)->completedFence;
}

bool FakeIsDeviceLost(void* context) noexcept {
    return static_cast<FakeHost*>(context)->deviceLost;
}

Result<void> FakeCreateSurface(
    void* context,
    const NativeSurfaceDescriptor& descriptor) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (host.surfaceCreated) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Surface is already created");
    }
    host.surface = descriptor;
    host.surfaceCreated = true;
    host.surfaceLost = false;
    ++host.surfaceCreateCount;
    return {};
}

void FakeDestroySurface(void* context) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    host.surfaceCreated = false;
    host.activeFrameSerial = 0U;
    ++host.surfaceDestroyCount;
}

Result<void> FakeResizeSurface(
    void* context,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (!host.surfaceCreated || host.surfaceLost) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Surface is unavailable");
    }
    host.surface.width = width;
    host.surface.height = height;
    ++host.resizeCount;
    return {};
}

Result<ExternalRenderTargetDescriptor> FakeAcquireSurfaceTarget(
    void* context,
    std::uint64_t frameSerial) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (!host.surfaceCreated || host.surfaceLost ||
        host.activeFrameSerial != 0U) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Surface target cannot be acquired");
    }

    ExternalRenderTargetDescriptor target;
    target.width = host.surface.width;
    target.height = host.surface.height;
    target.colorFormat = host.surface.colorFormat;
    target.depthStencilFormat = host.surface.depthStencilFormat;
    target.sampleCount = host.surface.sampleCount;
    target.defaultFramebuffer =
        host.surface.kind == SurfaceKind::WebGL2Canvas ||
        host.surface.kind == SurfaceKind::WglWindow ||
        host.surface.kind == SurfaceKind::GlxWindow ||
        host.surface.kind == SurfaceKind::EglWindow;
    target.colorTarget = target.defaultFramebuffer
        ? 0U
        : static_cast<std::uintptr_t>(UINT64_C(0x10000) + frameSerial);
    target.stableId = host.surface.stableId ^ frameSerial;

    host.activeFrameSerial = frameSerial;
    ++host.acquireCount;
    return target;
}

Result<void> FakePresentSurface(
    void* context,
    std::uint64_t frameSerial,
    FenceValue signalFence) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (host.activeFrameSerial != frameSerial ||
        signalFence == 0U ||
        signalFence > host.completedFence) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Surface frame is not ready to present");
    }
    host.activeFrameSerial = 0U;
    ++host.presentCount;
    return {};
}

void FakeDiscardSurfaceFrame(
    void* context,
    std::uint64_t frameSerial) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (host.activeFrameSerial == frameSerial) {
        host.activeFrameSerial = 0U;
        ++host.discardCount;
    }
}

void FakeNotifySurfaceLost(void* context) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    host.surfaceLost = true;
    host.activeFrameSerial = 0U;
}

Result<void> FakeRestoreSurface(
    void* context,
    const NativeSurfaceDescriptor& descriptor) noexcept {
    FakeHost& host = *static_cast<FakeHost*>(context);
    if (!host.surfaceCreated || !host.surfaceLost) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Surface is not lost");
    }
    host.surface = descriptor;
    host.surfaceLost = false;
    ++host.restoreCount;
    return {};
}

bool FakeIsSurfaceLost(void* context) noexcept {
    return static_cast<FakeHost*>(context)->surfaceLost;
}

HostedGraphicsApi MakeHostedApi(FakeHost& host) noexcept {
    HostedGraphicsApi api;
    api.structSize = static_cast<std::uint32_t>(sizeof(HostedGraphicsApi));
    api.context = &host;
    api.kind = GraphicsBackendKind::Sokol;
    api.deviceCapabilities = &FakeDeviceCapabilities;
    api.graphicsCapabilities = &FakeGraphicsCapabilities;
    api.surfaceCapabilities = &FakeSurfaceCapabilities;
    api.createResource = &FakeCreateResource;
    api.destroyResource = &FakeDestroyResource;
    api.configureTexture = &FakeConfigureTexture;
    api.configureSampler = &FakeConfigureSampler;
    api.configurePipeline = &FakeConfigurePipeline;
    api.submitLegacy = &FakeLegacySubmit;
    api.uploadBuffer = &FakeUploadBuffer;
    api.uploadTexture = &FakeUploadTexture;
    api.beginRenderPass = &FakeBeginRenderPass;
    api.endRenderPass = &FakeEndRenderPass;
    api.bindPipeline = &FakeBindPipeline;
    api.bindVertexBuffer = &FakeBindVertexBuffer;
    api.bindIndexBuffer = &FakeBindIndexBuffer;
    api.bindUniformBuffer = &FakeBindUniformBuffer;
    api.bindTextureSampler = &FakeBindTextureSampler;
    api.setScissor = &FakeSetScissor;
    api.draw = &FakeDraw;
    api.drawIndexed = &FakeDrawIndexed;
    api.signalFence = &FakeSignalFence;
    api.completedFence = &FakeCompletedFence;
    api.isDeviceLost = &FakeIsDeviceLost;
    api.createSurface = &FakeCreateSurface;
    api.destroySurface = &FakeDestroySurface;
    api.resizeSurface = &FakeResizeSurface;
    api.acquireSurfaceTarget = &FakeAcquireSurfaceTarget;
    api.presentSurface = &FakePresentSurface;
    api.discardSurfaceFrame = &FakeDiscardSurfaceFrame;
    api.notifySurfaceLost = &FakeNotifySurfaceLost;
    api.restoreSurface = &FakeRestoreSurface;
    api.isSurfaceLost = &FakeIsSurfaceLost;
    return api;
}

bool TestSurfaceDescriptorValidation() {
    const SurfaceCapabilities capabilities = MakeSurfaceCapabilities();

    NativeSurfaceDescriptor headless = MakeHeadlessSurface();
    CHECK(ValidateNativeSurfaceDescriptor(headless, capabilities));

    NativeSurfaceDescriptor d3d11 = headless;
    d3d11.kind = SurfaceKind::D3D11Window;
    d3d11.d3d11.window = 1U;
    d3d11.d3d11.device = 2U;
    d3d11.d3d11.immediateContext = 3U;
    CHECK(ValidateNativeSurfaceDescriptor(d3d11, capabilities));
    d3d11.d3d11.device = 0U;
    CHECK(!ValidateNativeSurfaceDescriptor(d3d11, capabilities));

    NativeSurfaceDescriptor wgl = headless;
    wgl.kind = SurfaceKind::WglWindow;
    wgl.wgl.deviceContext = 11U;
    wgl.wgl.renderContext = 12U;
    CHECK(ValidateNativeSurfaceDescriptor(wgl, capabilities));

    NativeSurfaceDescriptor glx = headless;
    glx.kind = SurfaceKind::GlxWindow;
    glx.glx.display = 21U;
    glx.glx.drawable = 22U;
    glx.glx.context = 23U;
    CHECK(ValidateNativeSurfaceDescriptor(glx, capabilities));

    NativeSurfaceDescriptor egl = headless;
    egl.kind = SurfaceKind::EglWindow;
    egl.egl.display = 31U;
    egl.egl.surface = 32U;
    egl.egl.context = 33U;
    CHECK(ValidateNativeSurfaceDescriptor(egl, capabilities));

    NativeSurfaceDescriptor webgl = MakeWebGlSurface();
    CHECK(ValidateNativeSurfaceDescriptor(webgl, capabilities));

    NativeSurfaceDescriptor external = headless;
    external.kind = SurfaceKind::ExternalRenderTarget;
    external.external.colorTarget = 41U;
    CHECK(ValidateNativeSurfaceDescriptor(external, capabilities));

    ExternalRenderTargetDescriptor target;
    target.width = 64U;
    target.height = 48U;
    target.colorTarget = 51U;
    CHECK(ValidateExternalRenderTargetDescriptor(target));
    target.colorFormat = GraphicsTextureFormat::Depth24Stencil8;
    CHECK(!ValidateExternalRenderTargetDescriptor(target));
    return true;
}

bool TestHostedOffscreenSmoke() {
    FakeHost host;
    HostedGraphicsBackend backend(MakeHostedApi(host));
    CHECK(backend.IsValid());
    CHECK(backend.Kind() == GraphicsBackendKind::Sokol);

    RhiDevice device(backend);
    CHECK(device.Initialize());
    GraphicsResourceFactory resources(device, backend);

    BufferDescriptor vertexDescriptor;
    vertexDescriptor.sizeBytes = 256U;
    vertexDescriptor.usage = BufferUsage::Vertex;
    Result<ResourceHandle> vertex = resources.CreateBuffer(vertexDescriptor);
    CHECK(vertex);

    BufferDescriptor indexDescriptor;
    indexDescriptor.sizeBytes = 128U;
    indexDescriptor.usage = BufferUsage::Index;
    Result<ResourceHandle> index = resources.CreateBuffer(indexDescriptor);
    CHECK(index);

    BufferDescriptor uniformDescriptor;
    uniformDescriptor.sizeBytes = 256U;
    uniformDescriptor.usage = BufferUsage::Uniform;
    Result<ResourceHandle> uniform = resources.CreateBuffer(uniformDescriptor);
    CHECK(uniform);

    TextureResourceDescriptor textureDescriptor;
    textureDescriptor.width = 4U;
    textureDescriptor.height = 4U;
    textureDescriptor.usage =
        TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination);
    Result<ResourceHandle> texture =
        resources.CreateTexture(textureDescriptor);
    CHECK(texture);

    TextureResourceDescriptor targetDescriptor;
    targetDescriptor.width = 64U;
    targetDescriptor.height = 48U;
    targetDescriptor.usage =
        TextureUsageBit(TextureUsage::RenderTarget);
    Result<ResourceHandle> target =
        resources.CreateRenderTarget(targetDescriptor);
    CHECK(target);

    SamplerDescriptor samplerDescriptor;
    Result<ResourceHandle> sampler =
        resources.CreateSampler(samplerDescriptor);
    CHECK(sampler);

    PipelineDescriptor pipelineDescriptor = MakePipeline();
    Result<ResourceHandle> pipeline =
        resources.CreatePipeline(pipelineDescriptor);
    CHECK(pipeline);

    SurfaceSession surface(backend);
    CHECK(surface.Initialize(MakeHeadlessSurface()));
    SurfaceGraphicsQueue queue(backend, surface);
    CHECK(queue.Initialize());
    Result<SurfaceFrame> frame = surface.AcquireFrame();
    CHECK(frame);

    const std::uint8_t vertexBytes[32]{};
    const std::uint8_t indexBytes[12]{};
    const std::uint8_t uniformBytes[16]{};
    const std::uint8_t textureBytes[64]{};

    GraphicsCommandEncoder encoder;
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
    pass.colorAttachments[0].clearColor =
        {0.1F, 0.2F, 0.3F, 1.0F};
    CHECK(encoder.BeginRenderPass(pass));
    CHECK(encoder.BindPipeline(pipeline.Value()));
    CHECK(encoder.BindVertexBuffer(0U, vertex.Value()));
    CHECK(encoder.BindIndexBuffer(index.Value(), IndexType::UInt16));
    CHECK(encoder.BindUniformBuffer(
        0U, uniform.Value(), 0U, 16U));
    CHECK(encoder.BindTextureSampler(
        0U, texture.Value(), sampler.Value()));
    CHECK(encoder.SetScissor({0.0, 0.0, 64.0, 48.0}));
    CHECK(encoder.DrawIndexed(6U));
    CHECK(encoder.EndRenderPass());

    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    CHECK(commands);
    const std::uint64_t commandHash = commands.Value().StableHash();
    Result<FenceValue> fence =
        queue.SubmitAndPresent(frame.Value(), commands.Value());
    CHECK(fence && fence.Value() == 1U);

    CHECK(host.resourcesCreated == 7U);
    CHECK(host.resourcesConfigured == 4U);
    CHECK(host.uploadBufferCount == 3U);
    CHECK(host.uploadTextureCount == 1U);
    CHECK(host.uploadedBytes == 124U);
    CHECK(host.beginPassCount == 1U);
    CHECK(host.endPassCount == 1U);
    CHECK(host.bindingCount == 6U);
    CHECK(host.drawCount == 1U);
    CHECK(host.signalCount == 1U);
    CHECK(host.presentCount == 1U);
    CHECK(queue.LastCapture().presented);
    CHECK(queue.LastCapture().commandHash == commandHash);
    CHECK(queue.LastCapture().commandCount == 13U);
    CHECK(queue.LastCapture().uploadByteCount == 124U);
    CHECK(queue.LastCapture().width == 64U);
    CHECK(queue.LastCapture().height == 48U);

    const ResourceHandle handles[] = {
        vertex.Value(),
        index.Value(),
        uniform.Value(),
        texture.Value(),
        target.Value(),
        sampler.Value(),
        pipeline.Value()};
    for (ResourceHandle handle : handles) {
        CHECK(device.DestroyResource(handle, fence.Value()));
    }
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected && collected.Value() == 7U);
    CHECK(host.resourcesDestroyed == 7U);

    surface.Shutdown();
    CHECK(host.surfaceDestroyCount == 1U);
    return true;
}

bool TestResizeAndContextLoss() {
    FakeHost host;
    HostedGraphicsBackend backend(MakeHostedApi(host));
    SurfaceSession surface(backend);
    CHECK(surface.Initialize(MakeWebGlSurface()));
    SurfaceGraphicsQueue queue(backend, surface);
    CHECK(queue.Initialize());

    const std::uint64_t initialGeneration = surface.Generation();
    Result<SurfaceFrame> first = surface.AcquireFrame();
    CHECK(first);
    CHECK(!surface.Resize(640U, 360U));
    CHECK(surface.DiscardFrame(first.Value()));
    CHECK(surface.Resize(640U, 360U));
    CHECK(surface.Generation() > initialGeneration);
    CHECK(host.resizeCount == 1U);

    Result<SurfaceFrame> second = surface.AcquireFrame();
    CHECK(second);
    CHECK(surface.NotifyContextLost());
    CHECK(surface.State() == SurfaceState::Lost);
    CHECK(host.surfaceLost);
    CHECK(host.discardCount == 2U);
    CHECK(!surface.AcquireFrame());

    NativeSurfaceDescriptor restored = MakeWebGlSurface(800U, 450U);
    CHECK(surface.Restore(restored));
    CHECK(surface.State() == SurfaceState::Ready);
    CHECK(host.restoreCount == 1U);

    Result<SurfaceFrame> third = surface.AcquireFrame();
    CHECK(third);
    CHECK(third.Value().target.defaultFramebuffer);
    GraphicsCommandEncoder encoder;
    Result<GraphicsCommandBuffer> empty = encoder.Finish();
    CHECK(empty);
    Result<FenceValue> fence =
        queue.SubmitAndPresent(third.Value(), empty.Value());
    CHECK(fence && fence.Value() == 1U);
    CHECK(host.presentCount == 1U);
    return true;
}

bool TestInvalidHostedApiAndDeviceLoss() {
    HostedGraphicsApi emptyApi;
    HostedGraphicsBackend empty(emptyApi);
    CHECK(!empty.IsValid());

    FakeHost host;
    HostedGraphicsApi api = MakeHostedApi(host);
    api.drawIndexed = nullptr;
    HostedGraphicsBackend incomplete(api);
    CHECK(!incomplete.IsValid());

    HostedGraphicsBackend backend(MakeHostedApi(host));
    host.deviceLost = true;
    ResourceDescriptor descriptor;
    descriptor.type = ResourceType::Buffer;
    descriptor.buffer.sizeBytes = 16U;
    Result<void> created = backend.CreateResource(
        {0U, 1U, ResourceType::Buffer}, descriptor);
    CHECK(!created);
    CHECK(created.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

} // namespace

int main() {
    if (!TestSurfaceDescriptorValidation()) return 1;
    if (!TestHostedOffscreenSmoke()) return 1;
    if (!TestResizeAndContextLoss()) return 1;
    if (!TestInvalidHostedApiAndDeviceLoss()) return 1;
    std::puts("Aero hosted surface backend tests passed");
    return 0;
}
