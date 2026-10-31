#include "D3D11RenderDevice.hpp"
#include <cstring>
#include <algorithm>
#include <vector>

#include "AeroD3D11Shaders.hpp"

namespace Aero::Render {

namespace {

template<class T>
void ReleaseCom(T*& value) noexcept {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

#if !defined(NDEBUG)
constexpr GUID kD3DDebugObjectNameGuid = {
    0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}
};

void SetD3DDebugName(ID3D11DeviceChild* resource, const char* label) noexcept {
    if (resource != nullptr && label != nullptr && label[0] != '\0') {
        resource->SetPrivateData(
            kD3DDebugObjectNameGuid,
            static_cast<UINT>(std::strlen(label)),
            label);
    }
}

#define AERO_DX_NAME(res, label) SetD3DDebugName(res, label)
#define AERO_DX_BEGIN_EVENT(marker, name) if ((marker) != nullptr) (marker)->BeginEvent(name)
#define AERO_DX_END_EVENT(marker) if ((marker) != nullptr) (marker)->EndEvent()
#else
#define AERO_DX_NAME(res, label) ((void)0)
#define AERO_DX_BEGIN_EVENT(marker, name) ((void)0)
#define AERO_DX_END_EVENT(marker) ((void)0)
#endif

uint32_t HashBytesFNV1a(const void* data, size_t size) noexcept {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash != 0U ? hash : 1U;
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
    uint32_t attributes, const void* code, uint32_t size, const char* label) noexcept {
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
    AERO_DX_NAME(layout, label);
    return layout;
}

Base::Result<void> D3D11RenderDevice::CreateBuffer(
    DynamicBuffer& buffer, uint32_t size, D3D11_BIND_FLAG flags, const char* label) noexcept {
    buffer.size = size;
    buffer.pos = 0U;
    buffer.drawPos = 0U;
    buffer.flags = flags;
    buffer.obj = nullptr;
    buffer.isMapped = false;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = size;
    desc.BindFlags = flags;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device_->CreateBuffer(&desc, nullptr, &buffer.obj))) {
        return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create D3D11 dynamic buffer");
    }
    AERO_DX_NAME(buffer.obj, label);
    return {};
}

void* D3D11RenderDevice::MapBuffer(DynamicBuffer& buffer, uint32_t bytes) noexcept {
    if (context_ == nullptr || buffer.obj == nullptr || bytes == 0U || bytes > buffer.size) {
        return nullptr;
    }

    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (buffer.flags == D3D11_BIND_CONSTANT_BUFFER) {
        mapType = D3D11_MAP_WRITE_DISCARD;
        buffer.drawPos = 0U;
    } else {
        const uint32_t alignedBytes = (bytes + 63U) & ~63U;
        if (buffer.pos == 0U || buffer.pos + alignedBytes > buffer.size) {
            mapType = D3D11_MAP_WRITE_DISCARD;
            buffer.pos = 0U;
        }
        buffer.drawPos = buffer.pos;
        buffer.pos += alignedBytes;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(buffer.obj, 0, mapType, 0, &mapped)) || mapped.pData == nullptr) {
        return nullptr;
    }
    buffer.isMapped = true;
    return static_cast<uint8_t*>(mapped.pData) + buffer.drawPos;
}

void D3D11RenderDevice::UnmapBuffer(DynamicBuffer& buffer) noexcept {
    if (context_ != nullptr && buffer.obj != nullptr && buffer.isMapped) {
        context_->Unmap(buffer.obj, 0);
        buffer.isMapped = false;
    }
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

    if (context_ != nullptr) {
        context_->QueryInterface(
            __uuidof(ID3DUserDefinedAnnotation),
            reinterpret_cast<void**>(&groupMarker_));
    }

    state_ = RenderDeviceState::Ready;
    return {};
}

Base::Result<void> D3D11RenderDevice::InitPipelines() noexcept {
    Base::Result<void> res = CreateBuffer(
        vertices_, DYNAMIC_VB_SIZE, D3D11_BIND_VERTEX_BUFFER, "Aero_DynamicVB");
    if (!res) return res;

    res = CreateBuffer(
        indices_, DYNAMIC_IB_SIZE, D3D11_BIND_INDEX_BUFFER, "Aero_DynamicIB");
    if (!res) return res;

    res = CreateBuffer(vertexCB_[0], 16, D3D11_BIND_CONSTANT_BUFFER, "Aero_VertexCB0");
    if (!res) return res;
    res = CreateBuffer(vertexCB_[1], 64, D3D11_BIND_CONSTANT_BUFFER, "Aero_VertexCB1");
    if (!res) return res;
    res = CreateBuffer(pixelCB_[0], 32, D3D11_BIND_CONSTANT_BUFFER, "Aero_PixelCB0");
    if (!res) return res;
    res = CreateBuffer(pixelCB_[1], 32, D3D11_BIND_CONSTANT_BUFFER, "Aero_PixelCB1");
    if (!res) return res;

    // 4-state rasterizer table: [0]=Solid+Scissor, [1]=Wire+Scissor, [2]=Solid, [3]=Wire
    D3D11_RASTERIZER_DESC rsDesc{};
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthBias = 0;
    rsDesc.DepthBiasClamp = 0.0f;
    rsDesc.SlopeScaledDepthBias = 0.0f;
    rsDesc.DepthClipEnable = TRUE;
    rsDesc.MultisampleEnable = TRUE;
    rsDesc.AntialiasedLineEnable = FALSE;
    const char* rsNames[4] = {"RS_Solid_Scissor", "RS_Wire_Scissor", "RS_Solid", "RS_Wire"};

    for (int i = 0; i < 4; ++i) {
        rsDesc.FillMode = (i & 1) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.ScissorEnable = (i < 2) ? TRUE : FALSE;
        device_->CreateRasterizerState(&rsDesc, &rasterizerStates_[i]);
        AERO_DX_NAME(rasterizerStates_[i], rsNames[i]);
    }

    auto createPS = [&](const void* code, size_t size, const char* label) -> ID3D11PixelShader* {
        ID3D11PixelShader* ps = nullptr;
        if (code != nullptr && size > 0U) {
            device_->CreatePixelShader(code, size, nullptr, &ps);
            if (ps != nullptr) {
                AERO_DX_NAME(ps, label);
            }
        }
        return ps;
    };

    pixelShaders_[Shader::RGBA] = createPS(AeroD3D11ShaderPSRGBA, sizeof(AeroD3D11ShaderPSRGBA), "PS_RGBA");
    pixelShaders_[Shader::Mask] = createPS(AeroD3D11ShaderPSMask, sizeof(AeroD3D11ShaderPSMask), "PS_Mask");
    pixelShaders_[Shader::Clear] = createPS(AeroD3D11ShaderPSClear, sizeof(AeroD3D11ShaderPSClear), "PS_Clear");

    pixelShaders_[Shader::Path_Solid] = createPS(AeroD3D11ShaderPSPath_Solid, sizeof(AeroD3D11ShaderPSPath_Solid), "PS_Path_Solid");
    pixelShaders_[Shader::Path_Linear] = createPS(AeroD3D11ShaderPSPath_Linear, sizeof(AeroD3D11ShaderPSPath_Linear), "PS_Path_Linear");
    pixelShaders_[Shader::Path_Radial] = createPS(AeroD3D11ShaderPSPath_Radial, sizeof(AeroD3D11ShaderPSPath_Radial), "PS_Path_Radial");
    pixelShaders_[Shader::Path_Pattern] = createPS(AeroD3D11ShaderPSPath_Pattern, sizeof(AeroD3D11ShaderPSPath_Pattern), "PS_Path_Pattern");
    pixelShaders_[Shader::Path_Pattern_Clamp] = createPS(AeroD3D11ShaderPSPath_Pattern_Clamp, sizeof(AeroD3D11ShaderPSPath_Pattern_Clamp), "PS_Path_Pattern_Clamp");
    pixelShaders_[Shader::Path_Pattern_Repeat] = createPS(AeroD3D11ShaderPSPath_Pattern_Repeat, sizeof(AeroD3D11ShaderPSPath_Pattern_Repeat), "PS_Path_Pattern_Repeat");
    pixelShaders_[Shader::Path_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSPath_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSPath_Pattern_MirrorU), "PS_Path_Pattern_MirrorU");
    pixelShaders_[Shader::Path_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSPath_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSPath_Pattern_MirrorV), "PS_Path_Pattern_MirrorV");
    pixelShaders_[Shader::Path_Pattern_Mirror] = createPS(AeroD3D11ShaderPSPath_Pattern_Mirror, sizeof(AeroD3D11ShaderPSPath_Pattern_Mirror), "PS_Path_Pattern_Mirror");

    pixelShaders_[Shader::Path_AA_Solid] = createPS(AeroD3D11ShaderPSPath_AA_Solid, sizeof(AeroD3D11ShaderPSPath_AA_Solid), "PS_Path_AA_Solid");
    pixelShaders_[Shader::Path_AA_Linear] = createPS(AeroD3D11ShaderPSPath_AA_Linear, sizeof(AeroD3D11ShaderPSPath_AA_Linear), "PS_Path_AA_Linear");
    pixelShaders_[Shader::Path_AA_Radial] = createPS(AeroD3D11ShaderPSPath_AA_Radial, sizeof(AeroD3D11ShaderPSPath_AA_Radial), "PS_Path_AA_Radial");
    pixelShaders_[Shader::Path_AA_Pattern] = createPS(AeroD3D11ShaderPSPath_AA_Pattern, sizeof(AeroD3D11ShaderPSPath_AA_Pattern), "PS_Path_AA_Pattern");
    pixelShaders_[Shader::Path_AA_Pattern_Clamp] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_Clamp, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_Clamp), "PS_Path_AA_Pattern_Clamp");
    pixelShaders_[Shader::Path_AA_Pattern_Repeat] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_Repeat, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_Repeat), "PS_Path_AA_Pattern_Repeat");
    pixelShaders_[Shader::Path_AA_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_MirrorU), "PS_Path_AA_Pattern_MirrorU");
    pixelShaders_[Shader::Path_AA_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_MirrorV), "PS_Path_AA_Pattern_MirrorV");
    pixelShaders_[Shader::Path_AA_Pattern_Mirror] = createPS(AeroD3D11ShaderPSPath_AA_Pattern_Mirror, sizeof(AeroD3D11ShaderPSPath_AA_Pattern_Mirror), "PS_Path_AA_Pattern_Mirror");

    pixelShaders_[Shader::SDF_Solid] = createPS(AeroD3D11ShaderPSSDF_Solid, sizeof(AeroD3D11ShaderPSSDF_Solid), "PS_SDF_Solid");
    pixelShaders_[Shader::SDF_Linear] = createPS(AeroD3D11ShaderPSSDF_Linear, sizeof(AeroD3D11ShaderPSSDF_Linear), "PS_SDF_Linear");
    pixelShaders_[Shader::SDF_Radial] = createPS(AeroD3D11ShaderPSSDF_Radial, sizeof(AeroD3D11ShaderPSSDF_Radial), "PS_SDF_Radial");
    pixelShaders_[Shader::SDF_Pattern] = createPS(AeroD3D11ShaderPSSDF_Pattern, sizeof(AeroD3D11ShaderPSSDF_Pattern), "PS_SDF_Pattern");
    pixelShaders_[Shader::SDF_Pattern_Clamp] = createPS(AeroD3D11ShaderPSSDF_Pattern_Clamp, sizeof(AeroD3D11ShaderPSSDF_Pattern_Clamp), "PS_SDF_Pattern_Clamp");
    pixelShaders_[Shader::SDF_Pattern_Repeat] = createPS(AeroD3D11ShaderPSSDF_Pattern_Repeat, sizeof(AeroD3D11ShaderPSSDF_Pattern_Repeat), "PS_SDF_Pattern_Repeat");
    pixelShaders_[Shader::SDF_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSSDF_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSSDF_Pattern_MirrorU), "PS_SDF_Pattern_MirrorU");
    pixelShaders_[Shader::SDF_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSSDF_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSSDF_Pattern_MirrorV), "PS_SDF_Pattern_MirrorV");
    pixelShaders_[Shader::SDF_Pattern_Mirror] = createPS(AeroD3D11ShaderPSSDF_Pattern_Mirror, sizeof(AeroD3D11ShaderPSSDF_Pattern_Mirror), "PS_SDF_Pattern_Mirror");

    pixelShaders_[Shader::SDF_LCD_Solid] = createPS(AeroD3D11ShaderPSSDF_LCD_Solid, sizeof(AeroD3D11ShaderPSSDF_LCD_Solid), "PS_SDF_LCD_Solid");
    pixelShaders_[Shader::SDF_LCD_Linear] = createPS(AeroD3D11ShaderPSSDF_LCD_Linear, sizeof(AeroD3D11ShaderPSSDF_LCD_Linear), "PS_SDF_LCD_Linear");
    pixelShaders_[Shader::SDF_LCD_Radial] = createPS(AeroD3D11ShaderPSSDF_LCD_Radial, sizeof(AeroD3D11ShaderPSSDF_LCD_Radial), "PS_SDF_LCD_Radial");
    pixelShaders_[Shader::SDF_LCD_Pattern] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern), "PS_SDF_LCD_Pattern");
    pixelShaders_[Shader::SDF_LCD_Pattern_Clamp] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_Clamp, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_Clamp), "PS_SDF_LCD_Pattern_Clamp");
    pixelShaders_[Shader::SDF_LCD_Pattern_Repeat] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_Repeat, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_Repeat), "PS_SDF_LCD_Pattern_Repeat");
    pixelShaders_[Shader::SDF_LCD_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorU), "PS_SDF_LCD_Pattern_MirrorU");
    pixelShaders_[Shader::SDF_LCD_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_MirrorV), "PS_SDF_LCD_Pattern_MirrorV");
    pixelShaders_[Shader::SDF_LCD_Pattern_Mirror] = createPS(AeroD3D11ShaderPSSDF_LCD_Pattern_Mirror, sizeof(AeroD3D11ShaderPSSDF_LCD_Pattern_Mirror), "PS_SDF_LCD_Pattern_Mirror");

    pixelShaders_[Shader::Opacity_Solid] = createPS(AeroD3D11ShaderPSOpacity_Solid, sizeof(AeroD3D11ShaderPSOpacity_Solid), "PS_Opacity_Solid");
    pixelShaders_[Shader::Opacity_Linear] = createPS(AeroD3D11ShaderPSOpacity_Linear, sizeof(AeroD3D11ShaderPSOpacity_Linear), "PS_Opacity_Linear");
    pixelShaders_[Shader::Opacity_Radial] = createPS(AeroD3D11ShaderPSOpacity_Radial, sizeof(AeroD3D11ShaderPSOpacity_Radial), "PS_Opacity_Radial");
    pixelShaders_[Shader::Opacity_Pattern] = createPS(AeroD3D11ShaderPSOpacity_Pattern, sizeof(AeroD3D11ShaderPSOpacity_Pattern), "PS_Opacity_Pattern");
    pixelShaders_[Shader::Opacity_Pattern_Clamp] = createPS(AeroD3D11ShaderPSOpacity_Pattern_Clamp, sizeof(AeroD3D11ShaderPSOpacity_Pattern_Clamp), "PS_Opacity_Pattern_Clamp");
    pixelShaders_[Shader::Opacity_Pattern_Repeat] = createPS(AeroD3D11ShaderPSOpacity_Pattern_Repeat, sizeof(AeroD3D11ShaderPSOpacity_Pattern_Repeat), "PS_Opacity_Pattern_Repeat");
    pixelShaders_[Shader::Opacity_Pattern_MirrorU] = createPS(AeroD3D11ShaderPSOpacity_Pattern_MirrorU, sizeof(AeroD3D11ShaderPSOpacity_Pattern_MirrorU), "PS_Opacity_Pattern_MirrorU");
    pixelShaders_[Shader::Opacity_Pattern_MirrorV] = createPS(AeroD3D11ShaderPSOpacity_Pattern_MirrorV, sizeof(AeroD3D11ShaderPSOpacity_Pattern_MirrorV), "PS_Opacity_Pattern_MirrorV");
    pixelShaders_[Shader::Opacity_Pattern_Mirror] = createPS(AeroD3D11ShaderPSOpacity_Pattern_Mirror, sizeof(AeroD3D11ShaderPSOpacity_Pattern_Mirror), "PS_Opacity_Pattern_Mirror");

    pixelShaders_[Shader::Upsample] = createPS(AeroD3D11ShaderPSUpsample, sizeof(AeroD3D11ShaderPSUpsample), "PS_Upsample");
    pixelShaders_[Shader::Downsample] = createPS(AeroD3D11ShaderPSDownsample, sizeof(AeroD3D11ShaderPSDownsample), "PS_Downsample");
    pixelShaders_[Shader::Shadow] = createPS(AeroD3D11ShaderPSShadow, sizeof(AeroD3D11ShaderPSShadow), "PS_Shadow");
    pixelShaders_[Shader::Blur] = createPS(AeroD3D11ShaderPSBlur, sizeof(AeroD3D11ShaderPSBlur), "PS_Blur");

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
                layouts_[format] = CreateLayout(AttributesForFormat[format], entry.code, entry.size, "Aero_InputLayout");
            }
            AERO_DX_NAME(vs, "Aero_VS");
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
        AERO_DX_NAME(samplers_[v], "Sampler");
    }

    // Create 6 blend modes
    for (int bm = 0; bm < BlendMode::Count; ++bm) {
        D3D11_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        auto& rt = blendDesc.RenderTarget[0];
        rt.BlendEnable = (bm != BlendMode::Src);
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        switch (bm) {
        case BlendMode::Src:
            rt.SrcBlend = D3D11_BLEND_ONE;
            rt.DestBlend = D3D11_BLEND_ZERO;
            rt.BlendOp = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_ZERO;
            rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            break;
        case BlendMode::SrcOver:
        default:
            rt.SrcBlend = D3D11_BLEND_ONE;
            rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOp = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            break;
        case BlendMode::SrcOver_Multiply:
            rt.SrcBlend = D3D11_BLEND_DEST_COLOR;
            rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOp = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            break;
        case BlendMode::SrcOver_Screen:
            rt.SrcBlend = D3D11_BLEND_ONE;
            rt.DestBlend = D3D11_BLEND_INV_SRC_COLOR;
            rt.BlendOp = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            break;
        case BlendMode::SrcOver_Additive:
            rt.SrcBlend = D3D11_BLEND_ONE;
            rt.DestBlend = D3D11_BLEND_ONE;
            rt.BlendOp = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            break;
        case BlendMode::SrcOver_Dual:
            rt.SrcBlend = D3D11_BLEND_ONE;
            rt.DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
            rt.BlendOp = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
            rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            break;
        }
        device_->CreateBlendState(&blendDesc, &blendStates_[bm]);
        AERO_DX_NAME(blendStates_[bm], "BlendState");
    }

    // 1 shared blendStateNoColor_ (used when colorEnable == 0)
    {
        D3D11_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
        device_->CreateBlendState(&blendDesc, &blendStateNoColor_);
        AERO_DX_NAME(blendStateNoColor_, "BlendState_NoColor");
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
        AERO_DX_NAME(stencilStates_[sm], "DepthStencilState");
    }

    return {};
}

void D3D11RenderDevice::ReleasePipelines() noexcept {
    ReleaseCom(vertices_.obj);
    vertices_ = {};
    ReleaseCom(indices_.obj);
    indices_ = {};
    for (int i = 0; i < 2; ++i) {
        ReleaseCom(vertexCB_[i].obj);
        vertexCB_[i] = {};
        ReleaseCom(pixelCB_[i].obj);
        pixelCB_[i] = {};
        vertexCBHash_[i] = 0U;
        pixelCBHash_[i] = 0U;
    }
    for (int i = 0; i < 4; ++i) {
        ReleaseCom(rasterizerStates_[i]);
    }
    for (int i = 0; i < BlendMode::Count; ++i) {
        ReleaseCom(blendStates_[i]);
    }
    ReleaseCom(blendStateNoColor_);
    for (int i = 0; i < StencilMode::Count; ++i) {
        ReleaseCom(stencilStates_[i]);
    }
    for (uint8_t i = 0; i < StateCache::kSamplerTableSize; ++i) {
        ReleaseCom(samplers_[i]);
    }
    for (auto& entry : customShaders_) {
        ReleaseCom(entry.shader);
    }
    customShaders_.Clear();

    std::vector<ID3D11PixelShader*> uniquePS;
    auto addUniquePS = [&](ID3D11PixelShader* ps) {
        if (ps != nullptr && std::find(uniquePS.begin(), uniquePS.end(), ps) == uniquePS.end()) {
            uniquePS.push_back(ps);
        }
    };
    for (uint32_t i = 0; i < Shader::Count; ++i) {
        addUniquePS(pixelShaders_[i]);
        pixelShaders_[i] = nullptr;
    }
    for (auto* ps : uniquePS) {
        ps->Release();
    }

    std::vector<ID3D11InputLayout*> uniqueLayouts;
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
    ReleaseCom(groupMarker_);
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

void D3D11RenderDevice::InvalidateStateCache() noexcept {
    if (context_ == nullptr) return;
    ID3D11Buffer* vsCBs[] = { vertexCB_[0].obj, vertexCB_[1].obj };
    context_->VSSetConstantBuffers(0, 2, vsCBs);
    ID3D11Buffer* psCBs[] = { pixelCB_[0].obj, pixelCB_[1].obj };
    context_->PSSetConstantBuffers(0, 2, psCBs);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cachedIB_ = nullptr;
    cachedIBOffset_ = UINT32_MAX;
    cachedVB_ = nullptr;
    cachedVBStride_ = 0U;
    cachedVBOffset_ = UINT32_MAX;
    cachedLayout_ = nullptr;
    cachedVS_ = nullptr;
    cachedPS_ = nullptr;
    cachedRasterizer_ = nullptr;
    cachedBlend_ = nullptr;
    cachedDS_ = nullptr;
    cachedStencilRef_ = UINT32_MAX;
    std::memset(cachedTextures_, 0, sizeof(cachedTextures_));
    std::memset(cachedSamplers_, 0, sizeof(cachedSamplers_));
}

void D3D11RenderDevice::ClearTextures() noexcept {
    if (context_ == nullptr) return;
    std::memset(cachedTextures_, 0, sizeof(cachedTextures_));
    context_->PSSetShaderResources(0, StateCache::kTextureSlots, cachedTextures_);
}

void D3D11RenderDevice::SetInputLayout(ID3D11InputLayout* layout) noexcept {
    if (cachedLayout_ != layout) {
        context_->IASetInputLayout(layout);
        cachedLayout_ = layout;
    }
}

void D3D11RenderDevice::SetVertexShader(ID3D11VertexShader* vs) noexcept {
    if (cachedVS_ != vs) {
        context_->VSSetShader(vs, nullptr, 0);
        cachedVS_ = vs;
    }
}

void D3D11RenderDevice::SetPixelShader(ID3D11PixelShader* ps) noexcept {
    if (cachedPS_ != ps) {
        context_->PSSetShader(ps, nullptr, 0);
        cachedPS_ = ps;
    }
}

void D3D11RenderDevice::SetRasterizerState(ID3D11RasterizerState* rs) noexcept {
    if (cachedRasterizer_ != rs) {
        context_->RSSetState(rs);
        cachedRasterizer_ = rs;
    }
}

void D3D11RenderDevice::SetBlendState(ID3D11BlendState* bs) noexcept {
    if (cachedBlend_ != bs) {
        context_->OMSetBlendState(bs, nullptr, 0xFFFFFFFF);
        cachedBlend_ = bs;
    }
}

void D3D11RenderDevice::SetDepthStencilState(ID3D11DepthStencilState* dss, uint32_t stencilRef) noexcept {
    if (cachedDS_ != dss || cachedStencilRef_ != stencilRef) {
        context_->OMSetDepthStencilState(dss, stencilRef);
        cachedDS_ = dss;
        cachedStencilRef_ = stencilRef;
    }
}

void D3D11RenderDevice::SetTexture(uint32_t slot, ID3D11ShaderResourceView* srv) noexcept {
    if (slot < StateCache::kTextureSlots && cachedTextures_[slot] != srv) {
        context_->PSSetShaderResources(slot, 1, &srv);
        cachedTextures_[slot] = srv;
    }
}

void D3D11RenderDevice::SetSampler(uint32_t slot, ID3D11SamplerState* sampler) noexcept {
    if (slot < StateCache::kTextureSlots && cachedSamplers_[slot] != sampler) {
        context_->PSSetSamplers(slot, 1, &sampler);
        cachedSamplers_[slot] = sampler;
    }
}

Ref<RenderTarget> D3D11RenderDevice::CreateRenderTarget(
    const char* label, uint32_t width, uint32_t height,
    uint32_t sampleCount, bool needsStencil) noexcept {
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

    AERO_DX_NAME(tex, label);
    AERO_DX_NAME(rtv, label);
    if (srv != nullptr) AERO_DX_NAME(srv, label);
    if (dsv != nullptr) AERO_DX_NAME(dsv, label);

    Ref<D3D11Texture> d3dTex = Base::MakeRef<D3D11Texture>(
        tex, srv, width, height, false, true);

    Ref<RenderDevice> self = Ref<RenderDevice>::TryFromBorrowed(*this);
    return Base::MakeRef<D3D11RenderTarget>(
        std::move(self), std::move(d3dTex), rtv, dsv, width, height);
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
    if (device_ == nullptr) return {};

    const uint32_t levels = numLevels > 0 ? numLevels : 1;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = levels;
    desc.ArraySize = 1;
    desc.Format = (format == TextureFormat::R8) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = (data != nullptr && data[0] != nullptr) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = S_OK;
    if (data != nullptr && data[0] != nullptr) {
        const uint32_t bpp = (format == TextureFormat::R8 ? 1U : 4U);
        std::vector<D3D11_SUBRESOURCE_DATA> subData(levels);
        for (uint32_t i = 0; i < levels; ++i) {
            subData[i].pSysMem = data[i] != nullptr ? data[i] : data[0];
            const uint32_t mipWidth = std::max(1U, width >> i);
            subData[i].SysMemPitch = mipWidth * bpp;
            subData[i].SysMemSlicePitch = 0;
        }
        hr = device_->CreateTexture2D(&desc, subData.data(), &tex);
    } else {
        hr = device_->CreateTexture2D(&desc, nullptr, &tex);
    }
    if (FAILED(hr) || tex == nullptr) return {};

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device_->CreateShaderResourceView(tex, nullptr, &srv);
    if (FAILED(hr)) {
        ReleaseCom(tex);
        return {};
    }

    AERO_DX_NAME(tex, label);
    AERO_DX_NAME(srv, label);

    return Base::MakeRef<D3D11Texture>(
        tex, srv, width, height, levels > 1, format != TextureFormat::RGBX8);
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
    AERO_DX_BEGIN_EVENT(groupMarker_, L"Aero.Offscreen");
    InvalidateStateCache();
    EnableScissorRect();
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (currentTarget_->GetRTV() != nullptr) {
        context_->ClearRenderTargetView(currentTarget_->GetRTV(), clearColor);
    }
    if (currentTarget_->GetDSV() != nullptr) {
        context_->ClearDepthStencilView(currentTarget_->GetDSV(), D3D11_CLEAR_STENCIL, 0.0f, 0);
    }
}

void D3D11RenderDevice::EndOffscreenRender() noexcept {
    ClearTextures();
    AERO_DX_END_EVENT(groupMarker_);
}

void D3D11RenderDevice::BeginOnscreenRender() noexcept {
    if (context_ == nullptr || currentTarget_ == nullptr) return;
    AERO_DX_BEGIN_EVENT(groupMarker_, L"Aero.Onscreen");
    InvalidateStateCache();
    if (currentTarget_->GetRTV() != nullptr) {
        const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context_->ClearRenderTargetView(currentTarget_->GetRTV(), clearColor);
    }
    if (currentTarget_->GetDSV() != nullptr) {
        context_->ClearDepthStencilView(currentTarget_->GetDSV(), D3D11_CLEAR_STENCIL, 0.0f, 0);
    }
}

void D3D11RenderDevice::EndOnscreenRender() noexcept {
    ClearTextures();
    AERO_DX_END_EVENT(groupMarker_);
}

void D3D11RenderDevice::SetRenderTarget(RenderTarget* surface) noexcept {
    if (context_ == nullptr) return;
    ClearTextures();
    InvalidateStateCache();
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

        float viewportData[4] = { vp.Width, vp.Height, 0.0f, 0.0f };
        const uint32_t vpHash = HashBytesFNV1a(viewportData, sizeof(viewportData));
        if (vertexCBHash_[0] != vpHash) {
            if (void* ptr = MapBuffer(vertexCB_[0], sizeof(viewportData))) {
                std::memcpy(ptr, viewportData, sizeof(viewportData));
                UnmapBuffer(vertexCB_[0]);
                vertexCBHash_[0] = vpHash;
            }
        }
    }
}

void D3D11RenderDevice::BeginTile(RenderTarget* surface, const Tile& tile) noexcept {
    static_cast<void>(surface);
    if (context_ == nullptr) return;

    D3D11_RECT scissor{};
    scissor.left = static_cast<LONG>(tile.x);
    scissor.top = static_cast<LONG>(tile.y);
    scissor.right = static_cast<LONG>(tile.x + tile.width);
    scissor.bottom = static_cast<LONG>(tile.y + tile.height);
    context_->RSSetScissorRects(1, &scissor);
    EnableScissorRect();
}

void D3D11RenderDevice::EndTile(RenderTarget* surface) noexcept {
    static_cast<void>(surface);
    DisableScissorRect();
}

void D3D11RenderDevice::ResolveRenderTarget(
    RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept {
    static_cast<void>(surface);
    static_cast<void>(tiles);
    static_cast<void>(numTiles);
}

void* D3D11RenderDevice::MapVertices(uint32_t bytes) noexcept {
    return MapBuffer(vertices_, bytes);
}

void D3D11RenderDevice::UnmapVertices() noexcept {
    UnmapBuffer(vertices_);
}

void* D3D11RenderDevice::MapIndices(uint32_t bytes) noexcept {
    return MapBuffer(indices_, bytes);
}

void D3D11RenderDevice::UnmapIndices() noexcept {
    UnmapBuffer(indices_);
}

ID3D11PixelShader* D3D11RenderDevice::GetOrCreateCustomShader(
    const void* bytecode, uint32_t size) noexcept {
    if (device_ == nullptr || bytecode == nullptr || size == 0U) {
        return pixelShaders_[Shader::Path_Pattern];
    }
    const uint32_t hash = HashBytesFNV1a(bytecode, size);
    for (const auto& entry : customShaders_) {
        if (entry.bytecodeHash == hash && entry.bytecodeSize == size) {
            return entry.shader;
        }
    }

    ID3D11PixelShader* created = nullptr;
    if (FAILED(device_->CreatePixelShader(bytecode, size, nullptr, &created)) || created == nullptr) {
        return pixelShaders_[Shader::Path_Pattern];
    }
    AERO_DX_NAME(created, "Aero_CustomPS");
    static_cast<void>(customShaders_.PushBack(CustomShaderCacheEntry{hash, size, created}));
    return created;
}

void D3D11RenderDevice::DrawBatch(const Batch& batch) noexcept {
    if (context_ == nullptr || batch.numIndices == 0U) return;

    // 1. 解析并绑定 VS / InputLayout / PS
    const uint8_t vertexEnum = (batch.shader.v < Shader::Count)
        ? VertexForShader[batch.shader.v]
        : static_cast<uint8_t>(Shader::Vertex::PosColorCoverage);
    const VertexShaderEntry& vsEntry = (vertexEnum < Shader::Vertex::Count)
        ? vertexShaders_[vertexEnum]
        : vertexShaders_[Shader::Vertex::PosColorCoverage];

    ID3D11PixelShader* ps = nullptr;
    if (batch.shader.v == Shader::Custom_Effect) {
        ps = GetOrCreateCustomShader(batch.pixelShader, batch.pixelShaderSize);
    } else if (batch.shader.v < Shader::Count && pixelShaders_[batch.shader.v] != nullptr) {
        ps = pixelShaders_[batch.shader.v];
    } else {
        ps = pixelShaders_[Shader::Path_Solid];
    }

    if (vsEntry.shader == nullptr || vsEntry.layout == nullptr || ps == nullptr) return;

    SetInputLayout(vsEntry.layout);
    SetVertexShader(vsEntry.shader);
    SetPixelShader(ps);

    // 2. 绑定 VB / IB 与基于 Hash 短路的 Constant Buffers
    const uint32_t vbOffset = vertices_.drawPos + batch.vertexOffset;
    const uint32_t stride = vsEntry.stride != 0U ? vsEntry.stride : 24U;
    if (cachedVB_ != vertices_.obj || cachedVBStride_ != stride || cachedVBOffset_ != vbOffset) {
        context_->IASetVertexBuffers(0, 1, &vertices_.obj, &stride, &vbOffset);
        cachedVB_ = vertices_.obj;
        cachedVBStride_ = stride;
        cachedVBOffset_ = vbOffset;
    }
    if (cachedIB_ != indices_.obj || cachedIBOffset_ != indices_.drawPos) {
        context_->IASetIndexBuffer(indices_.obj, DXGI_FORMAT_R16_UINT, indices_.drawPos);
        cachedIB_ = indices_.obj;
        cachedIBOffset_ = indices_.drawPos;
    }

    for (uint32_t i = 0U; i < 2U; ++i) {
        const auto& u = batch.pixelUniforms[i];
        if (u.values != nullptr && u.numDwords > 0U) {
            const uint32_t byteSize = std::min(u.numDwords, 8U) * sizeof(float);
            const uint32_t hash = u.hash != 0U ? u.hash : HashBytesFNV1a(u.values, byteSize);
            if (pixelCBHash_[i] != hash) {
                if (void* dst = MapBuffer(pixelCB_[i], byteSize)) {
                    std::memcpy(dst, u.values, byteSize);
                    UnmapBuffer(pixelCB_[i]);
                    pixelCBHash_[i] = hash;
                }
            }
        }
    }

    for (uint32_t i = 1U; i < 2U; ++i) {
        const auto& u = batch.vertexUniforms[i];
        if (u.values != nullptr && u.numDwords > 0U) {
            float cbData[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            const uint32_t count = std::min(u.numDwords, 4U);
            const auto* vals = static_cast<const float*>(u.values);
            for (uint32_t c = 0; c < count; ++c) {
                cbData[c] = vals[c];
            }
            const uint32_t byteSize = sizeof(cbData);
            const uint32_t hash = u.hash != 0U ? u.hash : HashBytesFNV1a(cbData, byteSize);
            if (vertexCBHash_[i] != hash) {
                if (void* dst = MapBuffer(vertexCB_[i], byteSize)) {
                    std::memcpy(dst, cbData, byteSize);
                    UnmapBuffer(vertexCB_[i]);
                    vertexCBHash_[i] = hash;
                }
            }
        }
    }

    // 3. 绑定渲染状态（4 态光栅化表 + 6+1 混合状态 + 模板状态）
    const auto f = batch.renderState.f;
    SetRasterizerState(rasterizerStates_[scissorStateIndex_ + (f.wireframe & 1U)]);

    ID3D11BlendState* blend = f.colorEnable
        ? blendStates_[f.blendMode < BlendMode::Count ? f.blendMode : BlendMode::SrcOver]
        : blendStateNoColor_;
    SetBlendState(blend);

    if (f.stencilMode < StencilMode::Count) {
        SetDepthStencilState(stencilStates_[f.stencilMode], batch.stencilRef);
    }

    // 4. 按需绑定非空纹理与采样器（避免每批次遍历空槽位）
    if (batch.pattern != nullptr) {
        SetTexture(0, static_cast<D3D11Texture*>(batch.pattern)->GetNativeSRV());
        SetSampler(0, samplers_[StateCache::ClampSamplerIndex(batch.patternSampler.v)]);
    }
    if (batch.ramps != nullptr) {
        SetTexture(1, static_cast<D3D11Texture*>(batch.ramps)->GetNativeSRV());
        SetSampler(1, samplers_[StateCache::ClampSamplerIndex(batch.rampsSampler.v)]);
    }
    if (batch.image != nullptr) {
        SetTexture(2, static_cast<D3D11Texture*>(batch.image)->GetNativeSRV());
        SetSampler(2, samplers_[StateCache::ClampSamplerIndex(batch.imageSampler.v)]);
    }
    if (batch.glyphs != nullptr) {
        SetTexture(3, static_cast<D3D11Texture*>(batch.glyphs)->GetNativeSRV());
        SetSampler(3, samplers_[StateCache::ClampSamplerIndex(batch.glyphsSampler.v)]);
    }
    if (batch.shadow != nullptr) {
        SetTexture(4, static_cast<D3D11Texture*>(batch.shadow)->GetNativeSRV());
        SetSampler(4, samplers_[StateCache::ClampSamplerIndex(batch.shadowSampler.v)]);
    }

    context_->DrawIndexed(batch.numIndices, batch.startIndex, 0);
}

Base::Result<Base::Ref<Aero::RenderDevice>> D3D11::CreateDevice(
    const ::Aero::Render::D3D11::DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();

    D3D11RenderDeviceOptions nativeOptions;
    nativeOptions.borrowedDevice = reinterpret_cast<ID3D11Device*>(options.device);
    nativeOptions.borrowedContext = reinterpret_cast<ID3D11DeviceContext*>(options.immediateContext);
    nativeOptions.useWarp = options.useWarp;
    nativeOptions.allowWarpFallback = options.allowWarpFallback;
    nativeOptions.enableDebugLayer = options.enableDebugLayer;

    Base::Ref<D3D11RenderDevice> device =
        Base::MakeRefWithAllocator<D3D11RenderDevice>(
            selected, nativeOptions, &selected);
    if (!device) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");

    Base::Result<void> init = device->Initialize();
    if (!init) return init.GetStatus();

    return Base::Ref<Aero::RenderDevice>(std::move(device));
}

Base::Result<Base::Ref<Aero::RenderTarget>> D3D11::CreateTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11::TargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    if (!device) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Device is null");
    }

    if (options.acquireTarget != nullptr) {
        D3D11::EmbeddedTarget targetInfo{};
        Base::Status st = options.acquireTarget(options.callbackContext, &targetInfo);
        if (st.IsOk() && targetInfo.width > 0 && targetInfo.height > 0) {
            auto* d3dDev = static_cast<D3D11RenderDevice*>(device.Get());
            static_cast<void>(d3dDev);
            ID3D11RenderTargetView* rtv = reinterpret_cast<ID3D11RenderTargetView*>(targetInfo.renderTargetView);
            ID3D11DepthStencilView* dsv = reinterpret_cast<ID3D11DepthStencilView*>(targetInfo.depthStencilView);
            ID3D11Texture2D* tex = reinterpret_cast<ID3D11Texture2D*>(targetInfo.texture2D);

            Ref<D3D11Texture> d3dTex = Base::MakeRefWithAllocator<D3D11Texture>(
                selected, tex, nullptr, targetInfo.width, targetInfo.height, false, true);

            Ref<RenderDevice> devCopy = device;
            return Base::Ref<Aero::RenderTarget>(
                Base::MakeRefWithAllocator<D3D11RenderTarget>(
                    selected, std::move(devCopy), std::move(d3dTex), rtv, dsv,
                    targetInfo.width, targetInfo.height));
        }
    }

    const uint32_t fallbackWidth = options.width > 0 ? options.width : 800U;
    const uint32_t fallbackHeight = options.height > 0 ? options.height : 600U;
    return device->CreateRenderTarget("DefaultTarget", fallbackWidth, fallbackHeight, 1, false);
}

} // namespace Aero::Render
