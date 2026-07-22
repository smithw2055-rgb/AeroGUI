#include <Aero/Rhi/D3D11Backend.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

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

bool CompileShader(
    const char* source,
    const char* entryPoint,
    const char* target,
    ID3DBlob** output) noexcept {
    if (source == nullptr || entryPoint == nullptr || target == nullptr ||
        output == nullptr) {
        return false;
    }

    *output = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result = D3DCompile(
        source,
        std::strlen(source),
        "AeroD3D11Smoke.hlsl",
        nullptr,
        nullptr,
        entryPoint,
        target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
        0U,
        output,
        &errors);
    if (FAILED(result)) {
        if (errors != nullptr) {
            std::fprintf(
                stderr,
                "D3DCompile failed: %.*s\n",
                static_cast<int>(errors->GetBufferSize()),
                static_cast<const char*>(errors->GetBufferPointer()));
        }
        ReleaseCom(errors);
        ReleaseCom(*output);
        return false;
    }
    ReleaseCom(errors);
    return true;
}

PipelineDescriptor MakePipeline(
    ID3DBlob& vertexShader,
    ID3DBlob& pixelShader) noexcept {
    PipelineDescriptor descriptor;
    descriptor.vertexShader.stage = ShaderStage::Vertex;
    descriptor.vertexShader.language = ShaderLanguage::Dxbc;
    descriptor.vertexShader.bytecode = static_cast<const std::uint8_t*>(
        vertexShader.GetBufferPointer());
    descriptor.vertexShader.bytecodeSize = static_cast<std::uint32_t>(
        vertexShader.GetBufferSize());
    descriptor.vertexShader.entryPoint = StringView("vs_main");
    descriptor.vertexShader.stableId = UINT64_C(0xD3110001);

    descriptor.fragmentShader.stage = ShaderStage::Fragment;
    descriptor.fragmentShader.language = ShaderLanguage::Dxbc;
    descriptor.fragmentShader.bytecode = static_cast<const std::uint8_t*>(
        pixelShader.GetBufferPointer());
    descriptor.fragmentShader.bytecodeSize = static_cast<std::uint32_t>(
        pixelShader.GetBufferSize());
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

bool TestOffscreenRectangleAndReadback() {
    static constexpr const char* ShaderSource = R"HLSL(
struct VSInput {
    float2 position : ATTR0;
    float2 uv : ATTR1;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer TintBuffer : register(b0) {
    float4 tint;
};

Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

VSOutput vs_main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    return sourceTexture.Sample(sourceSampler, input.uv) * tint;
}
)HLSL";

    ID3DBlob* vertexShader = nullptr;
    ID3DBlob* pixelShader = nullptr;
    CHECK(CompileShader(ShaderSource, "vs_main", "vs_4_0", &vertexShader));
    CHECK(CompileShader(ShaderSource, "ps_main", "ps_4_0", &pixelShader));
    CHECK(vertexShader->GetBufferSize() <= UINT32_MAX);
    CHECK(pixelShader->GetBufferSize() <= UINT32_MAX);

    D3D11BackendOptions options;
    options.deviceMode = D3D11DeviceMode::Warp;
    options.allowWarpFallback = false;
    D3D11GraphicsBackend backend(options);
    CHECK(backend.Initialize());

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
    targetDescriptor.usage = TextureUsageBit(TextureUsage::RenderTarget) |
        TextureUsageBit(TextureUsage::CopySource);
    Result<ResourceHandle> target = resources.CreateRenderTarget(targetDescriptor);
    CHECK(target);

    SamplerDescriptor samplerDescriptor;
    samplerDescriptor.minFilter = FilterMode::Nearest;
    samplerDescriptor.magFilter = FilterMode::Nearest;
    samplerDescriptor.mipFilter = FilterMode::Nearest;
    Result<ResourceHandle> sampler = resources.CreateSampler(samplerDescriptor);
    CHECK(sampler);

    PipelineDescriptor pipelineDescriptor = MakePipeline(
        *vertexShader, *pixelShader);
    Result<ResourceHandle> pipeline = resources.CreatePipeline(pipelineDescriptor);
    CHECK(pipeline);
    ReleaseCom(pixelShader);
    ReleaseCom(vertexShader);

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
    CHECK(encoder.BeginRenderPass(pass));
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
    CHECK(commands.Value().CommandCount() == 13U);

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

    auto* nativeDevice = reinterpret_cast<ID3D11Device*>(backend.NativeDevice());
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
        sampler.Value(),
        pipeline.Value(),
        imported.Value()};
    for (ResourceHandle handle : handles) {
        CHECK(device.DestroyResource(handle, fence.Value()));
    }
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected);
    CHECK(collected.Value() == 8U);
    CHECK(device.LiveResourceCount() == 0U);
    CHECK(backend.LiveResourceCount() == 0U);
    return true;
}

} // namespace

int main() {
    if (!TestWarpDeviceAndBorrowedMode()) return 1;
    if (!TestOffscreenRectangleAndReadback()) return 1;
    std::puts("Aero D3D11 backend tests passed");
    return 0;
}
