#include "D3D11RenderDevice.hpp"
#include <cstring>
#include <algorithm>
#include <vector>

#include "AeroD3D11RenderFrameSolidVertexShader.hpp"
#include "AeroD3D11RenderFrameSolidPixelShader.hpp"
#include "AeroD3D11RenderFramePatternVertexShader.hpp"
#include "AeroD3D11RenderFramePatternPixelShader.hpp"
#include "AeroD3D11RenderFrameSDFVertexShader.hpp"
#include "AeroD3D11RenderFrameSDFPixelShader.hpp"
#include "AeroD3D11RenderFrameBlurVertexShader.hpp"
#include "AeroD3D11RenderFrameBlurPixelShader.hpp"
#include "AeroD3D11RenderFrameShadowVertexShader.hpp"
#include "AeroD3D11RenderFrameShadowPixelShader.hpp"
#include "AeroD3D11RenderFrameMaskVertexShader.hpp"
#include "AeroD3D11RenderFrameMaskPixelShader.hpp"

namespace Aero::Render {

namespace {

template<class T>
void ReleaseCom(T*& value) noexcept {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

} // namespace

D3D11Texture::D3D11Texture(
    ID3D11Texture2D* texture,
    ID3D11ShaderResourceView* srv,
    uint32_t width,
    uint32_t height,
    bool hasMipMaps,
    bool hasAlpha) noexcept
    : texture_(texture), srv_(srv), width_(width), height_(height),
      hasMipMaps_(hasMipMaps), hasAlpha_(hasAlpha) {}

D3D11Texture::~D3D11Texture() noexcept {
    ReleaseCom(srv_);
    ReleaseCom(texture_);
}

D3D11RenderTarget::D3D11RenderTarget(
    Ref<RenderDevice> device,
    Ref<D3D11Texture> texture,
    ID3D11RenderTargetView* rtv,
    ID3D11DepthStencilView* dsv,
    uint32_t width,
    uint32_t height) noexcept
    : texture_(std::move(texture)), rtv_(rtv), dsv_(dsv),
      width_(width), height_(height) {
    device_ = std::move(device);
    kind_ = RenderTargetKind::Embedded;
    state_ = RenderTargetState::Ready;
}

D3D11RenderTarget::~D3D11RenderTarget() noexcept {
    ReleaseCom(rtv_);
    ReleaseCom(dsv_);
}

void D3D11RenderTarget::SetRTV(ID3D11RenderTargetView* rtv) noexcept {
    if (rtv_ != rtv) {
        ReleaseCom(rtv_);
        rtv_ = rtv;
        if (rtv_ != nullptr) rtv_->AddRef();
    }
}

void D3D11RenderTarget::SetDSV(ID3D11DepthStencilView* dsv) noexcept {
    if (dsv_ != dsv) {
        ReleaseCom(dsv_);
        dsv_ = dsv;
        if (dsv_ != nullptr) dsv_->AddRef();
    }
}

void D3D11RenderTarget::SetSize(uint32_t width, uint32_t height) noexcept {
    width_ = width;
    height_ = height;
}

D3D11RenderDevice::D3D11RenderDevice(
    const D3D11RenderDeviceOptions& options,
    Base::IAllocator* allocator) noexcept
    : options_(options), allocator_(allocator) {
    backend_ = RenderBackendKind::D3D11;
    caps_.centerPixelOffset = 0.0f;
    caps_.linearRendering = false;
    caps_.subpixelRendering = false;
    caps_.depthRangeZeroToOne = true;
    caps_.clipSpaceYInverted = false;
}

D3D11RenderDevice::~D3D11RenderDevice() noexcept {
    Shutdown();
}

Base::Result<void> D3D11RenderDevice::Initialize() noexcept {
    if (device_ != nullptr) return {};

    if (options_.borrowedDevice != nullptr && options_.borrowedContext != nullptr) {
        device_ = options_.borrowedDevice;
        device_->AddRef();
        context_ = options_.borrowedContext;
        context_->AddRef();
        ownsDevice_ = false;
    } else {
        UINT flags = 0;
        if (options_.enableDebugLayer) {
            flags |= D3D11_CREATE_DEVICE_DEBUG;
        }
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        D3D_FEATURE_LEVEL featureLevel;
        D3D_DRIVER_TYPE driverType = options_.useWarp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
        HRESULT hr = D3D11CreateDevice(
            nullptr, driverType, nullptr, flags,
            featureLevels, sizeof(featureLevels) / sizeof(featureLevels[0]),
            D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
        if (FAILED(hr) && options_.allowWarpFallback && driverType != D3D_DRIVER_TYPE_WARP) {
            hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                featureLevels, sizeof(featureLevels) / sizeof(featureLevels[0]),
                D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
        }
        if (FAILED(hr)) {
            return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to create D3D11 device");
        }
        ownsDevice_ = true;
    }

    Base::Result<void> pipelines = InitPipelines();
    if (!pipelines) {
        Shutdown();
        return pipelines;
    }

    state_ = RenderDeviceState::Ready;
    return {};
}

Base::Result<void> D3D11RenderDevice::InitPipelines() noexcept {
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = DYNAMIC_VB_SIZE;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = device_->CreateBuffer(&vbDesc, nullptr, &dynamicVB_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create dynamic VB");

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = DYNAMIC_IB_SIZE;
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device_->CreateBuffer(&ibDesc, nullptr, &dynamicIB_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create dynamic IB");

    for (int i = 0; i < 2; ++i) {
        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.ByteWidth = 256;
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device_->CreateBuffer(&cbDesc, nullptr, &vertexCB_[i]);
        if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create vertex CB");
        hr = device_->CreateBuffer(&cbDesc, nullptr, &pixelCB_[i]);
        if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create pixel CB");
    }

    D3D11_RASTERIZER_DESC rsDesc{};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.ScissorEnable = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rsDesc, &rasterizerSolid_);

    rsDesc.ScissorEnable = TRUE;
    device_->CreateRasterizerState(&rsDesc, &rasterizerScissor_);

    // Create shaders and the input layout matching UiFrameEncoder::Vertex2D
    // (24 bytes: float2 position @0, uint32 RGBA8 @8, float2 uv0 @12, float coverage @20)
    hr = device_->CreateVertexShader(
        AeroD3D11RenderFrameSolidVertexShader,
        sizeof(AeroD3D11RenderFrameSolidVertexShader),
        nullptr, &solidVertexShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create vertex shader");

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFrameSolidPixelShader,
        sizeof(AeroD3D11RenderFrameSolidPixelShader),
        nullptr, &solidPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create solid pixel shader");

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFramePatternPixelShader,
        sizeof(AeroD3D11RenderFramePatternPixelShader),
        nullptr, &patternPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create pattern pixel shader");

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFrameSDFPixelShader,
        sizeof(AeroD3D11RenderFrameSDFPixelShader),
        nullptr, &sdfPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create SDF pixel shader");

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFrameBlurPixelShader,
        sizeof(AeroD3D11RenderFrameBlurPixelShader),
        nullptr, &blurPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create blur pixel shader");

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFrameShadowPixelShader,
        sizeof(AeroD3D11RenderFrameShadowPixelShader),
        nullptr, &shadowPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create shadow pixel shader");

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFrameMaskPixelShader,
        sizeof(AeroD3D11RenderFrameMaskPixelShader),
        nullptr, &maskPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create mask pixel shader");

    const D3D11_INPUT_ELEMENT_DESC vertexLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COVERAGE", 0, DXGI_FORMAT_R32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = device_->CreateInputLayout(
        vertexLayout,
        static_cast<UINT>(sizeof(vertexLayout) / sizeof(vertexLayout[0])),
        AeroD3D11RenderFrameSolidVertexShader,
        sizeof(AeroD3D11RenderFrameSolidVertexShader),
        &vertex2DInputLayout_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create vertex input layout");

    // Create sampler states indexed by SamplerState.v
    // (wrapMode:3 bits | minmagFilter:1 << 3 | mipFilter:2 << 4)
    for (uint8_t v = 0; v < 64; ++v) {
        const uint8_t wrapMode = v & 0x7;
        const uint8_t minmag = (v >> 3) & 0x1;
        const uint8_t mip = (v >> 4) & 0x3;

        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = (minmag == MinMagFilter::Linear)
            ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
            : D3D11_FILTER_MIN_MAG_MIP_POINT;
        switch (wrapMode) {
        case WrapMode::ClampToZero:
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.BorderColor[0] = 0.0f;
            samplerDesc.BorderColor[1] = 0.0f;
            samplerDesc.BorderColor[2] = 0.0f;
            samplerDesc.BorderColor[3] = 0.0f;
            break;
        case WrapMode::Repeat:
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
        case WrapMode::MirrorU:
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        case WrapMode::MirrorV:
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        case WrapMode::Mirror:
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;
        case WrapMode::ClampToEdge:
        default:
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        }
        samplerDesc.MaxLOD = (mip == MipFilter::Linear) ? D3D11_FLOAT32_MAX : 0.0f;
        device_->CreateSamplerState(&samplerDesc, &samplers_[v]);
    }

    // Create blend states
    for (int colorEnable = 0; colorEnable < 2; ++colorEnable) {
        for (int b = 0; b < BlendMode::Count; ++b) {
            D3D11_BLEND_DESC blendDesc{};
            blendDesc.RenderTarget[0].BlendEnable = (b != BlendMode::Src);
            blendDesc.RenderTarget[0].RenderTargetWriteMask = colorEnable ? D3D11_COLOR_WRITE_ENABLE_ALL : 0;
            switch (b) {
            case BlendMode::Src:
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                break;
            case BlendMode::SrcOver:
            default:
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                break;
            case BlendMode::SrcOver_Multiply:
                // out = Cs*Cd + Cd*(1-As)
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                break;
            case BlendMode::SrcOver_Screen:
                // out = Cs + Cd*(1-Cs)
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                break;
            case BlendMode::SrcOver_Additive:
                // out = Cs + Cd
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                break;
            case BlendMode::SrcOver_Dual:
                // Dual-source blending requires a second shader output that the
                // current pixel shaders do not provide; keep SrcOver semantics.
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                break;
            }
            device_->CreateBlendState(&blendDesc, &blendStates_[colorEnable * BlendMode::Count + b]);
        }
    }

    // Create depth-stencil states for stencil-based clipping.
    for (int sm = 0; sm < StencilMode::Count; ++sm) {
        D3D11_DEPTH_STENCIL_DESC dsDesc{};
        dsDesc.DepthEnable = FALSE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.StencilReadMask = 0xFF;
        dsDesc.StencilWriteMask = 0xFF;
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
        dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace = dsDesc.FrontFace;

        switch (sm) {
        case StencilMode::Disabled:
        default:
            dsDesc.StencilEnable = FALSE;
            break;
        case StencilMode::Equal_Keep:
        case StencilMode::Equal_Keep_ZTest:
            dsDesc.StencilEnable = TRUE;
            break;
        case StencilMode::Equal_Incr:
            dsDesc.StencilEnable = TRUE;
            dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
            dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
            break;
        case StencilMode::Equal_Decr:
            dsDesc.StencilEnable = TRUE;
            dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
            dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
            break;
        case StencilMode::Clear:
            dsDesc.StencilEnable = TRUE;
            dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
            dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
            dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
            dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
            break;
        case StencilMode::Disabled_ZTest:
            dsDesc.DepthEnable = TRUE;
            dsDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
            dsDesc.StencilEnable = FALSE;
            break;
        }
        device_->CreateDepthStencilState(&dsDesc, &stencilStates_[sm]);
    }

    return {};
}

void D3D11RenderDevice::ReleasePipelines() noexcept {
    ReleaseCom(dynamicVB_);
    ReleaseCom(dynamicIB_);
    for (int i = 0; i < 2; ++i) {
        ReleaseCom(vertexCB_[i]);
        ReleaseCom(pixelCB_[i]);
    }
    for (int i = 0; i < BlendMode::Count * 2; ++i) {
        ReleaseCom(blendStates_[i]);
    }
    for (int i = 0; i < StencilMode::Count; ++i) {
        ReleaseCom(stencilStates_[i]);
    }
    ReleaseCom(rasterizerSolid_);
    ReleaseCom(rasterizerScissor_);
    for (int i = 0; i < 64; ++i) {
        ReleaseCom(samplers_[i]);
    }
    ReleaseCom(vertex2DInputLayout_);
    ReleaseCom(solidVertexShader_);
    ReleaseCom(solidPixelShader_);
    ReleaseCom(patternPixelShader_);
    ReleaseCom(sdfPixelShader_);
    ReleaseCom(blurPixelShader_);
    ReleaseCom(shadowPixelShader_);
    ReleaseCom(maskPixelShader_);
}

void D3D11RenderDevice::Shutdown() noexcept {
    ReleasePipelines();
    ReleaseCom(context_);
    ReleaseCom(device_);
    state_ = RenderDeviceState::Shutdown;
}

void D3D11RenderDevice::NotifyBackendDeviceLost() noexcept {
    Shutdown();
}

Result<void> D3D11RenderDevice::RestoreBackendDevice() noexcept {
    return Initialize();
}

Result<void> D3D11RenderDevice::WaitBackendIdle(uint32_t timeoutMilliseconds) noexcept {
    static_cast<void>(timeoutMilliseconds);
    if (context_ != nullptr) {
        context_->Flush();
    }
    return {};
}

Ref<RenderTarget> D3D11RenderDevice::CreateRenderTarget(
    const char* label, uint32_t width, uint32_t height,
    uint32_t sampleCount, bool needsStencil) noexcept {
    static_cast<void>(label);
    if (device_ == nullptr) return {};

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = sampleCount;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device_->CreateTexture2D(&texDesc, nullptr, &tex);
    if (FAILED(hr)) return {};

    ID3D11RenderTargetView* rtv = nullptr;
    hr = device_->CreateRenderTargetView(tex, nullptr, &rtv);
    if (FAILED(hr)) {
        ReleaseCom(tex);
        return {};
    }

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device_->CreateShaderResourceView(tex, nullptr, &srv);
    if (FAILED(hr)) {
        ReleaseCom(rtv);
        ReleaseCom(tex);
        return {};
    }

    ID3D11DepthStencilView* dsv = nullptr;
    if (needsStencil) {
        D3D11_TEXTURE2D_DESC dsDesc = texDesc;
        dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ID3D11Texture2D* dsTex = nullptr;
        hr = device_->CreateTexture2D(&dsDesc, nullptr, &dsTex);
        if (SUCCEEDED(hr)) {
            device_->CreateDepthStencilView(dsTex, nullptr, &dsv);
            ReleaseCom(dsTex);
        }
    }

    Ref<D3D11Texture> d3dTex = Base::MakeRef<D3D11Texture>(
        tex, srv, width, height, false, true).Value();

    Ref<RenderDevice> self = Ref<RenderDevice>::FromBorrowed(*this);
    return Base::MakeRef<D3D11RenderTarget>(
        std::move(self), std::move(d3dTex), rtv, dsv, width, height).Value();
}

Ref<RenderTarget> D3D11RenderDevice::CloneRenderTarget(
    const char* label, RenderTarget* surface) noexcept {
    if (surface == nullptr) return {};
    auto* src = static_cast<D3D11RenderTarget*>(surface);
    return CreateRenderTarget(label, src->GetWidth(), src->GetHeight(), 1, src->GetDSV() != nullptr);
}

Ref<Texture> D3D11RenderDevice::CreateTexture(
    const char* label, uint32_t width, uint32_t height,
    uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept {
    static_cast<void>(label);
    if (device_ == nullptr) return {};

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = numLevels > 0 ? numLevels : 1;
    desc.ArraySize = 1;
    desc.Format = (format == TextureFormat::R8) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = (data != nullptr) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = S_OK;
    std::vector<uint8_t> zeroBuffer;
    if (data != nullptr && data[0] != nullptr) {
        D3D11_SUBRESOURCE_DATA subData{};
        subData.pSysMem = data[0];
        subData.SysMemPitch = width * (format == TextureFormat::R8 ? 1 : 4);
        hr = device_->CreateTexture2D(&desc, &subData, &tex);
    } else {
        const uint32_t bpp = (format == TextureFormat::R8 ? 1U : 4U);
        zeroBuffer.resize(width * height * bpp, 0);
        D3D11_SUBRESOURCE_DATA subData{};
        subData.pSysMem = zeroBuffer.data();
        subData.SysMemPitch = width * bpp;
        hr = device_->CreateTexture2D(&desc, &subData, &tex);
    }
    if (FAILED(hr)) return {};

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device_->CreateShaderResourceView(tex, nullptr, &srv);
    if (FAILED(hr)) {
        ReleaseCom(tex);
        return {};
    }

    return Base::MakeRef<D3D11Texture>(
        tex, srv, width, height, numLevels > 1, format != TextureFormat::RGBX8).Value();
}

void D3D11RenderDevice::BeginUpdatingTextures() noexcept {}

void D3D11RenderDevice::UpdateTexture(
    Texture* texture, uint32_t level, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, const void* data) noexcept {
    if (context_ == nullptr || texture == nullptr || data == nullptr) return;
    auto* d3dTex = static_cast<D3D11Texture*>(texture);

    D3D11_BOX box{};
    box.left = x;
    box.top = y;
    box.front = 0;
    box.right = x + width;
    box.bottom = y + height;
    box.back = 1;

    D3D11_TEXTURE2D_DESC texDesc{};
    d3dTex->GetNativeTexture()->GetDesc(&texDesc);
    const uint32_t bpp = (texDesc.Format == DXGI_FORMAT_R8_UNORM) ? 1U : 4U;
    const uint32_t pitch = width * bpp;
    context_->UpdateSubresource(d3dTex->GetNativeTexture(), level, &box, data, pitch, 0);
}

void D3D11RenderDevice::EndUpdatingTextures(Texture** textures, uint32_t count) noexcept {
    static_cast<void>(textures);
    static_cast<void>(count);
}

void D3D11RenderDevice::BeginOffscreenRender() noexcept {}
void D3D11RenderDevice::EndOffscreenRender() noexcept {}
void D3D11RenderDevice::BeginOnscreenRender() noexcept {}
void D3D11RenderDevice::EndOnscreenRender() noexcept {}

void D3D11RenderDevice::SetRenderTarget(RenderTarget* surface) noexcept {
    if (context_ == nullptr) return;
    currentTarget_ = static_cast<D3D11RenderTarget*>(surface);
    if (currentTarget_ != nullptr) {
        ID3D11RenderTargetView* rtv = currentTarget_->GetRTV();
        ID3D11DepthStencilView* dsv = currentTarget_->GetDSV();
        context_->OMSetRenderTargets(1, &rtv, dsv);

        D3D11_VIEWPORT vp{};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width = static_cast<float>(currentTarget_->GetWidth());
        vp.Height = static_cast<float>(currentTarget_->GetHeight());
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &vp);

        // Update the viewport constant buffer consumed by the vertex shaders
        if (vertexCB_[0] != nullptr) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(context_->Map(
                    vertexCB_[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                float* viewport = static_cast<float*>(mapped.pData);
                viewport[0] = vp.Width;
                viewport[1] = vp.Height;
                viewport[2] = 0.0f;
                viewport[3] = 0.0f;
                context_->Unmap(vertexCB_[0], 0);
            }
        }
    }
}

void D3D11RenderDevice::BeginTile(RenderTarget* surface, const Tile& tile) noexcept {
    if (context_ == nullptr || surface == nullptr) return;
    auto* target = static_cast<D3D11RenderTarget*>(surface);

    // Clear the stencil buffer at the start of each frame/tile when present.
    if (target->GetDSV() != nullptr) {
        context_->ClearDepthStencilView(
            target->GetDSV(), D3D11_CLEAR_STENCIL, 0.0f, 0);
    }

    // Scissor the tile for the rest of the frame.
    D3D11_RECT scissor{};
    scissor.left = static_cast<LONG>(tile.x);
    scissor.top = static_cast<LONG>(tile.y);
    scissor.right = static_cast<LONG>(tile.x + tile.width);
    scissor.bottom = static_cast<LONG>(tile.y + tile.height);
    context_->RSSetScissorRects(1, &scissor);
    currentRasterizer_ = rasterizerScissor_;
    context_->RSSetState(currentRasterizer_);
}

void D3D11RenderDevice::EndTile(RenderTarget* surface) noexcept {
    static_cast<void>(surface);
    if (context_ != nullptr) {
        currentRasterizer_ = rasterizerSolid_;
        context_->RSSetState(currentRasterizer_);
    }
}

void D3D11RenderDevice::ResolveRenderTarget(
    RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept {
    static_cast<void>(surface);
    static_cast<void>(tiles);
    static_cast<void>(numTiles);
}

void* D3D11RenderDevice::MapVertices(uint32_t bytes) noexcept {
    if (context_ == nullptr || dynamicVB_ == nullptr) return nullptr;
    static_cast<void>(bytes);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context_->Map(dynamicVB_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    return SUCCEEDED(hr) ? mapped.pData : nullptr;
}

void D3D11RenderDevice::UnmapVertices() noexcept {
    if (context_ != nullptr && dynamicVB_ != nullptr) {
        context_->Unmap(dynamicVB_, 0);
    }
}

void* D3D11RenderDevice::MapIndices(uint32_t bytes) noexcept {
    if (context_ == nullptr || dynamicIB_ == nullptr) return nullptr;
    static_cast<void>(bytes);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context_->Map(dynamicIB_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    return SUCCEEDED(hr) ? mapped.pData : nullptr;
}

void D3D11RenderDevice::UnmapIndices() noexcept {
    if (context_ != nullptr && dynamicIB_ != nullptr) {
        context_->Unmap(dynamicIB_, 0);
    }
}

void D3D11RenderDevice::DrawBatch(const Batch& batch) noexcept {
    if (context_ == nullptr || batch.numIndices == 0U) return;

    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11SamplerState* sampler = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11SamplerState* maskSampler = nullptr;
    ID3D11ShaderResourceView* maskSrv = nullptr;
    bool setTextureSize = false;
    switch (batch.shader.v) {
    case Shader::Path_Solid:
    case Shader::Path_AA_Solid:
        pixelShader = solidPixelShader_;
        break;
    case Shader::Path_Pattern:
        pixelShader = patternPixelShader_;
        sampler = samplers_[batch.imageSampler.v & 0x3F];
        if (batch.image != nullptr) {
            srv = static_cast<D3D11Texture*>(batch.image)->GetNativeSRV();
        }
        break;
    case Shader::SDF_Solid:
        pixelShader = sdfPixelShader_;
        sampler = samplers_[batch.glyphsSampler.v & 0x3F];
        if (batch.glyphs != nullptr) {
            srv = static_cast<D3D11Texture*>(batch.glyphs)->GetNativeSRV();
        }
        break;
    case Shader::Blur:
        pixelShader = blurPixelShader_;
        sampler = samplers_[batch.imageSampler.v & 0x3F];
        if (batch.image != nullptr) {
            srv = static_cast<D3D11Texture*>(batch.image)->GetNativeSRV();
        }
        setTextureSize = true;
        break;
    case Shader::Shadow:
        pixelShader = shadowPixelShader_;
        sampler = samplers_[batch.imageSampler.v & 0x3F];
        if (batch.image != nullptr) {
            srv = static_cast<D3D11Texture*>(batch.image)->GetNativeSRV();
        }
        setTextureSize = true;
        break;
    case Shader::Mask:
        pixelShader = maskPixelShader_;
        sampler = samplers_[batch.imageSampler.v & 0x3F];
        if (batch.image != nullptr) {
            srv = static_cast<D3D11Texture*>(batch.image)->GetNativeSRV();
        }
        maskSampler = samplers_[batch.shadowSampler.v & 0x3F];
        if (batch.shadow != nullptr) {
            maskSrv = static_cast<D3D11Texture*>(batch.shadow)->GetNativeSRV();
        }
        break;
    default:
        pixelShader = solidPixelShader_;
        break;
    }

    if (solidVertexShader_ == nullptr || pixelShader == nullptr ||
        vertex2DInputLayout_ == nullptr) {
        return;
    }

    context_->IASetInputLayout(vertex2DInputLayout_);
    context_->VSSetShader(solidVertexShader_, nullptr, 0);
    context_->PSSetShader(pixelShader, nullptr, 0);

    // UiFrameEncoder::Vertex2D is 24 bytes: float2 pos, uint32 color, float2 uv, float coverage
    const UINT stride = 24;
    const UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, &dynamicVB_, &stride, &offset);
    context_->IASetIndexBuffer(dynamicIB_, DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11Buffer* vsConstants[] = { vertexCB_[0] };
    context_->VSSetConstantBuffers(0, 1, vsConstants);

    // Set blend state
    const uint8_t colorEnable = batch.renderState.f.colorEnable;
    const uint8_t blendMode = batch.renderState.f.blendMode;
    if (blendMode < BlendMode::Count) {
        ID3D11BlendState* bs = blendStates_[colorEnable * BlendMode::Count + blendMode];
        if (bs != nullptr) {
            context_->OMSetBlendState(bs, nullptr, 0xFFFFFFFF);
        }
    }

    // Set depth-stencil state
    const uint8_t stencilMode = batch.renderState.f.stencilMode;
    if (stencilMode < StencilMode::Count) {
        ID3D11DepthStencilState* ds = stencilStates_[stencilMode];
        if (ds != nullptr) {
            context_->OMSetDepthStencilState(ds, batch.stencilRef);
        }
    }

    // Set rasterizer
    if (currentRasterizer_ != nullptr) {
        context_->RSSetState(currentRasterizer_);
    } else {
        context_->RSSetState(rasterizerSolid_);
    }

    // Bind texture and sampler if present
    if (srv != nullptr) {
        context_->PSSetShaderResources(0, 1, &srv);
        if (sampler != nullptr) {
            context_->PSSetSamplers(0, 1, &sampler);
        }
    }
    if (maskSrv != nullptr) {
        context_->PSSetShaderResources(1, 1, &maskSrv);
        if (maskSampler != nullptr) {
            context_->PSSetSamplers(1, 1, &maskSampler);
        }
    }

    if (setTextureSize && batch.pixelUniforms[0].values != nullptr &&
        batch.pixelUniforms[0].numDwords >= 2U) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context_->Map(
                pixelCB_[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            float* data = static_cast<float*>(mapped.pData);
            data[0] = static_cast<const float*>(
                batch.pixelUniforms[0].values)[0];
            data[1] = static_cast<const float*>(
                batch.pixelUniforms[0].values)[1];
            context_->Unmap(pixelCB_[0], 0);
        }
        ID3D11Buffer* psConstants[] = { pixelCB_[0] };
        context_->PSSetConstantBuffers(1, 1, psConstants);
    }

    context_->DrawIndexed(batch.numIndices, batch.startIndex, 0);
}

} // namespace Aero::Render
