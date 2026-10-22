#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <AeroRender/Texture.hpp>
#include <AeroRender/D3D11.hpp>
#include "render/common/StateCache.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi.h>
#endif

namespace Aero::Render {

class D3D11Texture final : public Texture {
public:
    D3D11Texture(
        ID3D11Texture2D* texture,
        ID3D11ShaderResourceView* srv,
        uint32_t width,
        uint32_t height,
        bool hasMipMaps,
        bool hasAlpha) noexcept;
    ~D3D11Texture() noexcept override;

    uint32_t GetWidth() const noexcept override { return width_; }
    uint32_t GetHeight() const noexcept override { return height_; }
    bool HasMipMaps() const noexcept override { return hasMipMaps_; }
    bool IsInverted() const noexcept override { return false; }
    bool HasAlpha() const noexcept override { return hasAlpha_; }

    ID3D11Texture2D* GetNativeTexture() const noexcept { return texture_; }
    ID3D11ShaderResourceView* GetNativeSRV() const noexcept { return srv_; }

private:
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* srv_ = nullptr;
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
    bool hasMipMaps_ = false;
    bool hasAlpha_ = true;
};

class D3D11RenderTarget final : public RenderTarget {
public:
    D3D11RenderTarget(
        Ref<RenderDevice> device,
        Ref<D3D11Texture> texture,
        ID3D11RenderTargetView* rtv,
        ID3D11DepthStencilView* dsv,
        uint32_t width,
        uint32_t height) noexcept;
    ~D3D11RenderTarget() noexcept override;

    Texture* GetTexture() noexcept override { return texture_.Get(); }
    ID3D11RenderTargetView* GetRTV() const noexcept { return rtv_; }
    ID3D11DepthStencilView* GetDSV() const noexcept { return dsv_; }
    uint32_t GetWidth() const noexcept { return width_; }
    uint32_t GetHeight() const noexcept { return height_; }

    void SetRTV(ID3D11RenderTargetView* rtv) noexcept;
    void SetDSV(ID3D11DepthStencilView* dsv) noexcept;
    void SetSize(uint32_t width, uint32_t height) noexcept;

private:
    Ref<D3D11Texture> texture_;
    ID3D11RenderTargetView* rtv_ = nullptr;
    ID3D11DepthStencilView* dsv_ = nullptr;
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
};

struct D3D11RenderDeviceOptions {
    ID3D11Device* borrowedDevice = nullptr;
    ID3D11DeviceContext* borrowedContext = nullptr;
    bool enableDebugLayer = false;
    bool useWarp = false;
    bool allowWarpFallback = true;
};

class D3D11RenderDevice final : public Aero::Render::RenderDeviceBase {
public:
    explicit D3D11RenderDevice(
        const D3D11RenderDeviceOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11RenderDevice() noexcept override;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    const DeviceCaps& GetCaps() const noexcept override { return caps_; }

    Ref<RenderTarget> CreateRenderTarget(
        const char* label, uint32_t width, uint32_t height,
        uint32_t sampleCount, bool needsStencil) noexcept override;

    Ref<RenderTarget> CloneRenderTarget(
        const char* label, RenderTarget* surface) noexcept override;

    Ref<Texture> CreateTexture(
        const char* label, uint32_t width, uint32_t height,
        uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept override;

    void BeginUpdatingTextures() noexcept override;
    void UpdateTexture(
        Texture* texture, uint32_t level, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height, const void* data) noexcept override;
    void EndUpdatingTextures(
        Texture** textures, uint32_t count) noexcept override;

    void BeginOffscreenRender() noexcept override;
    void EndOffscreenRender() noexcept override;
    void BeginOnscreenRender() noexcept override;
    void EndOnscreenRender() noexcept override;

    void SetRenderTarget(RenderTarget* surface) noexcept override;
    void BeginTile(RenderTarget* surface, const Tile& tile) noexcept override;
    void EndTile(RenderTarget* surface) noexcept override;
    void ResolveRenderTarget(
        RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept override;

    void* MapVertices(uint32_t bytes) noexcept override;
    void UnmapVertices() noexcept override;
    void* MapIndices(uint32_t bytes) noexcept override;
    void UnmapIndices() noexcept override;

    void DrawBatch(const Batch& batch) noexcept override;

    void EnableScissorRect() noexcept { scissorStateIndex_ = 0U; }
    void DisableScissorRect() noexcept { scissorStateIndex_ = 2U; }

    ID3D11Device* NativeDevice() const noexcept { return device_; }
    ID3D11DeviceContext* NativeContext() const noexcept { return context_; }

protected:
    RenderBackendKind BackendKind() const noexcept override {
        return RenderBackendKind::D3D11;
    }
    void NotifyBackendDeviceLost() noexcept override;
    Result<void> RestoreBackendDevice() noexcept override;
    Result<void> WaitBackendIdle(uint32_t timeoutMilliseconds) noexcept override;

private:
    struct DynamicBuffer {
        uint32_t size = 0U;
        uint32_t pos = 0U;
        uint32_t drawPos = 0U;
        D3D11_BIND_FLAG flags = D3D11_BIND_VERTEX_BUFFER;
        ID3D11Buffer* obj = nullptr;
        bool isMapped = false;
    };

    struct VertexShaderEntry {
        ID3D11VertexShader* shader = nullptr;
        ID3D11InputLayout* layout = nullptr;
        uint32_t stride = 0U;
    };

    struct CustomShaderCacheEntry {
        uint32_t bytecodeHash = 0U;
        uint32_t bytecodeSize = 0U;
        ID3D11PixelShader* shader = nullptr;
    };

    Base::Result<void> CreateBuffer(
        DynamicBuffer& buffer, uint32_t size, D3D11_BIND_FLAG flags, const char* label) noexcept;
    void* MapBuffer(DynamicBuffer& buffer, uint32_t bytes) noexcept;
    void UnmapBuffer(DynamicBuffer& buffer) noexcept;

    Base::Result<void> InitD3D11() noexcept;
    Base::Result<void> InitPipelines() noexcept;
    void ReleasePipelines() noexcept;
    ID3D11InputLayout* CreateLayout(
        uint32_t attributes, const void* code, uint32_t size, const char* label) noexcept;
    ID3D11PixelShader* GetOrCreateCustomShader(const void* bytecode, uint32_t size) noexcept;

    void InvalidateStateCache() noexcept;
    void ClearTextures() noexcept;

    void SetInputLayout(ID3D11InputLayout* layout) noexcept;
    void SetVertexShader(ID3D11VertexShader* vs) noexcept;
    void SetPixelShader(ID3D11PixelShader* ps) noexcept;
    void SetRasterizerState(ID3D11RasterizerState* rs) noexcept;
    void SetBlendState(ID3D11BlendState* bs) noexcept;
    void SetDepthStencilState(ID3D11DepthStencilState* dss, uint32_t stencilRef) noexcept;
    void SetTexture(uint32_t slot, ID3D11ShaderResourceView* srv) noexcept;
    void SetSampler(uint32_t slot, ID3D11SamplerState* sampler) noexcept;

    D3D11RenderDeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3DUserDefinedAnnotation* groupMarker_ = nullptr;
    bool ownsDevice_ = false;

    DynamicBuffer vertices_{};
    DynamicBuffer indices_{};
    DynamicBuffer vertexCB_[2]{};
    DynamicBuffer pixelCB_[2]{};
    uint32_t vertexCBHash_[2]{};
    uint32_t pixelCBHash_[2]{};

    // 4-state rasterizer table: [0]=Solid+Scissor, [1]=Wire+Scissor, [2]=Solid, [3]=Wire
    ID3D11RasterizerState* rasterizerStates_[4] = {};
    uint32_t scissorStateIndex_ = 2U;

    // 6 blend modes + 1 shared NoColor blend state for stencil-only passes
    ID3D11BlendState* blendStates_[BlendMode::Count] = {};
    ID3D11BlendState* blendStateNoColor_ = nullptr;
    ID3D11DepthStencilState* stencilStates_[StencilMode::Count] = {};
    ID3D11SamplerState* samplers_[StateCache::kSamplerTableSize] = {};

    VertexShaderEntry vertexShaders_[Shader::Vertex::Count] = {};
    ID3D11InputLayout* layouts_[Shader::Vertex::Format::Count] = {};
    ID3D11PixelShader* pixelShaders_[Shader::Count] = {};
    Base::Vector<CustomShaderCacheEntry> customShaders_{allocator_};

    // Direct pointer state cache (zero overhead)
    ID3D11Buffer* cachedIB_ = nullptr;
    uint32_t cachedIBOffset_ = UINT32_MAX;
    ID3D11Buffer* cachedVB_ = nullptr;
    uint32_t cachedVBStride_ = 0U;
    uint32_t cachedVBOffset_ = UINT32_MAX;
    ID3D11InputLayout* cachedLayout_ = nullptr;
    ID3D11VertexShader* cachedVS_ = nullptr;
    ID3D11PixelShader* cachedPS_ = nullptr;
    ID3D11RasterizerState* cachedRasterizer_ = nullptr;
    ID3D11BlendState* cachedBlend_ = nullptr;
    ID3D11DepthStencilState* cachedDS_ = nullptr;
    uint32_t cachedStencilRef_ = UINT32_MAX;
    ID3D11ShaderResourceView* cachedTextures_[StateCache::kTextureSlots] = {};
    ID3D11SamplerState* cachedSamplers_[StateCache::kTextureSlots] = {};

    DeviceCaps caps_{};
    D3D11RenderTarget* currentTarget_ = nullptr;
};

} // namespace Aero::Render
