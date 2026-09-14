#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
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
#include <d3d11.h>
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
    Base::Result<void> InitD3D11() noexcept;
    Base::Result<void> InitPipelines() noexcept;
    void ReleasePipelines() noexcept;

    D3D11RenderDeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    bool ownsDevice_ = false;

    ID3D11Buffer* dynamicVB_ = nullptr;
    ID3D11Buffer* dynamicIB_ = nullptr;
    ID3D11Buffer* vertexCB_[2] = {};
    ID3D11Buffer* pixelCB_[2] = {};

    ID3D11BlendState* blendStates_[BlendMode::Count * 2] = {};
    ID3D11DepthStencilState* stencilStates_[StencilMode::Count] = {};
    ID3D11RasterizerState* rasterizerSolid_ = nullptr;
    ID3D11RasterizerState* rasterizerScissor_ = nullptr;
    ID3D11SamplerState* samplers_[64] = {};

    ID3D11VertexShader* solidVertexShader_ = nullptr;
    ID3D11PixelShader* solidPixelShader_ = nullptr;
    ID3D11PixelShader* patternPixelShader_ = nullptr;
    ID3D11PixelShader* sdfPixelShader_ = nullptr;
    ID3D11PixelShader* blurPixelShader_ = nullptr;
    ID3D11PixelShader* shadowPixelShader_ = nullptr;
    ID3D11PixelShader* maskPixelShader_ = nullptr;
    ID3D11PixelShader* linearPixelShader_ = nullptr;
    ID3D11PixelShader* radialPixelShader_ = nullptr;
    ID3D11PixelShader* customEffectPixelShader_ = nullptr;
    ID3D11InputLayout* vertex2DInputLayout_ = nullptr;

    DeviceCaps caps_{};
    D3D11RenderTarget* currentTarget_ = nullptr;
    ID3D11RasterizerState* currentRasterizer_ = nullptr;
    StateCache stateCache_{};
};

} // namespace Aero::Render
