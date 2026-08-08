#include "render/opengl33/OpenGL33RenderDevice.hpp"
#include "render/RenderDeviceState.hpp"
#include "render/opengl33/OpenGL33Shaders.hpp"

#include <Aero/Base/Vector.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace Aero::Graphics {
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
constexpr GlEnum GlDestinationColor = 0x0306U;
constexpr GlEnum GlOneMinusSourceColor = 0x0301U;
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
    case BlendFactor::DestinationColor:
        return GlDestinationColor;
    case BlendFactor::OneMinusSourceColor:
        return GlOneMinusSourceColor;
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

struct OpenGL33RenderDeviceState  {
    struct ResourceRecord  {
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

    struct PendingFence  {
        FenceValue value = 0U;
        GlSync sync = nullptr;
    };

    struct BufferBinding  {
        ResourceHandle handle;
        std::uint64_t offset = 0U;
    };

    explicit OpenGL33RenderDeviceState(
        const GlFunctionTable& functionTable,
        const GlContextBinding& contextBinding,
        const OpenGL33RenderDeviceOptions& backendOptions,
        Base::IAllocator* allocatorValue) noexcept
        : functions(functionTable),
          context(contextBinding),
          options(backendOptions),
          allocator(allocatorValue != nullptr
              ? allocatorValue
              : &Base::GetDefaultAllocator()),
          resources(allocator),
          pendingFences(allocator) {}

    GlFunctionTable functions;
    GlContextBinding context;
    OpenGL33RenderDeviceOptions options;
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
            ValidateGlContextBinding(context);
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

static_assert(
    sizeof(OpenGL33RenderDeviceState) <= 16384,
    "OpenGL render-device inline state storage is too small");
static_assert(
    alignof(OpenGL33RenderDeviceState) <= alignof(std::max_align_t),
    "OpenGL render-device state requires stronger alignment");

OpenGL33RenderDevice::OpenGL33RenderDevice(
    const GlFunctionTable& functions,
    const GlContextBinding& context,
    const OpenGL33RenderDeviceOptions& options,
    Base::IAllocator* allocator) noexcept
    : Aero::RenderDevice::Access(
          allocator != nullptr ? *allocator : Base::GetDefaultAllocator()),
      functions_(functions),
      context_(context),
      options_(options),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

OpenGL33RenderDevice::OpenGL33RenderDevice(
    const ::Aero::Render::OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator) noexcept
    : Aero::RenderDevice::Access(
          allocator != nullptr ? *allocator : Base::GetDefaultAllocator()),
      hostOptions_(options),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      hostManaged_(true) {
    options_.embeddingMode =
        options.statePolicy ==
            ::Aero::Render::OpenGL33StatePreservationPolicy::PreserveRequiredState
        ? GlEmbeddingMode::PreserveAndRestore
        : GlEmbeddingMode::HostReset;
    options_.checkErrors = options.checkErrors;
}

OpenGL33RenderDevice::OpenGL33RenderDevice(
    Base::IAllocator* allocator) noexcept
    : Aero::RenderDevice::Access(
          allocator != nullptr ? *allocator : Base::GetDefaultAllocator()),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

void OpenGL33RenderDevice::ConfigureContext(
    const GlFunctionTable& functions,
    const GlContextBinding& context,
    const OpenGL33RenderDeviceOptions& options) noexcept {
    if (state_ != nullptr) return;
    functions_ = functions;
    context_ = context;
    options_ = options;
    hostManaged_ = false;
}

OpenGL33RenderDevice::~OpenGL33RenderDevice() noexcept {
    Shutdown();
}

Base::Result<void> OpenGL33RenderDevice::Initialize() noexcept {
    if (state_ != nullptr && state_->initialized && AreResourcesReady()) {
        return {};
    }
    if (hostManaged_) {
        Base::Result<void> current = MakeCurrent();
        if (!current) return current.GetStatus();
        Base::Result<GlFunctionTable> loaded =
            LoadGlFunctionTable(&ResolveHost, this);
        if (!loaded) return loaded.GetStatus();
        functions_ = loaded.Value();
        context_ = {};
        context_.userData = this;
        context_.contextHandle = this;
        context_.resolve = &ResolveHost;
        context_.isCurrent = &IsHostCurrent;
        context_.currentThreadToken = &CurrentThreadToken;
        context_.owningThreadToken = CurrentThreadToken(nullptr);
        context_.generation = hostOptions_.contextGeneration != nullptr
            ? hostOptions_.contextGeneration(hostOptions_.callbackContext)
            : 0U;
        context_.embeddingMode = options_.embeddingMode;
        if (context_.generation == 0U) {
            return InvalidArgument("OpenGL context generation is zero");
        }
    }
    Base::Result<GlCapabilities> queried =
        QueryGlCapabilities(functions_, context_);
    if (!queried) {
        return queried.GetStatus();
    }
    if (state_ == nullptr) {
        state_ = new (stateStorage_) OpenGL33RenderDeviceState(
            functions_, context_, options_, allocator_);
    }

    state_->glCapabilities = queried.Value();
    Base::Result<void> cacheResult = state_->stateCache.Initialize(
        functions_, state_->glCapabilities);
    if (!cacheResult) {
        return cacheResult;
    }
    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    state_->functions.genVertexArrays(1, &state_->vertexArray);
    state_->functions.genFramebuffers(
        1, &state_->submissionFramebuffer);
    state_->functions.genFramebuffers(
        1, &state_->readbackFramebuffer);
    Base::Result<void> error = state_->CheckError(
        "OpenGL backend object initialization failed");
    Base::Result<void> end = state_->EndStateScope();
    if (!error) {
        return error;
    }
    if (!end) {
        return end;
    }
    if (state_->vertexArray == 0U ||
        state_->submissionFramebuffer == 0U ||
        state_->readbackFramebuffer == 0U) {
        return InvalidState(
            "OpenGL backend returned invalid object names");
    }
    state_->initialized = true;
    Base::Result<void> resources = InitializeResources();
    if (!resources) {
        Shutdown();
        return resources.GetStatus();
    }
    Base::Result<std::uint64_t> generation = AdvanceGeneration();
    if (!generation) {
        Shutdown();
        return generation.GetStatus();
    }
    deviceLost_ = false;
    return {};
}

void OpenGL33RenderDevice::Shutdown() noexcept {
    ShutdownResources();
    if (state_ == nullptr) {
        return;
    }
    const bool canDelete =
        ValidateGlContextBinding(state_->context).HasValue() &&
        state_->context.generation ==
            state_->glCapabilities.contextGeneration &&
        !state_->deviceLost;
    if (canDelete) {
        for (OpenGL33RenderDeviceState::PendingFence& fence : state_->pendingFences) {
            if (fence.sync != nullptr) {
                state_->functions.deleteSync(fence.sync);
            }
        }
        for (OpenGL33RenderDeviceState::ResourceRecord& record : state_->resources) {
            state_->DeleteNative(record);
        }
        if (state_->readbackFramebuffer != 0U) {
            state_->functions.deleteFramebuffers(
                1, &state_->readbackFramebuffer);
        }
        if (state_->submissionFramebuffer != 0U) {
            state_->functions.deleteFramebuffers(
                1, &state_->submissionFramebuffer);
        }
        if (state_->vertexArray != 0U) {
            state_->functions.deleteVertexArrays(
                1, &state_->vertexArray);
        }
    }
    state_->~OpenGL33RenderDeviceState();
    state_ = nullptr;
}

void OpenGL33RenderDevice::NotifyContextLost() noexcept {
    if (state_ != nullptr) {
        state_->deviceLost = true;
        state_->stateCache.Invalidate(
            state_->context.generation + 1U);
    }
    deviceLost_ = true;
}

bool OpenGL33RenderDevice::IsInitialized() const noexcept {
    return state_ != nullptr && state_->initialized;
}

GlProcAddress OpenGL33RenderDevice::ResolveHost(
    void* context,
    const char* name) noexcept {
    auto* device = static_cast<OpenGL33RenderDevice*>(context);
    if (device == nullptr || device->hostOptions_.resolve == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<GlProcAddress>(
        device->hostOptions_.resolve(
            device->hostOptions_.callbackContext, name));
}

bool OpenGL33RenderDevice::IsHostCurrent(
    void* context,
    const void*) noexcept {
    auto* device = static_cast<OpenGL33RenderDevice*>(context);
    return device != nullptr && device->hostOptions_.isCurrent != nullptr &&
        device->hostOptions_.isCurrent(device->hostOptions_.callbackContext);
}

GlThreadToken OpenGL33RenderDevice::CurrentThreadToken(void*) noexcept {
    GlThreadToken value = static_cast<GlThreadToken>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return value != 0U ? value : 1U;
}

Base::Result<void> OpenGL33RenderDevice::MakeCurrent() noexcept {
    if (!hostManaged_ || hostOptions_.makeCurrent == nullptr) return {};
    Base::Status current =
        hostOptions_.makeCurrent(hostOptions_.callbackContext);
    return current.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(current);
}

Base::Result<FenceValue> OpenGL33RenderDevice::DrawBatch(
    ::Aero::Render::RenderBatch&& batch) noexcept {
    if (!IsInitialized() || !AreResourcesReady()) {
        return NotInitialized("OpenGL device is not initialized");
    }
    if (batch.Empty()) return FenceValue{0U};
    return SubmitBatch(batch);
}

void OpenGL33RenderDevice::NotifyDeviceLost() noexcept {
    if (deviceLost_) return;
    NotifyContextLost();
    Shutdown();
}

Base::Result<void> OpenGL33RenderDevice::RestoreDevice() noexcept {
    if (!deviceLost_) {
        return InvalidState("OpenGL device is not lost");
    }
    return Initialize();
}

Base::Result<void> OpenGL33RenderDevice::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    const FenceValue fence = LastSubmittedFence();
    return fence != 0U
        ? WaitForFence(
              fence,
              static_cast<std::uint64_t>(timeoutMilliseconds) * UINT64_C(1000000))
        : Base::Result<void>();
}

::Aero::Render::BackendHealth
OpenGL33RenderDevice::GetDeviceHealth() const noexcept {
    if (deviceLost_ || IsDeviceLost()) {
        return ::Aero::Render::BackendHealth::DeviceLost;
    }
    return IsInitialized() && AreResourcesReady()
        ? ::Aero::Render::BackendHealth::Ready
        : ::Aero::Render::BackendHealth::Failed;
}

Base::Result<void> OpenGL33RenderDevice::ConfigureNativePipeline(
    ResourceHandle handle,
    ::Aero::Render::UiPipelineKey key) noexcept {
    return ConfigurePipeline(
        handle, ::Aero::Render::MakeOpenGL33UiPipeline(key));
}

std::uint32_t OpenGL33RenderDevice::LiveResourceCount() const noexcept {
    return state_ != nullptr ? state_->resources.Size() : 0U;
}

DeviceCapabilities
OpenGL33RenderDevice::Capabilities() const noexcept {
    DeviceCapabilities capabilities;
    capabilities.maxFramesInFlight = 2U;
    capabilities.maxTextureDimension =
        state_ != nullptr
        ? state_->glCapabilities.limits.maxTextureSize
        : 0U;
    capabilities.supportsTimestampQueries = false;
    return capabilities;
}

::Aero::Graphics::GraphicsCapabilities
OpenGL33RenderDevice::QueryGraphicsCapabilities() const noexcept {
    ::Aero::Graphics::GraphicsCapabilities capabilities;
    capabilities.backendKind = NativeRenderBackendKind::OpenGL33;
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
    if (state_ != nullptr) {
        capabilities.maxColorAttachments = std::min(
            state_->glCapabilities.limits.maxColorAttachments,
            MaxColorAttachments);
        capabilities.maxVertexAttributes = std::min(
            state_->glCapabilities.limits.maxVertexAttributes,
            MaxVertexAttributes);
        capabilities.maxSampledTextures = std::min(
            state_->glCapabilities.limits.maxCombinedTextureUnits,
            MaxCachedGlTextureUnits);
        capabilities.uniformBufferAlignment =
            state_->glCapabilities.limits.uniformBufferOffsetAlignment;
    }
    return capabilities;
}

Base::Result<void> OpenGL33RenderDevice::CreateResource(
    ResourceHandle handle,
    const ResourceDescriptor& descriptor) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (!handle.IsValid() ||
        descriptor.type != handle.type ||
        descriptor.type == ResourceType::Invalid ||
        state_->Find(handle) != nullptr) {
        return InvalidArgument(
            "OpenGL resource handle or descriptor is invalid");
    }

    OpenGL33RenderDeviceState::ResourceRecord record;
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
        Base::Result<void> scope = state_->BeginStateScope();
        if (!scope) {
            return scope;
        }
        state_->functions.genBuffers(1, &record.buffer);
        state_->functions.bindBuffer(target, record.buffer);
        state_->functions.bufferData(
            target,
            size,
            nullptr,
            BufferStorageUsage(descriptor.buffer.usage));
        Base::Result<void> error = state_->CheckError(
            "OpenGL buffer creation failed");
        Base::Result<void> end = state_->EndStateScope();
        if (!error || !end || record.buffer == 0U) {
            if (record.buffer != 0U) {
                state_->functions.deleteBuffers(1, &record.buffer);
            }
            return !error ? error.GetStatus() :
                (!end ? end.GetStatus() :
                 InvalidState("OpenGL buffer name is invalid"));
        }
        record.configured = true;
    }

    Base::Result<void> appended =
        state_->resources.PushBack(record);
    if (!appended) {
        state_->DeleteNative(record);
        return appended;
    }
    return {};
}

void OpenGL33RenderDevice::DestroyResource(
    ResourceHandle handle) noexcept {
    if (state_ == nullptr) {
        return;
    }
    const std::uint32_t index = state_->FindIndex(handle);
    if (index == UINT32_MAX) {
        return;
    }
    OpenGL33RenderDeviceState::ResourceRecord& record = state_->resources[index];
    const bool canDelete =
        !state_->deviceLost &&
        ValidateGlContextBinding(state_->context).HasValue() &&
        state_->context.generation ==
            state_->glCapabilities.contextGeneration;
    if (canDelete) {
        state_->DeleteNative(record);
    }
    state_->RemoveResourceAt(index);
}

Base::Result<void> OpenGL33RenderDevice::ConfigureTexture(
    ResourceHandle handle,
    const TextureResourceDescriptor& descriptor) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(handle);
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
            state_->glCapabilities.limits.maxTextureSize ||
        descriptor.height >
            state_->glCapabilities.limits.maxTextureSize ||
        descriptor.arrayLayers >
            state_->glCapabilities.limits.maxArrayTextureLayers ||
        descriptor.sampleCount >
            state_->glCapabilities.limits.maxSamples ||
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
    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    state_->functions.genTextures(1, &record->texture);
    state_->functions.activeTexture(GlConstant::Texture0);
    state_->functions.bindTexture(target, record->texture);
    if (target == GlTexture2DMultisample) {
        state_->functions.texImage2DMultisample(
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
                state_->functions.texImage3D(
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
                state_->functions.texImage2D(
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
        state_->functions.texParameteri(
            target, GlTextureBaseLevel, 0);
        state_->functions.texParameteri(
            target,
            GlTextureMaxLevel,
            static_cast<GlInt>(descriptor.mipLevels - 1U));
        state_->functions.texParameteri(
            target, GlTextureMinFilter, GlLinear);
        state_->functions.texParameteri(
            target, GlTextureMagFilter, GlLinear);
        state_->functions.texParameteri(
            target, GlTextureWrapS, GlClampToEdge);
        state_->functions.texParameteri(
            target, GlTextureWrapT, GlClampToEdge);
    }
    Base::Result<void> error = state_->CheckError(
        "OpenGL texture creation failed");
    Base::Result<void> end = state_->EndStateScope();
    if (!error || !end || record->texture == 0U) {
        if (record->texture != 0U) {
            state_->functions.deleteTextures(1, &record->texture);
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

Base::Result<void> OpenGL33RenderDevice::ConfigureSampler(
    ResourceHandle handle,
    const SamplerDescriptor& descriptor) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::Sampler ||
        record->configured) {
        return InvalidArgument(
            "OpenGL sampler resource cannot be configured");
    }
    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    state_->functions.genSamplers(1, &record->sampler);
    state_->functions.samplerParameteri(
        record->sampler,
        GlTextureMinFilter,
        static_cast<GlInt>(MinFilter(descriptor)));
    state_->functions.samplerParameteri(
        record->sampler,
        GlTextureMagFilter,
        static_cast<GlInt>(MagFilter(descriptor.magFilter)));
    state_->functions.samplerParameteri(
        record->sampler,
        GlTextureWrapS,
        static_cast<GlInt>(AddressModeValue(descriptor.addressU)));
    state_->functions.samplerParameteri(
        record->sampler,
        GlTextureWrapT,
        static_cast<GlInt>(AddressModeValue(descriptor.addressV)));
    state_->functions.samplerParameteri(
        record->sampler,
        GlTextureWrapR,
        static_cast<GlInt>(AddressModeValue(descriptor.addressW)));
    state_->functions.samplerParameterf(
        record->sampler, GlTextureMinLod, descriptor.minLod);
    state_->functions.samplerParameterf(
        record->sampler, GlTextureMaxLod, descriptor.maxLod);
    Base::Result<void> error = state_->CheckError(
        "OpenGL sampler creation failed");
    Base::Result<void> end = state_->EndStateScope();
    if (!error || !end || record->sampler == 0U) {
        if (record->sampler != 0U) {
            state_->functions.deleteSamplers(1, &record->sampler);
            record->sampler = 0U;
        }
        return !error ? error.GetStatus() :
            (!end ? end.GetStatus() :
             InvalidState("OpenGL sampler name is invalid"));
    }
    record->configured = true;
    return {};
}

Base::Result<void>
ValidateOpenGL33NativePipelineState(
    const NativePipelineState& descriptor) noexcept {
    const NativeShaderProgram& vertex =
        descriptor.vertexShader;
    const NativeShaderProgram& fragment =
        descriptor.fragmentShader;
    if (vertex.stage != ShaderStage::Vertex ||
        fragment.stage != ShaderStage::Fragment ||
        vertex.language != ShaderLanguage::Glsl330 ||
        fragment.language != ShaderLanguage::Glsl330) {
        return InvalidArgument(
            "OpenGL pipeline requires vertex and fragment GLSL 330 shaders");
    }
    if (vertex.bytecode == nullptr ||
        fragment.bytecode == nullptr ||
        vertex.bytecodeSize == 0U ||
        fragment.bytecodeSize == 0U ||
        vertex.entryPoint.Empty() ||
        fragment.entryPoint.Empty() ||
        vertex.stableId == 0U ||
        fragment.stableId == 0U ||
        vertex.stableId == fragment.stableId) {
        return InvalidArgument(
            "OpenGL shader package metadata is incomplete");
    }
    if (vertex.bytecodeSize >
            static_cast<std::uint32_t>(
                std::numeric_limits<GlInt>::max()) ||
        fragment.bytecodeSize >
            static_cast<std::uint32_t>(
                std::numeric_limits<GlInt>::max())) {
        return OutOfRange(
            "OpenGL shader source exceeds GLint length");
    }
    return {};
}

Base::Result<void> OpenGL33RenderDevice::ConfigurePipeline(
    ResourceHandle handle,
    const NativePipelineState& descriptor) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::Pipeline ||
        record->configured) {
        return InvalidArgument(
            "OpenGL pipeline requires unconfigured GLSL 330 shaders");
    }
    Base::Result<void> valid =
        ValidateOpenGL33NativePipelineState(
            descriptor);
    if (!valid) {
        return valid.GetStatus();
    }

    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    const auto compile = [&](const NativeShaderProgram& shader)
        noexcept -> Base::Result<GlUInt> {
        const GlEnum stage = shader.stage == ShaderStage::Vertex
            ? GlVertexShader
            : GlFragmentShader;
        const GlUInt nativeShader =
            state_->functions.createShader(stage);
        if (nativeShader == 0U) {
            return InvalidState(
                "OpenGL failed to allocate a shader object");
        }
        const auto* source = reinterpret_cast<const GlChar*>(
            shader.bytecode);
        const GlInt length =
            static_cast<GlInt>(shader.bytecodeSize);
        state_->functions.shaderSource(
            nativeShader, 1, &source, &length);
        state_->functions.compileShader(nativeShader);
        GlInt compiled = 0;
        state_->functions.getShaderiv(
            nativeShader, GlCompileStatus, &compiled);
        if (compiled == 0) {
            state_->functions.deleteShader(nativeShader);
            return InvalidArgument(
                "OpenGL GLSL 330 shader compilation failed");
        }
        return nativeShader;
    };

    Base::Result<GlUInt> vertex =
        compile(descriptor.vertexShader);
    if (!vertex) {
        static_cast<void>(state_->EndStateScope());
        return vertex.GetStatus();
    }
    Base::Result<GlUInt> fragment =
        compile(descriptor.fragmentShader);
    if (!fragment) {
        state_->functions.deleteShader(vertex.Value());
        static_cast<void>(state_->EndStateScope());
        return fragment.GetStatus();
    }

    const GlUInt program = state_->functions.createProgram();
    state_->functions.attachShader(program, vertex.Value());
    state_->functions.attachShader(program, fragment.Value());
    state_->functions.linkProgram(program);
    GlInt linked = 0;
    state_->functions.getProgramiv(program, GlLinkStatus, &linked);
    state_->functions.detachShader(program, vertex.Value());
    state_->functions.detachShader(program, fragment.Value());
    state_->functions.deleteShader(vertex.Value());
    state_->functions.deleteShader(fragment.Value());
    if (program == 0U || linked == 0) {
        if (program != 0U) {
            state_->functions.deleteProgram(program);
        }
        static_cast<void>(state_->EndStateScope());
        return InvalidArgument(
            "OpenGL GLSL 330 program link failed");
    }

    state_->functions.useProgram(program);
    const std::uint32_t bindingCount = std::min(
        state_->glCapabilities.limits.maxCombinedTextureUnits,
        MaxCachedGlTextureUnits);
    char name[32]{};
    for (std::uint32_t slot = 0U;
         slot < bindingCount;
         ++slot) {
        const int textureNameLength = std::snprintf(
            name, sizeof(name), "AeroTexture%u", slot);
        if (textureNameLength > 0) {
            const GlInt location =
                state_->functions.getUniformLocation(program, name);
            if (location >= 0) {
                state_->functions.uniform1i(
                    location, static_cast<GlInt>(slot));
            }
        }
        const int blockNameLength = std::snprintf(
            name, sizeof(name), "AeroUniform%u", slot);
        if (blockNameLength > 0) {
            const GlUInt block =
                state_->functions.getUniformBlockIndex(program, name);
            if (block != GlInvalidIndex) {
                state_->functions.uniformBlockBinding(
                    program, block, slot);
            }
        }
    }

    Base::Result<void> error = state_->CheckError(
        "OpenGL pipeline creation failed");
    Base::Result<void> end = state_->EndStateScope();
    if (!error || !end) {
        state_->functions.deleteProgram(program);
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
OpenGL33RenderDevice::ImportExternalRenderTarget(
    ResourceHandle handle,
    const OpenGL33RenderTargetBinding& descriptor) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::RenderTarget ||
        record->configured ||
        descriptor.contextGeneration != state_->context.generation ||
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
OpenGL33RenderDevice::ImportExternalTexture(
    ResourceHandle handle,
    const OpenGL33ExternalTextureDescriptor& descriptor) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(handle);
    if (record == nullptr ||
        handle.type != ResourceType::Texture ||
        record->configured ||
        descriptor.texture == 0U ||
        descriptor.contextGeneration != state_->context.generation ||
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

Base::Result<void> OpenGL33RenderDevice::UpdateNativeBuffer(
    ResourceHandle buffer,
    std::uint64_t destinationOffset,
    Base::Span<const std::uint8_t> data) noexcept {
    if (state_ == nullptr) {
        return NotInitialized("OpenGL device is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) return ready;
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(buffer);
    if (record == nullptr || record->buffer == 0U || data.Empty() ||
        destinationOffset > record->baseDescriptor.buffer.sizeBytes ||
        data.Size() > record->baseDescriptor.buffer.sizeBytes - destinationOffset) {
        return InvalidArgument("OpenGL buffer update is invalid");
    }
    GlSizePtr offset = 0;
    GlSizePtr size = 0;
    if (!CheckedGlSize(destinationOffset, offset) ||
        !CheckedGlSize(data.Size(), size)) {
        return OutOfRange("OpenGL buffer update exceeds pointer range");
    }
    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) return scope;
    const GlEnum target = BufferTarget(record->baseDescriptor.buffer.usage);
    state_->functions.bindBuffer(target, record->buffer);
    state_->functions.bufferSubData(target, offset, size, data.Data());
    Base::Result<void> error = state_->CheckError(
        "OpenGL buffer update failed");
    Base::Result<void> end = state_->EndStateScope();
    return !error ? error : end;
}

Base::Result<void> OpenGL33RenderDevice::UpdateNativeTexture(
    ResourceHandle texture,
    const TextureRegion& region,
    Base::Span<const std::uint8_t> data) noexcept {
    if (state_ == nullptr) {
        return NotInitialized("OpenGL device is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) return ready;
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(texture);
    if (record == nullptr || record->texture == 0U || !record->configured ||
        record->external || data.Empty() ||
        record->textureDescriptor.sampleCount != 1U ||
        !HasTextureUsage(
            record->textureDescriptor.usage,
            TextureUsage::CopyDestination) ||
        region.width == 0U || region.height == 0U ||
        region.mipLevel >= record->textureDescriptor.mipLevels ||
        region.arrayLayer >= record->textureDescriptor.arrayLayers) {
        return InvalidArgument("OpenGL texture update is invalid");
    }
    std::uint32_t mipWidth = record->textureDescriptor.width;
    std::uint32_t mipHeight = record->textureDescriptor.height;
    for (std::uint16_t mip = 0U; mip < region.mipLevel; ++mip) {
        mipWidth = std::max(1U, mipWidth / 2U);
        mipHeight = std::max(1U, mipHeight / 2U);
    }
    if (region.x > mipWidth || region.width > mipWidth - region.x ||
        region.y > mipHeight || region.height > mipHeight - region.y ||
        region.x > static_cast<std::uint32_t>(
            std::numeric_limits<GlInt>::max()) ||
        region.y > static_cast<std::uint32_t>(
            std::numeric_limits<GlInt>::max())) {
        return InvalidArgument("OpenGL texture update exceeds the mip extent");
    }
    const std::uint32_t bytesPerPixel =
        BytesPerPixel(record->textureDescriptor.format);
    if (bytesPerPixel == 0U ||
        region.width > UINT32_MAX / bytesPerPixel ||
        region.bytesPerRow < region.width * bytesPerPixel ||
        region.bytesPerRow % bytesPerPixel != 0U ||
        region.bytesPerRow / bytesPerPixel >
            static_cast<std::uint32_t>(std::numeric_limits<GlInt>::max()) ||
        region.height - 1U >
            (UINT32_MAX - region.width * bytesPerPixel) / region.bytesPerRow ||
        data.Size() < (region.height - 1U) * region.bytesPerRow +
            region.width * bytesPerPixel) {
        return InvalidArgument("OpenGL texture update row pitch is invalid");
    }
    GlSize width = 0;
    GlSize height = 0;
    if (!CheckedGlSizeValue(region.width, width) ||
        !CheckedGlSizeValue(region.height, height)) {
        return OutOfRange("OpenGL texture update exceeds GLsizei");
    }
    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) return scope;
    state_->functions.activeTexture(GlConstant::Texture0);
    state_->functions.bindTexture(record->textureTarget, record->texture);
    state_->functions.pixelStorei(GlConstant::UnpackAlignment, 1);
    state_->functions.pixelStorei(
        GlConstant::UnpackRowLength,
        static_cast<GlInt>(region.bytesPerRow / bytesPerPixel));
    if (record->textureTarget == GlConstant::Texture2DArray) {
        state_->functions.texSubImage3D(
            record->textureTarget,
            region.mipLevel,
            static_cast<GlInt>(region.x),
            static_cast<GlInt>(region.y),
            region.arrayLayer,
            width,
            height,
            1,
            TextureDataFormat(record->textureDescriptor.format),
            TextureDataType(record->textureDescriptor.format),
            data.Data());
    } else {
        state_->functions.texSubImage2D(
            record->textureTarget,
            region.mipLevel,
            static_cast<GlInt>(region.x),
            static_cast<GlInt>(region.y),
            width,
            height,
            TextureDataFormat(record->textureDescriptor.format),
            TextureDataType(record->textureDescriptor.format),
            data.Data());
    }
    Base::Result<void> error = state_->CheckError(
        "OpenGL texture update failed");
    Base::Result<void> end = state_->EndStateScope();
    return !error ? error : end;
}

Base::Result<void> OpenGL33RenderDevice::Submit(
    const ::Aero::Render::RenderBatch& batch,
    ResourceHandle pipeline,
    FenceValue signalFence) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (signalFence == 0U ||
        signalFence <= state_->lastSubmittedFence) {
        return InvalidArgument(
            "OpenGL submission fence must increase monotonically");
    }

    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    state_->ResetSubmission();
    const auto fail = [&](Base::Status status) noexcept
        -> Base::Result<void> {
        state_->ResetSubmission();
        static_cast<void>(state_->EndStateScope());
        return status;
    };

    const auto beginRenderPass = [&](
        const RenderPassDescriptor& pass) noexcept
        -> Base::Result<void> {
            if (state_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL render passes cannot be nested"));
            }
            OpenGL33RenderDeviceState::ResourceRecord* firstColor = nullptr;
            if (pass.colorAttachmentCount > 0U) {
                firstColor = state_->Find(
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
                : state_->submissionFramebuffer;
            Base::Result<void> bindFramebuffer =
                state_->stateCache.BindDrawFramebuffer(framebuffer);
            if (!bindFramebuffer) {
                return fail(bindFramebuffer.GetStatus());
            }

            GlEnum drawBuffers[MaxColorAttachments]{};
            std::uint32_t targetWidth = 0U;
            std::uint32_t targetHeight = 0U;
            for (std::uint32_t index = 0U;
                 index < pass.colorAttachmentCount;
                 ++index) {
                OpenGL33RenderDeviceState::ResourceRecord* attachment =
                    state_->Find(pass.colorAttachments[index].target);
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
                    state_->functions.framebufferTexture2D(
                        GlConstant::DrawFramebuffer,
                        GlConstant::ColorAttachment0 + index,
                        attachment->textureTarget,
                        attachment->texture,
                        0);
                }
                drawBuffers[index] =
                    GlConstant::ColorAttachment0 + index;
                state_->activeAttachmentTextures[index] =
                    attachment->texture;
            }
            if (!directExternal) {
                for (std::uint32_t index =
                        pass.colorAttachmentCount;
                     index < MaxColorAttachments;
                     ++index) {
                    state_->functions.framebufferTexture2D(
                        GlConstant::DrawFramebuffer,
                        GlConstant::ColorAttachment0 + index,
                        GlConstant::Texture2D,
                        0U,
                        0);
                }
            }

            if (pass.hasDepthStencil) {
                OpenGL33RenderDeviceState::ResourceRecord* depth = state_->Find(
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
                state_->functions.framebufferTexture2D(
                    GlConstant::DrawFramebuffer,
                    GlDepthStencilAttachment,
                    depth->textureTarget,
                    depth->texture,
                    0);
                state_->activeAttachmentTextures[
                    MaxColorAttachments] = depth->texture;
            } else if (!directExternal) {
                state_->functions.framebufferTexture2D(
                    GlConstant::DrawFramebuffer,
                    GlDepthStencilAttachment,
                    GlConstant::Texture2D,
                    0U,
                    0);
            }

            if (!directExternal) {
                state_->functions.drawBuffers(
                    static_cast<GlSize>(
                        pass.colorAttachmentCount),
                    pass.colorAttachmentCount > 0U
                        ? drawBuffers
                        : nullptr);
                if (state_->functions.checkFramebufferStatus(
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
                state_->stateCache.SetViewport(viewport.Value());
            if (!viewportResult) {
                return fail(viewportResult.GetStatus());
            }
            Base::Result<void> scissorResult =
                state_->stateCache.SetScissor(
                    true, viewport.Value());
            if (!scissorResult) {
                return fail(scissorResult.GetStatus());
            }
            state_->renderPassHeight = targetHeight;

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
                    state_->functions.clearBufferfv(
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
                    state_->functions.clearBufferfi(
                        GlDepthStencil,
                        0,
                        pass.depthStencil.clearDepth,
                        static_cast<GlInt>(
                            pass.depthStencil.clearStencil));
                } else if (clearDepth) {
                    const GlFloat depth =
                        pass.depthStencil.clearDepth;
                    state_->functions.clearBufferfv(
                        GlDepth, 0, &depth);
                } else if (clearStencil) {
                    const GlInt stencil =
                        static_cast<GlInt>(
                            pass.depthStencil.clearStencil);
                    state_->functions.clearBufferiv(
                        GlStencil, 0, &stencil);
                }
            }
            state_->inRenderPass = true;
            return {};
    };

    const auto endRenderPass = [&]() noexcept -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL render pass is not active"));
            }
            state_->inRenderPass = false;
            for (GlUInt& attachment :
                 state_->activeAttachmentTextures) {
                attachment = 0U;
            }
            return {};
    };

    const auto bindPipeline = [&](
        ResourceHandle pipelineHandle) noexcept
        -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL pipeline binding requires a render pass"));
            }
            OpenGL33RenderDeviceState::ResourceRecord* pipeline =
                state_->Find(pipelineHandle);
            if (pipeline == nullptr ||
                pipeline->program == 0U ||
                !pipeline->configured) {
                return fail(InvalidArgument(
                    "OpenGL pipeline is not configured"));
            }
            Base::Result<void> programResult =
                state_->stateCache.UseProgram(pipeline->program);
            Base::Result<void> vaoResult =
                state_->stateCache.BindVertexArray(
                    state_->vertexArray);
            Base::Result<void> blendResult =
                state_->stateCache.SetBlendState(
                    ToGlBlendState(pipeline->blend));
            Base::Result<void> depthResult =
                state_->stateCache.SetDepthState(
                    ToGlDepthState(pipeline->depthStencil));
            Base::Result<void> stencilResult =
                state_->stateCache.SetStencilState(
                    ToGlStencilState(pipeline->depthStencil));
            Base::Result<void> rasterResult =
                state_->stateCache.SetRasterState(
                    ToGlRasterState(pipeline->raster));
            if (!programResult || !vaoResult || !blendResult ||
                !depthResult || !stencilResult || !rasterResult) {
                return fail(InvalidState(
                    "OpenGL pipeline state binding failed"));
            }
            state_->currentPipeline = pipelineHandle;
            state_->vertexStateDirty = true;
            return {};
    };

    const auto bindVertexBuffer = [&](
        std::uint32_t slot,
        ResourceHandle bufferHandle,
        std::uint64_t offset) noexcept
        -> Base::Result<void> {
            const OpenGL33RenderDeviceState::ResourceRecord* pipeline =
                state_->Find(state_->currentPipeline);
            const OpenGL33RenderDeviceState::ResourceRecord* buffer =
                state_->Find(bufferHandle);
            if (!state_->inRenderPass || pipeline == nullptr ||
                buffer == nullptr || buffer->buffer == 0U ||
                buffer->baseDescriptor.buffer.usage !=
                    BufferUsage::Vertex ||
                slot >=
                    pipeline->vertexLayout.bufferCount ||
                offset >
                    buffer->baseDescriptor.buffer.sizeBytes) {
                return fail(InvalidArgument(
                    "OpenGL vertex-buffer binding is invalid"));
            }
            state_->vertexBuffers[slot] = {bufferHandle, offset};
            state_->vertexStateDirty = true;
            return {};
    };

    const auto bindIndexBuffer = [&](
        ResourceHandle bufferHandle,
        std::uint64_t offset,
        IndexType indexType) noexcept
        -> Base::Result<void> {
            const OpenGL33RenderDeviceState::ResourceRecord* buffer =
                state_->Find(bufferHandle);
            if (!state_->inRenderPass ||
                buffer == nullptr || buffer->buffer == 0U ||
                buffer->baseDescriptor.buffer.usage !=
                    BufferUsage::Index ||
                offset >
                    buffer->baseDescriptor.buffer.sizeBytes) {
                return fail(InvalidArgument(
                    "OpenGL index-buffer binding is invalid"));
            }
            state_->indexBuffer = {bufferHandle, offset};
            state_->indexType = indexType;
            state_->vertexStateDirty = true;
            return {};
    };

    const auto bindUniformBuffer = [&](
        std::uint32_t slot,
        ResourceHandle bufferHandle,
        std::uint64_t offsetBytes,
        std::uint32_t sizeBytes) noexcept
        -> Base::Result<void> {
            if (!state_->inRenderPass ||
                slot >=
                    state_->glCapabilities.limits.
                        maxUniformBufferBindings) {
                return fail(InvalidArgument(
                    "OpenGL uniform-buffer slot is invalid"));
            }
            OpenGL33RenderDeviceState::ResourceRecord* buffer =
                state_->Find(bufferHandle);
            if (buffer == nullptr || buffer->buffer == 0U ||
                buffer->baseDescriptor.buffer.usage !=
                    BufferUsage::Uniform ||
                sizeBytes == 0U ||
                offsetBytes %
                    state_->glCapabilities.limits.
                        uniformBufferOffsetAlignment != 0U ||
                sizeBytes >
                    state_->glCapabilities.limits.
                        maxUniformBlockSize ||
                offsetBytes >
                    buffer->baseDescriptor.buffer.sizeBytes ||
                sizeBytes >
                    buffer->baseDescriptor.buffer.sizeBytes -
                        offsetBytes) {
                return fail(InvalidArgument(
                    "OpenGL uniform-buffer binding is invalid"));
            }
            GlIntPtr offset = 0;
            GlSizePtr size = 0;
            if (!CheckedGlSize(offsetBytes, offset) ||
                !CheckedGlSize(sizeBytes, size)) {
                return fail(OutOfRange(
                    "OpenGL uniform-buffer range exceeds pointer range"));
            }
            Base::Result<void> bindResult =
                state_->stateCache.BindUniformBuffer(buffer->buffer);
            if (!bindResult) {
                return fail(bindResult.GetStatus());
            }
            state_->functions.bindBufferRange(
                GlConstant::UniformBuffer,
                slot,
                buffer->buffer,
                offset,
                size);
            return {};
    };

    const auto bindTextureSampler = [&](
        std::uint32_t slot,
        ResourceHandle textureHandle,
        ResourceHandle samplerHandle) noexcept
        -> Base::Result<void> {
            if (!state_->inRenderPass ||
                slot >=
                    QueryGraphicsCapabilities().
                        maxSampledTextures) {
                return fail(InvalidArgument(
                    "OpenGL texture slot is invalid"));
            }
            OpenGL33RenderDeviceState::ResourceRecord* texture =
                state_->Find(textureHandle);
            OpenGL33RenderDeviceState::ResourceRecord* sampler =
                state_->Find(samplerHandle);
            if (texture == nullptr || sampler == nullptr ||
                texture->texture == 0U ||
                sampler->sampler == 0U ||
                !HasTextureUsage(
                    texture->textureDescriptor.usage,
                    TextureUsage::Sampled) ||
                texture->textureTarget ==
                    GlTexture2DMultisample ||
                state_->IsActiveAttachment(texture->texture)) {
                return fail(InvalidArgument(
                    "OpenGL texture or sampler binding is invalid"));
            }
            Base::Result<void> bindResult =
                state_->stateCache.BindTextureSampler(
                    slot,
                    texture->textureTarget,
                    texture->texture,
                    sampler->sampler);
            if (!bindResult) {
                return fail(bindResult.GetStatus());
            }
            return {};
    };

    const auto setScissor = [&](
        Base::Rect rect) noexcept -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return fail(InvalidState(
                    "OpenGL scissor requires a render pass"));
            }
            Base::Result<GlRectangleState> rectangle =
                ToGlRectangle(rect, state_->renderPassHeight);
            if (!rectangle) {
                return fail(rectangle.GetStatus());
            }
            const OpenGL33RenderDeviceState::ResourceRecord* pipeline =
                state_->Find(state_->currentPipeline);
            const bool enabled =
                pipeline == nullptr ||
                pipeline->raster.scissorEnabled;
            Base::Result<void> setResult =
                state_->stateCache.SetScissor(
                    enabled, rectangle.Value());
            if (!setResult) {
                return fail(setResult.GetStatus());
            }
            return {};
    };

    const auto draw = [&](
        std::uint32_t first,
        std::uint32_t count,
        std::uint32_t instanceCount,
        std::uint32_t firstInstance) noexcept
        -> Base::Result<void> {
            if (!state_->inRenderPass ||
                !state_->currentPipeline.IsValid() ||
                count == 0U ||
                instanceCount == 0U) {
                return fail(InvalidState(
                    "OpenGL draw state is incomplete"));
            }
            if (firstInstance != 0U) {
                return fail(Unsupported(
                    "OpenGL 3.3 does not support base-instance draws"));
            }
            if (first >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlInt>::max()) ||
                count >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max()) ||
                instanceCount >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max())) {
                return fail(OutOfRange(
                    "OpenGL draw arguments exceed GL integer range"));
            }
            Base::Result<void> prepared =
                state_->PrepareVertexInput(false);
            if (!prepared) {
                return fail(prepared.GetStatus());
            }
            const OpenGL33RenderDeviceState::ResourceRecord* pipeline =
                state_->Find(state_->currentPipeline);
            const GlEnum topology =
                Topology(pipeline->topology);
            if (instanceCount == 1U) {
                state_->functions.drawArrays(
                    topology,
                    static_cast<GlInt>(first),
                    static_cast<GlSize>(count));
            } else {
                state_->functions.drawArraysInstanced(
                    topology,
                    static_cast<GlInt>(first),
                    static_cast<GlSize>(count),
                    static_cast<GlSize>(instanceCount));
            }
            return {};
    };

    const auto drawIndexed = [&](
        std::uint32_t first,
        std::uint32_t count,
        std::uint32_t instanceCount,
        std::uint32_t firstInstance,
        std::int32_t baseVertex) noexcept
        -> Base::Result<void> {
            if (!state_->inRenderPass ||
                !state_->currentPipeline.IsValid() ||
                count == 0U ||
                instanceCount == 0U) {
                return fail(InvalidState(
                    "OpenGL indexed-draw state is incomplete"));
            }
            if (firstInstance != 0U) {
                return fail(Unsupported(
                    "OpenGL 3.3 does not support base-instance draws"));
            }
            if (count >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max()) ||
                instanceCount >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<GlSize>::max())) {
                return fail(OutOfRange(
                    "OpenGL indexed-draw arguments exceed GL integer range"));
            }
            Base::Result<void> prepared =
                state_->PrepareVertexInput(true);
            if (!prepared) {
                return fail(prepared.GetStatus());
            }
            const std::uint32_t indexSize =
                state_->indexType == IndexType::UInt16
                ? 2U
                : 4U;
            if (first >
                    (std::numeric_limits<std::uintptr_t>::max() -
                     state_->indexBuffer.offset) /
                        indexSize) {
                return fail(OutOfRange(
                    "OpenGL index offset exceeds pointer range"));
            }
            const std::uintptr_t pointer =
                static_cast<std::uintptr_t>(
                    state_->indexBuffer.offset) +
                static_cast<std::uintptr_t>(first) *
                    indexSize;
            const OpenGL33RenderDeviceState::ResourceRecord* pipeline =
                state_->Find(state_->currentPipeline);
            const GlEnum topology =
                Topology(pipeline->topology);
            const GlEnum indexType =
                state_->indexType == IndexType::UInt16
                ? GlUnsignedShort
                : GlUnsignedInt;
            if (instanceCount == 1U) {
                state_->functions.drawElementsBaseVertex(
                    topology,
                    static_cast<GlSize>(count),
                    indexType,
                    reinterpret_cast<const void*>(pointer),
                    baseVertex);
            } else {
                state_->functions.drawElementsInstancedBaseVertex(
                    topology,
                    static_cast<GlSize>(count),
                    indexType,
                    reinterpret_cast<const void*>(pointer),
                    static_cast<GlSize>(instanceCount),
                    baseVertex);
            }
            return {};
    };

    Base::Result<void> executed = beginRenderPass(batch.pass);
    if (!executed) return executed;

    executed = bindPipeline(pipeline);
    if (!executed) return executed;

    for (std::uint32_t slot = 0U; slot < MaxVertexBuffers; ++slot) {
        if (!batch.drawState.vertexBuffers[slot].IsValid()) continue;
        executed = bindVertexBuffer(
            slot,
            batch.drawState.vertexBuffers[slot],
            batch.drawState.vertexOffsets[slot]);
        if (!executed) return executed;
    }

    if (batch.indexed && batch.drawState.indexBuffer.IsValid()) {
        executed = bindIndexBuffer(
            batch.drawState.indexBuffer,
            batch.drawState.indexOffset,
            batch.drawState.indexType);
        if (!executed) return executed;
    }

    for (std::uint32_t slot = 0U; slot < 4U; ++slot) {
        if (!batch.drawState.uniformBuffers[slot].IsValid()) continue;
        executed = bindUniformBuffer(
            slot,
            batch.drawState.uniformBuffers[slot],
            batch.drawState.uniformOffsets[slot],
            batch.drawState.uniformSizes[slot]);
        if (!executed) return executed;
    }

    for (std::uint32_t slot = 0U; slot < 8U; ++slot) {
        if (!batch.drawState.textures[slot].IsValid() ||
            !batch.drawState.samplers[slot].IsValid()) continue;
        executed = bindTextureSampler(
            slot,
            batch.drawState.textures[slot],
            batch.drawState.samplers[slot]);
        if (!executed) return executed;
    }

    executed = setScissor(batch.drawState.scissor);
    if (!executed) return executed;

    executed = batch.indexed
        ? drawIndexed(
              batch.first,
              batch.count,
              batch.instanceCount,
              batch.firstInstance,
              batch.baseVertex)
        : draw(
              batch.first,
              batch.count,
              batch.instanceCount,
              batch.firstInstance);
    if (!executed) return executed;

    executed = endRenderPass();
    if (!executed) return executed;

    if (state_->inRenderPass) {
        return fail(InvalidState(
            "OpenGL batch ended inside a render pass"));
    }
    Base::Result<void> error = state_->CheckError(
        "OpenGL command submission failed");
    if (!error) {
        return fail(error.GetStatus());
    }
    GlSync sync = state_->functions.fenceSync(
        GlConstant::SyncGpuCommandsComplete, 0U);
    if (sync == nullptr) {
        return fail(InvalidState(
            "OpenGL fence creation failed"));
    }
    state_->functions.flush();
    Base::Result<void> end = state_->EndStateScope();
    if (!end) {
        state_->functions.deleteSync(sync);
        return end;
    }
    OpenGL33RenderDeviceState::PendingFence fence;
    fence.value = signalFence;
    fence.sync = sync;
    Base::Result<void> queuedFence =
        state_->pendingFences.PushBack(fence);
    if (!queuedFence) {
        state_->functions.deleteSync(sync);
        return queuedFence;
    }
    state_->lastSubmittedFence = signalFence;
    state_->ResetSubmission();
    return {};
}

FenceValue OpenGL33RenderDevice::LastSubmittedFence() const noexcept {
    return state_ != nullptr ? state_->lastSubmittedFence : 0U;
}

FenceValue OpenGL33RenderDevice::CompletedFence() const noexcept {
    if (state_ == nullptr || state_->deviceLost ||
        !ValidateGlContextBinding(state_->context)) {
        return state_ != nullptr ? state_->completedFence : 0U;
    }
    state_->PollFences();
    return state_->completedFence;
}

bool OpenGL33RenderDevice::IsDeviceLost() const noexcept {
    return state_ == nullptr ||
        !state_->initialized ||
        state_->deviceLost;
}

Base::Result<void> OpenGL33RenderDevice::WaitForFence(
    FenceValue fence,
    std::uint64_t timeoutNanoseconds) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (fence == 0U || fence > state_->lastSubmittedFence ||
        timeoutNanoseconds == 0U) {
        return InvalidArgument(
            "OpenGL wait fence or timeout is invalid");
    }
    if (CompletedFence() >= fence) {
        return {};
    }
    for (const OpenGL33RenderDeviceState::PendingFence& pending :
         state_->pendingFences) {
        if (pending.value < fence) {
            continue;
        }
        const GlEnum result = state_->functions.clientWaitSync(
            pending.sync,
            GlConstant::SyncFlushCommandsBit,
            timeoutNanoseconds);
        if (result == GlConstant::TimeoutExpired) {
            return InvalidState(
                "Timed out while waiting for an OpenGL fence");
        }
        if (result == GlConstant::WaitFailed) {
            state_->deviceLost = true;
            return InvalidState(
                "OpenGL fence wait failed");
        }
        state_->PollFences();
        return CompletedFence() >= fence
            ? Base::Result<void>{}
            : Base::Result<void>{
                InvalidState(
                    "OpenGL fence did not reach the requested value")};
    }
    return NotFound("OpenGL fence was not found");
}

Base::Result<void> OpenGL33RenderDevice::ReadbackTexture(
    ResourceHandle handle,
    Base::Span<std::uint8_t> destination,
    std::uint32_t destinationRowPitch) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(handle);
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

    Base::Result<void> scope = state_->BeginStateScope();
    if (!scope) {
        return scope;
    }
    Base::Result<void> bindResult =
        state_->stateCache.BindDrawFramebuffer(
            state_->readbackFramebuffer);
    if (!bindResult) {
        static_cast<void>(state_->EndStateScope());
        return bindResult;
    }
    Base::Result<void> bindReadResult =
        state_->stateCache.BindReadFramebuffer(
            state_->readbackFramebuffer);
    if (!bindReadResult) {
        static_cast<void>(state_->EndStateScope());
        return bindReadResult;
    }
    state_->functions.framebufferTexture2D(
        GlConstant::DrawFramebuffer,
        GlConstant::ColorAttachment0,
        record->textureTarget,
        record->texture,
        0);
    if (state_->functions.checkFramebufferStatus(
            GlConstant::DrawFramebuffer) !=
        GlConstant::FramebufferComplete) {
        static_cast<void>(state_->EndStateScope());
        return InvalidState(
            "OpenGL readback framebuffer is incomplete");
    }
    GlInt packAlignment = 0;
    GlInt packRowLength = 0;
    GlInt packSkipRows = 0;
    GlInt packSkipPixels = 0;
    state_->functions.getIntegerv(
        GlConstant::PackAlignment, &packAlignment);
    state_->functions.getIntegerv(
        GlConstant::PackRowLength, &packRowLength);
    state_->functions.getIntegerv(
        GlConstant::PackSkipRows, &packSkipRows);
    state_->functions.getIntegerv(
        GlConstant::PackSkipPixels, &packSkipPixels);
    state_->functions.pixelStorei(
        GlConstant::PackAlignment, 1);
    state_->functions.pixelStorei(
        GlConstant::PackRowLength, 0);
    state_->functions.pixelStorei(
        GlConstant::PackSkipRows, 0);
    state_->functions.pixelStorei(
        GlConstant::PackSkipPixels, 0);
    state_->functions.readBuffer(GlConstant::ColorAttachment0);

    GlSize width = 0;
    static_cast<void>(CheckedGlSizeValue(
        record->textureDescriptor.width, width));
    for (std::uint32_t row = 0U;
         row < record->textureDescriptor.height;
         ++row) {
        const GlInt sourceY = static_cast<GlInt>(
            record->textureDescriptor.height - row - 1U);
        state_->functions.readPixels(
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
    state_->functions.pixelStorei(
        GlConstant::PackAlignment, packAlignment);
    state_->functions.pixelStorei(
        GlConstant::PackRowLength, packRowLength);
    state_->functions.pixelStorei(
        GlConstant::PackSkipRows, packSkipRows);
    state_->functions.pixelStorei(
        GlConstant::PackSkipPixels, packSkipPixels);
    Base::Result<void> error = state_->CheckError(
        "OpenGL texture readback failed");
    Base::Result<void> end = state_->EndStateScope();
    if (!error) {
        return error;
    }
    return end;
}

Base::Result<std::uint64_t>
OpenGL33RenderDevice::ReadbackTextureChecksum(
    ResourceHandle handle) noexcept {
    if (state_ == nullptr) {
        return NotInitialized(
            "OpenGL 3.3 backend is not initialized");
    }
    const OpenGL33RenderDeviceState::ResourceRecord* record = state_->Find(handle);
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
    Aero::RenderDevice::Access& device,
    OpenGL33RenderDevice& backend,
    const OpenGL33RenderTargetBinding& descriptor) noexcept {
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
    Aero::RenderDevice::Access& device,
    OpenGL33RenderDevice& backend,
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

} // namespace Aero::Graphics
