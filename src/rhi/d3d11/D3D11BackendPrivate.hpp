#pragma once
#include <Aero/Rhi/D3D11Backend.hpp>

#if !defined(_WIN32)
#error "Aero D3D11 backend is only available on Windows"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

// winspool.h exposes DeviceCapabilities as an ANSI/Unicode selection macro.
// AeroRHI uses DeviceCapabilities as a C++ type, so keep the Windows macro
// from rewriting backend method definitions that appear after windows.h.
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
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

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfRange, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

Base::Status DeviceFailure(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InternalError, message);
}

template<class T>
void ReleaseCom(T*& value) noexcept {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

bool IsDeviceRemoval(HRESULT result) noexcept {
    return result == DXGI_ERROR_DEVICE_HUNG ||
        result == DXGI_ERROR_DEVICE_REMOVED ||
        result == DXGI_ERROR_DEVICE_RESET ||
        result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

Base::Status StatusFromHResult(
    HRESULT result,
    const char* failureMessage,
    const char* lostMessage) noexcept {
    return IsDeviceRemoval(result)
        ? InvalidState(lostMessage)
        : DeviceFailure(failureMessage);
}

std::uint32_t BytesPerPixel(GraphicsTextureFormat format) noexcept {
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

DXGI_FORMAT ToDxgiFormat(GraphicsTextureFormat format) noexcept {
    switch (format) {
    case GraphicsTextureFormat::Rgba8Unorm:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case GraphicsTextureFormat::Bgra8Unorm:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case GraphicsTextureFormat::R8Unorm:
        return DXGI_FORMAT_R8_UNORM;
    case GraphicsTextureFormat::Depth24Stencil8:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
    return DXGI_FORMAT_UNKNOWN;
}

TextureFormat ToBaseTextureFormat(GraphicsTextureFormat format) noexcept {
    switch (format) {
    case GraphicsTextureFormat::Rgba8Unorm:
        return TextureFormat::Rgba8Unorm;
    case GraphicsTextureFormat::Bgra8Unorm:
        return TextureFormat::Bgra8Unorm;
    case GraphicsTextureFormat::R8Unorm:
        return TextureFormat::R8Unorm;
    case GraphicsTextureFormat::Depth24Stencil8:
        break;
    }
    return TextureFormat::R8Unorm;
}

DXGI_FORMAT ToDxgiVertexFormat(VertexFormat format) noexcept {
    switch (format) {
    case VertexFormat::Float:
        return DXGI_FORMAT_R32_FLOAT;
    case VertexFormat::Float2:
        return DXGI_FORMAT_R32G32_FLOAT;
    case VertexFormat::Float3:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case VertexFormat::Float4:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case VertexFormat::UByte4Normalized:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case VertexFormat::UShort2:
        return DXGI_FORMAT_R16G16_UINT;
    case VertexFormat::UShort4:
        return DXGI_FORMAT_R16G16B16A16_UINT;
    }
    return DXGI_FORMAT_UNKNOWN;
}

D3D11_PRIMITIVE_TOPOLOGY ToD3DTopology(PrimitiveTopology topology) noexcept {
    switch (topology) {
    case PrimitiveTopology::TriangleList:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveTopology::TriangleStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case PrimitiveTopology::LineList:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case PrimitiveTopology::LineStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    }
    return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

D3D11_BLEND ToD3DBlend(BlendFactor factor) noexcept {
    switch (factor) {
    case BlendFactor::Zero:
        return D3D11_BLEND_ZERO;
    case BlendFactor::One:
        return D3D11_BLEND_ONE;
    case BlendFactor::SourceAlpha:
        return D3D11_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFactor::DestinationAlpha:
        return D3D11_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDestinationAlpha:
        return D3D11_BLEND_INV_DEST_ALPHA;
    }
    return D3D11_BLEND_ZERO;
}

D3D11_BLEND_OP ToD3DBlendOperation(BlendOperation operation) noexcept {
    switch (operation) {
    case BlendOperation::Add:
        return D3D11_BLEND_OP_ADD;
    case BlendOperation::Subtract:
        return D3D11_BLEND_OP_SUBTRACT;
    case BlendOperation::ReverseSubtract:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    case BlendOperation::Minimum:
        return D3D11_BLEND_OP_MIN;
    case BlendOperation::Maximum:
        return D3D11_BLEND_OP_MAX;
    }
    return D3D11_BLEND_OP_ADD;
}

D3D11_CULL_MODE ToD3DCullMode(CullMode mode) noexcept {
    switch (mode) {
    case CullMode::None:
        return D3D11_CULL_NONE;
    case CullMode::Front:
        return D3D11_CULL_FRONT;
    case CullMode::Back:
        return D3D11_CULL_BACK;
    }
    return D3D11_CULL_NONE;
}

D3D11_FILL_MODE ToD3DFillMode(FillMode mode) noexcept {
    return mode == FillMode::Wireframe
        ? D3D11_FILL_WIREFRAME
        : D3D11_FILL_SOLID;
}

D3D11_COMPARISON_FUNC ToD3DCompare(CompareOperation operation) noexcept {
    switch (operation) {
    case CompareOperation::Never:
        return D3D11_COMPARISON_NEVER;
    case CompareOperation::Less:
        return D3D11_COMPARISON_LESS;
    case CompareOperation::Equal:
        return D3D11_COMPARISON_EQUAL;
    case CompareOperation::LessEqual:
        return D3D11_COMPARISON_LESS_EQUAL;
    case CompareOperation::Greater:
        return D3D11_COMPARISON_GREATER;
    case CompareOperation::NotEqual:
        return D3D11_COMPARISON_NOT_EQUAL;
    case CompareOperation::GreaterEqual:
        return D3D11_COMPARISON_GREATER_EQUAL;
    case CompareOperation::Always:
        return D3D11_COMPARISON_ALWAYS;
    }
    return D3D11_COMPARISON_ALWAYS;
}

D3D11_STENCIL_OP ToD3DStencilOperation(StencilOperation operation) noexcept {
    switch (operation) {
    case StencilOperation::Keep:
        return D3D11_STENCIL_OP_KEEP;
    case StencilOperation::Zero:
        return D3D11_STENCIL_OP_ZERO;
    case StencilOperation::Replace:
        return D3D11_STENCIL_OP_REPLACE;
    case StencilOperation::IncrementClamp:
        return D3D11_STENCIL_OP_INCR_SAT;
    case StencilOperation::DecrementClamp:
        return D3D11_STENCIL_OP_DECR_SAT;
    case StencilOperation::Invert:
        return D3D11_STENCIL_OP_INVERT;
    }
    return D3D11_STENCIL_OP_KEEP;
}

D3D11_TEXTURE_ADDRESS_MODE ToD3DAddress(AddressMode mode) noexcept {
    switch (mode) {
    case AddressMode::ClampToEdge:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    case AddressMode::Repeat:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    case AddressMode::MirrorRepeat:
        return D3D11_TEXTURE_ADDRESS_MIRROR;
    }
    return D3D11_TEXTURE_ADDRESS_CLAMP;
}

D3D11_FILTER ToD3DFilter(const SamplerDescriptor& descriptor) noexcept {
    if (descriptor.maxAnisotropy > 1U) {
        return D3D11_FILTER_ANISOTROPIC;
    }

    const bool minLinear = descriptor.minFilter == FilterMode::Linear;
    const bool magLinear = descriptor.magFilter == FilterMode::Linear;
    const bool mipLinear = descriptor.mipFilter == FilterMode::Linear;
    const std::uint32_t bits =
        (minLinear ? 4U : 0U) |
        (magLinear ? 2U : 0U) |
        (mipLinear ? 1U : 0U);

    switch (bits) {
    case 0U:
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case 1U:
        return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    case 2U:
        return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    case 3U:
        return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    case 4U:
        return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    case 5U:
        return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    case 6U:
        return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    case 7U:
        return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    }
    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

bool CheckedUint(std::uint64_t value, UINT& output) noexcept {
    if (value > static_cast<std::uint64_t>(UINT_MAX)) {
        return false;
    }
    output = static_cast<UINT>(value);
    return true;
}

bool CheckedLong(double value, LONG& output) noexcept {
    if (!std::isfinite(value) ||
        value < static_cast<double>(LONG_MIN) ||
        value > static_cast<double>(LONG_MAX)) {
        return false;
    }
    output = static_cast<LONG>(value);
    return true;
}

Base::Result<D3D11_RECT> ToD3DRect(Core::Rect rect) noexcept {
    if (!Core::IsValidLayoutRect(rect)) {
        return InvalidArgument("D3D11 rectangle is invalid");
    }

    const double rightValue = rect.x + rect.width;
    const double bottomValue = rect.y + rect.height;
    D3D11_RECT result{};
    if (!CheckedLong(rect.x, result.left) ||
        !CheckedLong(rect.y, result.top) ||
        !CheckedLong(rightValue, result.right) ||
        !CheckedLong(bottomValue, result.bottom)) {
        return OutOfRange("D3D11 rectangle exceeds LONG range");
    }
    return result;
}

void HashBytes(
    std::uint64_t& hash,
    const std::uint8_t* bytes,
    std::uint32_t size) noexcept {
    for (std::uint32_t index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= HashPrime;
    }
}

} // namespace

struct D3D11GraphicsBackend::Impl final {
    struct ResourceRecord final {
        ResourceHandle handle;
        ResourceDescriptor baseDescriptor;
        TextureResourceDescriptor textureDescriptor;
        VertexLayoutDescriptor vertexLayout;
        D3D11_PRIMITIVE_TOPOLOGY topology =
            D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

        ID3D11Buffer* buffer = nullptr;
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* shaderResourceView = nullptr;
        ID3D11RenderTargetView* renderTargetView = nullptr;
        ID3D11DepthStencilView* depthStencilView = nullptr;
        ID3D11SamplerState* sampler = nullptr;
        ID3D11VertexShader* vertexShader = nullptr;
        ID3D11PixelShader* pixelShader = nullptr;
        ID3D11InputLayout* inputLayout = nullptr;
        ID3D11BlendState* blendState = nullptr;
        ID3D11RasterizerState* rasterizerState = nullptr;
        ID3D11DepthStencilState* depthStencilState = nullptr;

        bool configured = false;
        bool external = false;

        ResourceRecord() noexcept = default;
        ResourceRecord(const ResourceRecord&) = delete;
        ResourceRecord& operator=(const ResourceRecord&) = delete;

        ResourceRecord(ResourceRecord&& other) noexcept {
            MoveFrom(other);
        }

        ResourceRecord& operator=(ResourceRecord&& other) noexcept {
            if (this != &other) {
                ReleaseAll();
                MoveFrom(other);
            }
            return *this;
        }

        ~ResourceRecord() noexcept {
            ReleaseAll();
        }

        void ReleaseTextureObjects() noexcept {
            ReleaseCom(depthStencilView);
            ReleaseCom(renderTargetView);
            ReleaseCom(shaderResourceView);
            ReleaseCom(texture);
        }

        void ReleasePipelineObjects() noexcept {
            ReleaseCom(depthStencilState);
            ReleaseCom(rasterizerState);
            ReleaseCom(blendState);
            ReleaseCom(inputLayout);
            ReleaseCom(pixelShader);
            ReleaseCom(vertexShader);
        }

        void ReleaseAll() noexcept {
            ReleasePipelineObjects();
            ReleaseCom(sampler);
            ReleaseTextureObjects();
            ReleaseCom(buffer);
            configured = false;
            external = false;
        }

    private:
        void MoveFrom(ResourceRecord& other) noexcept {
            handle = other.handle;
            baseDescriptor = other.baseDescriptor;
            textureDescriptor = other.textureDescriptor;
            vertexLayout = other.vertexLayout;
            topology = other.topology;
            buffer = other.buffer;
            texture = other.texture;
            shaderResourceView = other.shaderResourceView;
            renderTargetView = other.renderTargetView;
            depthStencilView = other.depthStencilView;
            sampler = other.sampler;
            vertexShader = other.vertexShader;
            pixelShader = other.pixelShader;
            inputLayout = other.inputLayout;
            blendState = other.blendState;
            rasterizerState = other.rasterizerState;
            depthStencilState = other.depthStencilState;
            configured = other.configured;
            external = other.external;

            other.buffer = nullptr;
            other.texture = nullptr;
            other.shaderResourceView = nullptr;
            other.renderTargetView = nullptr;
            other.depthStencilView = nullptr;
            other.sampler = nullptr;
            other.vertexShader = nullptr;
            other.pixelShader = nullptr;
            other.inputLayout = nullptr;
            other.blendState = nullptr;
            other.rasterizerState = nullptr;
            other.depthStencilState = nullptr;
            other.configured = false;
            other.external = false;
        }
    };

    struct PendingFence final {
        FenceValue value = 0U;
        ID3D11Query* query = nullptr;

        PendingFence() noexcept = default;
        PendingFence(const PendingFence&) = delete;
        PendingFence& operator=(const PendingFence&) = delete;

        PendingFence(PendingFence&& other) noexcept
            : value(other.value), query(other.query) {
            other.value = 0U;
            other.query = nullptr;
        }

        PendingFence& operator=(PendingFence&& other) noexcept {
            if (this != &other) {
                ReleaseCom(query);
                value = other.value;
                query = other.query;
                other.value = 0U;
                other.query = nullptr;
            }
            return *this;
        }

        ~PendingFence() noexcept {
            ReleaseCom(query);
        }
    };

    explicit Impl(Base::IAllocator* allocator) noexcept
        : resources(allocator), pendingFences(allocator) {}

    ~Impl() noexcept {
        Reset();
    }

    Base::Vector<ResourceRecord> resources;
    mutable Base::Vector<PendingFence> pendingFences;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_10_0;
    ResourceHandle currentPipeline;
    FenceValue lastSubmittedFence = 0U;
    mutable FenceValue completedFence = 0U;
    mutable bool deviceLost = false;
    bool initialized = false;
    bool inRenderPass = false;

    void Reset() noexcept {
        if (context != nullptr) {
            ID3D11ShaderResourceView* nullViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
            context->PSSetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                nullViews);
            context->OMSetRenderTargets(0U, nullptr, nullptr);
            context->ClearState();
            context->Flush();
        }
        pendingFences.Clear();
        resources.Clear();
        ReleaseCom(context);
        ReleaseCom(device);
        currentPipeline = {};
        lastSubmittedFence = 0U;
        completedFence = 0U;
        deviceLost = false;
        initialized = false;
        inRenderPass = false;
    }

    ResourceRecord* Find(ResourceHandle handle) noexcept {
        for (ResourceRecord& record : resources) {
            if (record.handle == handle) {
                return &record;
            }
        }
        return nullptr;
    }

    const ResourceRecord* Find(ResourceHandle handle) const noexcept {
        for (const ResourceRecord& record : resources) {
            if (record.handle == handle) {
                return &record;
            }
        }
        return nullptr;
    }

    std::uint32_t FindIndex(ResourceHandle handle) const noexcept {
        for (std::uint32_t index = 0U; index < resources.Size(); ++index) {
            if (resources[index].handle == handle) {
                return index;
            }
        }
        return UINT32_MAX;
    }

    void RemoveResourceAt(std::uint32_t index) noexcept {
        if (index >= resources.Size()) {
            return;
        }
        for (std::uint32_t current = index + 1U;
             current < resources.Size();
             ++current) {
            resources[current - 1U] = std::move(resources[current]);
        }
        resources.PopBack();
    }

    void RemoveFirstFence() const noexcept {
        if (pendingFences.Empty()) {
            return;
        }
        for (std::uint32_t index = 1U;
             index < pendingFences.Size();
             ++index) {
            pendingFences[index - 1U] = std::move(pendingFences[index]);
        }
        pendingFences.PopBack();
    }

    void PollFences() const noexcept {
        if (!initialized || context == nullptr || device == nullptr || deviceLost) {
            return;
        }

        const HRESULT removed = device->GetDeviceRemovedReason();
        if (FAILED(removed)) {
            deviceLost = true;
            return;
        }

        while (!pendingFences.Empty()) {
            PendingFence& pending = pendingFences[0U];
            const HRESULT result = context->GetData(
                pending.query,
                nullptr,
                0U,
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (result == S_FALSE) {
                break;
            }
            if (FAILED(result)) {
                deviceLost = true;
                break;
            }
            completedFence = pending.value;
            RemoveFirstFence();
        }
    }

    Base::Result<void> VerifyReady() const noexcept {
        if (!initialized || device == nullptr || context == nullptr) {
            return NotInitialized("D3D11 backend is not initialized");
        }
        PollFences();
        return deviceLost
            ? Base::Result<void>(InvalidState("D3D11 device is lost"))
            : Base::Result<void>();
    }
};

} // namespace Aero::Rhi
