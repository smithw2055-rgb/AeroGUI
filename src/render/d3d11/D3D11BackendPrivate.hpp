#pragma once
#include "render/d3d11/D3D11Backend.hpp"

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
#include <d3dcompiler.h>
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

namespace Aero::Graphics {
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

Base::Result<void> ValidateDxbcReflection(
    const NativeShaderProgram& shader,
    D3D11_SHADER_VERSION_TYPE expectedType,
    const VertexLayoutDescriptor* vertexLayout) noexcept {
    ID3D11ShaderReflection* reflection = nullptr;
    const HRESULT reflectionResult = D3DReflect(
        shader.bytecode,
        shader.bytecodeSize,
        __uuidof(ID3D11ShaderReflection),
        reinterpret_cast<void**>(&reflection));
    if (FAILED(reflectionResult)) {
        return InvalidArgument("D3D11 shader package is not valid DXBC");
    }

    D3D11_SHADER_DESC shaderDescriptor{};
    const HRESULT descriptionResult = reflection->GetDesc(&shaderDescriptor);
    if (FAILED(descriptionResult) ||
        static_cast<D3D11_SHADER_VERSION_TYPE>(
            D3D11_SHVER_GET_TYPE(shaderDescriptor.Version)) != expectedType) {
        ReleaseCom(reflection);
        return InvalidArgument("DXBC package has the wrong shader stage");
    }

    if (vertexLayout != nullptr) {
        std::uint32_t vertexAttributeParameterCount = 0U;
        for (UINT parameterIndex = 0U;
             parameterIndex < shaderDescriptor.InputParameters;
             ++parameterIndex) {
            D3D11_SIGNATURE_PARAMETER_DESC parameter{};
            if (FAILED(reflection->GetInputParameterDesc(
                    parameterIndex, &parameter))) {
                ReleaseCom(reflection);
                return InvalidArgument("DXBC vertex input reflection failed");
            }
            if (parameter.SystemValueType == D3D_NAME_UNDEFINED) {
                ++vertexAttributeParameterCount;
            }
        }
        if (vertexAttributeParameterCount != vertexLayout->attributeCount) {
            ReleaseCom(reflection);
            return InvalidArgument(
                "DXBC vertex input signature does not match the vertex layout");
        }

        for (std::uint32_t attributeIndex = 0U;
             attributeIndex < vertexLayout->attributeCount;
             ++attributeIndex) {
            const VertexAttribute& attribute =
                vertexLayout->attributes[attributeIndex];
            bool found = false;
            for (UINT parameterIndex = 0U;
                 parameterIndex < shaderDescriptor.InputParameters;
                 ++parameterIndex) {
                D3D11_SIGNATURE_PARAMETER_DESC parameter{};
                if (FAILED(reflection->GetInputParameterDesc(
                        parameterIndex, &parameter))) {
                    ReleaseCom(reflection);
                    return InvalidArgument("DXBC vertex input reflection failed");
                }
                if (parameter.SemanticName != nullptr &&
                    std::strcmp(parameter.SemanticName, "ATTR") == 0 &&
                    parameter.SemanticIndex == attribute.location) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ReleaseCom(reflection);
                return InvalidArgument(
                    "DXBC vertex input semantic does not match the vertex layout");
            }
        }
    }

    ReleaseCom(reflection);
    return {};
}

Base::Result<void> CollectDxbcConstantBufferRequirements(
    const NativeShaderProgram& shader,
    std::uint32_t (&minimumSizes)[
        D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]) noexcept {
    ID3D11ShaderReflection* reflection = nullptr;
    const HRESULT reflectionResult = D3DReflect(
        shader.bytecode,
        shader.bytecodeSize,
        __uuidof(ID3D11ShaderReflection),
        reinterpret_cast<void**>(&reflection));
    if (FAILED(reflectionResult)) {
        return InvalidArgument("D3D11 shader package is not valid DXBC");
    }

    D3D11_SHADER_DESC shaderDescriptor{};
    const HRESULT descriptionResult = reflection->GetDesc(&shaderDescriptor);
    if (FAILED(descriptionResult)) {
        ReleaseCom(reflection);
        return InvalidArgument("DXBC constant-buffer reflection failed");
    }

    for (UINT resourceIndex = 0U;
         resourceIndex < shaderDescriptor.BoundResources;
         ++resourceIndex) {
        D3D11_SHADER_INPUT_BIND_DESC binding{};
        if (FAILED(reflection->GetResourceBindingDesc(
                resourceIndex, &binding))) {
            ReleaseCom(reflection);
            return InvalidArgument("DXBC resource binding reflection failed");
        }
        if (binding.Type != D3D_SIT_CBUFFER) {
            continue;
        }
        if (binding.Name == nullptr || binding.BindCount == 0U ||
            binding.BindPoint >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT ||
            binding.BindCount >
                D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT -
                    binding.BindPoint) {
            ReleaseCom(reflection);
            return InvalidArgument("DXBC constant-buffer binding is invalid");
        }

        ID3D11ShaderReflectionConstantBuffer* constantBuffer =
            reflection->GetConstantBufferByName(binding.Name);
        D3D11_SHADER_BUFFER_DESC constantBufferDescriptor{};
        if (constantBuffer == nullptr ||
            FAILED(constantBuffer->GetDesc(&constantBufferDescriptor)) ||
            constantBufferDescriptor.Size == 0U ||
            constantBufferDescriptor.Size % 16U != 0U ||
            constantBufferDescriptor.Size >
                D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16U) {
            ReleaseCom(reflection);
            return InvalidArgument("DXBC constant-buffer layout is invalid");
        }

        for (UINT bindingIndex = 0U;
             bindingIndex < binding.BindCount;
             ++bindingIndex) {
            const UINT slot = binding.BindPoint + bindingIndex;
            if (minimumSizes[slot] < constantBufferDescriptor.Size) {
                minimumSizes[slot] = constantBufferDescriptor.Size;
            }
        }
    }

    ReleaseCom(reflection);
    return {};
}

Base::Result<void> CollectDxbcTextureSamplerRequirements(
    const NativeShaderProgram& shader,
    bool allowTextureSamplers,
    bool (&textureSlots)[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT],
    D3D_SRV_DIMENSION (&textureDimensions)[
        D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT],
    bool (&samplerSlots)[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]) noexcept {
    ID3D11ShaderReflection* reflection = nullptr;
    const HRESULT reflectionResult = D3DReflect(
        shader.bytecode,
        shader.bytecodeSize,
        __uuidof(ID3D11ShaderReflection),
        reinterpret_cast<void**>(&reflection));
    if (FAILED(reflectionResult)) {
        return InvalidArgument("D3D11 shader package is not valid DXBC");
    }

    D3D11_SHADER_DESC shaderDescriptor{};
    const HRESULT descriptionResult = reflection->GetDesc(&shaderDescriptor);
    if (FAILED(descriptionResult)) {
        ReleaseCom(reflection);
        return InvalidArgument("DXBC resource binding reflection failed");
    }

    for (UINT resourceIndex = 0U;
         resourceIndex < shaderDescriptor.BoundResources;
         ++resourceIndex) {
        D3D11_SHADER_INPUT_BIND_DESC binding{};
        if (FAILED(reflection->GetResourceBindingDesc(
                resourceIndex, &binding))) {
            ReleaseCom(reflection);
            return InvalidArgument("DXBC resource binding reflection failed");
        }
        if (binding.Type == D3D_SIT_CBUFFER) {
            continue;
        }
        if (!allowTextureSamplers) {
            ReleaseCom(reflection);
            return Unsupported(
                "D3D11 vertex shaders cannot bind texture or sampler resources");
        }

        bool* slots = nullptr;
        UINT slotCount = 0U;
        if (binding.Type == D3D_SIT_TEXTURE &&
            (binding.Dimension == D3D_SRV_DIMENSION_TEXTURE2D ||
             binding.Dimension == D3D_SRV_DIMENSION_TEXTURE2DARRAY)) {
            slots = textureSlots;
            // BindTextureSampler() binds the SRV and sampler at one shared
            // AeroRHI slot, so texture bindings must also fit the sampler
            // stage's smaller slot range.
            slotCount = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
        } else if (binding.Type == D3D_SIT_SAMPLER) {
            slots = samplerSlots;
            slotCount = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
        } else {
            ReleaseCom(reflection);
            return Unsupported(
                "D3D11 pipeline contains an unsupported shader resource binding");
        }
        if (binding.BindCount == 0U || binding.BindPoint >= slotCount ||
            binding.BindCount > slotCount - binding.BindPoint) {
            ReleaseCom(reflection);
            return InvalidArgument("DXBC texture or sampler binding is invalid");
        }
        for (UINT bindingIndex = 0U;
             bindingIndex < binding.BindCount;
             ++bindingIndex) {
            const UINT slot = binding.BindPoint + bindingIndex;
            if (binding.Type == D3D_SIT_TEXTURE) {
                if (textureDimensions[slot] != D3D_SRV_DIMENSION_UNKNOWN &&
                    textureDimensions[slot] != binding.Dimension) {
                    ReleaseCom(reflection);
                    return InvalidArgument(
                        "DXBC texture binding has conflicting SRV dimensions");
                }
                textureDimensions[slot] = binding.Dimension;
            }
            slots[slot] = true;
        }
    }

    ReleaseCom(reflection);
    return {};
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
    case BlendFactor::DestinationColor:
        return D3D11_BLEND_DEST_COLOR;
    case BlendFactor::OneMinusSourceColor:
        return D3D11_BLEND_INV_SRC_COLOR;
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

Base::Result<D3D11_RECT> ToD3DRect(Base::Rect rect) noexcept {
    if (!Base::IsValidRect(rect)) {
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

struct D3D11CommandQueue::Impl  {
    struct ResourceRecord  {
        ResourceHandle handle;
        Base::IAllocator* allocator = nullptr;
        ResourceDescriptor baseDescriptor;
        TextureResourceDescriptor textureDescriptor;
        VertexLayoutDescriptor vertexLayout;
        D3D11_PRIMITIVE_TOPOLOGY topology =
            D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        std::uint32_t uniformBufferMinimumSizes[
            D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
        bool pixelTextureSlots[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        D3D_SRV_DIMENSION pixelTextureDimensions[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        bool pixelSamplerSlots[
            D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]{};

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
        std::uint8_t* uniformShadow = nullptr;
        std::uint32_t uniformShadowSize = 0U;

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
            std::memset(
                uniformBufferMinimumSizes, 0, sizeof(uniformBufferMinimumSizes));
            std::memset(pixelTextureSlots, 0, sizeof(pixelTextureSlots));
            std::memset(
                pixelTextureDimensions, 0, sizeof(pixelTextureDimensions));
            std::memset(pixelSamplerSlots, 0, sizeof(pixelSamplerSlots));
        }

        void ReleaseAll() noexcept {
            ReleaseUniformShadow();
            ReleasePipelineObjects();
            ReleaseCom(sampler);
            ReleaseTextureObjects();
            ReleaseCom(buffer);
            configured = false;
            external = false;
        }

        void ReleaseUniformShadow() noexcept {
            if (uniformShadow != nullptr && allocator != nullptr) {
                allocator->Deallocate(
                    uniformShadow,
                    uniformShadowSize,
                    alignof(std::uint8_t),
                    Base::MemoryTag::Render);
            }
            uniformShadow = nullptr;
            uniformShadowSize = 0U;
        }

    private:
        void MoveFrom(ResourceRecord& other) noexcept {
            handle = other.handle;
            allocator = other.allocator;
            baseDescriptor = other.baseDescriptor;
            textureDescriptor = other.textureDescriptor;
            vertexLayout = other.vertexLayout;
            topology = other.topology;
            std::memcpy(
                uniformBufferMinimumSizes,
                other.uniformBufferMinimumSizes,
                sizeof(uniformBufferMinimumSizes));
            std::memcpy(pixelTextureSlots, other.pixelTextureSlots,
                sizeof(pixelTextureSlots));
            std::memcpy(
                pixelTextureDimensions,
                other.pixelTextureDimensions,
                sizeof(pixelTextureDimensions));
            std::memcpy(pixelSamplerSlots, other.pixelSamplerSlots,
                sizeof(pixelSamplerSlots));
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
            uniformShadow = other.uniformShadow;
            uniformShadowSize = other.uniformShadowSize;
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
            other.uniformShadow = nullptr;
            other.uniformShadowSize = 0U;
            other.allocator = nullptr;
            std::memset(
                other.uniformBufferMinimumSizes,
                0,
                sizeof(other.uniformBufferMinimumSizes));
            std::memset(other.pixelTextureSlots, 0, sizeof(other.pixelTextureSlots));
            std::memset(
                other.pixelTextureDimensions,
                0,
                sizeof(other.pixelTextureDimensions));
            std::memset(other.pixelSamplerSlots, 0, sizeof(other.pixelSamplerSlots));
            other.configured = false;
            other.external = false;
        }
    };

    struct PendingFence  {
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

    struct SubmissionStateCache  {
        ResourceHandle pipeline;
        ID3D11Buffer* vertexBuffers[
            D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
        UINT vertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
        UINT vertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11Buffer* indexBuffer = nullptr;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        UINT indexOffset = 0U;
        ID3D11Buffer* vertexConstantBuffers[
            D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
        ID3D11Buffer* pixelConstantBuffers[
            D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
        std::uint32_t uniformBufferSizes[
            D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
        ID3D11ShaderResourceView* pixelShaderResources[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        D3D_SRV_DIMENSION pixelShaderResourceDimensions[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11SamplerState* pixelSamplers[
            D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]{};
        D3D11_VIEWPORT viewport{};
        bool hasViewport = false;
        D3D11_RECT scissor{};
        bool hasScissor = false;

        void Reset() noexcept {
            pipeline = {};
            std::memset(vertexBuffers, 0, sizeof(vertexBuffers));
            std::memset(vertexStrides, 0, sizeof(vertexStrides));
            std::memset(vertexOffsets, 0, sizeof(vertexOffsets));
            indexBuffer = nullptr;
            indexFormat = DXGI_FORMAT_UNKNOWN;
            indexOffset = 0U;
            std::memset(
                vertexConstantBuffers, 0, sizeof(vertexConstantBuffers));
            std::memset(
                pixelConstantBuffers, 0, sizeof(pixelConstantBuffers));
            std::memset(uniformBufferSizes, 0, sizeof(uniformBufferSizes));
            std::memset(
                pixelShaderResources, 0, sizeof(pixelShaderResources));
            std::memset(
                pixelShaderResourceDimensions,
                0,
                sizeof(pixelShaderResourceDimensions));
            std::memset(pixelSamplers, 0, sizeof(pixelSamplers));
            viewport = {};
            hasViewport = false;
            scissor = {};
            hasScissor = false;
        }

        void ResetPixelShaderResources() noexcept {
            std::memset(
                pixelShaderResources, 0, sizeof(pixelShaderResources));
            std::memset(
                pixelShaderResourceDimensions,
                0,
                sizeof(pixelShaderResourceDimensions));
        }
    };

    // The intentionally bounded state set changed by Submit(). It is
    // used only by the explicit PreserveRequiredState integration contract;
    // HostResetsState avoids the Get* calls entirely.
    struct RequiredStateSnapshot  {
        ID3D11InputLayout* inputLayout = nullptr;
        ID3D11Buffer* vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
        UINT vertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
        UINT vertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11Buffer* indexBuffer = nullptr;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        UINT indexOffset = 0U;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

        ID3D11VertexShader* vertexShader = nullptr;
        ID3D11ClassInstance* vertexClassInstances[D3D11_SHADER_MAX_INTERFACES]{};
        UINT vertexClassInstanceCount = 0U;
        ID3D11PixelShader* pixelShader = nullptr;
        ID3D11ClassInstance* pixelClassInstances[D3D11_SHADER_MAX_INTERFACES]{};
        UINT pixelClassInstanceCount = 0U;
        ID3D11Buffer* vertexConstantBuffers[
            D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
        ID3D11Buffer* pixelConstantBuffers[
            D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
        ID3D11ShaderResourceView* vertexShaderResources[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11ShaderResourceView* pixelShaderResources[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11ShaderResourceView* geometryShaderResources[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11ShaderResourceView* hullShaderResources[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11ShaderResourceView* domainShaderResources[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11ShaderResourceView* computeShaderResources[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        ID3D11SamplerState* pixelSamplers[
            D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]{};

        ID3D11RasterizerState* rasterizerState = nullptr;
        D3D11_VIEWPORT viewports[
            D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        UINT viewportCount = 0U;
        D3D11_RECT scissors[
            D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        UINT scissorCount = 0U;
        ID3D11BlendState* blendState = nullptr;
        FLOAT blendFactor[4]{};
        UINT sampleMask = UINT_MAX;
        ID3D11DepthStencilState* depthStencilState = nullptr;
        UINT stencilReference = 0U;
        ID3D11RenderTargetView* renderTargets[
            D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
        ID3D11DepthStencilView* depthStencilView = nullptr;
        bool captured = false;

        RequiredStateSnapshot() noexcept = default;
        RequiredStateSnapshot(const RequiredStateSnapshot&) = delete;
        RequiredStateSnapshot& operator=(const RequiredStateSnapshot&) = delete;

        ~RequiredStateSnapshot() noexcept {
            Release();
        }

        void Capture(ID3D11DeviceContext* context) noexcept {
            if (context == nullptr || captured) {
                return;
            }

            context->IAGetInputLayout(&inputLayout);
            context->IAGetVertexBuffers(
                0U,
                D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
                vertexBuffers,
                vertexStrides,
                vertexOffsets);
            context->IAGetIndexBuffer(&indexBuffer, &indexFormat, &indexOffset);
            context->IAGetPrimitiveTopology(&topology);

            vertexClassInstanceCount = D3D11_SHADER_MAX_INTERFACES;
            context->VSGetShader(
                &vertexShader, vertexClassInstances, &vertexClassInstanceCount);
            pixelClassInstanceCount = D3D11_SHADER_MAX_INTERFACES;
            context->PSGetShader(
                &pixelShader, pixelClassInstances, &pixelClassInstanceCount);
            context->VSGetConstantBuffers(
                0U,
                D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                vertexConstantBuffers);
            context->PSGetConstantBuffers(
                0U,
                D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                pixelConstantBuffers);
            context->VSGetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                vertexShaderResources);
            context->PSGetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                pixelShaderResources);
            context->GSGetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                geometryShaderResources);
            context->HSGetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                hullShaderResources);
            context->DSGetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                domainShaderResources);
            context->CSGetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                computeShaderResources);
            context->PSGetSamplers(
                0U,
                D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,
                pixelSamplers);

            context->RSGetState(&rasterizerState);
            viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            context->RSGetViewports(&viewportCount, viewports);
            scissorCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            context->RSGetScissorRects(&scissorCount, scissors);
            context->OMGetBlendState(&blendState, blendFactor, &sampleMask);
            context->OMGetDepthStencilState(&depthStencilState, &stencilReference);
            context->OMGetRenderTargets(
                D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                renderTargets,
                &depthStencilView);
            captured = true;
        }

        void Restore(ID3D11DeviceContext* context) noexcept {
            if (!captured || context == nullptr) {
                return;
            }

            context->IASetInputLayout(inputLayout);
            context->IASetVertexBuffers(
                0U,
                D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
                vertexBuffers,
                vertexStrides,
                vertexOffsets);
            context->IASetIndexBuffer(indexBuffer, indexFormat, indexOffset);
            context->IASetPrimitiveTopology(topology);
            context->VSSetShader(
                vertexShader, vertexClassInstances, vertexClassInstanceCount);
            context->PSSetShader(
                pixelShader, pixelClassInstances, pixelClassInstanceCount);
            context->VSSetConstantBuffers(
                0U,
                D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                vertexConstantBuffers);
            context->PSSetConstantBuffers(
                0U,
                D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                pixelConstantBuffers);
            context->VSSetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                vertexShaderResources);
            context->PSSetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                pixelShaderResources);
            context->GSSetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                geometryShaderResources);
            context->HSSetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                hullShaderResources);
            context->DSSetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                domainShaderResources);
            context->CSSetShaderResources(
                0U,
                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                computeShaderResources);
            context->PSSetSamplers(
                0U,
                D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,
                pixelSamplers);
            context->RSSetState(rasterizerState);
            context->RSSetViewports(viewportCount, viewports);
            context->RSSetScissorRects(scissorCount, scissors);
            context->OMSetBlendState(blendState, blendFactor, sampleMask);
            context->OMSetDepthStencilState(depthStencilState, stencilReference);
            context->OMSetRenderTargets(
                D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                renderTargets,
                depthStencilView);
            Release();
        }

    private:
        template<class T, std::size_t Count>
        static void ReleaseArray(T* (&values)[Count]) noexcept {
            for (T*& value : values) {
                ReleaseCom(value);
            }
        }

        void Release() noexcept {
            ReleaseCom(inputLayout);
            ReleaseArray(vertexBuffers);
            ReleaseCom(indexBuffer);
            ReleaseCom(vertexShader);
            ReleaseArray(vertexClassInstances);
            ReleaseCom(pixelShader);
            ReleaseArray(pixelClassInstances);
            ReleaseArray(vertexConstantBuffers);
            ReleaseArray(pixelConstantBuffers);
            ReleaseArray(vertexShaderResources);
            ReleaseArray(pixelShaderResources);
            ReleaseArray(geometryShaderResources);
            ReleaseArray(hullShaderResources);
            ReleaseArray(domainShaderResources);
            ReleaseArray(computeShaderResources);
            ReleaseArray(pixelSamplers);
            ReleaseCom(rasterizerState);
            ReleaseCom(blendState);
            ReleaseCom(depthStencilState);
            ReleaseArray(renderTargets);
            ReleaseCom(depthStencilView);
            captured = false;
        }
    };

    explicit Impl(Base::IAllocator* allocatorValue) noexcept
        : allocator(allocatorValue != nullptr
              ? allocatorValue
              : &Base::GetDefaultAllocator()),
          resources(allocator),
          pendingFences(allocator) {}

    ~Impl() noexcept {
        Reset();
    }

    Base::IAllocator* allocator = nullptr;
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
    bool supportsTimestampQueries = false;
    bool inRenderPass = false;
    SubmissionStateCache submissionState;
    // D3D11.0 has no *SetConstantBuffers1 range binding. Reusable per-slot
    // scratch buffers preserve AeroRHI's offset contract on FL10_0 by copying
    // the requested aligned byte range before each bind.
    ID3D11Buffer* uniformOffsetBuffers[
        D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
    std::uint32_t uniformOffsetBufferSizes[
        D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
    ID3D11Texture2D* activeColorTextures[MaxColorAttachments]{};
    ID3D11Texture2D* activeDepthStencilTexture = nullptr;

    void ClearActiveAttachments() noexcept {
        for (ID3D11Texture2D*& texture : activeColorTextures) {
            texture = nullptr;
        }
        activeDepthStencilTexture = nullptr;
    }

    bool IsActiveAttachment(const ID3D11Texture2D* texture) const noexcept {
        if (texture == nullptr) {
            return false;
        }
        for (const ID3D11Texture2D* active : activeColorTextures) {
            if (active == texture) {
                return true;
            }
        }
        return activeDepthStencilTexture == texture;
    }

    void ClearShaderResourceBindings() noexcept {
        ID3D11ShaderResourceView* nullViews[
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        context->VSSetShaderResources(
            0U, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
        context->PSSetShaderResources(
            0U, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
        context->GSSetShaderResources(
            0U, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
        context->HSSetShaderResources(
            0U, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
        context->DSSetShaderResources(
            0U, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
        context->CSSetShaderResources(
            0U, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
    }

    void ReleaseUniformOffsetBuffers() noexcept {
        for (ID3D11Buffer*& buffer : uniformOffsetBuffers) {
            ReleaseCom(buffer);
        }
        std::memset(
            uniformOffsetBufferSizes, 0, sizeof(uniformOffsetBufferSizes));
    }

    void Reset() noexcept {
        if (context != nullptr) {
            ClearShaderResourceBindings();
            context->OMSetRenderTargets(0U, nullptr, nullptr);
            context->ClearState();
            context->Flush();
        }
        pendingFences.Clear();
        resources.Clear();
        ReleaseUniformOffsetBuffers();
        ReleaseCom(context);
        ReleaseCom(device);
        currentPipeline = {};
        lastSubmittedFence = 0U;
        completedFence = 0U;
        deviceLost = false;
        initialized = false;
        supportsTimestampQueries = false;
        inRenderPass = false;
        submissionState.Reset();
        ClearActiveAttachments();
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

} // namespace Aero::Graphics
