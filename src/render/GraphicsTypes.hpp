#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Geometry.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Graphics {

constexpr std::uint32_t RenderResourceAbiVersion = 2U;
using FenceValue = std::uint64_t;

struct DeviceCapabilities  {
    std::uint32_t abiVersion = RenderResourceAbiVersion;
    std::uint32_t maxFramesInFlight = 2U;
    std::uint32_t maxTextureDimension = 16384U;
    bool supportsTimestampQueries = false;
};

enum class ResourceType : std::uint8_t {
    Invalid = 0U,
    Buffer,
    Texture,
    Sampler,
    Pipeline,
    RenderTarget
};

struct ResourceHandle  {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;
    ResourceType type = ResourceType::Invalid;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U &&
            type != ResourceType::Invalid;
    }
};

constexpr bool operator==(
    ResourceHandle left,
    ResourceHandle right) noexcept {
    return left.index == right.index &&
        left.generation == right.generation &&
        left.type == right.type;
}

constexpr bool operator!=(
    ResourceHandle left,
    ResourceHandle right) noexcept {
    return !(left == right);
}

enum class BufferUsage : std::uint8_t {
    Vertex = 0U,
    Index,
    Uniform,
    Upload
};

enum class TextureFormat : std::uint8_t {
    Rgba8Unorm = 0U,
    Bgra8Unorm,
    R8Unorm
};

struct BufferDescriptor  {
    std::uint64_t sizeBytes = 0U;
    BufferUsage usage = BufferUsage::Vertex;
};

struct TextureDescriptor  {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    TextureFormat format = TextureFormat::Rgba8Unorm;
};

struct ResourceDescriptor  {
    ResourceType type = ResourceType::Invalid;
    BufferDescriptor buffer;
    TextureDescriptor texture;
};

enum class NativeRenderBackendKind : std::uint8_t;
struct GraphicsCapabilities;
struct TextureResourceDescriptor;
struct SamplerDescriptor;
struct NativePipelineState;
constexpr std::uint32_t GraphicsAbiVersion = 2U;
constexpr std::uint32_t MaxVertexBuffers = 4U;
constexpr std::uint32_t MaxVertexAttributes = 16U;
constexpr std::uint32_t MaxColorAttachments = 4U;

enum class NativeRenderBackendKind : std::uint8_t {
    Invalid = 0U,
    D3D11,
    OpenGL33
};

enum class GraphicsFeature : std::uint64_t {
    TextureSampling = UINT64_C(1) << 0U,
    RenderTargets = UINT64_C(1) << 1U,
    VertexIndexBuffers = UINT64_C(1) << 2U,
    UniformBuffers = UINT64_C(1) << 3U,
    DepthStencil = UINT64_C(1) << 4U,
    Instancing = UINT64_C(1) << 5U,
    Scissor = UINT64_C(1) << 6U,
    AnisotropicFiltering = UINT64_C(1) << 7U,
    TimestampQueries = UINT64_C(1) << 8U
};

using GraphicsFeatureFlags = std::uint64_t;

constexpr GraphicsFeatureFlags FeatureBit(
    GraphicsFeature feature) noexcept {
    return static_cast<GraphicsFeatureFlags>(feature);
}

constexpr bool HasAllFeatures(
    GraphicsFeatureFlags available,
    GraphicsFeatureFlags required) noexcept {
    return (available & required) == required;
}

enum class ShaderStage : std::uint8_t {
    Vertex = 0U,
    Fragment
};

enum class ShaderLanguage : std::uint8_t {
    Invalid = 0U,
    Dxbc,
    Glsl330
};

using ShaderLanguageFlags = std::uint32_t;

constexpr ShaderLanguageFlags ShaderLanguageBit(
    ShaderLanguage language) noexcept {
    return language == ShaderLanguage::Invalid
        ? 0U
        : (UINT32_C(1) << static_cast<std::uint32_t>(language));
}

struct GraphicsCapabilities  {
    std::uint32_t abiVersion = GraphicsAbiVersion;
    NativeRenderBackendKind backendKind = NativeRenderBackendKind::Invalid;
    GraphicsFeatureFlags features = 0U;
    ShaderLanguageFlags shaderLanguages = 0U;
    std::uint32_t maxColorAttachments = 1U;
    std::uint32_t maxVertexAttributes = 8U;
    std::uint32_t maxSampledTextures = 8U;
    std::uint32_t uniformBufferAlignment = 16U;
};

struct NativeShaderProgram  {
    ShaderStage stage = ShaderStage::Vertex;
    ShaderLanguage language = ShaderLanguage::Invalid;
    const std::uint8_t* bytecode = nullptr;
    std::uint32_t bytecodeSize = 0U;
    Base::StringView entryPoint;
    std::uint64_t stableId = 0U;
};

enum class VertexFormat : std::uint8_t {
    Float = 0U,
    Float2,
    Float3,
    Float4,
    UByte4Normalized,
    UShort2,
    UShort4
};

enum class VertexStepMode : std::uint8_t {
    PerVertex = 0U,
    PerInstance
};

struct VertexBufferLayout  {
    std::uint32_t stride = 0U;
    VertexStepMode stepMode = VertexStepMode::PerVertex;
};

struct VertexAttribute  {
    std::uint8_t location = 0U;
    std::uint8_t bufferSlot = 0U;
    VertexFormat format = VertexFormat::Float2;
    std::uint32_t offset = 0U;
};

struct VertexLayoutDescriptor  {
    std::uint8_t bufferCount = 0U;
    std::uint8_t attributeCount = 0U;
    VertexBufferLayout buffers[MaxVertexBuffers]{};
    VertexAttribute attributes[MaxVertexAttributes]{};
};

enum class PrimitiveTopology : std::uint8_t {
    TriangleList = 0U,
    TriangleStrip,
    LineList,
    LineStrip
};

enum class BlendFactor : std::uint8_t {
    Zero = 0U,
    One,
    SourceAlpha,
    OneMinusSourceAlpha,
    DestinationAlpha,
    OneMinusDestinationAlpha,
    DestinationColor,
    OneMinusSourceColor
};

enum class BlendOperation : std::uint8_t {
    Add = 0U,
    Subtract,
    ReverseSubtract,
    Minimum,
    Maximum
};

struct BlendComponent  {
    BlendFactor source = BlendFactor::One;
    BlendFactor destination = BlendFactor::Zero;
    BlendOperation operation = BlendOperation::Add;
};

struct BlendState  {
    bool enabled = false;
    BlendComponent color;
    BlendComponent alpha;
    std::uint8_t writeMask = 0x0FU;
};

enum class CullMode : std::uint8_t {
    None = 0U,
    Front,
    Back
};

enum class FrontFace : std::uint8_t {
    CounterClockwise = 0U,
    Clockwise
};

enum class FillMode : std::uint8_t {
    Solid = 0U,
    Wireframe
};

struct RasterState  {
    CullMode cullMode = CullMode::None;
    FrontFace frontFace = FrontFace::CounterClockwise;
    FillMode fillMode = FillMode::Solid;
    bool scissorEnabled = true;
};

enum class CompareOperation : std::uint8_t {
    Never = 0U,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum class StencilOperation : std::uint8_t {
    Keep = 0U,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert
};

struct StencilFaceState  {
    CompareOperation compare = CompareOperation::Always;
    StencilOperation fail = StencilOperation::Keep;
    StencilOperation depthFail = StencilOperation::Keep;
    StencilOperation pass = StencilOperation::Keep;
};

struct DepthStencilState  {
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    CompareOperation depthCompare = CompareOperation::Always;
    bool stencilEnabled = false;
    StencilFaceState front;
    StencilFaceState back;
    std::uint8_t stencilReadMask = 0xFFU;
    std::uint8_t stencilWriteMask = 0xFFU;
};

enum class GraphicsTextureFormat : std::uint8_t {
    Rgba8Unorm = 0U,
    Bgra8Unorm,
    R8Unorm,
    Depth24Stencil8
};

struct NativePipelineState  {
    NativeShaderProgram vertexShader;
    NativeShaderProgram fragmentShader;
    VertexLayoutDescriptor vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    BlendState blend;
    RasterState raster;
    DepthStencilState depthStencil;
    GraphicsTextureFormat colorFormat = GraphicsTextureFormat::Rgba8Unorm;
    GraphicsTextureFormat depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    std::uint8_t sampleCount = 1U;
};

enum class TextureUsage : std::uint32_t {
    Sampled = UINT32_C(1) << 0U,
    RenderTarget = UINT32_C(1) << 1U,
    CopySource = UINT32_C(1) << 2U,
    CopyDestination = UINT32_C(1) << 3U
};

using TextureUsageFlags = std::uint32_t;

constexpr TextureUsageFlags TextureUsageBit(
    TextureUsage usage) noexcept {
    return static_cast<TextureUsageFlags>(usage);
}

constexpr bool HasTextureUsage(
    TextureUsageFlags value,
    TextureUsage usage) noexcept {
    return (value & TextureUsageBit(usage)) != 0U;
}

struct TextureResourceDescriptor  {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint16_t mipLevels = 1U;
    std::uint16_t arrayLayers = 1U;
    std::uint8_t sampleCount = 1U;
    GraphicsTextureFormat format = GraphicsTextureFormat::Rgba8Unorm;
    TextureUsageFlags usage = TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination);
};

enum class FilterMode : std::uint8_t {
    Nearest = 0U,
    Linear
};

enum class AddressMode : std::uint8_t {
    ClampToEdge = 0U,
    Repeat,
    MirrorRepeat
};

struct SamplerDescriptor  {
    FilterMode minFilter = FilterMode::Linear;
    FilterMode magFilter = FilterMode::Linear;
    FilterMode mipFilter = FilterMode::Linear;
    AddressMode addressU = AddressMode::ClampToEdge;
    AddressMode addressV = AddressMode::ClampToEdge;
    AddressMode addressW = AddressMode::ClampToEdge;
    float minLod = 0.0F;
    float maxLod = 32.0F;
    std::uint8_t maxAnisotropy = 1U;
};

enum class LoadOperation : std::uint8_t {
    Load = 0U,
    Clear,
    DontCare
};

enum class StoreOperation : std::uint8_t {
    Store = 0U,
    DontCare
};

struct ColorAttachmentDescriptor  {
    ResourceHandle target;
    LoadOperation load = LoadOperation::Load;
    StoreOperation store = StoreOperation::Store;
    Base::Color clearColor;
};

struct DepthStencilAttachmentDescriptor  {
    ResourceHandle target;
    LoadOperation depthLoad = LoadOperation::Load;
    StoreOperation depthStore = StoreOperation::Store;
    LoadOperation stencilLoad = LoadOperation::Load;
    StoreOperation stencilStore = StoreOperation::Store;
    float clearDepth = 1.0F;
    std::uint32_t clearStencil = 0U;
};

struct RenderPassDescriptor  {
    Base::Rect renderArea;
    std::uint8_t colorAttachmentCount = 0U;
    ColorAttachmentDescriptor colorAttachments[MaxColorAttachments]{};
    bool hasDepthStencil = false;
    DepthStencilAttachmentDescriptor depthStencil;
};

struct TextureRegion  {
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint16_t mipLevel = 0U;
    std::uint16_t arrayLayer = 0U;
    std::uint32_t bytesPerRow = 0U;
};

enum class IndexType : std::uint8_t {
    UInt16 = 0U,
    UInt32
};

AERO_API Base::Result<void> ValidateTextureDescriptor(
    const TextureResourceDescriptor& descriptor,
    const GraphicsCapabilities& capabilities) noexcept;
AERO_API Base::Result<void> ValidateSamplerDescriptor(
    const SamplerDescriptor& descriptor,
    const GraphicsCapabilities& capabilities) noexcept;
AERO_API Base::Result<void> ValidateNativePipeline(
    const NativePipelineState& descriptor,
    const GraphicsCapabilities& capabilities) noexcept;
AERO_API std::uint64_t StableNativePipelineHash(
    const NativePipelineState& descriptor) noexcept;

} // namespace Aero::Graphics
