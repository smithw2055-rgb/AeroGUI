#include <Aero/Rhi/D3D11Backend.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "AeroD3D11RectanglePixelShader.hpp"
#include "AeroD3D11RectangleVertexShader.hpp"

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

template<class T>
void ReleaseCom(T*& value) noexcept {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

bool HasCleanDebugInfoQueue(ID3D11InfoQueue& infoQueue) noexcept {
    const UINT64 messageCount =
        infoQueue.GetNumStoredMessagesAllowedByRetrievalFilter();
    bool clean = true;
    for (UINT64 index = 0U; index < messageCount; ++index) {
        SIZE_T messageSize = 0U;
        if (FAILED(infoQueue.GetMessage(index, nullptr, &messageSize)) ||
            messageSize == 0U) {
            std::fprintf(stderr, "Could not inspect D3D11 debug-layer message\n");
            return false;
        }

        auto* message = static_cast<D3D11_MESSAGE*>(HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, messageSize));
        if (message == nullptr) {
            std::fprintf(stderr, "Could not allocate D3D11 debug-layer message\n");
            return false;
        }

        const HRESULT result = infoQueue.GetMessage(index, message, &messageSize);
        if (SUCCEEDED(result) &&
            message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING) {
            std::fprintf(
                stderr,
                "D3D11 debug-layer diagnostic (%u): %s\n",
                static_cast<unsigned int>(message->Severity),
                message->pDescription != nullptr ? message->pDescription : "");
            clean = false;
        }
        if (FAILED(result)) {
            std::fprintf(stderr, "Could not read D3D11 debug-layer message\n");
            clean = false;
        }
        HeapFree(GetProcessHeap(), 0U, message);
    }
    return clean;
}

PipelineDescriptor MakePipeline() noexcept {
    PipelineDescriptor descriptor;
    descriptor.vertexShader.stage = ShaderStage::Vertex;
    descriptor.vertexShader.language = ShaderLanguage::Dxbc;
    descriptor.vertexShader.bytecode = AeroD3D11RectangleVertexShader;
    descriptor.vertexShader.bytecodeSize = sizeof(AeroD3D11RectangleVertexShader);
    descriptor.vertexShader.entryPoint = StringView("vs_main");
    descriptor.vertexShader.stableId = UINT64_C(0xD3110001);

    descriptor.fragmentShader.stage = ShaderStage::Fragment;
    descriptor.fragmentShader.language = ShaderLanguage::Dxbc;
    descriptor.fragmentShader.bytecode = AeroD3D11RectanglePixelShader;
    descriptor.fragmentShader.bytecodeSize = sizeof(AeroD3D11RectanglePixelShader);
    descriptor.fragmentShader.entryPoint = StringView("ps_main");
    descriptor.fragmentShader.stableId = UINT64_C(0xD3110002);

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
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.raster.scissorEnabled = true;
    return descriptor;
}

bool TestOffscreenRectangleAndReadback(
    const D3D11BackendOptions& backendOptions);

bool TestWarpDeviceAndBorrowedMode() {
    D3D11BackendOptions options;
    options.deviceMode = D3D11DeviceMode::Warp;
    options.allowWarpFallback = false;

    D3D11GraphicsBackend owned(options);
    CHECK(owned.Initialize());
    CHECK(owned.IsInitialized());
    CHECK(!owned.IsDeviceLost());
    CHECK(owned.Kind() == GraphicsBackendKind::D3D11);
    CHECK(owned.NativeFeatureLevel() >=
        static_cast<std::uint32_t>(D3D_FEATURE_LEVEL_10_0));
    CHECK(owned.QueryGraphicsCapabilities().shaderLanguages ==
        ShaderLanguageBit(ShaderLanguage::Dxbc));
    auto* nativeDevice = reinterpret_cast<ID3D11Device*>(owned.NativeDevice());
    D3D11_QUERY_DESC timestampQueryDescriptor{};
    timestampQueryDescriptor.Query = D3D11_QUERY_TIMESTAMP;
    ID3D11Query* timestampQuery = nullptr;
    const bool nativeTimestampQueries = SUCCEEDED(nativeDevice->CreateQuery(
        &timestampQueryDescriptor, &timestampQuery));
    ReleaseCom(timestampQuery);
    CHECK(owned.Capabilities().supportsTimestampQueries == nativeTimestampQueries);
    const GraphicsFeatureFlags features =
        owned.QueryGraphicsCapabilities().features;
    CHECK((features & FeatureBit(GraphicsFeature::TimestampQueries)) != 0U
        ? nativeTimestampQueries
        : !nativeTimestampQueries);

    D3D11BackendOptions borrowedOptions;
    borrowedOptions.deviceMode = D3D11DeviceMode::Borrowed;
    borrowedOptions.borrowedDevice = owned.NativeDevice();
    borrowedOptions.borrowedImmediateContext = owned.NativeImmediateContext();
    D3D11GraphicsBackend borrowed(borrowedOptions);
    CHECK(borrowed.Initialize());
    CHECK(borrowed.NativeDevice() == owned.NativeDevice());
    CHECK(borrowed.NativeImmediateContext() == owned.NativeImmediateContext());
    borrowed.Shutdown();
    owned.Shutdown();
    return true;
}

bool TestOffscreenRectangleAndReadback(
    const D3D11BackendOptions& backendOptions) {
    CHECK(sizeof(AeroD3D11RectangleVertexShader) <= UINT32_MAX);
    CHECK(sizeof(AeroD3D11RectanglePixelShader) <= UINT32_MAX);

    const D3D11StatePolicy statePolicy = backendOptions.statePolicy;
    D3D11GraphicsBackend backend(backendOptions);
    CHECK(backend.Initialize());

    ID3D11InfoQueue* debugInfoQueue = nullptr;
    if (backendOptions.enableDebugLayer) {
        auto* debugDevice = reinterpret_cast<ID3D11Device*>(backend.NativeDevice());
        CHECK(SUCCEEDED(debugDevice->QueryInterface(
            __uuidof(ID3D11InfoQueue),
            reinterpret_cast<void**>(&debugInfoQueue))));
        debugInfoQueue->ClearStoredMessages();
    }

    RhiDevice device(backend);
    CHECK(device.Initialize());
    GraphicsResourceFactory resources(device, backend);

    BufferDescriptor vertexDescriptor;
    vertexDescriptor.sizeBytes = 64U;
    vertexDescriptor.usage = BufferUsage::Vertex;
    Result<ResourceHandle> vertex = resources.CreateBuffer(vertexDescriptor);
    CHECK(vertex);

    BufferDescriptor indexDescriptor;
    indexDescriptor.sizeBytes = 12U;
    indexDescriptor.usage = BufferUsage::Index;
    Result<ResourceHandle> index = resources.CreateBuffer(indexDescriptor);
    CHECK(index);

    BufferDescriptor uniformDescriptor;
    uniformDescriptor.sizeBytes = 16U;
    uniformDescriptor.usage = BufferUsage::Uniform;
    Result<ResourceHandle> uniform = resources.CreateBuffer(uniformDescriptor);
    CHECK(uniform);

    TextureResourceDescriptor sampledDescriptor;
    sampledDescriptor.width = 1U;
    sampledDescriptor.height = 1U;
    sampledDescriptor.format = GraphicsTextureFormat::Rgba8Unorm;
    sampledDescriptor.usage = TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination);
    Result<ResourceHandle> sampled = resources.CreateTexture(sampledDescriptor);
    CHECK(sampled);

    TextureResourceDescriptor targetDescriptor;
    targetDescriptor.width = 64U;
    targetDescriptor.height = 64U;
    targetDescriptor.format = GraphicsTextureFormat::Bgra8Unorm;
    targetDescriptor.usage = TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::RenderTarget) |
        TextureUsageBit(TextureUsage::CopySource);
    Result<ResourceHandle> target = resources.CreateTexture(targetDescriptor);
    CHECK(target);

    TextureResourceDescriptor depthDescriptor;
    depthDescriptor.width = 64U;
    depthDescriptor.height = 64U;
    depthDescriptor.format = GraphicsTextureFormat::Depth24Stencil8;
    depthDescriptor.usage = TextureUsageBit(TextureUsage::RenderTarget);
    Result<ResourceHandle> depthStencil = resources.CreateTexture(depthDescriptor);
    CHECK(depthStencil);

    SamplerDescriptor samplerDescriptor;
    samplerDescriptor.minFilter = FilterMode::Nearest;
    samplerDescriptor.magFilter = FilterMode::Nearest;
    samplerDescriptor.mipFilter = FilterMode::Nearest;
    Result<ResourceHandle> sampler = resources.CreateSampler(samplerDescriptor);
    CHECK(sampler);

    auto* nativeDevice = reinterpret_cast<ID3D11Device*>(backend.NativeDevice());
    auto* nativeContext = reinterpret_cast<ID3D11DeviceContext*>(
        backend.NativeImmediateContext());
    D3D11_TEXTURE2D_DESC hostSampledDescriptor{};
    hostSampledDescriptor.Width = 1U;
    hostSampledDescriptor.Height = 1U;
    hostSampledDescriptor.MipLevels = 1U;
    hostSampledDescriptor.ArraySize = 1U;
    hostSampledDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    hostSampledDescriptor.SampleDesc.Count = 1U;
    hostSampledDescriptor.Usage = D3D11_USAGE_DEFAULT;
    hostSampledDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* hostSampledTexture = nullptr;
    ID3D11ShaderResourceView* hostSampledView = nullptr;
    CHECK(SUCCEEDED(nativeDevice->CreateTexture2D(
        &hostSampledDescriptor, nullptr, &hostSampledTexture)));
    CHECK(SUCCEEDED(nativeDevice->CreateShaderResourceView(
        hostSampledTexture, nullptr, &hostSampledView)));
    nativeContext->VSSetShaderResources(0U, 1U, &hostSampledView);
    nativeContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    // A resource cannot be an output attachment and a PS SRV in one pass.
    // The backend must reject this before D3D11 needs to resolve the hazard.
    GraphicsCommandEncoder hazardEncoder;
    RenderPassDescriptor hazardPass;
    hazardPass.renderArea = {0.0, 0.0, 64.0, 64.0};
    hazardPass.colorAttachmentCount = 1U;
    hazardPass.colorAttachments[0].target = target.Value();
    CHECK(hazardEncoder.BeginRenderPass(hazardPass));
    CHECK(hazardEncoder.BindTextureSampler(0U, target.Value(), sampler.Value()));
    CHECK(hazardEncoder.EndRenderPass());
    Result<GraphicsCommandBuffer> hazardCommands = hazardEncoder.Finish();
    CHECK(hazardCommands);
    CHECK(!backend.SubmitGraphics(hazardCommands.Value(), 1U));
    ID3D11ShaderResourceView* remainingView = nullptr;
    nativeContext->VSGetShaderResources(0U, 1U, &remainingView);
    CHECK(statePolicy == D3D11StatePolicy::PreserveRequiredState
        ? remainingView == hostSampledView
        : remainingView == nullptr);
    ReleaseCom(remainingView);
    ReleaseCom(hostSampledView);
    ReleaseCom(hostSampledTexture);

    PipelineDescriptor pipelineDescriptor = MakePipeline();
    pipelineDescriptor.depthStencil.depthTestEnabled = true;
    pipelineDescriptor.depthStencil.depthWriteEnabled = true;
    pipelineDescriptor.depthStencil.depthCompare = CompareOperation::LessEqual;
    Result<ResourceHandle> pipeline = resources.CreatePipeline(pipelineDescriptor);
    CHECK(pipeline);
    PipelineDescriptor mismatchedLayout = pipelineDescriptor;
    mismatchedLayout.vertexLayout.attributeCount = 1U;
    CHECK(!resources.CreatePipeline(mismatchedLayout));
    PipelineDescriptor mismatchedStage = pipelineDescriptor;
    mismatchedStage.fragmentShader.bytecode = AeroD3D11RectangleVertexShader;
    mismatchedStage.fragmentShader.bytecodeSize =
        sizeof(AeroD3D11RectangleVertexShader);
    CHECK(!resources.CreatePipeline(mismatchedStage));

    struct Vertex final {
        float x;
        float y;
        float u;
        float v;
    };
    const Vertex vertices[4] = {
        {-1.0F, -1.0F, 0.0F, 1.0F},
        {-1.0F,  1.0F, 0.0F, 0.0F},
        { 1.0F,  1.0F, 1.0F, 0.0F},
        { 1.0F, -1.0F, 1.0F, 1.0F}};
    const std::uint16_t indices[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    const float tint[4] = {1.0F, 0.0F, 0.0F, 1.0F};
    const std::uint8_t whitePixel[4] = {255U, 255U, 255U, 255U};

    GraphicsCommandEncoder encoder;
    CHECK(encoder.UploadBuffer(
        vertex.Value(),
        0U,
        Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(vertices),
            static_cast<std::uint32_t>(sizeof(vertices)))));
    CHECK(encoder.UploadBuffer(
        index.Value(),
        0U,
        Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(indices),
            static_cast<std::uint32_t>(sizeof(indices)))));
    CHECK(encoder.UploadBuffer(
        uniform.Value(),
        0U,
        Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(tint),
            static_cast<std::uint32_t>(sizeof(tint)))));
    CHECK(encoder.UploadTexture(
        sampled.Value(),
        {0U, 0U, 1U, 1U, 0U, 0U, 4U},
        whitePixel));

    RenderPassDescriptor pass;
    pass.renderArea = {0.0, 0.0, 64.0, 64.0};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0].target = target.Value();
    pass.colorAttachments[0].load = LoadOperation::Clear;
    pass.colorAttachments[0].store = StoreOperation::Store;
    pass.colorAttachments[0].clearColor = {0.0F, 0.0F, 0.0F, 1.0F};
    pass.hasDepthStencil = true;
    pass.depthStencil.target = depthStencil.Value();
    pass.depthStencil.depthLoad = LoadOperation::Clear;
    pass.depthStencil.stencilLoad = LoadOperation::Clear;
    pass.depthStencil.clearDepth = 1.0F;
    pass.depthStencil.clearStencil = 0U;
    CHECK(encoder.BeginRenderPass(pass));
    CHECK(encoder.BindPipeline(pipeline.Value()));
    CHECK(encoder.BindVertexBuffer(0U, vertex.Value()));
    CHECK(encoder.BindIndexBuffer(index.Value(), IndexType::UInt16));
    CHECK(encoder.BindUniformBuffer(0U, uniform.Value(), 0U, 16U));
    CHECK(encoder.BindTextureSampler(0U, sampled.Value(), sampler.Value()));
    CHECK(encoder.SetScissor({0.0, 0.0, 64.0, 64.0}));
    // Rebind the same complete draw state to exercise the D3D11 submission
    // cache. It must not alter output or bypass the normal validation path.
    CHECK(encoder.BindPipeline(pipeline.Value()));
    CHECK(encoder.BindVertexBuffer(0U, vertex.Value()));
    CHECK(encoder.BindIndexBuffer(index.Value(), IndexType::UInt16));
    CHECK(encoder.BindUniformBuffer(0U, uniform.Value(), 0U, 16U));
    CHECK(encoder.BindTextureSampler(0U, sampled.Value(), sampler.Value()));
    CHECK(encoder.SetScissor({0.0, 0.0, 64.0, 64.0}));
    CHECK(encoder.DrawIndexed(6U));
    CHECK(encoder.EndRenderPass());

    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    CHECK(commands);
    CHECK(commands.Value().CommandCount() == 19U);

    GraphicsQueue queue(backend);
    CHECK(queue.Initialize());
    Result<FenceValue> fence = queue.Submit(commands.Value());
    CHECK(fence);
    CHECK(fence.Value() == 1U);
    CHECK(backend.WaitForFence(fence.Value()));
    CHECK(backend.CompletedFence() >= fence.Value());

    std::uint8_t pixels[64U * 64U * 4U]{};
    CHECK(backend.ReadbackTexture(target.Value(), pixels, 64U * 4U));
    for (std::uint32_t pixel = 0U; pixel < 64U * 64U; ++pixel) {
        const std::uint32_t offset = pixel * 4U;
        CHECK(pixels[offset + 0U] == 0U);
        CHECK(pixels[offset + 1U] == 0U);
        CHECK(pixels[offset + 2U] == 255U);
        CHECK(pixels[offset + 3U] == 255U);
    }

    Result<std::uint64_t> checksum = backend.ReadbackTextureChecksum(
        target.Value());
    CHECK(checksum);
    CHECK(checksum.Value() == UINT64_C(0xCBE6CF88E951A383));
    std::printf(
        "Aero D3D11 WARP checksum: 0x%016llX\n",
        static_cast<unsigned long long>(checksum.Value()));

    D3D11_PRIMITIVE_TOPOLOGY remainingTopology =
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    nativeContext->IAGetPrimitiveTopology(&remainingTopology);
    CHECK(statePolicy == D3D11StatePolicy::PreserveRequiredState
        ? remainingTopology == D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        : remainingTopology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_TEXTURE2D_DESC externalNative{};
    externalNative.Width = 8U;
    externalNative.Height = 8U;
    externalNative.MipLevels = 1U;
    externalNative.ArraySize = 1U;
    externalNative.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    externalNative.SampleDesc.Count = 1U;
    externalNative.Usage = D3D11_USAGE_DEFAULT;
    externalNative.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D* externalTexture = nullptr;
    CHECK(SUCCEEDED(nativeDevice->CreateTexture2D(
        &externalNative, nullptr, &externalTexture)));

    D3D11ExternalRenderTargetDescriptor external;
    external.texture2D = reinterpret_cast<std::uintptr_t>(externalTexture);
    external.texture.width = 8U;
    external.texture.height = 8U;
    external.texture.format = GraphicsTextureFormat::Bgra8Unorm;
    external.texture.usage = TextureUsageBit(TextureUsage::RenderTarget);
    external.stableId = UINT64_C(0xD311BACC);
    Result<ResourceHandle> imported = ImportD3D11ExternalRenderTarget(
        device, backend, external);
    CHECK(imported);
    ReleaseCom(externalTexture);
    CHECK(device.IsAlive(imported.Value()));

    const ResourceHandle handles[] = {
        vertex.Value(),
        index.Value(),
        uniform.Value(),
        sampled.Value(),
        target.Value(),
        depthStencil.Value(),
        sampler.Value(),
        pipeline.Value(),
        imported.Value()};
    for (ResourceHandle handle : handles) {
        CHECK(device.DestroyResource(handle, fence.Value()));
    }
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected);
    CHECK(collected.Value() == 9U);
    CHECK(device.LiveResourceCount() == 0U);
    CHECK(backend.LiveResourceCount() == 0U);
    if (debugInfoQueue != nullptr) {
        CHECK(HasCleanDebugInfoQueue(*debugInfoQueue));
        ReleaseCom(debugInfoQueue);
    }
    return true;
}

bool TestDeferredResourceStress() {
    static constexpr std::uint32_t IterationCount = 64U;
    static constexpr std::uint32_t ResourcesPerIteration = 3U;

    D3D11BackendOptions options;
    options.deviceMode = D3D11DeviceMode::Warp;
    options.allowWarpFallback = false;

    D3D11GraphicsBackend backend(options);
    CHECK(backend.Initialize());
    RhiDevice device(backend);
    CHECK(device.Initialize());
    GraphicsResourceFactory resources(device, backend);
    GraphicsQueue queue(backend);
    CHECK(queue.Initialize());

    GraphicsCommandEncoder encoder;
    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    CHECK(commands);
    CHECK(commands.Value().CommandCount() == 0U);

    FenceValue lastFence = 0U;
    for (std::uint32_t iteration = 0U;
         iteration < IterationCount;
         ++iteration) {
        BufferDescriptor bufferDescriptor;
        bufferDescriptor.sizeBytes = 256U;
        bufferDescriptor.usage = BufferUsage::Vertex;
        Result<ResourceHandle> buffer = resources.CreateBuffer(bufferDescriptor);
        CHECK(buffer);

        TextureResourceDescriptor textureDescriptor;
        textureDescriptor.width = 4U;
        textureDescriptor.height = 4U;
        textureDescriptor.format = GraphicsTextureFormat::Rgba8Unorm;
        textureDescriptor.usage = TextureUsageBit(TextureUsage::Sampled) |
            TextureUsageBit(TextureUsage::CopyDestination);
        Result<ResourceHandle> texture = resources.CreateTexture(textureDescriptor);
        CHECK(texture);

        SamplerDescriptor samplerDescriptor;
        Result<ResourceHandle> sampler = resources.CreateSampler(samplerDescriptor);
        CHECK(sampler);

        Result<FenceValue> submitted = queue.Submit(commands.Value());
        CHECK(submitted);
        lastFence = submitted.Value();
        CHECK(device.DestroyResource(buffer.Value(), lastFence));
        CHECK(device.DestroyResource(texture.Value(), lastFence));
        CHECK(device.DestroyResource(sampler.Value(), lastFence));
        CHECK(device.LiveResourceCount() == 0U);
        CHECK(device.PendingDestroyCount() ==
            (iteration + 1U) * ResourcesPerIteration);
    }

    CHECK(backend.LiveResourceCount() ==
        IterationCount * ResourcesPerIteration);
    CHECK(backend.WaitForFence(lastFence));
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected);
    CHECK(collected.Value() == IterationCount * ResourcesPerIteration);
    CHECK(device.PendingDestroyCount() == 0U);
    CHECK(device.LiveResourceCount() == 0U);
    CHECK(backend.LiveResourceCount() == 0U);
    return true;
}

bool TestHardwareDeviceWhenAvailable() {
    D3D11BackendOptions options;
    options.deviceMode = D3D11DeviceMode::Hardware;
    options.allowWarpFallback = false;

    D3D11GraphicsBackend backend(options);
    Result<void> initialized = backend.Initialize();
    if (!initialized) {
        std::printf(
            "D3D11 hardware-adapter regression skipped: %s\n",
            initialized.GetStatus().message);
        return true;
    }
    CHECK(backend.NativeFeatureLevel() >=
        static_cast<std::uint32_t>(D3D_FEATURE_LEVEL_10_0));
    CHECK(!backend.IsDeviceLost());
    std::printf(
        "D3D11 hardware adapter probe passed (feature level 0x%04X)\n",
        backend.NativeFeatureLevel());
    backend.Shutdown();
    return TestOffscreenRectangleAndReadback(options);
}

bool TestBorrowedStatePreservation() {
    D3D11BackendOptions hostOptions;
    hostOptions.deviceMode = D3D11DeviceMode::Warp;
    hostOptions.allowWarpFallback = false;
    D3D11GraphicsBackend host(hostOptions);
    CHECK(host.Initialize());

    D3D11BackendOptions borrowedOptions;
    borrowedOptions.deviceMode = D3D11DeviceMode::Borrowed;
    borrowedOptions.statePolicy = D3D11StatePolicy::PreserveRequiredState;
    borrowedOptions.borrowedDevice = host.NativeDevice();
    borrowedOptions.borrowedImmediateContext = host.NativeImmediateContext();
    const bool passed = TestOffscreenRectangleAndReadback(borrowedOptions);
    host.Shutdown();
    return passed;
}

bool TestFl10BorrowedDevice() {
    const D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_10_0;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_9_1;
    const HRESULT created = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_WARP,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &requestedLevel,
        1U,
        D3D11_SDK_VERSION,
        &device,
        &createdLevel,
        &context);
    CHECK(SUCCEEDED(created));
    CHECK(device != nullptr && context != nullptr);
    CHECK(createdLevel == D3D_FEATURE_LEVEL_10_0);

    D3D11BackendOptions options;
    options.deviceMode = D3D11DeviceMode::Borrowed;
    options.borrowedDevice = reinterpret_cast<std::uintptr_t>(device);
    options.borrowedImmediateContext = reinterpret_cast<std::uintptr_t>(context);
    D3D11GraphicsBackend probe(options);
    CHECK(probe.Initialize());
    CHECK(probe.NativeFeatureLevel() ==
        static_cast<std::uint32_t>(D3D_FEATURE_LEVEL_10_0));
    CHECK(probe.Capabilities().maxTextureDimension == 8192U);
    probe.Shutdown();

    const bool passed = TestOffscreenRectangleAndReadback(options);
    ReleaseCom(context);
    ReleaseCom(device);
    return passed;
}

bool TestDebugLayerCleanWhenAvailable() {
    D3D11BackendOptions options;
    options.deviceMode = D3D11DeviceMode::Warp;
    options.allowWarpFallback = false;
    options.enableDebugLayer = true;

    D3D11GraphicsBackend probe(options);
    Result<void> initialized = probe.Initialize();
    if (!initialized) {
        std::printf(
            "D3D11 debug-layer regression skipped: %s\n",
            initialized.GetStatus().message);
        return true;
    }
    probe.Shutdown();
    return TestOffscreenRectangleAndReadback(options);
}

} // namespace

int main() {
    if (!TestWarpDeviceAndBorrowedMode()) return 1;
    if (!TestHardwareDeviceWhenAvailable()) return 1;
    D3D11BackendOptions ownedOptions;
    ownedOptions.deviceMode = D3D11DeviceMode::Warp;
    ownedOptions.allowWarpFallback = false;
    if (!TestOffscreenRectangleAndReadback(ownedOptions)) return 1;
    if (!TestDeferredResourceStress()) return 1;
    if (!TestBorrowedStatePreservation()) return 1;
    if (!TestFl10BorrowedDevice()) return 1;
    if (!TestDebugLayerCleanWhenAvailable()) return 1;
    std::puts("Aero D3D11 backend tests passed");
    return 0;
}
