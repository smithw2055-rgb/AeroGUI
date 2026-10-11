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
#include "AeroD3D11RenderFrameLinearVertexShader.hpp"
#include "AeroD3D11RenderFrameLinearPixelShader.hpp"
#include "AeroD3D11RenderFrameRadialVertexShader.hpp"
#include "AeroD3D11RenderFrameRadialPixelShader.hpp"

#include "AeroD3D11ShaderVSPos.hpp"
#include "AeroD3D11ShaderVSPosColor.hpp"
#include "AeroD3D11ShaderVSPosTex0.hpp"
#include "AeroD3D11ShaderVSPosColorCoverage.hpp"
#include "AeroD3D11ShaderVSPosTex0Coverage.hpp"
#include "AeroD3D11ShaderVSPosColorTex1_SDF.hpp"
#include "AeroD3D11ShaderVSPosTex0Tex1.hpp"
#include "AeroD3D11ShaderVSPosColorTex0Tex1.hpp"

#include "AeroD3D11ShaderPSRGBA.hpp"
#include "AeroD3D11ShaderPSClear.hpp"
#include "AeroD3D11ShaderPSPath_Solid.hpp"
#include "AeroD3D11ShaderPSPath_Linear.hpp"
#include "AeroD3D11ShaderPSPath_Radial.hpp"
#include "AeroD3D11ShaderPSPath_Pattern.hpp"
#include "AeroD3D11ShaderPSPath_Pattern_Clamp.hpp"
#include "AeroD3D11ShaderPSPath_Pattern_Repeat.hpp"
#include "AeroD3D11ShaderPSPath_Pattern_MirrorU.hpp"
#include "AeroD3D11ShaderPSPath_Pattern_MirrorV.hpp"
#include "AeroD3D11ShaderPSPath_Pattern_Mirror.hpp"
#include "AeroD3D11ShaderPSPath_AA_Solid.hpp"
#include "AeroD3D11ShaderPSPath_AA_Linear.hpp"
#include "AeroD3D11ShaderPSPath_AA_Radial.hpp"
#include "AeroD3D11ShaderPSPath_AA_Pattern.hpp"
#include "AeroD3D11ShaderPSPath_AA_Pattern_Clamp.hpp"
#include "AeroD3D11ShaderPSPath_AA_Pattern_Repeat.hpp"
#include "AeroD3D11ShaderPSPath_AA_Pattern_MirrorU.hpp"
#include "AeroD3D11ShaderPSPath_AA_Pattern_MirrorV.hpp"
#include "AeroD3D11ShaderPSPath_AA_Pattern_Mirror.hpp"
#include "AeroD3D11ShaderPSSDF_Solid.hpp"
#include "AeroD3D11ShaderPSSDF_Linear.hpp"
#include "AeroD3D11ShaderPSSDF_Radial.hpp"
#include "AeroD3D11ShaderPSSDF_Pattern.hpp"
#include "AeroD3D11ShaderPSSDF_Pattern_Clamp.hpp"
#include "AeroD3D11ShaderPSSDF_Pattern_Repeat.hpp"
#include "AeroD3D11ShaderPSSDF_Pattern_MirrorU.hpp"
#include "AeroD3D11ShaderPSSDF_Pattern_MirrorV.hpp"
#include "AeroD3D11ShaderPSSDF_Pattern_Mirror.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Solid.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Linear.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Radial.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Pattern.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Pattern_Clamp.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Pattern_Repeat.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorU.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorV.hpp"
#include "AeroD3D11ShaderPSSDF_LCD_Pattern_Mirror.hpp"
#include "AeroD3D11ShaderPSOpacity_Solid.hpp"
#include "AeroD3D11ShaderPSOpacity_Linear.hpp"
#include "AeroD3D11ShaderPSOpacity_Radial.hpp"
#include "AeroD3D11ShaderPSOpacity_Pattern.hpp"
#include "AeroD3D11ShaderPSOpacity_Pattern_Clamp.hpp"
#include "AeroD3D11ShaderPSOpacity_Pattern_Repeat.hpp"
#include "AeroD3D11ShaderPSOpacity_Pattern_MirrorU.hpp"
#include "AeroD3D11ShaderPSOpacity_Pattern_MirrorV.hpp"
#include "AeroD3D11ShaderPSOpacity_Pattern_Mirror.hpp"
#include "AeroD3D11ShaderPSUpsample.hpp"
#include "AeroD3D11ShaderPSDownsample.hpp"

namespace Aero::Render {

namespace {

template<class T>
void ReleaseCom(T*& value) noexcept {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

std::pair<const char*, uint32_t> SemanticForAttr(uint32_t attr) noexcept {
    switch (attr) {
    case Shader::Vertex::Format::Attr::Pos: return {"POSITION", 0};
    case Shader::Vertex::Format::Attr::Color: return {"COLOR", 0};
    case Shader::Vertex::Format::Attr::Tex0: return {"TEXCOORD", 0};
    case Shader::Vertex::Format::Attr::Tex1: return {"TEXCOORD", 1};
    case Shader::Vertex::Format::Attr::Coverage: return {"COVERAGE", 0};
    case Shader::Vertex::Format::Attr::Rect: return {"RECT", 0};
    case Shader::Vertex::Format::Attr::Tile: return {"TILE", 0};
    case Shader::Vertex::Format::Attr::ImagePos: return {"IMAGE_POSITION", 0};
    default: return {"TEXCOORD", 0};
    }
}

DXGI_FORMAT DxgiFormatForType(uint8_t type) noexcept {
    switch (type) {
    case Shader::Vertex::Format::Attr::Type::Float: return DXGI_FORMAT_R32_FLOAT;
    case Shader::Vertex::Format::Attr::Type::Float2: return DXGI_FORMAT_R32G32_FLOAT;
    case Shader::Vertex::Format::Attr::Type::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case Shader::Vertex::Format::Attr::Type::UByte4Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case Shader::Vertex::Format::Attr::Type::UShort4Norm: return DXGI_FORMAT_R16G16B16A16_UNORM;
    default: return DXGI_FORMAT_R32G32_FLOAT;
    }
}

} // namespace

ID3D11InputLayout* D3D11RenderDevice::CreateLayout(
    uint32_t attributes, const void* code, uint32_t size) noexcept {
    if (device_ == nullptr || code == nullptr || size == 0U) return nullptr;

    D3D11_INPUT_ELEMENT_DESC descs[Shader::Vertex::Format::Attr::Count];
    uint32_t n = 0;

    for (uint32_t i = 0; i < Shader::Vertex::Format::Attr::Count; ++i) {
        if (attributes & (1U << i)) {
            descs[n].InputSlot = 0;
            descs[n].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
            descs[n].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            descs[n].InstanceDataStepRate = 0;
            const auto sem = SemanticForAttr(i);
            descs[n].SemanticName = sem.first;
            descs[n].SemanticIndex = sem.second;
            descs[n].Format = DxgiFormatForType(TypeForAttr[i]);
            ++n;
        }
    }

    ID3D11InputLayout* layout = nullptr;
    if (FAILED(device_->CreateInputLayout(descs, n, code, size, &layout))) {
        return nullptr;
    }
    return layout;
}

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
    const uint32_t vbRingSize = 2 * 1024 * 1024;
    vbState_.size = vbRingSize;
    vbState_.currentPos = 0U;
    vbState_.drawPos = 0U;
    vbState_.isMapped = false;

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = vbRingSize;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = device_->CreateBuffer(&vbDesc, nullptr, &dynamicVB_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create dynamic VB");

    const uint32_t ibRingSize = 1024 * 1024;
    ibState_.size = ibRingSize;
    ibState_.currentPos = 0U;
    ibState_.drawPos = 0U;
    ibState_.isMapped = false;

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = ibRingSize;
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

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFrameLinearPixelShader,
        sizeof(AeroD3D11RenderFrameLinearPixelShader),
        nullptr, &linearPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create linear pixel shader");

    hr = device_->CreatePixelShader(
        AeroD3D11RenderFrameRadialPixelShader,
        sizeof(AeroD3D11RenderFrameRadialPixelShader),
        nullptr, &radialPixelShader_);
    if (FAILED(hr)) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create radial pixel shader");

    auto createPS = [&](const void* code, size_t size) -> ID3D11PixelShader* {
        ID3D11PixelShader* ps = nullptr;
        if (code != nullptr && size > 0U) {
            device_->CreatePixelShader(code, size, nullptr, &ps);
        }
        return ps;
    };

    pixelShaders_[Shader::RGBA] = createPS(AeroD3D11ShaderPSRGBA, sizeof(AeroD3D11ShaderPSRGBA));
    pixelShaders_[Shader::Mask] = maskPixelShader_;
    pixelShaders_[Shader::Clear] = createPS(AeroD3D11ShaderPSClear, sizeof(AeroD3D11ShaderPSClear));

    pixelShaders_[Shader::Path_Solid] = solidPixelShader_;
    pixelShaders_[Shader::Path_Linear] = linearPixelShader_;
    pixelShaders_[Shader::Path_Radial] = radialPixelShader_;
    pixelShaders_[Shader::Path_Pattern] = patternPixelShader_;
    pixelShaders_[Shader::Path_Pattern_Clamp] = createPS(AeroD3D11ShaderPSPath_Pattern_Clamp, sizeof(AeroD3D11ShaderPSPath_Pattern_Clamp));
    pixelShaders_[Shader::Path_Pattern_Repeat] = createPS(AeroD3D11ShaderPSPath_Pattern_Repeat, sizeof(AeroD3D11ShaderPSPath_Pattern_Repeat));
    pixelShaders_[Shader::Path_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSPath_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSPath_Pattern_MirrorU));
    pixelShaders_[Shader::Path_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSPath_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSPath_Pattern_MirrorV));
    pixelShaders_[Shader::Path_Pattern_Mirror] = createPS(AeroD3D11ShaderPSPath_Pattern_Mirror, sizeof(AeroD3D11ShaderPSPath_Pattern_Mirror));

    pixelShaders_[Shader::Path_AA_Solid] = createPS(AeroD3D11ShaderPSPath_AA_Solid, sizeof(AeroD3D11ShaderPSPath_AA_Solid));
    pixelShaders_[Shader::Path_AA_Linear] = createPS(AeroD3D11ShaderPSPath_AA_Linear, sizeof(AeroD3D11ShaderPSPath_AA_Linear));
    pixelShaders_[Shader::Path_AA_Radial] = createPS(AeroD3D11ShaderPSPath_AA_Radial, sizeof(AeroD3D11ShaderPSPath_AA_Radial));
    pixelShaders_[Shader::Path_AA_Pattern] = createPS(AeroD3D11ShaderPSPath_AA_Pattern, sizeof(AeroD3D11ShaderPSPath_AA_Pattern));
    pixelShaders_[Shader::Path_AA_Pattern_Clamp] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_Clamp, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_Clamp));
    pixelShaders_[Shader::Path_AA_Pattern_Repeat] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_Repeat, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_Repeat));
    pixelShaders_[Shader::Path_AA_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_MirrorU));
    pixelShaders_[Shader::Path_AA_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_MirrorV));
    pixelShaders_[Shader::Path_AA_Pattern_Mirror] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_Mirror, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_Mirror));

    pixelShaders_[Shader::SDF_Solid] = sdfPixelShader_;
    pixelShaders_[Shader::SDF_Linear] = createPS(AeroD3D11ShaderPSSDF_Linear, sizeof(AeroD3D11ShaderPSSDF_Linear));
    pixelShaders_[Shader::SDF_Radial] = createPS(AeroD3D11ShaderPSSDF_Radial, sizeof(AeroD3D11ShaderPSSDF_Radial));
    pixelShaders_[Shader::SDF_Pattern] = createPS(AeroD3D11ShaderPSSDF_Pattern, sizeof(AeroD3D11ShaderPSSDF_Pattern));
    pixelShaders_[Shader::SDF_Pattern_Clamp] = createPS(AeroD3D11ShaderPSSDF_Pattern_Clamp, sizeof(AeroD3D11ShaderPSSDF_Pattern_Clamp));
    pixelShaders_[Shader::SDF_Pattern_Repeat] = createPS(AeroD3D11ShaderPSSDF_Pattern_Repeat, sizeof(AeroD3D11ShaderPSSDF_Pattern_Repeat));
    pixelShaders_[Shader::SDF_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSSDF_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSSDF_Pattern_MirrorU));
    pixelShaders_[Shader::SDF_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSSDF_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSSDF_Pattern_MirrorV));
    pixelShaders_[Shader::SDF_Pattern_Mirror] = createPS(AeroD3D11ShaderPSSDF_Pattern_Mirror, sizeof(AeroD3D11ShaderPSSDF_Pattern_Mirror));

    pixelShaders_[Shader::SDF_LCD_Solid] = createPS(AeroD3D11ShaderPSSDF_LCD_Solid, sizeof(AeroD3D11ShaderPSSDF_LCD_Solid));
    pixelShaders_[Shader::SDF_LCD_Linear] = createPS(AeroD3D11ShaderPSSDF_LCD_Linear, sizeof(AeroD3D11ShaderPSSDF_LCD_Linear));
    pixelShaders_[Shader::SDF_LCD_Radial] = createPS(AeroD3D11ShaderPSSDF_LCD_Radial, sizeof(AeroD3D11ShaderPSSDF_LCD_Radial));
    pixelShaders_[Shader::SDF_LCD_Pattern] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern));
    pixelShaders_[Shader::SDF_LCD_Pattern_Clamp] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_Clamp, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_Clamp));
    pixelShaders_[Shader::SDF_LCD_Pattern_Repeat] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_Repeat, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_Repeat));
    pixelShaders_[Shader::SDF_LCD_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorU));
    pixelShaders_[Shader::SDF_LCD_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorV));
    pixelShaders_[Shader::SDF_LCD_Pattern_Mirror] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_Mirror, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_Mirror));

    pixelShaders_[Shader::Opacity_Solid] = createPS(AeroD3D11ShaderPSOpacity_Solid, sizeof(AeroD3D11ShaderPSOpacity_Solid));
    pixelShaders_[Shader::Opacity_Linear] = createPS(AeroD3D11ShaderPSOpacity_Linear, sizeof(AeroD3D11ShaderPSOpacity_Linear));
    pixelShaders_[Shader::Opacity_Radial] = createPS(AeroD3D11ShaderPSOpacity_Radial, sizeof(AeroD3D11ShaderPSOpacity_Radial));
    pixelShaders_[Shader::Opacity_Pattern] = createPS(AeroD3D11ShaderPSOpacity_Pattern, sizeof(AeroD3D11ShaderPSOpacity_Pattern));
    pixelShaders_[Shader::Opacity_Pattern_Clamp] = createPS(AeroD3D11ShaderPSOpacity_Pattern_Clamp, sizeof(AeroD3D11ShaderPSOpacity_Pattern_Clamp));
    pixelShaders_[Shader::Opacity_Pattern_Repeat] = createPS(AeroD3D11ShaderPSOpacity_Pattern_Repeat, sizeof(AeroD3D11ShaderPSOpacity_Pattern_Repeat));
    pixelShaders_[Shader::Opacity_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSOpacity_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSOpacity_Pattern_MirrorU));
    pixelShaders_[Shader::Opacity_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSOpacity_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSOpacity_Pattern_MirrorV));
    pixelShaders_[Shader::Opacity_Pattern_Mirror] = createPS(AeroD3D11ShaderPSOpacity_Pattern_Mirror, sizeof(AeroD3D11ShaderPSOpacity_Pattern_Mirror));

    pixelShaders_[Shader::Upsample] = createPS(AeroD3D11ShaderPSUpsample, sizeof(AeroD3D11ShaderPSUpsample));
    pixelShaders_[Shader::Downsample] = createPS(AeroD3D11ShaderPSDownsample, sizeof(AeroD3D11ShaderPSDownsample));

    pixelShaders_[Shader::Shadow] = shadowPixelShader_;
    pixelShaders_[Shader::Blur] = blurPixelShader_;

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

    struct VSInitEntry {
        uint32_t vertexEnum;
        const void* code;
        uint32_t size;
    };

    const VSInitEntry vsList[] = {
        {Shader::Vertex::Pos, AeroD3D11ShaderVSPos, sizeof(AeroD3D11ShaderVSPos)},
        {Shader::Vertex::PosColor, AeroD3D11ShaderVSPosColor, sizeof(AeroD3D11ShaderVSPosColor)},
        {Shader::Vertex::PosTex0, AeroD3D11ShaderVSPosTex0, sizeof(AeroD3D11ShaderVSPosTex0)},
        {Shader::Vertex::PosColorCoverage, AeroD3D11ShaderVSPosColorCoverage, sizeof(AeroD3D11ShaderVSPosColorCoverage)},
        {Shader::Vertex::PosTex0Coverage, AeroD3D11ShaderVSPosTex0Coverage, sizeof(AeroD3D11ShaderVSPosTex0Coverage)},
        {Shader::Vertex::PosColorTex1_SDF, AeroD3D11ShaderVSPosColorTex1_SDF, sizeof(AeroD3D11ShaderVSPosColorTex1_SDF)},
        {Shader::Vertex::PosColorTex1, AeroD3D11ShaderVSPosColorTex1_SDF, sizeof(AeroD3D11ShaderVSPosColorTex1_SDF)},
        {Shader::Vertex::PosTex0Tex1, AeroD3D11ShaderVSPosTex0Tex1, sizeof(AeroD3D11ShaderVSPosTex0Tex1)},
        {Shader::Vertex::PosTex0Tex1_SDF, AeroD3D11ShaderVSPosTex0Tex1, sizeof(AeroD3D11ShaderVSPosTex0Tex1)},
        {Shader::Vertex::PosTex0Tex1_Downsample, AeroD3D11ShaderVSPosTex0Tex1, sizeof(AeroD3D11ShaderVSPosTex0Tex1)},
        {Shader::Vertex::PosColorTex0Tex1, AeroD3D11ShaderVSPosColorTex0Tex1, sizeof(AeroD3D11ShaderVSPosColorTex0Tex1)}
    };

    for (const auto& entry : vsList) {
        ID3D11VertexShader* vs = nullptr;
        if (SUCCEEDED(device_->CreateVertexShader(entry.code, entry.size, nullptr, &vs)) && vs != nullptr) {
            const uint32_t format = FormatForVertex[entry.vertexEnum];
            if (layouts_[format] == nullptr) {
                layouts_[format] = CreateLayout(AttributesForFormat[format], entry.code, entry.size);
            }
            vertexShaders_[entry.vertexEnum].shader = vs;
            vertexShaders_[entry.vertexEnum].layout = layouts_[format];
            vertexShaders_[entry.vertexEnum].stride = SizeForFormat[format];
        }
    }

    // Create sampler states indexed by SamplerState.v
    // (wrapMode:3 bits | minmagFilter:1 << 3 | mipFilter:2 << 4)
    for (uint8_t v = 0; v < StateCache::kSamplerTableSize; ++v) {
        const uint8_t wrapMode = StateCache::UnpackWrap(v);
        const uint8_t minmag = StateCache::UnpackMinMag(v);
        const uint8_t mip = StateCache::UnpackMip(v);

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
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
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
    vbState_ = DynamicBufferState{};
    ibState_ = DynamicBufferState{};
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
    for (uint8_t i = 0; i < StateCache::kSamplerTableSize; ++i) {
        ReleaseCom(samplers_[i]);
    }
    ReleaseCom(solidVertexShader_);

    std::vector<ID3D11PixelShader*> uniquePS;
    auto addUniquePS = [&](ID3D11PixelShader* ps) {
        if (ps != nullptr && std::find(uniquePS.begin(), uniquePS.end(), ps) == uniquePS.end()) {
            uniquePS.push_back(ps);
        }
    };
    addUniquePS(solidPixelShader_);
    addUniquePS(patternPixelShader_);
    addUniquePS(sdfPixelShader_);
    addUniquePS(blurPixelShader_);
    addUniquePS(shadowPixelShader_);
    addUniquePS(maskPixelShader_);
    addUniquePS(linearPixelShader_);
    addUniquePS(radialPixelShader_);
    addUniquePS(customEffectPixelShader_);
    for (uint32_t i = 0; i < Shader::Count; ++i) {
        addUniquePS(pixelShaders_[i]);
        pixelShaders_[i] = nullptr;
    }
    solidPixelShader_ = nullptr;
    patternPixelShader_ = nullptr;
    sdfPixelShader_ = nullptr;
    blurPixelShader_ = nullptr;
    shadowPixelShader_ = nullptr;
    maskPixelShader_ = nullptr;
    linearPixelShader_ = nullptr;
    radialPixelShader_ = nullptr;
    customEffectPixelShader_ = nullptr;
    for (auto* ps : uniquePS) {
        ps->Release();
    }

    std::vector<ID3D11InputLayout*> uniqueLayouts;
    if (vertex2DInputLayout_ != nullptr) {
        uniqueLayouts.push_back(vertex2DInputLayout_);
        vertex2DInputLayout_ = nullptr;
    }
    for (uint32_t i = 0; i < Shader::Vertex::Count; ++i) {
        ReleaseCom(vertexShaders_[i].shader);
        vertexShaders_[i].layout = nullptr;
    }
    for (uint32_t i = 0; i < Shader::Vertex::Format::Count; ++i) {
        if (layouts_[i] != nullptr && std::find(uniqueLayouts.begin(), uniqueLayouts.end(), layouts_[i]) == uniqueLayouts.end()) {
            uniqueLayouts.push_back(layouts_[i]);
        }
        layouts_[i] = nullptr;
    }
    for (auto* layout : uniqueLayouts) {
        layout->Release();
    }
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
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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

    Ref<RenderDevice> self = Ref<RenderDevice>::TryFromBorrowed(*this);
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

void D3D11RenderDevice::BeginOffscreenRender() noexcept {
    if (context_ == nullptr || currentTarget_ == nullptr) return;
    stateCache_.Reset();
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context_->ClearRenderTargetView(currentTarget_->GetRTV(), clearColor);
    ID3D11DepthStencilView* dsv = currentTarget_->GetDSV();
    if (dsv != nullptr) {
        context_->ClearDepthStencilView(dsv, D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
}
void D3D11RenderDevice::EndOffscreenRender() noexcept {}
void D3D11RenderDevice::BeginOnscreenRender() noexcept {
    if (context_ == nullptr || currentTarget_ == nullptr) return;
    stateCache_.Reset();
    ID3D11DepthStencilView* dsv = currentTarget_->GetDSV();
    if (dsv != nullptr) {
        context_->ClearDepthStencilView(dsv, D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
}
void D3D11RenderDevice::EndOnscreenRender() noexcept {}

void D3D11RenderDevice::SetRenderTarget(RenderTarget* surface) noexcept {
    if (context_ == nullptr) return;
    stateCache_.Reset();
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
    const uint32_t alignedBytes = (bytes + 63U) & ~63U;
    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (vbState_.currentPos + alignedBytes > vbState_.size) {
        mapType = D3D11_MAP_WRITE_DISCARD;
        vbState_.currentPos = 0U;
    } else if (vbState_.currentPos == 0U) {
        mapType = D3D11_MAP_WRITE_DISCARD;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context_->Map(dynamicVB_, 0, mapType, 0, &mapped);
    if (FAILED(hr) || mapped.pData == nullptr) return nullptr;
    vbState_.drawPos = vbState_.currentPos;
    vbState_.currentPos += alignedBytes;
    vbState_.isMapped = true;
    return static_cast<uint8_t*>(mapped.pData) + vbState_.drawPos;
}

void D3D11RenderDevice::UnmapVertices() noexcept {
    if (context_ != nullptr && dynamicVB_ != nullptr && vbState_.isMapped) {
        context_->Unmap(dynamicVB_, 0);
        vbState_.isMapped = false;
    }
}

void* D3D11RenderDevice::MapIndices(uint32_t bytes) noexcept {
    if (context_ == nullptr || dynamicIB_ == nullptr) return nullptr;
    const uint32_t alignedBytes = (bytes + 63U) & ~63U;
    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (ibState_.currentPos + alignedBytes > ibState_.size) {
        mapType = D3D11_MAP_WRITE_DISCARD;
        ibState_.currentPos = 0U;
    } else if (ibState_.currentPos == 0U) {
        mapType = D3D11_MAP_WRITE_DISCARD;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context_->Map(dynamicIB_, 0, mapType, 0, &mapped);
    if (FAILED(hr) || mapped.pData == nullptr) return nullptr;
    ibState_.drawPos = ibState_.currentPos;
    ibState_.currentPos += alignedBytes;
    ibState_.isMapped = true;
    return static_cast<uint8_t*>(mapped.pData) + ibState_.drawPos;
}

void D3D11RenderDevice::UnmapIndices() noexcept {
    if (context_ != nullptr && dynamicIB_ != nullptr && ibState_.isMapped) {
        context_->Unmap(dynamicIB_, 0);
        ibState_.isMapped = false;
    }
}

void D3D11RenderDevice::DrawBatch(const Batch& batch) noexcept {
    if (context_ == nullptr || batch.numIndices == 0U) return;

    ID3D11PixelShader* pixelShader = nullptr;
    if (batch.shader.v == Shader::Custom_Effect) {
        pixelShader = customEffectPixelShader_ != nullptr
            ? customEffectPixelShader_ : pixelShaders_[Shader::Path_Pattern];
        if (batch.pixelShader != nullptr) {
            ID3D11PixelShader* loaded = nullptr;
            if (SUCCEEDED(device_->CreatePixelShader(
                    batch.pixelShader,
                    batch.vertexUniforms[1].numDwords,
                    nullptr,
                    &loaded)) &&
                loaded != nullptr) {
                ReleaseCom(customEffectPixelShader_);
                customEffectPixelShader_ = loaded;
                pixelShader = customEffectPixelShader_;
            }
        }
    } else if (batch.shader.v < Shader::Count && pixelShaders_[batch.shader.v] != nullptr) {
        pixelShader = pixelShaders_[batch.shader.v];
    } else {
        pixelShader = solidPixelShader_;
    }

    const uint8_t vertexEnum = (batch.shader.v < Shader::Count)
        ? VertexForShader[batch.shader.v] : 0U;
    const uint8_t format = (vertexEnum < Shader::Vertex::Count)
        ? FormatForVertex[vertexEnum] : 0U;
    static_cast<void>(format);
    const VertexShaderEntry& vsEntry = (vertexEnum < Shader::Vertex::Count)
        ? vertexShaders_[vertexEnum] : VertexShaderEntry{};

    ID3D11VertexShader* vertexShader = vsEntry.shader != nullptr
        ? vsEntry.shader : solidVertexShader_;
    ID3D11InputLayout* inputLayout = vsEntry.layout != nullptr
        ? vsEntry.layout : vertex2DInputLayout_;
    const UINT stride = vsEntry.stride != 0U
        ? vsEntry.stride : 24U;

    if (vertexShader == nullptr || pixelShader == nullptr ||
        inputLayout == nullptr) {
        return;
    }

    const ShaderPipelineKey pipelineKey{batch.shader.v};
    const bool pipelineEnumChanged = stateCache_.UpdatePipeline(pipelineKey);
    const bool pipelineHandleChanged = stateCache_.UpdatePipelineHandle(
        reinterpret_cast<std::uintptr_t>(pixelShader));
    if (pipelineEnumChanged || pipelineHandleChanged) {
        context_->IASetInputLayout(inputLayout);
        context_->VSSetShader(vertexShader, nullptr, 0);
        context_->PSSetShader(pixelShader, nullptr, 0);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11Buffer* vsConstants[] = { vertexCB_[0] };
        context_->VSSetConstantBuffers(0, 1, vsConstants);
    } else {
        context_->IASetInputLayout(inputLayout);
        context_->VSSetShader(vertexShader, nullptr, 0);
    }

    const UINT offset = vbState_.drawPos + batch.vertexOffset;
    context_->IASetVertexBuffers(0, 1, &dynamicVB_, &stride, &offset);
    context_->IASetIndexBuffer(dynamicIB_, DXGI_FORMAT_R16_UINT, ibState_.drawPos);

    const BlendStateKey blendKey{
        batch.renderState.f.blendMode,
        batch.renderState.f.colorEnable};
    if (blendKey.blendMode < BlendMode::Count &&
        stateCache_.UpdateBlend(blendKey)) {
        ID3D11BlendState* bs =
            blendStates_[BlendStateKey::TableIndex(blendKey)];
        if (bs != nullptr) {
            context_->OMSetBlendState(bs, nullptr, 0xFFFFFFFF);
        }
    }

    const DepthStencilStateKey dsKey{
        batch.renderState.f.stencilMode,
        batch.stencilRef};
    if (dsKey.stencilMode < StencilMode::Count &&
        stateCache_.UpdateDepthStencil(dsKey)) {
        ID3D11DepthStencilState* ds = stencilStates_[dsKey.stencilMode];
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

    // 5-slot texture binding
    ID3D11ShaderResourceView* srvs[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    ID3D11SamplerState* samplers[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

    // Slot 0: Pattern (with fallback to image, ramps or glyphs for legacy single-slot draws)
    Texture* patTex = batch.pattern ? batch.pattern : batch.image;
    if (patTex != nullptr) {
        srvs[0] = static_cast<D3D11Texture*>(patTex)->GetNativeSRV();
        const uint8_t sIdx = StateCache::ClampSamplerIndex(
            batch.pattern ? batch.patternSampler.v : batch.imageSampler.v);
        samplers[0] = samplers_[sIdx];
    } else if (batch.ramps != nullptr) {
        srvs[0] = static_cast<D3D11Texture*>(batch.ramps)->GetNativeSRV();
        const uint8_t sIdx = StateCache::ClampSamplerIndex(batch.rampsSampler.v);
        samplers[0] = samplers_[sIdx];
    } else if (batch.glyphs != nullptr) {
        srvs[0] = static_cast<D3D11Texture*>(batch.glyphs)->GetNativeSRV();
        const uint8_t sIdx = StateCache::ClampSamplerIndex(batch.glyphsSampler.v);
        samplers[0] = samplers_[sIdx];
    }

    // Slot 1: Ramps (for legacy Mask, fallback to shadow if ramps is null)
    Texture* rampsTex = batch.ramps ? batch.ramps : (batch.shader.v == Shader::Mask ? batch.shadow : nullptr);
    if (rampsTex != nullptr) {
        srvs[1] = static_cast<D3D11Texture*>(rampsTex)->GetNativeSRV();
        const uint8_t sIdx = StateCache::ClampSamplerIndex(
            batch.ramps ? batch.rampsSampler.v : batch.shadowSampler.v);
        samplers[1] = samplers_[sIdx];
    }

    // Slot 2: Image
    if (batch.image != nullptr) {
        srvs[2] = static_cast<D3D11Texture*>(batch.image)->GetNativeSRV();
        const uint8_t sIdx = StateCache::ClampSamplerIndex(batch.imageSampler.v);
        samplers[2] = samplers_[sIdx];
    }

    // Slot 3: Glyphs
    if (batch.glyphs != nullptr) {
        srvs[3] = static_cast<D3D11Texture*>(batch.glyphs)->GetNativeSRV();
        const uint8_t sIdx = StateCache::ClampSamplerIndex(batch.glyphsSampler.v);
        samplers[3] = samplers_[sIdx];
    }

    // Slot 4: Shadow (with fallback to image when shader == Shadow)
    Texture* shadowTex = batch.shadow ? batch.shadow : (batch.shader.v == Shader::Shadow ? batch.image : nullptr);
    if (shadowTex != nullptr) {
        srvs[4] = static_cast<D3D11Texture*>(shadowTex)->GetNativeSRV();
        const uint8_t sIdx = StateCache::ClampSamplerIndex(
            batch.shadow ? batch.shadowSampler.v : batch.imageSampler.v);
        samplers[4] = samplers_[sIdx];
    }

    context_->PSSetShaderResources(0, 5, srvs);
    context_->PSSetSamplers(0, 5, samplers);

    const bool setTextureSize = (batch.shader.v == Shader::Blur ||
                                batch.shader.v == Shader::Shadow ||
                                batch.shader.v == Shader::Downsample ||
                                batch.shader.v == Shader::Custom_Effect);
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
        context_->PSSetConstantBuffers(0, 1, psConstants);
        context_->PSSetConstantBuffers(1, 1, psConstants);
    } else if (batch.pixelUniforms[0].values != nullptr &&
               batch.pixelUniforms[0].numDwords >= 1U) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context_->Map(
                pixelCB_[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            float* data = static_cast<float*>(mapped.pData);
            const uint32_t count = std::min(batch.pixelUniforms[0].numDwords, 8U);
            std::memcpy(
                data,
                batch.pixelUniforms[0].values,
                static_cast<size_t>(count) * sizeof(float));
            context_->Unmap(pixelCB_[0], 0);
        }
        ID3D11Buffer* psConstants[] = { pixelCB_[0] };
        context_->PSSetConstantBuffers(0, 1, psConstants);
    }

    context_->DrawIndexed(batch.numIndices, batch.startIndex, 0);
}

} // namespace Aero::Render
