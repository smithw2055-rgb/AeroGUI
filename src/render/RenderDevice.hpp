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

constexpr std::uint32_t GraphicsDeviceAbiVersion = 2U;
using FenceValue = std::uint64_t;

struct DeviceCapabilities  {
    std::uint32_t abiVersion = GraphicsDeviceAbiVersion;
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

// Forward-declared graphics contracts keep the device/resource lifetime layer
// independent from command encoding details while retaining one backend API.
enum class GraphicsBackendKind : std::uint8_t;
struct GraphicsCapabilities;
struct TextureResourceDescriptor;
struct SamplerDescriptor;
struct PipelineDescriptor;
class CommandList;

class AERO_API GraphicsBackend {
public:
    virtual ~GraphicsBackend() = default;

    virtual DeviceCapabilities Capabilities() const noexcept = 0;
    virtual GraphicsBackendKind Kind() const noexcept = 0;
    virtual GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept = 0;

    virtual Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept = 0;
    virtual void DestroyResource(ResourceHandle handle) noexcept = 0;
    virtual Base::Result<void> ConfigureTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> ConfigureSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> ConfigurePipeline(
        ResourceHandle handle,
        const PipelineDescriptor& descriptor) noexcept = 0;

    // This is the only command submission path. Backends never consume
    // Integration::RenderFrame or UI objects directly.
    virtual Base::Result<void> Submit(
        const CommandList& commands,
        FenceValue signalFence) noexcept = 0;
    virtual FenceValue LastSubmittedFence() const noexcept = 0;
    virtual FenceValue CompletedFence() const noexcept = 0;
    virtual bool IsDeviceLost() const noexcept = 0;
};

class AERO_API GraphicsDevice  {
public:
    explicit GraphicsDevice(
        GraphicsBackend& backend,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~GraphicsDevice() noexcept;

    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    Base::Result<void> Initialize() noexcept;
    const DeviceCapabilities& Capabilities() const noexcept {
        return capabilities_;
    }
    GraphicsBackend& Backend() const noexcept { return *backend_; }

    Base::Result<ResourceHandle> CreateBuffer(
        const BufferDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateTexture(
        const TextureResourceDescriptor& descriptor) noexcept;
    // Reserves a sampled-texture handle without creating native storage.
    // Backend-specific import APIs attach a host-owned texture afterwards.
    Base::Result<ResourceHandle> CreateExternalTexture(
        const TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateRenderTarget(
        const TextureResourceDescriptor& descriptor) noexcept;
    // Reserves a render-target handle and backend record without creating
    // native storage. Platform backends use this before importing an
    // externally owned swap-chain image or framebuffer.
    Base::Result<ResourceHandle> CreateExternalRenderTarget(
        const TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateSampler(
        const SamplerDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreatePipeline(
        const PipelineDescriptor& descriptor) noexcept;

    Base::Result<void> DestroyResource(
        ResourceHandle handle,
        FenceValue retireAfter = 0U) noexcept;
    bool IsAlive(ResourceHandle handle) const noexcept;

    Base::Result<FenceValue> Submit(
        const CommandList& commands) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;

    FenceValue LastSubmittedFence() const noexcept {
        return lastSubmittedFence_;
    }
    std::uint32_t LiveResourceCount() const noexcept;
    std::uint32_t PendingDestroyCount() const noexcept {
        return deferred_.Size();
    }

private:
    struct ResourceSlot  {
        ResourceDescriptor descriptor;
        std::uint32_t generation = 1U;
        bool alive = false;
    };

    struct DeferredDestroy  {
        ResourceHandle handle;
        FenceValue retireAfter = 0U;
    };

    GraphicsBackend* backend_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    DeviceCapabilities capabilities_;
    Base::Vector<ResourceSlot> slots_;
    Base::Vector<DeferredDestroy> deferred_;
    FenceValue lastSubmittedFence_ = 0U;
    bool initialized_ = false;

    Base::Result<void> VerifyReady() const noexcept;
    Base::Result<void> ValidateDescriptor(
        const ResourceDescriptor& descriptor) const noexcept;
    Base::Result<ResourceHandle> CreateResource(
        const ResourceDescriptor& descriptor) noexcept;
    void Rollback(ResourceHandle handle) noexcept;
};


constexpr std::uint32_t GraphicsAbiVersion = 2U;
constexpr std::uint32_t MaxVertexBuffers = 4U;
constexpr std::uint32_t MaxVertexAttributes = 16U;
constexpr std::uint32_t MaxColorAttachments = 4U;

enum class GraphicsBackendKind : std::uint8_t {
    Invalid = 0U,
    Null,
    Sokol,
    D3D11,
    D3D12,
    Vulkan,
    Metal,
    OpenGL33,
    OpenGLES30,
    WebGL2,
    ConsolePrivate
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
    Dxil,
    SpirV,
    MetalLib,
    Glsl330,
    GlslEs300,
    Wgsl
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
    GraphicsBackendKind backendKind = GraphicsBackendKind::Invalid;
    GraphicsFeatureFlags features = 0U;
    ShaderLanguageFlags shaderLanguages = 0U;
    std::uint32_t maxColorAttachments = 1U;
    std::uint32_t maxVertexAttributes = 8U;
    std::uint32_t maxSampledTextures = 8U;
    std::uint32_t uniformBufferAlignment = 16U;
};

struct ShaderDescriptor  {
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

struct PipelineDescriptor  {
    ShaderDescriptor vertexShader;
    ShaderDescriptor fragmentShader;
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

enum class CommandKind : std::uint8_t {
    UploadBuffer = 0U,
    UploadTexture,
    BeginRenderPass,
    EndRenderPass,
    BindPipeline,
    BindVertexBuffer,
    BindIndexBuffer,
    BindUniformBuffer,
    BindTextureSampler,
    SetScissor,
    Draw,
    DrawIndexed
};

struct Command  {
    CommandKind kind = CommandKind::Draw;
    ResourceHandle resource0;
    ResourceHandle resource1;
    RenderPassDescriptor renderPass;
    TextureRegion textureRegion;
    Base::Rect rect;
    std::uint64_t resourceOffset = 0U;
    std::uint32_t resourceSize = 0U;
    std::uint32_t uploadOffset = 0U;
    std::uint32_t uploadSize = 0U;
    std::uint32_t slot = 0U;
    std::uint32_t first = 0U;
    std::uint32_t count = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t firstInstance = 0U;
    std::int32_t baseVertex = 0;
    IndexType indexType = IndexType::UInt16;
};

class AERO_API CommandList  {
public:
    explicit CommandList(
        Base::IAllocator* allocator = nullptr) noexcept
        : commands_(allocator), uploadBytes_(allocator) {}

    Base::Span<const Command> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    Base::Span<const std::uint8_t> UploadBytes() const noexcept {
        return {uploadBytes_.Data(), uploadBytes_.Size()};
    }
    std::uint32_t CommandCount() const noexcept {
        return commands_.Size();
    }
    std::uint32_t UploadByteCount() const noexcept {
        return uploadBytes_.Size();
    }
    std::uint64_t StableHash() const noexcept;

private:
    friend class CommandEncoder;
    Base::Vector<Command> commands_;
    Base::Vector<std::uint8_t> uploadBytes_;
};

class AERO_API CommandEncoder  {
public:
    explicit CommandEncoder(
        Base::IAllocator* allocator = nullptr) noexcept
        : buffer_(allocator) {}

    Base::Result<void> UploadBuffer(
        ResourceHandle buffer,
        std::uint64_t destinationOffset,
        Base::Span<const std::uint8_t> data) noexcept;
    Base::Result<void> UploadTexture(
        ResourceHandle texture,
        TextureRegion region,
        Base::Span<const std::uint8_t> data) noexcept;
    Base::Result<void> BeginRenderPass(
        const RenderPassDescriptor& descriptor) noexcept;
    Base::Result<void> EndRenderPass() noexcept;
    Base::Result<void> BindPipeline(
        ResourceHandle pipeline) noexcept;
    Base::Result<void> BindVertexBuffer(
        std::uint32_t slot,
        ResourceHandle buffer,
        std::uint64_t offset = 0U) noexcept;
    Base::Result<void> BindIndexBuffer(
        ResourceHandle buffer,
        IndexType type,
        std::uint64_t offset = 0U) noexcept;
    Base::Result<void> BindUniformBuffer(
        std::uint32_t slot,
        ResourceHandle buffer,
        std::uint64_t offset,
        std::uint32_t size) noexcept;
    Base::Result<void> BindTextureSampler(
        std::uint32_t slot,
        ResourceHandle texture,
        ResourceHandle sampler) noexcept;
    Base::Result<void> SetScissor(Base::Rect rect) noexcept;
    Base::Result<void> Draw(
        std::uint32_t vertexCount,
        std::uint32_t instanceCount = 1U,
        std::uint32_t firstVertex = 0U,
        std::uint32_t firstInstance = 0U) noexcept;
    Base::Result<void> DrawIndexed(
        std::uint32_t indexCount,
        std::uint32_t instanceCount = 1U,
        std::uint32_t firstIndex = 0U,
        std::int32_t baseVertex = 0,
        std::uint32_t firstInstance = 0U) noexcept;
    Base::Result<CommandList> Finish() noexcept;

private:
    CommandList buffer_;
    bool inRenderPass_ = false;
    bool finished_ = false;

    Base::Result<void> VerifyRecording() const noexcept;
    Base::Result<void> Append(
        const Command& command) noexcept;
    Base::Result<void> AppendUpload(
        Command& command,
        Base::Span<const std::uint8_t> data) noexcept;
};

AERO_API Base::Result<void> ValidateTextureDescriptor(
    const TextureResourceDescriptor& descriptor,
    const GraphicsCapabilities& capabilities) noexcept;
AERO_API Base::Result<void> ValidateSamplerDescriptor(
    const SamplerDescriptor& descriptor,
    const GraphicsCapabilities& capabilities) noexcept;
AERO_API Base::Result<void> ValidatePipelineDescriptor(
    const PipelineDescriptor& descriptor,
    const GraphicsCapabilities& capabilities) noexcept;
AERO_API std::uint64_t StablePipelineHash(
    const PipelineDescriptor& descriptor) noexcept;

struct BackendRequest  {
    GraphicsBackendKind preferred = GraphicsBackendKind::Invalid;
    GraphicsFeatureFlags requiredFeatures = 0U;
    ShaderLanguageFlags requiredShaderLanguages = 0U;
    bool allowFallback = true;
};

AERO_API Base::Result<GraphicsBackend*>
SelectGraphicsBackend(
    Base::Span<GraphicsBackend*> backends,
    const BackendRequest& request) noexcept;

class AERO_API NullGraphicsBackend  : public GraphicsBackend {
public:
    explicit NullGraphicsBackend(
        Base::IAllocator* allocator = nullptr) noexcept
        : resources_(allocator) {}

    DeviceCapabilities Capabilities() const noexcept override;
    GraphicsBackendKind Kind() const noexcept override {
        return GraphicsBackendKind::Null;
    }
    GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept override;

    Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept override;
    void DestroyResource(ResourceHandle handle) noexcept override;
    Base::Result<void> ConfigureTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept override;
    Base::Result<void> ConfigureSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept override;
    Base::Result<void> ConfigurePipeline(
        ResourceHandle handle,
        const PipelineDescriptor& descriptor) noexcept override;
    Base::Result<void> Submit(
        const CommandList& commands,
        FenceValue signalFence) noexcept override;
    FenceValue LastSubmittedFence() const noexcept override {
        return lastSubmittedFence_;
    }
    FenceValue CompletedFence() const noexcept override {
        return completedFence_;
    }
    bool IsDeviceLost() const noexcept override {
        return deviceLost_;
    }

    void CompleteThrough(FenceValue fence) noexcept;
    void SimulateDeviceLoss() noexcept;

    std::uint32_t SubmissionCount() const noexcept {
        return submissionCount_;
    }
    std::uint64_t LastGraphicsHash() const noexcept {
        return lastGraphicsHash_;
    }
    std::uint32_t LiveBackendResourceCount() const noexcept {
        return resources_.Size();
    }

private:
    enum class ConfigurationKind : std::uint8_t {
        None = 0U,
        Texture,
        Sampler,
        Pipeline
    };

    struct ResourceRecord  {
        ResourceHandle handle;
        ResourceDescriptor descriptor;
        TextureResourceDescriptor texture;
        std::uint64_t configurationHash = 0U;
        ConfigurationKind configuration = ConfigurationKind::None;
    };

    Base::Vector<ResourceRecord> resources_;
    FenceValue completedFence_ = 0U;
    FenceValue lastSubmittedFence_ = 0U;
    std::uint64_t lastGraphicsHash_ = 0U;
    std::uint32_t submissionCount_ = 0U;
    bool deviceLost_ = false;

    ResourceRecord* Find(ResourceHandle handle) noexcept;
    const ResourceRecord* Find(
        ResourceHandle handle) const noexcept;
    bool IsConfigured(
        ResourceHandle handle,
        ConfigurationKind configuration) const noexcept;
    Base::Result<void> ValidateCommands(
        const CommandList& commands) const noexcept;
};

struct SokolBackendApi  {
    std::uint32_t structSize = 0U;
    std::uint32_t abiVersion = GraphicsAbiVersion;
    void* context = nullptr;
    DeviceCapabilities (*deviceCapabilities)(void*) noexcept = nullptr;
    GraphicsCapabilities (*graphicsCapabilities)(void*) noexcept = nullptr;
    Base::Result<void> (*createResource)(
        void*, ResourceHandle, const ResourceDescriptor&) noexcept = nullptr;
    void (*destroyResource)(void*, ResourceHandle) noexcept = nullptr;
    Base::Result<void> (*configureTexture)(
        void*, ResourceHandle, const TextureResourceDescriptor&) noexcept = nullptr;
    Base::Result<void> (*configureSampler)(
        void*, ResourceHandle, const SamplerDescriptor&) noexcept = nullptr;
    Base::Result<void> (*configurePipeline)(
        void*, ResourceHandle, const PipelineDescriptor&) noexcept = nullptr;
    Base::Result<void> (*submit)(
        void*, const CommandList&, FenceValue) noexcept = nullptr;
    FenceValue (*completedFence)(void*) noexcept = nullptr;
    bool (*isDeviceLost)(void*) noexcept = nullptr;
};

class AERO_API SokolBackendAdapter  : public GraphicsBackend {
public:
    explicit SokolBackendAdapter(const SokolBackendApi& api) noexcept
        : api_(api) {}

    bool IsValid() const noexcept;
    DeviceCapabilities Capabilities() const noexcept override;
    GraphicsBackendKind Kind() const noexcept override {
        return GraphicsBackendKind::Sokol;
    }
    GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept override;
    Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept override;
    void DestroyResource(ResourceHandle handle) noexcept override;
    Base::Result<void> ConfigureTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept override;
    Base::Result<void> ConfigureSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept override;
    Base::Result<void> ConfigurePipeline(
        ResourceHandle handle,
        const PipelineDescriptor& descriptor) noexcept override;
    Base::Result<void> Submit(
        const CommandList& commands,
        FenceValue signalFence) noexcept override;
    FenceValue LastSubmittedFence() const noexcept override {
        return lastSubmittedFence_;
    }
    FenceValue CompletedFence() const noexcept override;
    bool IsDeviceLost() const noexcept override;

private:
    SokolBackendApi api_;
    FenceValue lastSubmittedFence_ = 0U;
};

} // namespace Aero::Graphics
