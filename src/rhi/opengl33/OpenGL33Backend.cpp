#include <Aero/Rhi/OpenGL33Backend.hpp>

#include <Aero/Base/Vector.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Rhi {
namespace {

constexpr GlEnum GlNoError = 0U;
constexpr GlEnum GlDynamicDraw = 0x88E8U;
constexpr GlEnum GlStaticDraw = 0x88E4U;
constexpr GlEnum GlTexture2DMultisample = 0x9100U;
constexpr GlEnum GlTextureBaseLevel = 0x813CU;
constexpr GlEnum GlTextureMaxLevel = 0x813DU;
constexpr GlEnum GlTextureMinFilter = 0x2801U;
constexpr GlEnum GlTextureMagFilter = 0x2800U;
constexpr GlEnum GlTextureWrapS = 0x2802U;
constexpr GlEnum GlTextureWrapT = 0x2803U;
constexpr GlEnum GlTextureWrapR = 0x8072U;
constexpr GlEnum GlTextureMinLod = 0x813AU;
constexpr GlEnum GlTextureMaxLod = 0x813BU;
constexpr GlEnum GlNearest = 0x2600U;
constexpr GlEnum GlLinear = 0x2601U;
constexpr GlEnum GlNearestMipmapNearest = 0x2700U;
constexpr GlEnum GlLinearMipmapNearest = 0x2701U;
constexpr GlEnum GlNearestMipmapLinear = 0x2702U;
constexpr GlEnum GlLinearMipmapLinear = 0x2703U;
constexpr GlEnum GlClampToEdge = 0x812FU;
constexpr GlEnum GlRepeat = 0x2901U;
constexpr GlEnum GlMirroredRepeat = 0x8370U;

constexpr GlEnum GlRgba8 = 0x8058U;
constexpr GlEnum GlR8 = 0x8229U;
constexpr GlEnum GlDepth24Stencil8 = 0x88F0U;
constexpr GlEnum GlRgba = 0x1908U;
constexpr GlEnum GlBgra = 0x80E1U;
constexpr GlEnum GlRed = 0x1903U;
constexpr GlEnum GlDepthStencil = 0x84F9U;
constexpr GlEnum GlUnsignedByte = 0x1401U;
constexpr GlEnum GlUnsignedShort = 0x1403U;
constexpr GlEnum GlUnsignedInt = 0x1405U;
constexpr GlEnum GlUnsignedInt248 = 0x84FAU;
constexpr GlEnum GlFloatType = 0x1406U;

constexpr GlEnum GlVertexShader = 0x8B31U;
constexpr GlEnum GlFragmentShader = 0x8B30U;
constexpr GlEnum GlCompileStatus = 0x8B81U;
constexpr GlEnum GlLinkStatus = 0x8B82U;
constexpr GlUInt GlInvalidIndex = UINT32_MAX;

constexpr GlEnum GlTriangles = 0x0004U;
constexpr GlEnum GlTriangleStrip = 0x0005U;
constexpr GlEnum GlLines = 0x0001U;
constexpr GlEnum GlLineStrip = 0x0003U;
constexpr GlEnum GlZero = 0U;
constexpr GlEnum GlOne = 1U;
constexpr GlEnum GlSourceAlpha = 0x0302U;
constexpr GlEnum GlOneMinusSourceAlpha = 0x0303U;
constexpr GlEnum GlDestinationAlpha = 0x0304U;
constexpr GlEnum GlOneMinusDestinationAlpha = 0x0305U;
constexpr GlEnum GlFuncAdd = 0x8006U;
constexpr GlEnum GlFuncSubtract = 0x800AU;
constexpr GlEnum GlFuncReverseSubtract = 0x800BU;
constexpr GlEnum GlMin = 0x8007U;
constexpr GlEnum GlMax = 0x8008U;
constexpr GlEnum GlNever = 0x0200U;
constexpr GlEnum GlLess = 0x0201U;
constexpr GlEnum GlEqual = 0x0202U;
constexpr GlEnum GlLessEqual = 0x0203U;
constexpr GlEnum GlGreater = 0x0204U;
constexpr GlEnum GlNotEqual = 0x0205U;
constexpr GlEnum GlGreaterEqual = 0x0206U;
constexpr GlEnum GlAlways = 0x0207U;
constexpr GlEnum GlKeep = 0x1E00U;
constexpr GlEnum GlReplace = 0x1E01U;
constexpr GlEnum GlIncrement = 0x1E02U;
constexpr GlEnum GlDecrement = 0x1E03U;
constexpr GlEnum GlInvert = 0x150AU;
constexpr GlEnum GlClockwise = 0x0900U;
constexpr GlEnum GlCounterClockwise = 0x0901U;
constexpr GlEnum GlFill = 0x1B02U;
constexpr GlEnum GlLine = 0x1B01U;

constexpr GlEnum GlDepthStencilAttachment = 0x821AU;
constexpr GlEnum GlColor = 0x1800U;
constexpr GlEnum GlDepth = 0x1801U;
constexpr GlEnum GlStencil = 0x1802U;

constexpr std::uint64_t HashOffset = UINT64_C(1469598103934665603);
constexpr std::uint64_t HashPrime = UINT64_C(1099511628211);

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory, message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfRange, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported, message);
}

GlEnum BufferTarget(BufferUsage usage) noexcept {
    switch (usage) {
    case BufferUsage::Vertex:
    case BufferUsage::Upload:
        return GlConstant::ArrayBuffer;
    case BufferUsage::Index:
        return GlConstant::ElementArrayBuffer;
    case BufferUsage::Uniform:
        return GlConstant::UniformBuffer;
    }
    return 0U;
}

GlEnum BufferStorageUsage(BufferUsage usage) noexcept {
    return usage == BufferUsage::Upload ||
        usage == BufferUsage::Uniform
        ? GlDynamicDraw
        : GlStaticDraw;
}

GlEnum TextureTarget(
    const TextureResourceDescriptor& descriptor) noexcept {
    if (descriptor.sampleCount > 1U) {
        return GlTexture2DMultisample;
    }
    return descriptor.arrayLayers > 1U
        ? GlConstant::Texture2DArray
        : GlConstant::Texture2D;
}

GlInt TextureInternalFormat(GraphicsTextureFormat format) noexcept {
    switch (format) {
    case GraphicsTextureFormat::Rgba8Unorm:
    case GraphicsTextureFormat::Bgra8Unorm:
        return static_cast<GlInt>(GlRgba8);
    case GraphicsTextureFormat::R8Unorm:
        return static_cast<GlInt>(GlR8);
    case GraphicsTextureFormat::Depth24Stencil8:
        return static_cast<GlInt>(GlDepth24Stencil8);
    }
    return 0;
}

GlEnum TextureDataFormat(GraphicsTextureFormat format) noexcept {
    switch (format) {
    case GraphicsTextureFormat::Rgba8Unorm:
        return GlRgba;
    case GraphicsTextureFormat::Bgra8Unorm:
        return GlBgra;
    case GraphicsTextureFormat::R8Unorm:
        return GlRed;
    case GraphicsTextureFormat::Depth24Stencil8:
        return GlDepthStencil;
    }
    return 0U;
}

GlEnum TextureDataType(GraphicsTextureFormat format) noexcept {
    return format == GraphicsTextureFormat::Depth24Stencil8
        ? GlUnsignedInt248
        : GlUnsignedByte;
}

TextureFormat BaseTextureFormat(
    GraphicsTextureFormat format) noexcept {
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

GlEnum MinFilter(const SamplerDescriptor& descriptor) noexcept {
    if (descriptor.mipFilter == FilterMode::Nearest) {
        return descriptor.minFilter == FilterMode::Nearest
            ? GlNearestMipmapNearest
            : GlLinearMipmapNearest;
    }
    return descriptor.minFilter == FilterMode::Nearest
        ? GlNearestMipmapLinear
        : GlLinearMipmapLinear;
}

GlEnum MagFilter(FilterMode filter) noexcept {
    return filter == FilterMode::Nearest ? GlNearest : GlLinear;
}

GlEnum AddressModeValue(AddressMode mode) noexcept {
    switch (mode) {
    case AddressMode::ClampToEdge:
        return GlClampToEdge;
    case AddressMode::Repeat:
        return GlRepeat;
    case AddressMode::MirrorRepeat:
        return GlMirroredRepeat;
    }
    return GlClampToEdge;
}

GlEnum Topology(PrimitiveTopology topology) noexcept {
    switch (topology) {
    case PrimitiveTopology::TriangleList:
        return GlTriangles;
    case PrimitiveTopology::TriangleStrip:
        return GlTriangleStrip;
    case PrimitiveTopology::LineList:
        return GlLines;
    case PrimitiveTopology::LineStrip:
        return GlLineStrip;
    }
    return 0U;
}

GlEnum BlendFactorValue(BlendFactor factor) noexcept {
    switch (factor) {
    case BlendFactor::Zero:
        return GlZero;
    case BlendFactor::One:
        return GlOne;
    case BlendFactor::SourceAlpha:
        return GlSourceAlpha;
    case BlendFactor::OneMinusSourceAlpha:
        return GlOneMinusSourceAlpha;
    case BlendFactor::DestinationAlpha:
        return GlDestinationAlpha;
    case BlendFactor::OneMinusDestinationAlpha:
        return GlOneMinusDestinationAlpha;
    }
    return GlOne;
}

GlEnum BlendOperationValue(BlendOperation operation) noexcept {
    switch (operation) {
    case BlendOperation::Add:
        return GlFuncAdd;
    case BlendOperation::Subtract:
        return GlFuncSubtract;
    case BlendOperation::ReverseSubtract:
        return GlFuncReverseSubtract;
    case BlendOperation::Minimum:
        return GlMin;
    case BlendOperation::Maximum:
        return GlMax;
    }
    return GlFuncAdd;
}

GlEnum CompareValue(CompareOperation operation) noexcept {
    switch (operation) {
    case CompareOperation::Never:
        return GlNever;
    case CompareOperation::Less:
        return GlLess;
    case CompareOperation::Equal:
        return GlEqual;
    case CompareOperation::LessEqual:
        return GlLessEqual;
    case CompareOperation::Greater:
        return GlGreater;
    case CompareOperation::NotEqual:
        return GlNotEqual;
    case CompareOperation::GreaterEqual:
        return GlGreaterEqual;
    case CompareOperation::Always:
        return GlAlways;
    }
    return GlAlways;
}

GlEnum StencilValue(StencilOperation operation) noexcept {
    switch (operation) {
    case StencilOperation::Keep:
        return GlKeep;
    case StencilOperation::Zero:
        return GlZero;
    case StencilOperation::Replace:
        return GlReplace;
    case StencilOperation::IncrementClamp:
        return GlIncrement;
    case StencilOperation::DecrementClamp:
        return GlDecrement;
    case StencilOperation::Invert:
        return GlInvert;
    }
    return GlKeep;
}

GlBlendState ToGlBlendState(const BlendState& source) noexcept {
    GlBlendState result;
    result.enabled = source.enabled;
    result.colorEquation = BlendOperationValue(source.color.operation);
    result.alphaEquation = BlendOperationValue(source.alpha.operation);
    result.sourceColor = BlendFactorValue(source.color.source);
    result.destinationColor =
        BlendFactorValue(source.color.destination);
    result.sourceAlpha = BlendFactorValue(source.alpha.source);
    result.destinationAlpha =
        BlendFactorValue(source.alpha.destination);
    result.writeRed = (source.writeMask & 0x01U) != 0U;
    result.writeGreen = (source.writeMask & 0x02U) != 0U;
    result.writeBlue = (source.writeMask & 0x04U) != 0U;
    result.writeAlpha = (source.writeMask & 0x08U) != 0U;
    return result;
}

GlDepthState ToGlDepthState(
    const DepthStencilState& source) noexcept {
    GlDepthState result;
    result.enabled = source.depthTestEnabled;
    result.function = CompareValue(source.depthCompare);
    result.writeEnabled = source.depthWriteEnabled;
    return result;
}

GlStencilFaceState ToGlStencilFace(
    const StencilFaceState& source,
    const DepthStencilState& owner) noexcept {
    GlStencilFaceState result;
    result.function = CompareValue(source.compare);
    result.reference = 0;
    result.readMask = owner.stencilReadMask;
    result.stencilFail = StencilValue(source.fail);
    result.depthFail = StencilValue(source.depthFail);
    result.pass = StencilValue(source.pass);
    result.writeMask = owner.stencilWriteMask;
    return result;
}

GlStencilState ToGlStencilState(
    const DepthStencilState& source) noexcept {
    GlStencilState result;
    result.enabled = source.stencilEnabled;
    result.front = ToGlStencilFace(source.front, source);
    result.back = ToGlStencilFace(source.back, source);
    return result;
}

GlRasterState ToGlRasterState(const RasterState& source) noexcept {
    GlRasterState result;
    result.cullEnabled = source.cullMode != CullMode::None;
    result.cullFace = source.cullMode == CullMode::Front
        ? GlConstant::Front
        : GlConstant::Back;
    result.frontFace = source.frontFace == FrontFace::Clockwise
        ? GlClockwise
        : GlCounterClockwise;
    result.polygonMode = source.fillMode == FillMode::Wireframe
        ? GlLine
        : GlFill;
    return result;
}

bool CheckedGlSize(std::uint64_t value, GlSizePtr& result) noexcept {
    if (value > static_cast<std::uint64_t>(
            std::numeric_limits<GlSizePtr>::max())) {
        return false;
    }
    result = static_cast<GlSizePtr>(value);
    return true;
}

bool CheckedGlSizeValue(std::uint32_t value, GlSize& result) noexcept {
    if (value > static_cast<std::uint32_t>(
            std::numeric_limits<GlSize>::max())) {
        return false;
    }
    result = static_cast<GlSize>(value);
    return true;
}

Base::Result<GlRectangleState> ToGlRectangle(
    Base::Rect rect,
    std::uint32_t targetHeight) noexcept {
    if (!Base::IsValidRect(rect) ||
        rect.x < 0.0F || rect.y < 0.0F ||
        rect.width < 0.0F || rect.height < 0.0F) {
        return InvalidArgument("OpenGL rectangle is invalid");
    }
    const double bottom =
        static_cast<double>(targetHeight) -
        (static_cast<double>(rect.y) +
         static_cast<double>(rect.height));
    if (bottom < 0.0 ||
        rect.x > static_cast<float>(
            std::numeric_limits<GlInt>::max()) ||
        rect.width > static_cast<float>(
            std::numeric_limits<GlSize>::max()) ||
        rect.height > static_cast<float>(
            std::numeric_limits<GlSize>::max()) ||
        bottom > static_cast<double>(
            std::numeric_limits<GlInt>::max())) {
        return OutOfRange("OpenGL rectangle exceeds integer range");
    }
    GlRectangleState result;
    result.x = static_cast<GlInt>(rect.x);
    result.y = static_cast<GlInt>(bottom);
    result.width = static_cast<GlSize>(rect.width);
    result.height = static_cast<GlSize>(rect.height);
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

struct OpenGL33GraphicsBackend::Impl final {
    struct ResourceRecord final {
        ResourceHandle handle;
        ResourceDescriptor baseDescriptor;
        TextureResourceDescriptor textureDescriptor;
        VertexLayoutDescriptor vertexLayout;
        BlendState blend;
        RasterState raster;
        DepthStencilState depthStencil;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        GlUInt buffer = 0U;
        GlUInt texture = 0U;
        GlUInt sampler = 0U;
        GlUInt program = 0U;
        GlUInt externalFramebuffer = 0U;
        GlUInt externalDepthStencilTexture = 0U;
        GlEnum textureTarget = GlConstant::Texture2D;
        bool configured = false;
        bool external = false;
        bool externalDefaultFramebuffer = false;
    };

    struct PendingFence final {
        FenceValue value = 0U;
        GlSync sync = nullptr;
    };

    struct BufferBinding final {
        ResourceHandle handle;
        std::uint64_t offset = 0U;
    };

    explicit Impl(
        const GlFunctionTable& functionTable,
        const GlContextContract& contextContract,
        const OpenGL33BackendOptions& backendOptions,
        Base::IAllocator* allocatorValue) noexcept
        : functions(functionTable),
          context(contextContract),
          options(backendOptions),
          allocator(allocatorValue != nullptr
              ? allocatorValue
              : &Base::GetDefaultAllocator()),
          resources(allocator),
          pendingFences(allocator) {}

    GlFunctionTable functions;
    GlContextContract context;
    OpenGL33BackendOptions options;
    Base::IAllocator* allocator = nullptr;
    Base::Vector<ResourceRecord> resources;
    mutable Base::Vector<PendingFence> pendingFences;
    GlCapabilities glCapabilities;
    GlStateCache stateCache;
    GlUInt vertexArray = 0U;
    GlUInt submissionFramebuffer = 0U;
    GlUInt readbackFramebuffer = 0U;
    ResourceHandle currentPipeline;
    BufferBinding vertexBuffers[MaxVertexBuffers]{};
    BufferBinding indexBuffer;
    IndexType indexType = IndexType::UInt16;
    std::uint32_t renderPassHeight = 0U;
    std::uint32_t enabledAttributeMask = 0U;
    GlUInt activeAttachmentTextures[MaxColorAttachments + 1U]{};
    FenceValue lastSubmittedFence = 0U;
    mutable FenceValue completedFence = 0U;
    mutable bool deviceLost = false;
    bool initialized = false;
    bool inRenderPass = false;
    bool vertexStateDirty = true;

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
        for (std::uint32_t index = 0U;
             index < resources.Size();
             ++index) {
            if (resources[index].handle == handle) {
                return index;
            }
        }
        return UINT32_MAX;
    }

    Base::Result<void> VerifyReady() const noexcept {
        if (!initialized) {
            return NotInitialized(
                "OpenGL 3.3 backend is not initialized");
        }
        if (deviceLost) {
            return InvalidState(
                "OpenGL 3.3 context has been lost");
        }
        Base::Result<void> contextResult =
            ValidateGlContextContract(context);
        if (!contextResult) {
            return contextResult.GetStatus();
        }
        if (context.generation !=
            glCapabilities.contextGeneration) {
            return InvalidState(
                "OpenGL context generation changed without backend recovery");
        }
        return {};
    }

    Base::Result<void> CheckError(const char* message) const noexcept {
        if (!options.checkErrors) {
            return {};
        }
        if (functions.getError() != GlNoError) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError, message);
        }
        return {};
    }

    Base::Result<void> BeginStateScope() noexcept {
        return stateCache.Begin(
            context.generation, options.embeddingMode);
    }

    Base::Result<void> EndStateScope() noexcept {
        return stateCache.End();
    }

    void RemoveResourceAt(std::uint32_t index) noexcept {
        if (index >= resources.Size()) {
            return;
        }
        for (std::uint32_t current = index + 1U;
             current < resources.Size();
             ++current) {
            resources[current - 1U] =
                std::move(resources[current]);
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
            pendingFences[index - 1U] =
                pendingFences[index];
        }
        pendingFences.PopBack();
    }

    void DeleteNative(ResourceRecord& record) noexcept {
        if (record.program != 0U) {
            functions.deleteProgram(record.program);
        }
        if (record.sampler != 0U) {
            functions.deleteSamplers(1, &record.sampler);
        }
        if (record.texture != 0U && !record.external) {
            functions.deleteTextures(1, &record.texture);
        }
        if (record.buffer != 0U) {
            functions.deleteBuffers(1, &record.buffer);
        }
        record = ResourceRecord{};
    }

    void ResetSubmission() noexcept {
        currentPipeline = {};
        for (BufferBinding& binding : vertexBuffers) {
            binding = {};
        }
        indexBuffer = {};
        indexType = IndexType::UInt16;
        renderPassHeight = 0U;
        for (GlUInt& attachment : activeAttachmentTextures) {
            attachment = 0U;
        }
        inRenderPass = false;
        vertexStateDirty = true;
    }

    bool IsActiveAttachment(GlUInt texture) const noexcept {
        if (texture == 0U) {
            return false;
        }
        for (GlUInt attachment : activeAttachmentTextures) {
            if (attachment == texture) {
                return true;
            }
        }
        return false;
    }

    void PollFences() const noexcept {
        while (!pendingFences.Empty()) {
            const PendingFence& pending = pendingFences[0U];
            const GlEnum result = functions.clientWaitSync(
                pending.sync, 0U, 0U);
            if (result == GlConstant::TimeoutExpired) {
                break;
            }
            if (result == GlConstant::WaitFailed) {
                deviceLost = true;
                break;
            }
            if (result != GlConstant::AlreadySignaled &&
                result != GlConstant::ConditionSatisfied) {
                break;
            }
            functions.deleteSync(pending.sync);
            completedFence = pending.value;
            RemoveFirstFence();
        }
    }

    Base::Result<void> PrepareVertexInput(
        bool indexed) noexcept {
        const ResourceRecord* pipeline = Find(currentPipeline);
        if (pipeline == nullptr || pipeline->program == 0U) {
            return InvalidState(
                "OpenGL draw pipeline is not configured");
        }
        if (!vertexStateDirty &&
            (!indexed || indexBuffer.handle.IsValid())) {
            return {};
        }

        Base::Result<void> vaoResult =
            stateCache.BindVertexArray(vertexArray);
        if (!vaoResult) {
            return vaoResult;
        }

        std::uint32_t requiredMask = 0U;
        for (std::uint32_t index = 0U;
             index < pipeline->vertexLayout.attributeCount;
             ++index) {
            const VertexAttribute& attribute =
                pipeline->vertexLayout.attributes[index];
            if (attribute.location >= MaxVertexAttributes ||
                attribute.bufferSlot >=
                    pipeline->vertexLayout.bufferCount ||
                attribute.bufferSlot >= MaxVertexBuffers) {
                return InvalidArgument(
                    "OpenGL vertex attribute is outside pipeline bounds");
            }
            const BufferBinding& binding =
                vertexBuffers[attribute.bufferSlot];
            const ResourceRecord* buffer = Find(binding.handle);
            if (buffer == nullptr || buffer->buffer == 0U ||
                buffer->baseDescriptor.type != ResourceType::Buffer) {
                return InvalidState(
                    "OpenGL draw is missing a vertex buffer");
            }
            Base::Result<void> bindResult =
                stateCache.BindArrayBuffer(buffer->buffer);
            if (!bindResult) {
                return bindResult;
            }

            GlInt components = 0;
            GlEnum type = GlFloatType;
            GlBoolean normalized = GlConstant::False;
            switch (attribute.format) {
            case VertexFormat::Float:
                components = 1;
                break;
            case VertexFormat::Float2:
                components = 2;
                break;
            case VertexFormat::Float3:
                components = 3;
                break;
            case VertexFormat::Float4:
                components = 4;
                break;
            case VertexFormat::UByte4Normalized:
                components = 4;
                type = GlUnsignedByte;
                normalized = GlConstant::True;
                break;
            case VertexFormat::UShort2:
                components = 2;
                type = GlUnsignedShort;
                break;
            case VertexFormat::UShort4:
                components = 4;
                type = GlUnsignedShort;
                break;
            }
            const VertexBufferLayout& layout =
                pipeline->vertexLayout.buffers[
                    attribute.bufferSlot];
            GlSize stride = 0;
            if (!CheckedGlSizeValue(layout.stride, stride) ||
                binding.offset >
                    std::numeric_limits<std::uintptr_t>::max() -
                        attribute.offset) {
                return OutOfRange(
                    "OpenGL vertex layout exceeds pointer range");
            }
            const std::uintptr_t pointer =
                static_cast<std::uintptr_t>(binding.offset) +
                attribute.offset;
            functions.enableVertexAttribArray(attribute.location);
            functions.vertexAttribPointer(
                attribute.location,
                components,
                type,
                normalized,
                stride,
                reinterpret_cast<const void*>(pointer));
            functions.vertexAttribDivisor(
                attribute.location,
                layout.stepMode == VertexStepMode::PerInstance
                    ? 1U
                    : 0U);
            requiredMask |=
                UINT32_C(1) << attribute.location;
        }
        for (std::uint32_t location = 0U;
             location < MaxVertexAttributes;
             ++location) {
            const std::uint32_t bit = UINT32_C(1) << location;
            if ((enabledAttributeMask & bit) != 0U &&
                (requiredMask & bit) == 0U) {
                functions.disableVertexAttribArray(location);
            }
        }
        enabledAttributeMask = requiredMask;

        if (indexed) {
            const ResourceRecord* buffer =
                Find(indexBuffer.handle);
            if (buffer == nullptr || buffer->buffer == 0U) {
                return InvalidState(
                    "OpenGL indexed draw is missing an index buffer");
            }
            Base::Result<void> bindResult =
                stateCache.BindElementArrayBuffer(buffer->buffer);
            if (!bindResult) {
                return bindResult;
            }
        }
        vertexStateDirty = false;
        return {};
    }
};

OpenGL33GraphicsBackend::OpenGL33GraphicsBackend(
    const GlFunctionTable& functions,
    const GlContextContract& context,
    const OpenGL33BackendOptions& options,
    Base::IAllocator* allocator) noexcept
    : functions_(functions),
      context_(context),
      options_(options),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

OpenGL33GraphicsBackend::~OpenGL33GraphicsBackend() noexcept {
    Shutdown();
}

Base::Result<void> OpenGL33GraphicsBackend::Initialize() noexcept {
    if (impl_ != nullptr && impl_->initialized) {
        return {};
    }
    Base::Result<GlCapabilities> queried =
        QueryGlCapabilities(functions_, context_);
    if (!queried) {
        return queried.GetStatus();
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory(
                "Failed to allocate OpenGL 3.3 backend state");
        }
        impl_ = new (memory) Impl(
            functions_, context_, options_, allocator_);
    }

    impl_->glCapabilities = queried.Value();
    Base::Result<void> cacheResult = impl_->stateCache.Initialize(
        functions_, impl_->glCapabilities);
    if (!cacheResult) {
        return cacheResult;
    }
    Base::Result<void> scope = impl_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    impl_->functions.genVertexArrays(1, &impl_->vertexArray);
    impl_->functions.genFramebuffers(
        1, &impl_->submissionFramebuffer);
    impl_->functions.genFramebuffers(
        1, &impl_->readbackFramebuffer);
    Base::Result<void> error = impl_->CheckError(
        "OpenGL backend object initialization failed");
    Base::Result<void> end = impl_->EndStateScope();
    if (!error) {
        return error;
    }
    if (!end) {
        return end;
    }
    if (impl_->vertexArray == 0U ||
        impl_->submissionFramebuffer == 0U ||
        impl_->readbackFramebuffer == 0U) {
        return InvalidState(
            "OpenGL backend returned invalid object names");
    }
    impl_->initialized = true;
    return {};
}

void OpenGL33GraphicsBackend::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    const bool canDelete =
        ValidateGlContextContract(impl_->context).HasValue() &&
        impl_->context.generation ==
            impl_->glCapabilities.contextGeneration &&
        !impl_->deviceLost;
    if (canDelete) {
        for (Impl::PendingFence& fence : impl_->pendingFences) {
            if (fence.sync != nullptr) {
                impl_->functions.deleteSync(fence.sync);
            }
        }
        for (Impl::ResourceRecord& record : impl_->resources) {
            impl_->DeleteNative(record);
        }
        if (impl_->readbackFramebuffer != 0U) {
            impl_->functions.deleteFramebuffers(
                1, &impl_->readbackFramebuffer);
        }
        if (impl_->submissionFramebuffer != 0U) {
            impl_->functions.deleteFramebuffers(
                1, &impl_->submissionFramebuffer);
        }
        if (impl_->vertexArray != 0U) {
            impl_->functions.deleteVertexArrays(
                1, &impl_->vertexArray);
        }
    }
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;
}

void OpenGL33GraphicsBackend::NotifyContextLost() noexcept {
    if (impl_ != nullptr) {
        impl_->deviceLost = true;
        impl_->stateCache.Invalidate(
            impl_->context.generation + 1U);
    }
}

bool OpenGL33GraphicsBackend::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

std::uint32_t OpenGL33GraphicsBackend::LiveResourceCount() const noexcept {
    return impl_ != nullptr ? impl_->resources.Size() : 0U;
}

DeviceCapabilities
OpenGL33GraphicsBackend::Capabilities() const noexcept {
    DeviceCapabilities capabilities;
    capabilities.maxFramesInFlight = 2U;
    capabilities.maxTextureDimension =
        impl_ != nullptr
        ? impl_->glCapabilities.limits.maxTextureSize
        : 0U;
    capabilities.supportsTimestampQueries = false;
    return capabilities;
}

GraphicsCapabilities
OpenGL33GraphicsBackend::QueryGraphicsCapabilities() const noexcept {
    GraphicsCapabilities capabilities;
    capabilities.backendKind = GraphicsBackendKind::OpenGL33;
    capabilities.features =
        FeatureBit(GraphicsFeature::TextureSampling) |
        FeatureBit(GraphicsFeature::RenderTargets) |
        FeatureBit(GraphicsFeature::VertexIndexBuffers) |
        FeatureBit(GraphicsFeature::UniformBuffers) |
        FeatureBit(GraphicsFeature::DepthStencil) |
        FeatureBit(GraphicsFeature::Instancing) |
        FeatureBit(GraphicsFeature::Scissor);
    capabilities.shaderLanguages =
        ShaderLanguageBit(ShaderLanguage::Glsl330);
    if (impl_ != nullptr) {
        capabilities.maxColorAttachments = std::min(
            impl_->glCapabilities.limits.maxColorAttachments,
            MaxColorAttachments);
        capabilities.maxVertexAttributes = std::min(
            impl_->glCapabilities.limits.maxVertexAttributes,
            MaxVertexAttributes);
        capabilities.maxSampledTextures = std::min(
            impl_->glCapabilities.limits.maxCombinedTextureUnits,
            MaxCachedGlTextureUnits);
        capabilities.uniformBufferAlignment =
            impl_->glCapabilities.limits.uniformBufferOffsetAlignment;
    }
    return capabilities;
}

Base::Result<void> OpenGL33GraphicsBackend::CreateResource(
    ResourceHandle handle,
    const ResourceDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (!handle.IsValid() ||
        descriptor.type != handle.type ||
        descriptor.type == ResourceType::Invalid ||
        impl_->Find(handle) != nullptr) {
        return InvalidArgument(
            "OpenGL resource handle or descriptor is invalid");
    }

    Impl::ResourceRecord record;
    record.handle = handle;
    record.baseDescriptor = descriptor;
    if (descriptor.type == ResourceType::Buffer) {
        GlSizePtr size = 0;
        const GlEnum target =
            BufferTarget(descriptor.buffer.usage);
        if (target == 0U ||
            descriptor.buffer.sizeBytes == 0U ||
            !CheckedGlSize(descriptor.buffer.sizeBytes, size)) {
            return InvalidArgument(
                "OpenGL buffer descriptor is invalid");
        }
        Base::Result<void> scope = impl_->BeginStateScope();
        if (!scope) {
            return scope;
        }
        impl_->functions.genBuffers(1, &record.buffer);
        impl_->functions.bindBuffer(target, record.buffer);
        impl_->functions.bufferData(
            target,
            size,
            nullptr,
            BufferStorageUsage(descriptor.buffer.usage));
        Base::Result<void> error = impl_->CheckError(
            "OpenGL buffer creation failed");
        Base::Result<void> end = impl_->EndStateScope();
        if (!error || !end || record.buffer == 0U) {
            if (record.buffer != 0U) {
                impl_->functions.deleteBuffers(1, &record.buffer);
            }
            return !error ? error.GetStatus() :
                (!end ? end.GetStatus() :
                 InvalidState("OpenGL buffer name is invalid"));
        }
        record.configured = true;
    }

    Base::Result<void> appended =
        impl_->resources.TryPushBack(record);
    if (!appended) {
        impl_->DeleteNative(record);
        return appended;
    }
    return {};
}

void OpenGL33GraphicsBackend::DestroyResource(
    ResourceHandle handle) noexcept {
    if (impl_ == nullptr) {
        return;
    }
    const std::uint32_t index = impl_->FindIndex(handle);
    if (index == UINT32_MAX) {
        return;
    }
    Impl::ResourceRecord& record = impl_->resources[index];
    const bool canDelete =
        !impl_->deviceLost &&
        ValidateGlContextContract(impl_->context).HasValue() &&
        impl_->context.generation ==
            impl_->glCapabilities.contextGeneration;
    if (canDelete) {
        impl_->DeleteNative(record);
    }
    impl_->RemoveResourceAt(index);
}

Base::Result<void> OpenGL33GraphicsBackend::ConfigureTexture(
    ResourceHandle handle,
    const TextureResourceDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr ||
        (handle.type != ResourceType::Texture &&
         handle.type != ResourceType::RenderTarget) ||
        record->configured || record->external) {
        return InvalidArgument(
            "OpenGL texture resource cannot be configured");
    }
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }
    if (descriptor.width >
            impl_->glCapabilities.limits.maxTextureSize ||
        descriptor.height >
            impl_->glCapabilities.limits.maxTextureSize ||
        descriptor.arrayLayers >
            impl_->glCapabilities.limits.maxArrayTextureLayers ||
        descriptor.sampleCount >
            impl_->glCapabilities.limits.maxSamples ||
        descriptor.width != record->baseDescriptor.texture.width ||
        descriptor.height != record->baseDescriptor.texture.height ||
        BaseTextureFormat(descriptor.format) !=
            record->baseDescriptor.texture.format) {
        return Unsupported(
            "OpenGL texture descriptor exceeds context or resource limits");
    }
    if (descriptor.sampleCount > 1U &&
        (descriptor.arrayLayers > 1U ||
         descriptor.mipLevels > 1U)) {
        return Unsupported(
            "OpenGL 3.3 multisample array or mip textures are unsupported");
    }
    GlSize width = 0;
    GlSize height = 0;
    GlSize layers = 0;
    if (!CheckedGlSizeValue(descriptor.width, width) ||
        !CheckedGlSizeValue(descriptor.height, height) ||
        !CheckedGlSizeValue(descriptor.arrayLayers, layers)) {
        return OutOfRange(
            "OpenGL texture dimensions exceed GLsizei");
    }

    const GlEnum target = TextureTarget(descriptor);
    const GlInt internalFormat =
        TextureInternalFormat(descriptor.format);
    const GlEnum dataFormat =
        TextureDataFormat(descriptor.format);
    const GlEnum dataType =
        TextureDataType(descriptor.format);
    Base::Result<void> scope = impl_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    impl_->functions.genTextures(1, &record->texture);
    impl_->functions.activeTexture(GlConstant::Texture0);
    impl_->functions.bindTexture(target, record->texture);
    if (target == GlTexture2DMultisample) {
        impl_->functions.texImage2DMultisample(
            target,
            descriptor.sampleCount,
            static_cast<GlEnum>(internalFormat),
            width,
            height,
            GlConstant::True);
    } else {
        std::uint32_t mipWidth = descriptor.width;
        std::uint32_t mipHeight = descriptor.height;
        for (std::uint16_t mip = 0U;
             mip < descriptor.mipLevels;
             ++mip) {
            GlSize mipWidthValue = 0;
            GlSize mipHeightValue = 0;
            static_cast<void>(
                CheckedGlSizeValue(mipWidth, mipWidthValue));
            static_cast<void>(
                CheckedGlSizeValue(mipHeight, mipHeightValue));
            if (target == GlConstant::Texture2DArray) {
                impl_->functions.texImage3D(
                    target,
                    mip,
                    internalFormat,
                    mipWidthValue,
                    mipHeightValue,
                    layers,
                    0,
                    dataFormat,
                    dataType,
                    nullptr);
            } else {
                impl_->functions.texImage2D(
                    target,
                    mip,
                    internalFormat,
                    mipWidthValue,
                    mipHeightValue,
                    0,
                    dataFormat,
                    dataType,
                    nullptr);
            }
            mipWidth = std::max(1U, mipWidth / 2U);
            mipHeight = std::max(1U, mipHeight / 2U);
        }
        impl_->functions.texParameteri(
            target, GlTextureBaseLevel, 0);
        impl_->functions.texParameteri(
            target,
            GlTextureMaxLevel,
            static_cast<GlInt>(descriptor.mipLevels - 1U));
        impl_->functions.texParameteri(
            target, GlTextureMinFilter, GlLinear);
        impl_->functions.texParameteri(
            target, GlTextureMagFilter, GlLinear);
        impl_->functions.texParameteri(
            target, GlTextureWrapS, GlClampToEdge);
        impl_->functions.texParameteri(
            target, GlTextureWrapT, GlClampToEdge);
    }
    Base::Result<void> error = impl_->CheckError(
        "OpenGL texture creation failed");
    Base::Result<void> end = impl_->EndStateScope();
    if (!error || !end || record->texture == 0U) {
        if (record->texture != 0U) {
            impl_->functions.deleteTextures(1, &record->texture);
            record->texture = 0U;
        }
        return !error ? error.GetStatus() :
            (!end ? end.GetStatus() :
             InvalidState("OpenGL texture name is invalid"));
    }
    record->textureDescriptor = descriptor;
    record->textureTarget = target;
    record->configured = true;
    return {};
}

Base::Result<void> OpenGL33GraphicsBackend::ConfigureSampler(
    ResourceHandle handle,
    const SamplerDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::Sampler ||
        record->configured) {
        return InvalidArgument(
            "OpenGL sampler resource cannot be configured");
    }
    Base::Result<void> scope = impl_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    impl_->functions.genSamplers(1, &record->sampler);
    impl_->functions.samplerParameteri(
        record->sampler,
        GlTextureMinFilter,
        static_cast<GlInt>(MinFilter(descriptor)));
    impl_->functions.samplerParameteri(
        record->sampler,
        GlTextureMagFilter,
        static_cast<GlInt>(MagFilter(descriptor.magFilter)));
    impl_->functions.samplerParameteri(
        record->sampler,
        GlTextureWrapS,
        static_cast<GlInt>(AddressModeValue(descriptor.addressU)));
    impl_->functions.samplerParameteri(
        record->sampler,
        GlTextureWrapT,
        static_cast<GlInt>(AddressModeValue(descriptor.addressV)));
    impl_->functions.samplerParameteri(
        record->sampler,
        GlTextureWrapR,
        static_cast<GlInt>(AddressModeValue(descriptor.addressW)));
    impl_->functions.samplerParameterf(
        record->sampler, GlTextureMinLod, descriptor.minLod);
    impl_->functions.samplerParameterf(
        record->sampler, GlTextureMaxLod, descriptor.maxLod);
    Base::Result<void> error = impl_->CheckError(
        "OpenGL sampler creation failed");
    Base::Result<void> end = impl_->EndStateScope();
    if (!error || !end || record->sampler == 0U) {
        if (record->sampler != 0U) {
            impl_->functions.deleteSamplers(1, &record->sampler);
            record->sampler = 0U;
        }
        return !error ? error.GetStatus() :
            (!end ? end.GetStatus() :
             InvalidState("OpenGL sampler name is invalid"));
    }
    record->configured = true;
    return {};
}

Base::Result<void> OpenGL33GraphicsBackend::ConfigurePipeline(
    ResourceHandle handle,
    const PipelineDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::Pipeline ||
        record->configured ||
        descriptor.vertexShader.language != ShaderLanguage::Glsl330 ||
        descriptor.fragmentShader.language != ShaderLanguage::Glsl330) {
        return InvalidArgument(
            "OpenGL pipeline requires unconfigured GLSL 330 shaders");
    }
    if (descriptor.vertexShader.bytecodeSize >
            static_cast<std::uint32_t>(
                std::numeric_limits<GlInt>::max()) ||
        descriptor.fragmentShader.bytecodeSize >
            static_cast<std::uint32_t>(
                std::numeric_limits<GlInt>::max())) {
        return OutOfRange(
            "OpenGL shader source exceeds GLint length");
    }

    Base::Result<void> scope = impl_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    const auto compile = [&](const ShaderDescriptor& shader)
        noexcept -> Base::Result<GlUInt> {
        const GlEnum stage = shader.stage == ShaderStage::Vertex
            ? GlVertexShader
            : GlFragmentShader;
        const GlUInt nativeShader =
            impl_->functions.createShader(stage);
        if (nativeShader == 0U) {
            return InvalidState(
                "OpenGL failed to allocate a shader object");
        }
        const auto* source = reinterpret_cast<const GlChar*>(
            shader.bytecode);
        const GlInt length =
            static_cast<GlInt>(shader.bytecodeSize);
        impl_->functions.shaderSource(
            nativeShader, 1, &source, &length);
        impl_->functions.compileShader(nativeShader);
        GlInt compiled = 0;
        impl_->functions.getShaderiv(
            nativeShader, GlCompileStatus, &compiled);
        if (compiled == 0) {
            impl_->functions.deleteShader(nativeShader);
            return InvalidArgument(
                "OpenGL GLSL 330 shader compilation failed");
        }
        return nativeShader;
    };

    Base::Result<GlUInt> vertex =
        compile(descriptor.vertexShader);
    if (!vertex) {
        static_cast<void>(impl_->EndStateScope());
        return vertex.GetStatus();
    }
    Base::Result<GlUInt> fragment =
        compile(descriptor.fragmentShader);
    if (!fragment) {
        impl_->functions.deleteShader(vertex.Value());
        static_cast<void>(impl_->EndStateScope());
        return fragment.GetStatus();
    }

    const GlUInt program = impl_->functions.createProgram();
    impl_->functions.attachShader(program, vertex.Value());
    impl_->functions.attachShader(program, fragment.Value());
    impl_->functions.linkProgram(program);
    GlInt linked = 0;
    impl_->functions.getProgramiv(program, GlLinkStatus, &linked);
    impl_->functions.detachShader(program, vertex.Value());
    impl_->functions.detachShader(program, fragment.Value());
    impl_->functions.deleteShader(vertex.Value());
    impl_->functions.deleteShader(fragment.Value());
    if (program == 0U || linked == 0) {
        if (program != 0U) {
            impl_->functions.deleteProgram(program);
        }
        static_cast<void>(impl_->EndStateScope());
        return InvalidArgument(
            "OpenGL GLSL 330 program link failed");
    }

    impl_->functions.useProgram(program);
    const std::uint32_t bindingCount = std::min(
        impl_->glCapabilities.limits.maxCombinedTextureUnits,
        MaxCachedGlTextureUnits);
    char name[32]{};
    for (std::uint32_t slot = 0U;
         slot < bindingCount;
         ++slot) {
        const int textureNameLength = std::snprintf(
            name, sizeof(name), "AeroTexture%u", slot);
        if (textureNameLength > 0) {
            const GlInt location =
                impl_->functions.getUniformLocation(program, name);
            if (location >= 0) {
                impl_->functions.uniform1i(
                    location, static_cast<GlInt>(slot));
            }
        }
        const int blockNameLength = std::snprintf(
            name, sizeof(name), "AeroUniform%u", slot);
        if (blockNameLength > 0) {
            const GlUInt block =
                impl_->functions.getUniformBlockIndex(program, name);
            if (block != GlInvalidIndex) {
                impl_->functions.uniformBlockBinding(
                    program, block, slot);
            }
        }
    }

    Base::Result<void> error = impl_->CheckError(
        "OpenGL pipeline creation failed");
    Base::Result<void> end = impl_->EndStateScope();
    if (!error || !end) {
        impl_->functions.deleteProgram(program);
        return !error ? error.GetStatus() : end.GetStatus();
    }
    record->program = program;
    record->vertexLayout = descriptor.vertexLayout;
    record->topology = descriptor.topology;
    record->blend = descriptor.blend;
    record->raster = descriptor.raster;
    record->depthStencil = descriptor.depthStencil;
    record->configured = true;
    return {};
}

Base::Result<void>
OpenGL33GraphicsBackend::ImportExternalRenderTarget(
    ResourceHandle handle,
    const OpenGL33ExternalRenderTargetDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::RenderTarget ||
        record->configured ||
        descriptor.contextGeneration != impl_->context.generation ||
        (descriptor.framebuffer == 0U &&
         descriptor.colorTexture == 0U &&
         !descriptor.defaultFramebuffer) ||
        (descriptor.defaultFramebuffer &&
         (descriptor.framebuffer != 0U ||
          descriptor.colorTexture != 0U)) ||
        descriptor.texture.width !=
            record->baseDescriptor.texture.width ||
        descriptor.texture.height !=
            record->baseDescriptor.texture.height ||
        BaseTextureFormat(descriptor.texture.format) !=
            record->baseDescriptor.texture.format ||
        !HasTextureUsage(
            descriptor.texture.usage,
            TextureUsage::RenderTarget) ||
        descriptor.texture.format ==
            GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument(
            "OpenGL external render-target descriptor is invalid");
    }
    record->textureDescriptor = descriptor.texture;
    record->textureTarget = TextureTarget(descriptor.texture);
    record->texture = descriptor.colorTexture;
    record->externalFramebuffer = descriptor.framebuffer;
    record->externalDepthStencilTexture =
        descriptor.depthStencilTexture;
    record->external = true;
    record->externalDefaultFramebuffer =
        descriptor.defaultFramebuffer;
    record->configured = true;
    return {};
}

Base::Result<void>
OpenGL33GraphicsBackend::ImportExternalTexture(
    ResourceHandle handle,
    const OpenGL33ExternalTextureDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::Texture ||
        record->configured ||
        descriptor.texture == 0U ||
        descriptor.contextGeneration != impl_->context.generation ||
        descriptor.descriptor.width !=
            record->baseDescriptor.texture.width ||
        descriptor.descriptor.height !=
            record->baseDescriptor.texture.height ||
        BaseTextureFormat(descriptor.descriptor.format) !=
            record->baseDescriptor.texture.format ||
        descriptor.descriptor.sampleCount != 1U ||
        !HasTextureUsage(
            descriptor.descriptor.usage,
            TextureUsage::Sampled)) {
        return InvalidArgument(
            "OpenGL external texture descriptor is invalid");
    }
    record->textureDescriptor = descriptor.descriptor;
    record->textureTarget = TextureTarget(descriptor.descriptor);
    record->texture = descriptor.texture;
    record->external = true;
    record->configured = true;
    return {};
}

Base::Result<void> OpenGL33GraphicsBackend::Submit(
    const CommandList& commands,
    FenceValue signalFence) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (signalFence == 0U ||
        signalFence <= impl_->lastSubmittedFence) {
        return InvalidArgument(
            "OpenGL submission fence must increase monotonically");
    }

    Base::Result<void> scope = impl_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    impl_->ResetSubmission();
    const auto fail = [&](Base::Status status) noexcept
        -> Base::Result<void> {
        impl_->ResetSubmission();
        static_cast<void>(impl_->EndStateScope());
        return status;
    };

    const Base::Span<const Command> encoded = commands.Commands();
    const Base::Span<const std::uint8_t> uploadBytes =
        commands.UploadBytes();
    for (std::uint32_t commandIndex = 0U;
         commandIndex < encoded.Size();
         ++commandIndex) {
        const Command& command = encoded[commandIndex];
        switch (command.kind) {
        case CommandKind::UploadBuffer: {
            Impl::ResourceRecord* record =
                impl_->Find(command.resource0);
            if (impl_->inRenderPass ||
                record == nullptr || record->buffer == 0U ||
                command.uploadOffset > uploadBytes.Size() ||
                command.uploadSize >
                    uploadBytes.Size() - command.uploadOffset ||
                command.resourceOffset >
                    record->baseDescriptor.buffer.sizeBytes ||
                command.uploadSize >
                    record->baseDescriptor.buffer.sizeBytes -
                        command.resourceOffset) {
                return fail(InvalidArgument(
                    "OpenGL buffer upload is invalid"));
            }
            GlSizePtr offset = 0;
            GlSizePtr size = 0;
            if (!CheckedGlSize(command.resourceOffset, offset) ||
                !CheckedGlSize(command.uploadSize, size)) {
                return fail(OutOfRange(
                    "OpenGL buffer upload exceeds pointer range"));
            }
            const GlEnum target = BufferTarget(
                record->baseDescriptor.buffer.usage);
            impl_->functions.bindBuffer(target, record->buffer);
            impl_->functions.bufferSubData(
                target,
                offset,
                size,
                uploadBytes.Data() + command.uploadOffset);
            break;
        }
        case CommandKind::UploadTexture: {
            Impl::ResourceRecord* record =
                impl_->Find(command.resource0);
            if (impl_->inRenderPass ||
                record == nullptr || record->texture == 0U ||
                !record->configured ||
                record->external ||
                record->textureDescriptor.sampleCount != 1U ||
                !HasTextureUsage(
                    record->textureDescriptor.usage,
                    TextureUsage::CopyDestination) ||
                command.uploadOffset > uploadBytes.Size() ||
                command.uploadSize >
                    uploadBytes.Size() - command.uploadOffset) {
                return fail(InvalidArgument(
                    "OpenGL texture upload is invalid"));
            }
            const TextureRegion& region = command.textureRegion;
            if (region.width == 0U || region.height == 0U ||
                region.mipLevel >=
                    record->textureDescriptor.mipLevels ||
                region.arrayLayer >=
                    record->textureDescriptor.arrayLayers) {
                return fail(InvalidArgument(
                    "OpenGL texture upload region is invalid"));
            }
            std::uint32_t mipWidth =
                record->textureDescriptor.width;
            std::uint32_t mipHeight =
                record->textureDescriptor.height;
            for (std::uint16_t mip = 0U;
                 mip < region.mipLevel;
                 ++mip) {
                mipWidth = std::max(1U, mipWidth / 2U);
                mipHeight = std::max(1U, mipHeight / 2U);
            }
            if (region.x > mipWidth ||
                region.width > mipWidth - region.x ||
                region.y > mipHeight ||
                region.height > mipHeight - region.y ||
                region.x >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlInt>::max()) ||
                region.y >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlInt>::max())) {
                return fail(InvalidArgument(
                    "OpenGL texture upload exceeds the mip extent"));
            }
            const std::uint32_t bytesPerPixel =
                BytesPerPixel(record->textureDescriptor.format);
            if (bytesPerPixel == 0U ||
                region.width >
                    UINT32_MAX / bytesPerPixel ||
                region.bytesPerRow <
                    region.width * bytesPerPixel ||
                region.bytesPerRow % bytesPerPixel != 0U ||
                region.bytesPerRow / bytesPerPixel >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlInt>::max()) ||
                region.height - 1U >
                    (UINT32_MAX -
                     region.width * bytesPerPixel) /
                        region.bytesPerRow ||
                command.uploadSize <
                    (region.height - 1U) *
                        region.bytesPerRow +
                    region.width * bytesPerPixel) {
                return fail(InvalidArgument(
                    "OpenGL texture upload row pitch is invalid"));
            }
            GlSize width = 0;
            GlSize height = 0;
            if (!CheckedGlSizeValue(
                    command.textureRegion.width, width) ||
                !CheckedGlSizeValue(
                    command.textureRegion.height, height)) {
                return fail(OutOfRange(
                    "OpenGL texture upload exceeds GLsizei"));
            }
            impl_->functions.activeTexture(GlConstant::Texture0);
            impl_->functions.bindTexture(
                record->textureTarget, record->texture);
            impl_->functions.pixelStorei(
                GlConstant::UnpackAlignment, 1);
                impl_->functions.pixelStorei(
                GlConstant::UnpackRowLength,
                static_cast<GlInt>(
                    region.bytesPerRow /
                    bytesPerPixel));
            const void* source =
                uploadBytes.Data() + command.uploadOffset;
            if (record->textureTarget ==
                GlConstant::Texture2DArray) {
                impl_->functions.texSubImage3D(
                    record->textureTarget,
                    region.mipLevel,
                    static_cast<GlInt>(region.x),
                    static_cast<GlInt>(region.y),
                    region.arrayLayer,
                    width,
                    height,
                    1,
                    TextureDataFormat(
                        record->textureDescriptor.format),
                    TextureDataType(
                        record->textureDescriptor.format),
                    source);
            } else {
                impl_->functions.texSubImage2D(
                    record->textureTarget,
                    region.mipLevel,
                    static_cast<GlInt>(region.x),
                    static_cast<GlInt>(region.y),
                    width,
                    height,
                    TextureDataFormat(
                        record->textureDescriptor.format),
                    TextureDataType(
                        record->textureDescriptor.format),
                    source);
            }
            break;
        }
        case CommandKind::BeginRenderPass: {
            if (impl_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL render passes cannot be nested"));
            }
            const RenderPassDescriptor& pass = command.renderPass;
            Impl::ResourceRecord* firstColor = nullptr;
            if (pass.colorAttachmentCount > 0U) {
                firstColor = impl_->Find(
                    pass.colorAttachments[0U].target);
            }
            const bool directExternal =
                pass.colorAttachmentCount == 1U &&
                !pass.hasDepthStencil &&
                firstColor != nullptr &&
                firstColor->external &&
                (firstColor->externalFramebuffer != 0U ||
                 firstColor->externalDefaultFramebuffer);
            GlUInt framebuffer = directExternal
                ? firstColor->externalFramebuffer
                : impl_->submissionFramebuffer;
            Base::Result<void> bindFramebuffer =
                impl_->stateCache.BindDrawFramebuffer(framebuffer);
            if (!bindFramebuffer) {
                return fail(bindFramebuffer.GetStatus());
            }

            GlEnum drawBuffers[MaxColorAttachments]{};
            std::uint32_t targetWidth = 0U;
            std::uint32_t targetHeight = 0U;
            for (std::uint32_t index = 0U;
                 index < pass.colorAttachmentCount;
                 ++index) {
                Impl::ResourceRecord* attachment =
                    impl_->Find(pass.colorAttachments[index].target);
                if (attachment == nullptr ||
                    !attachment->configured ||
                    attachment->handle.type !=
                        ResourceType::RenderTarget ||
                    !HasTextureUsage(
                        attachment->textureDescriptor.usage,
                        TextureUsage::RenderTarget) ||
                    attachment->textureDescriptor.format ==
                        GraphicsTextureFormat::Depth24Stencil8) {
                    return fail(InvalidArgument(
                        "OpenGL color attachment is invalid"));
                }
                if (index == 0U) {
                    targetWidth =
                        attachment->textureDescriptor.width;
                    targetHeight =
                        attachment->textureDescriptor.height;
                } else if (
                    targetWidth !=
                        attachment->textureDescriptor.width ||
                    targetHeight !=
                        attachment->textureDescriptor.height) {
                    return fail(InvalidArgument(
                        "OpenGL render attachments have different dimensions"));
                }
                if (!directExternal) {
                    if (attachment->texture == 0U ||
                        attachment->textureDescriptor.arrayLayers != 1U) {
                        return fail(Unsupported(
                            "OpenGL array render targets require a layer-aware path"));
                    }
                    impl_->functions.framebufferTexture2D(
                        GlConstant::DrawFramebuffer,
                        GlConstant::ColorAttachment0 + index,
                        attachment->textureTarget,
                        attachment->texture,
                        0);
                }
                drawBuffers[index] =
                    GlConstant::ColorAttachment0 + index;
                impl_->activeAttachmentTextures[index] =
                    attachment->texture;
            }
            if (!directExternal) {
                for (std::uint32_t index =
                        pass.colorAttachmentCount;
                     index < MaxColorAttachments;
                     ++index) {
                    impl_->functions.framebufferTexture2D(
                        GlConstant::DrawFramebuffer,
                        GlConstant::ColorAttachment0 + index,
                        GlConstant::Texture2D,
                        0U,
                        0);
                }
            }

            if (pass.hasDepthStencil) {
                Impl::ResourceRecord* depth = impl_->Find(
                    pass.depthStencil.target);
                if (depth == nullptr ||
                    !depth->configured ||
                    depth->texture == 0U ||
                    depth->handle.type !=
                        ResourceType::RenderTarget ||
                    !HasTextureUsage(
                        depth->textureDescriptor.usage,
                        TextureUsage::RenderTarget) ||
                    depth->textureDescriptor.format !=
                        GraphicsTextureFormat::Depth24Stencil8 ||
                    depth->textureDescriptor.arrayLayers != 1U) {
                    return fail(InvalidArgument(
                        "OpenGL depth-stencil attachment is invalid"));
                }
                if (targetWidth == 0U) {
                    targetWidth = depth->textureDescriptor.width;
                    targetHeight = depth->textureDescriptor.height;
                } else if (
                    targetWidth != depth->textureDescriptor.width ||
                    targetHeight != depth->textureDescriptor.height) {
                    return fail(InvalidArgument(
                        "OpenGL depth attachment dimensions differ"));
                }
                impl_->functions.framebufferTexture2D(
                    GlConstant::DrawFramebuffer,
                    GlDepthStencilAttachment,
                    depth->textureTarget,
                    depth->texture,
                    0);
                impl_->activeAttachmentTextures[
                    MaxColorAttachments] = depth->texture;
            } else if (!directExternal) {
                impl_->functions.framebufferTexture2D(
                    GlConstant::DrawFramebuffer,
                    GlDepthStencilAttachment,
                    GlConstant::Texture2D,
                    0U,
                    0);
            }

            if (!directExternal) {
                impl_->functions.drawBuffers(
                    static_cast<GlSize>(
                        pass.colorAttachmentCount),
                    pass.colorAttachmentCount > 0U
                        ? drawBuffers
                        : nullptr);
                if (impl_->functions.checkFramebufferStatus(
                        GlConstant::DrawFramebuffer) !=
                    GlConstant::FramebufferComplete) {
                    return fail(InvalidState(
                        "OpenGL framebuffer is incomplete"));
                }
            }
            Base::Result<GlRectangleState> viewport =
                ToGlRectangle(pass.renderArea, targetHeight);
            if (!viewport) {
                return fail(viewport.GetStatus());
            }
            Base::Result<void> viewportResult =
                impl_->stateCache.SetViewport(viewport.Value());
            if (!viewportResult) {
                return fail(viewportResult.GetStatus());
            }
            Base::Result<void> scissorResult =
                impl_->stateCache.SetScissor(
                    true, viewport.Value());
            if (!scissorResult) {
                return fail(scissorResult.GetStatus());
            }
            impl_->renderPassHeight = targetHeight;

            for (std::uint32_t index = 0U;
                 index < pass.colorAttachmentCount;
                 ++index) {
                const ColorAttachmentDescriptor& attachment =
                    pass.colorAttachments[index];
                if (attachment.load == LoadOperation::Clear) {
                    const GlFloat color[4] = {
                        attachment.clearColor.red,
                        attachment.clearColor.green,
                        attachment.clearColor.blue,
                        attachment.clearColor.alpha};
                    impl_->functions.clearBufferfv(
                        GlColor,
                        static_cast<GlInt>(index),
                        color);
                }
            }
            if (pass.hasDepthStencil) {
                const bool clearDepth =
                    pass.depthStencil.depthLoad ==
                    LoadOperation::Clear;
                const bool clearStencil =
                    pass.depthStencil.stencilLoad ==
                    LoadOperation::Clear;
                if (clearDepth && clearStencil) {
                    impl_->functions.clearBufferfi(
                        GlDepthStencil,
                        0,
                        pass.depthStencil.clearDepth,
                        static_cast<GlInt>(
                            pass.depthStencil.clearStencil));
                } else if (clearDepth) {
                    const GlFloat depth =
                        pass.depthStencil.clearDepth;
                    impl_->functions.clearBufferfv(
                        GlDepth, 0, &depth);
                } else if (clearStencil) {
                    const GlInt stencil =
                        static_cast<GlInt>(
                            pass.depthStencil.clearStencil);
                    impl_->functions.clearBufferiv(
                        GlStencil, 0, &stencil);
                }
            }
            impl_->inRenderPass = true;
            break;
        }
        case CommandKind::EndRenderPass:
            if (!impl_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL render pass is not active"));
            }
            impl_->inRenderPass = false;
            for (GlUInt& attachment :
                 impl_->activeAttachmentTextures) {
                attachment = 0U;
            }
            break;
        case CommandKind::BindPipeline: {
            if (!impl_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL pipeline binding requires a render pass"));
            }
            Impl::ResourceRecord* pipeline =
                impl_->Find(command.resource0);
            if (pipeline == nullptr ||
                pipeline->program == 0U ||
                !pipeline->configured) {
                return fail(InvalidArgument(
                    "OpenGL pipeline is not configured"));
            }
            Base::Result<void> programResult =
                impl_->stateCache.UseProgram(pipeline->program);
            Base::Result<void> vaoResult =
                impl_->stateCache.BindVertexArray(
                    impl_->vertexArray);
            Base::Result<void> blendResult =
                impl_->stateCache.SetBlendState(
                    ToGlBlendState(pipeline->blend));
            Base::Result<void> depthResult =
                impl_->stateCache.SetDepthState(
                    ToGlDepthState(pipeline->depthStencil));
            Base::Result<void> stencilResult =
                impl_->stateCache.SetStencilState(
                    ToGlStencilState(pipeline->depthStencil));
            Base::Result<void> rasterResult =
                impl_->stateCache.SetRasterState(
                    ToGlRasterState(pipeline->raster));
            if (!programResult || !vaoResult || !blendResult ||
                !depthResult || !stencilResult || !rasterResult) {
                return fail(InvalidState(
                    "OpenGL pipeline state binding failed"));
            }
            impl_->currentPipeline = command.resource0;
            impl_->vertexStateDirty = true;
            break;
        }
        case CommandKind::BindVertexBuffer:
            {
            const Impl::ResourceRecord* pipeline =
                impl_->Find(impl_->currentPipeline);
            const Impl::ResourceRecord* buffer =
                impl_->Find(command.resource0);
            if (!impl_->inRenderPass || pipeline == nullptr ||
                buffer == nullptr || buffer->buffer == 0U ||
                buffer->baseDescriptor.buffer.usage !=
                    BufferUsage::Vertex ||
                command.slot >=
                    pipeline->vertexLayout.bufferCount ||
                command.resourceOffset >
                    buffer->baseDescriptor.buffer.sizeBytes) {
                return fail(InvalidArgument(
                    "OpenGL vertex-buffer binding is invalid"));
            }
            impl_->vertexBuffers[command.slot] = {
                command.resource0, command.resourceOffset};
            impl_->vertexStateDirty = true;
            break;
            }
        case CommandKind::BindIndexBuffer:
            {
            const Impl::ResourceRecord* buffer =
                impl_->Find(command.resource0);
            if (!impl_->inRenderPass ||
                buffer == nullptr || buffer->buffer == 0U ||
                buffer->baseDescriptor.buffer.usage !=
                    BufferUsage::Index ||
                command.resourceOffset >
                    buffer->baseDescriptor.buffer.sizeBytes) {
                return fail(InvalidArgument(
                    "OpenGL index-buffer binding is invalid"));
            }
            impl_->indexBuffer = {
                command.resource0, command.resourceOffset};
            impl_->indexType = command.indexType;
            impl_->vertexStateDirty = true;
            break;
            }
        case CommandKind::BindUniformBuffer: {
            if (!impl_->inRenderPass ||
                command.slot >=
                    impl_->glCapabilities.limits.
                        maxUniformBufferBindings) {
                return fail(InvalidArgument(
                    "OpenGL uniform-buffer slot is invalid"));
            }
            Impl::ResourceRecord* buffer =
                impl_->Find(command.resource0);
            if (buffer == nullptr || buffer->buffer == 0U ||
                buffer->baseDescriptor.buffer.usage !=
                    BufferUsage::Uniform ||
                command.resourceSize == 0U ||
                command.resourceOffset %
                    impl_->glCapabilities.limits.
                        uniformBufferOffsetAlignment != 0U ||
                command.resourceSize >
                    impl_->glCapabilities.limits.
                        maxUniformBlockSize ||
                command.resourceOffset >
                    buffer->baseDescriptor.buffer.sizeBytes ||
                command.resourceSize >
                    buffer->baseDescriptor.buffer.sizeBytes -
                        command.resourceOffset) {
                return fail(InvalidArgument(
                    "OpenGL uniform-buffer binding is invalid"));
            }
            GlIntPtr offset = 0;
            GlSizePtr size = 0;
            if (!CheckedGlSize(command.resourceOffset, offset) ||
                !CheckedGlSize(command.resourceSize, size)) {
                return fail(OutOfRange(
                    "OpenGL uniform-buffer range exceeds pointer range"));
            }
            Base::Result<void> bindResult =
                impl_->stateCache.BindUniformBuffer(buffer->buffer);
            if (!bindResult) {
                return fail(bindResult.GetStatus());
            }
            impl_->functions.bindBufferRange(
                GlConstant::UniformBuffer,
                command.slot,
                buffer->buffer,
                offset,
                size);
            break;
        }
        case CommandKind::BindTextureSampler: {
            if (!impl_->inRenderPass ||
                command.slot >=
                    QueryGraphicsCapabilities().
                        maxSampledTextures) {
                return fail(InvalidArgument(
                    "OpenGL texture slot is invalid"));
            }
            Impl::ResourceRecord* texture =
                impl_->Find(command.resource0);
            Impl::ResourceRecord* sampler =
                impl_->Find(command.resource1);
            if (texture == nullptr || sampler == nullptr ||
                texture->texture == 0U ||
                sampler->sampler == 0U ||
                !HasTextureUsage(
                    texture->textureDescriptor.usage,
                    TextureUsage::Sampled) ||
                texture->textureTarget ==
                    GlTexture2DMultisample ||
                impl_->IsActiveAttachment(texture->texture)) {
                return fail(InvalidArgument(
                    "OpenGL texture or sampler binding is invalid"));
            }
            Base::Result<void> bindResult =
                impl_->stateCache.BindTextureSampler(
                    command.slot,
                    texture->textureTarget,
                    texture->texture,
                    sampler->sampler);
            if (!bindResult) {
                return fail(bindResult.GetStatus());
            }
            break;
        }
        case CommandKind::SetScissor: {
            if (!impl_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL scissor requires a render pass"));
            }
            Base::Result<GlRectangleState> rectangle =
                ToGlRectangle(
                    command.rect, impl_->renderPassHeight);
            if (!rectangle) {
                return fail(rectangle.GetStatus());
            }
            const Impl::ResourceRecord* pipeline =
                impl_->Find(impl_->currentPipeline);
            const bool enabled =
                pipeline == nullptr ||
                pipeline->raster.scissorEnabled;
            Base::Result<void> setResult =
                impl_->stateCache.SetScissor(
                    enabled, rectangle.Value());
            if (!setResult) {
                return fail(setResult.GetStatus());
            }
            break;
        }
        case CommandKind::Draw: {
            if (!impl_->inRenderPass ||
                !impl_->currentPipeline.IsValid() ||
                command.count == 0U ||
                command.instanceCount == 0U) {
                return fail(InvalidState(
                    "OpenGL draw state is incomplete"));
            }
            if (command.firstInstance != 0U) {
                return fail(Unsupported(
                    "OpenGL 3.3 does not support base-instance draws"));
            }
            if (command.first >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlInt>::max()) ||
                command.count >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max()) ||
                command.instanceCount >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max())) {
                return fail(OutOfRange(
                    "OpenGL draw arguments exceed GL integer range"));
            }
            Base::Result<void> prepared =
                impl_->PrepareVertexInput(false);
            if (!prepared) {
                return fail(prepared.GetStatus());
            }
            const Impl::ResourceRecord* pipeline =
                impl_->Find(impl_->currentPipeline);
            const GlEnum topology =
                Topology(pipeline->topology);
            if (command.instanceCount == 1U) {
                impl_->functions.drawArrays(
                    topology,
                    static_cast<GlInt>(command.first),
                    static_cast<GlSize>(command.count));
            } else {
                impl_->functions.drawArraysInstanced(
                    topology,
                    static_cast<GlInt>(command.first),
                    static_cast<GlSize>(command.count),
                    static_cast<GlSize>(command.instanceCount));
            }
            break;
        }
        case CommandKind::DrawIndexed: {
            if (!impl_->inRenderPass ||
                !impl_->currentPipeline.IsValid() ||
                command.count == 0U ||
                command.instanceCount == 0U) {
                return fail(InvalidState(
                    "OpenGL indexed-draw state is incomplete"));
            }
            if (command.firstInstance != 0U) {
                return fail(Unsupported(
                    "OpenGL 3.3 does not support base-instance draws"));
            }
            if (command.count >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max()) ||
                command.instanceCount >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max())) {
                return fail(OutOfRange(
                    "OpenGL indexed-draw arguments exceed GL integer range"));
            }
            Base::Result<void> prepared =
                impl_->PrepareVertexInput(true);
            if (!prepared) {
                return fail(prepared.GetStatus());
            }
            const std::uint32_t indexSize =
                impl_->indexType == IndexType::UInt16
                ? 2U
                : 4U;
            if (command.first >
                    (std::numeric_limits<std::uintptr_t>::max() -
                     impl_->indexBuffer.offset) /
                        indexSize) {
                return fail(OutOfRange(
                    "OpenGL index offset exceeds pointer range"));
            }
            const std::uintptr_t pointer =
                static_cast<std::uintptr_t>(
                    impl_->indexBuffer.offset) +
                static_cast<std::uintptr_t>(command.first) *
                    indexSize;
            const Impl::ResourceRecord* pipeline =
                impl_->Find(impl_->currentPipeline);
            const GlEnum topology =
                Topology(pipeline->topology);
            const GlEnum indexType =
                impl_->indexType == IndexType::UInt16
                ? GlUnsignedShort
                : GlUnsignedInt;
            if (command.instanceCount == 1U) {
                impl_->functions.drawElementsBaseVertex(
                    topology,
                    static_cast<GlSize>(command.count),
                    indexType,
                    reinterpret_cast<const void*>(pointer),
                    command.baseVertex);
            } else {
                impl_->functions.drawElementsInstancedBaseVertex(
                    topology,
                    static_cast<GlSize>(command.count),
                    indexType,
                    reinterpret_cast<const void*>(pointer),
                    static_cast<GlSize>(command.instanceCount),
                    command.baseVertex);
            }
            break;
        }
        default:
            return fail(InvalidArgument(
                "OpenGL graphics command kind is invalid"));
        }
    }

    if (impl_->inRenderPass) {
        return fail(InvalidState(
            "OpenGL command list ended inside a render pass"));
    }
    Base::Result<void> error = impl_->CheckError(
        "OpenGL command submission failed");
    if (!error) {
        return fail(error.GetStatus());
    }
    GlSync sync = impl_->functions.fenceSync(
        GlConstant::SyncGpuCommandsComplete, 0U);
    if (sync == nullptr) {
        return fail(InvalidState(
            "OpenGL fence creation failed"));
    }
    impl_->functions.flush();
    Base::Result<void> end = impl_->EndStateScope();
    if (!end) {
        impl_->functions.deleteSync(sync);
        return end;
    }
    Impl::PendingFence fence;
    fence.value = signalFence;
    fence.sync = sync;
    Base::Result<void> appended =
        impl_->pendingFences.TryPushBack(fence);
    if (!appended) {
        impl_->functions.deleteSync(sync);
        return appended;
    }
    impl_->lastSubmittedFence = signalFence;
    impl_->ResetSubmission();
    return {};
}

FenceValue OpenGL33GraphicsBackend::LastSubmittedFence() const noexcept {
    return impl_ != nullptr ? impl_->lastSubmittedFence : 0U;
}

FenceValue OpenGL33GraphicsBackend::CompletedFence() const noexcept {
    if (impl_ == nullptr || impl_->deviceLost ||
        !ValidateGlContextContract(impl_->context)) {
        return impl_ != nullptr ? impl_->completedFence : 0U;
    }
    impl_->PollFences();
    return impl_->completedFence;
}

bool OpenGL33GraphicsBackend::IsDeviceLost() const noexcept {
    return impl_ == nullptr ||
        !impl_->initialized ||
        impl_->deviceLost;
}

Base::Result<void> OpenGL33GraphicsBackend::WaitForFence(
    FenceValue fence,
    std::uint64_t timeoutNanoseconds) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (fence == 0U || fence > impl_->lastSubmittedFence ||
        timeoutNanoseconds == 0U) {
        return InvalidArgument(
            "OpenGL wait fence or timeout is invalid");
    }
    if (CompletedFence() >= fence) {
        return {};
    }
    for (const Impl::PendingFence& pending :
         impl_->pendingFences) {
        if (pending.value < fence) {
            continue;
        }
        const GlEnum result = impl_->functions.clientWaitSync(
            pending.sync,
            GlConstant::SyncFlushCommandsBit,
            timeoutNanoseconds);
        if (result == GlConstant::TimeoutExpired) {
            return InvalidState(
                "Timed out while waiting for an OpenGL fence");
        }
        if (result == GlConstant::WaitFailed) {
            impl_->deviceLost = true;
            return InvalidState(
                "OpenGL fence wait failed");
        }
        impl_->PollFences();
        return CompletedFence() >= fence
            ? Base::Result<void>{}
            : Base::Result<void>{
                InvalidState(
                    "OpenGL fence did not reach the requested value")};
    }
    return NotFound("OpenGL fence was not found");
}

Base::Result<void> OpenGL33GraphicsBackend::ReadbackTexture(
    ResourceHandle handle,
    Base::Span<std::uint8_t> destination,
    std::uint32_t destinationRowPitch) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr ||
        !record->configured ||
        record->texture == 0U ||
        record->textureDescriptor.sampleCount != 1U ||
        record->textureDescriptor.arrayLayers != 1U ||
        record->textureDescriptor.format ==
            GraphicsTextureFormat::Depth24Stencil8 ||
        !HasTextureUsage(
            record->textureDescriptor.usage,
            TextureUsage::CopySource)) {
        return InvalidArgument(
            "OpenGL texture is not eligible for readback");
    }
    const std::uint32_t bytesPerPixel =
        BytesPerPixel(record->textureDescriptor.format);
    if (record->textureDescriptor.width >
            UINT32_MAX / bytesPerPixel) {
        return OutOfRange(
            "OpenGL readback row size overflow");
    }
    const std::uint32_t rowBytes =
        record->textureDescriptor.width * bytesPerPixel;
    if (destinationRowPitch < rowBytes ||
        record->textureDescriptor.height >
            UINT32_MAX / destinationRowPitch ||
        destination.Size() <
            record->textureDescriptor.height *
                destinationRowPitch) {
        return InvalidArgument(
            "OpenGL readback destination is too small");
    }

    Base::Result<void> scope = impl_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    Base::Result<void> bindResult =
        impl_->stateCache.BindDrawFramebuffer(
            impl_->readbackFramebuffer);
    if (!bindResult) {
        static_cast<void>(impl_->EndStateScope());
        return bindResult;
    }
    Base::Result<void> bindReadResult =
        impl_->stateCache.BindReadFramebuffer(
            impl_->readbackFramebuffer);
    if (!bindReadResult) {
        static_cast<void>(impl_->EndStateScope());
        return bindReadResult;
    }
    impl_->functions.framebufferTexture2D(
        GlConstant::DrawFramebuffer,
        GlConstant::ColorAttachment0,
        record->textureTarget,
        record->texture,
        0);
    if (impl_->functions.checkFramebufferStatus(
            GlConstant::DrawFramebuffer) !=
        GlConstant::FramebufferComplete) {
        static_cast<void>(impl_->EndStateScope());
        return InvalidState(
            "OpenGL readback framebuffer is incomplete");
    }
    GlInt packAlignment = 0;
    GlInt packRowLength = 0;
    GlInt packSkipRows = 0;
    GlInt packSkipPixels = 0;
    impl_->functions.getIntegerv(
        GlConstant::PackAlignment, &packAlignment);
    impl_->functions.getIntegerv(
        GlConstant::PackRowLength, &packRowLength);
    impl_->functions.getIntegerv(
        GlConstant::PackSkipRows, &packSkipRows);
    impl_->functions.getIntegerv(
        GlConstant::PackSkipPixels, &packSkipPixels);
    impl_->functions.pixelStorei(
        GlConstant::PackAlignment, 1);
    impl_->functions.pixelStorei(
        GlConstant::PackRowLength, 0);
    impl_->functions.pixelStorei(
        GlConstant::PackSkipRows, 0);
    impl_->functions.pixelStorei(
        GlConstant::PackSkipPixels, 0);
    impl_->functions.readBuffer(GlConstant::ColorAttachment0);

    GlSize width = 0;
    static_cast<void>(CheckedGlSizeValue(
        record->textureDescriptor.width, width));
    for (std::uint32_t row = 0U;
         row < record->textureDescriptor.height;
         ++row) {
        const GlInt sourceY = static_cast<GlInt>(
            record->textureDescriptor.height - row - 1U);
        impl_->functions.readPixels(
            0,
            sourceY,
            width,
            1,
            TextureDataFormat(record->textureDescriptor.format),
            TextureDataType(record->textureDescriptor.format),
            destination.Data() +
                static_cast<std::size_t>(row) *
                    destinationRowPitch);
    }
    impl_->functions.pixelStorei(
        GlConstant::PackAlignment, packAlignment);
    impl_->functions.pixelStorei(
        GlConstant::PackRowLength, packRowLength);
    impl_->functions.pixelStorei(
        GlConstant::PackSkipRows, packSkipRows);
    impl_->functions.pixelStorei(
        GlConstant::PackSkipPixels, packSkipPixels);
    Base::Result<void> error = impl_->CheckError(
        "OpenGL texture readback failed");
    Base::Result<void> end = impl_->EndStateScope();
    if (!error) {
        return error;
    }
    return end;
}

Base::Result<std::uint64_t>
OpenGL33GraphicsBackend::ReadbackTextureChecksum(
    ResourceHandle handle) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    const Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr || !record->configured) {
        return InvalidArgument(
            "OpenGL checksum texture is invalid");
    }
    const std::uint32_t bytesPerPixel =
        BytesPerPixel(record->textureDescriptor.format);
    if (bytesPerPixel == 0U ||
        record->textureDescriptor.width >
            UINT32_MAX / bytesPerPixel) {
        return OutOfRange(
            "OpenGL checksum row size overflow");
    }
    const std::uint32_t rowPitch =
        record->textureDescriptor.width * bytesPerPixel;
    if (record->textureDescriptor.height >
        UINT32_MAX / rowPitch) {
        return OutOfRange(
            "OpenGL checksum buffer size overflow");
    }
    const std::uint32_t size =
        record->textureDescriptor.height * rowPitch;
    auto* bytes = static_cast<std::uint8_t*>(
        allocator_->Allocate({
            size,
            alignof(std::uint8_t),
            Base::MemoryTag::Render}));
    if (bytes == nullptr) {
        return OutOfMemory(
            "Failed to allocate OpenGL checksum readback buffer");
    }
    Base::Result<void> readback = ReadbackTexture(
        handle,
        Base::Span<std::uint8_t>(bytes, size),
        rowPitch);
    if (!readback) {
        allocator_->Deallocate(
            bytes,
            size,
            alignof(std::uint8_t),
            Base::MemoryTag::Render);
        return readback.GetStatus();
    }
    std::uint64_t hash = HashOffset;
    HashBytes(hash, bytes, size);
    allocator_->Deallocate(
        bytes,
        size,
        alignof(std::uint8_t),
        Base::MemoryTag::Render);
    return hash;
}

Base::Result<ResourceHandle>
ImportOpenGL33ExternalRenderTarget(
    RhiDevice& device,
    OpenGL33GraphicsBackend& backend,
    const OpenGL33ExternalRenderTargetDescriptor& descriptor) noexcept {
    if (descriptor.texture.format ==
        GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument(
            "OpenGL external color target cannot use a depth format");
    }
    Base::Result<ResourceHandle> created =
        device.CreateExternalRenderTarget(descriptor.texture);
    if (!created) {
        return created.GetStatus();
    }
    const ResourceHandle handle = created.Value();
    Base::Result<void> imported =
        backend.ImportExternalRenderTarget(handle, descriptor);
    if (!imported) {
        static_cast<void>(device.DestroyResource(handle, 0U));
        static_cast<void>(device.CollectGarbage());
        return imported.GetStatus();
    }
    return handle;
}

Base::Result<ResourceHandle>
ImportOpenGL33ExternalTexture(
    RhiDevice& device,
    OpenGL33GraphicsBackend& backend,
    const OpenGL33ExternalTextureDescriptor& descriptor) noexcept {
    Base::Result<ResourceHandle> created =
        device.CreateExternalTexture(descriptor.descriptor);
    if (!created) {
        return created.GetStatus();
    }
    const ResourceHandle handle = created.Value();
    Base::Result<void> imported =
        backend.ImportExternalTexture(handle, descriptor);
    if (!imported) {
        static_cast<void>(device.DestroyResource(handle, 0U));
        static_cast<void>(device.CollectGarbage());
        return imported.GetStatus();
    }
    return handle;
}

} // namespace Aero::Rhi
