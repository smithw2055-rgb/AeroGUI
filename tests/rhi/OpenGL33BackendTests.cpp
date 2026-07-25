#include <Aero/Rhi/OpenGL33Backend.hpp>

#include <cstdio>
#include <cstring>

namespace {
using namespace Aero::Base;
using namespace Aero::Rhi;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

constexpr GlEnum GlCompileStatus = 0x8B81U;
constexpr GlEnum GlLinkStatus = 0x8B82U;

struct FakeBackendGl final {
    GlUInt nextObject = 1U;
    GlThreadToken thread = 19U;
    bool current = true;
    std::uint32_t generatedObjects = 0U;
    std::uint32_t deletedBuffers = 0U;
    std::uint32_t deletedTextures = 0U;
    std::uint32_t deletedSamplers = 0U;
    std::uint32_t deletedPrograms = 0U;
    std::uint32_t bufferUploads = 0U;
    std::uint32_t textureUploads = 0U;
    std::uint32_t renderPasses = 0U;
    std::uint32_t drawCalls = 0U;
    std::uint32_t indexedDrawCalls = 0U;
    std::uint32_t fenceCount = 0U;
    std::uint32_t deletedFences = 0U;
    std::uint32_t readbackRows = 0U;
    std::uint32_t enabledAttributes = 0U;
    std::uint32_t disabledAttributes = 0U;
    std::uint32_t boundFramebuffer = 0U;
    std::uint32_t activeTextureUnit = 0U;
};

FakeBackendGl* ActiveFake = nullptr;

void GenericNoop() {}

GlProcAddress ResolveAll(void*, const char*) noexcept {
    return reinterpret_cast<GlProcAddress>(&GenericNoop);
}

bool IsCurrent(void* userData, const void* contextHandle) noexcept {
    auto* state = static_cast<FakeBackendGl*>(userData);
    return state->current && contextHandle == state;
}

GlThreadToken CurrentThread(void* userData) noexcept {
    return static_cast<FakeBackendGl*>(userData)->thread;
}

void GetIntegerv(GlEnum name, GlInt* value) {
    switch (name) {
    case GlConstant::MajorVersion:
        value[0] = 3;
        break;
    case GlConstant::MinorVersion:
        value[0] = 3;
        break;
    case GlConstant::ContextProfileMask:
        value[0] = GlConstant::ContextCoreProfileBit;
        break;
    case GlConstant::ContextFlags:
        value[0] = 0;
        break;
    case GlConstant::MaxTextureSize:
        value[0] = 8192;
        break;
    case GlConstant::MaxArrayTextureLayers:
        value[0] = 256;
        break;
    case GlConstant::MaxCombinedTextureImageUnits:
        value[0] = 16;
        break;
    case GlConstant::MaxVertexAttribs:
        value[0] = 16;
        break;
    case GlConstant::MaxUniformBlockSize:
        value[0] = 65536;
        break;
    case GlConstant::MaxUniformBufferBindings:
        value[0] = 36;
        break;
    case GlConstant::UniformBufferOffsetAlignment:
        value[0] = 256;
        break;
    case GlConstant::MaxSamples:
        value[0] = 8;
        break;
    case GlConstant::MaxColorAttachments:
        value[0] = 4;
        break;
    case GlConstant::PackAlignment:
        value[0] = 4;
        break;
    case GlConstant::PackRowLength:
    case GlConstant::PackSkipRows:
    case GlConstant::PackSkipPixels:
        value[0] = 0;
        break;
    default:
        value[0] = 0;
        break;
    }
}

void GetBooleanv(GlEnum, GlBoolean* value) {
    value[0] = 0U;
}

GlBoolean IsEnabled(GlEnum) {
    return 0U;
}

GlEnum GetError() {
    return 0U;
}

void GenObjects(GlSize count, GlUInt* objects) {
    for (GlSize index = 0; index < count; ++index) {
        objects[index] = ActiveFake->nextObject++;
        ++ActiveFake->generatedObjects;
    }
}

void DeleteBuffers(GlSize count, const GlUInt*) {
    ActiveFake->deletedBuffers += static_cast<std::uint32_t>(count);
}

void DeleteTextures(GlSize count, const GlUInt*) {
    ActiveFake->deletedTextures += static_cast<std::uint32_t>(count);
}

void DeleteSamplers(GlSize count, const GlUInt*) {
    ActiveFake->deletedSamplers += static_cast<std::uint32_t>(count);
}

void DeleteObjects(GlSize, const GlUInt*) {}

void BindBuffer(GlEnum, GlUInt) {}

void BufferData(GlEnum, GlSizePtr, const void*, GlEnum) {}

void BufferSubData(
    GlEnum,
    GlIntPtr,
    GlSizePtr,
    const void*) {
    ++ActiveFake->bufferUploads;
}

void BindBufferRange(
    GlEnum,
    GlUInt,
    GlUInt,
    GlIntPtr,
    GlSizePtr) {}

void BindBufferBase(GlEnum, GlUInt, GlUInt) {}

void BindVertexArray(GlUInt) {}

void EnableVertexAttribArray(GlUInt location) {
    ActiveFake->enabledAttributes |= UINT32_C(1) << location;
}

void DisableVertexAttribArray(GlUInt location) {
    ActiveFake->enabledAttributes &=
        ~(UINT32_C(1) << location);
    ActiveFake->disabledAttributes |= UINT32_C(1) << location;
}

void VertexAttribPointer(
    GlUInt,
    GlInt,
    GlEnum,
    GlBoolean,
    GlSize,
    const void*) {}

void VertexAttribDivisor(GlUInt, GlUInt) {}

void ActiveTexture(GlEnum texture) {
    ActiveFake->activeTextureUnit =
        texture - GlConstant::Texture0;
}

void BindTexture(GlEnum, GlUInt) {}

void TexImage2D(
    GlEnum,
    GlInt,
    GlInt,
    GlSize,
    GlSize,
    GlInt,
    GlEnum,
    GlEnum,
    const void*) {}

void TexSubImage2D(
    GlEnum,
    GlInt,
    GlInt,
    GlInt,
    GlSize,
    GlSize,
    GlEnum,
    GlEnum,
    const void*) {
    ++ActiveFake->textureUploads;
}

void TexImage3D(
    GlEnum,
    GlInt,
    GlInt,
    GlSize,
    GlSize,
    GlSize,
    GlInt,
    GlEnum,
    GlEnum,
    const void*) {}

void TexSubImage3D(
    GlEnum,
    GlInt,
    GlInt,
    GlInt,
    GlInt,
    GlSize,
    GlSize,
    GlSize,
    GlEnum,
    GlEnum,
    const void*) {
    ++ActiveFake->textureUploads;
}

void TexImage2DMultisample(
    GlEnum,
    GlSize,
    GlEnum,
    GlSize,
    GlSize,
    GlBoolean) {}

void TexParameteri(GlEnum, GlEnum, GlInt) {}

void GenerateMipmap(GlEnum) {}

void BindSampler(GlUInt, GlUInt) {}

void SamplerParameteri(GlUInt, GlEnum, GlInt) {}

void SamplerParameterf(GlUInt, GlEnum, GlFloat) {}

GlUInt CreateShader(GlEnum) {
    return ActiveFake->nextObject++;
}

void ShaderSource(
    GlUInt,
    GlSize,
    const GlChar* const*,
    const GlInt*) {}

void CompileShader(GlUInt) {}

void GetShaderiv(GlUInt, GlEnum name, GlInt* value) {
    value[0] = name == GlCompileStatus ? 1 : 0;
}

void GetShaderInfoLog(GlUInt, GlSize, GlSize*, GlChar*) {}

void DeleteShader(GlUInt) {}

GlUInt CreateProgram() {
    return ActiveFake->nextObject++;
}

void AttachShader(GlUInt, GlUInt) {}

void BindAttribLocation(GlUInt, GlUInt, const GlChar*) {}

void LinkProgram(GlUInt) {}

void GetProgramiv(GlUInt, GlEnum name, GlInt* value) {
    value[0] = name == GlLinkStatus ? 1 : 0;
}

void GetProgramInfoLog(GlUInt, GlSize, GlSize*, GlChar*) {}

void DetachShader(GlUInt, GlUInt) {}

void DeleteProgram(GlUInt) {
    ++ActiveFake->deletedPrograms;
}

void UseProgram(GlUInt) {}

GlInt GetUniformLocation(GlUInt, const GlChar*) {
    return -1;
}

void Uniform1i(GlInt, GlInt) {}
void Uniform1f(GlInt, GlFloat) {}
void Uniform2f(GlInt, GlFloat, GlFloat) {}
void Uniform4f(GlInt, GlFloat, GlFloat, GlFloat, GlFloat) {}
void UniformMatrix4fv(GlInt, GlSize, GlBoolean, const GlFloat*) {}

GlUInt GetUniformBlockIndex(GlUInt, const GlChar*) {
    return UINT32_MAX;
}

void UniformBlockBinding(GlUInt, GlUInt, GlUInt) {}

void BindFramebuffer(GlEnum, GlUInt framebuffer) {
    ActiveFake->boundFramebuffer = framebuffer;
}

void FramebufferTexture2D(
    GlEnum,
    GlEnum,
    GlEnum,
    GlUInt,
    GlInt) {}

GlEnum CheckFramebufferStatus(GlEnum) {
    return GlConstant::FramebufferComplete;
}

void BlitFramebuffer(
    GlInt, GlInt, GlInt, GlInt,
    GlInt, GlInt, GlInt, GlInt,
    GlBitfield, GlEnum) {}

void ClearColor(GlFloat, GlFloat, GlFloat, GlFloat) {}
void ClearDepth(double) {}
void ClearStencil(GlInt) {}
void Clear(GlBitfield) {}
void ClearBufferfv(GlEnum, GlInt, const GlFloat*) {
    ++ActiveFake->renderPasses;
}
void ClearBufferiv(GlEnum, GlInt, const GlInt*) {}
void ClearBufferfi(GlEnum, GlInt, GlFloat, GlInt) {}
void DrawBuffers(GlSize, const GlEnum*) {}
void ReadBuffer(GlEnum) {}

void Enable(GlEnum) {}
void Disable(GlEnum) {}
void Viewport(GlInt, GlInt, GlSize, GlSize) {}
void Scissor(GlInt, GlInt, GlSize, GlSize) {}
void BlendEquationSeparate(GlEnum, GlEnum) {}
void BlendFuncSeparate(GlEnum, GlEnum, GlEnum, GlEnum) {}
void ColorMask(GlBoolean, GlBoolean, GlBoolean, GlBoolean) {}
void DepthFunc(GlEnum) {}
void DepthMask(GlBoolean) {}
void CullFace(GlEnum) {}
void FrontFace(GlEnum) {}
void PolygonMode(GlEnum, GlEnum) {}
void StencilFuncSeparate(GlEnum, GlEnum, GlInt, GlUInt) {}
void StencilOpSeparate(GlEnum, GlEnum, GlEnum, GlEnum) {}
void StencilMaskSeparate(GlEnum, GlUInt) {}
void PixelStorei(GlEnum, GlInt) {}

void DrawArrays(GlEnum, GlInt, GlSize) {
    ++ActiveFake->drawCalls;
}

void DrawElements(GlEnum, GlSize, GlEnum, const void*) {
    ++ActiveFake->indexedDrawCalls;
}

void DrawArraysInstanced(GlEnum, GlInt, GlSize, GlSize) {
    ++ActiveFake->drawCalls;
}

void DrawElementsInstanced(
    GlEnum, GlSize, GlEnum, const void*, GlSize) {
    ++ActiveFake->indexedDrawCalls;
}

void DrawElementsBaseVertex(
    GlEnum, GlSize, GlEnum, const void*, GlInt) {
    ++ActiveFake->indexedDrawCalls;
}

void DrawElementsInstancedBaseVertex(
    GlEnum, GlSize, GlEnum, const void*, GlSize, GlInt) {
    ++ActiveFake->indexedDrawCalls;
}

GlSync FenceSync(GlEnum, GlBitfield) {
    ++ActiveFake->fenceCount;
    const std::uintptr_t token =
        static_cast<std::uintptr_t>(ActiveFake->fenceCount);
    return reinterpret_cast<GlSync>(token);
}

void DeleteSync(GlSync) {
    ++ActiveFake->deletedFences;
}

GlEnum ClientWaitSync(GlSync, GlBitfield, std::uint64_t) {
    return GlConstant::AlreadySignaled;
}

void Flush() {}

void ReadPixels(
    GlInt,
    GlInt y,
    GlSize width,
    GlSize,
    GlEnum,
    GlEnum,
    void* destination) {
    auto* bytes = static_cast<std::uint8_t*>(destination);
    const std::size_t byteCount =
        static_cast<std::size_t>(width) * 4U;
    std::memset(
        bytes,
        static_cast<unsigned char>(y + 1),
        byteCount);
    ++ActiveFake->readbackRows;
}

GlFunctionTable MakeFunctions(FakeBackendGl& state) {
    ActiveFake = &state;
    Result<GlFunctionTable> loaded =
        LoadGlFunctionTable(&ResolveAll, &state);
    if (!loaded) {
        return {};
    }
    GlFunctionTable functions = loaded.Value();
    functions.getIntegerv = &GetIntegerv;
    functions.getBooleanv = &GetBooleanv;
    functions.getError = &GetError;
    functions.isEnabled = &IsEnabled;
    functions.enable = &Enable;
    functions.disable = &Disable;
    functions.viewport = &Viewport;
    functions.scissor = &Scissor;
    functions.blendEquationSeparate = &BlendEquationSeparate;
    functions.blendFuncSeparate = &BlendFuncSeparate;
    functions.colorMask = &ColorMask;
    functions.depthFunc = &DepthFunc;
    functions.depthMask = &DepthMask;
    functions.cullFace = &CullFace;
    functions.frontFace = &FrontFace;
    functions.polygonMode = &PolygonMode;
    functions.stencilFuncSeparate = &StencilFuncSeparate;
    functions.stencilOpSeparate = &StencilOpSeparate;
    functions.stencilMaskSeparate = &StencilMaskSeparate;
    functions.pixelStorei = &PixelStorei;
    functions.activeTexture = &ActiveTexture;
    functions.genBuffers = &GenObjects;
    functions.deleteBuffers = &DeleteBuffers;
    functions.bindBuffer = &BindBuffer;
    functions.bufferData = &BufferData;
    functions.bufferSubData = &BufferSubData;
    functions.bindBufferRange = &BindBufferRange;
    functions.bindBufferBase = &BindBufferBase;
    functions.genVertexArrays = &GenObjects;
    functions.deleteVertexArrays = &DeleteObjects;
    functions.bindVertexArray = &BindVertexArray;
    functions.enableVertexAttribArray = &EnableVertexAttribArray;
    functions.disableVertexAttribArray = &DisableVertexAttribArray;
    functions.vertexAttribPointer = &VertexAttribPointer;
    functions.vertexAttribDivisor = &VertexAttribDivisor;
    functions.genTextures = &GenObjects;
    functions.deleteTextures = &DeleteTextures;
    functions.bindTexture = &BindTexture;
    functions.texImage2D = &TexImage2D;
    functions.texSubImage2D = &TexSubImage2D;
    functions.texImage3D = &TexImage3D;
    functions.texSubImage3D = &TexSubImage3D;
    functions.texImage2DMultisample = &TexImage2DMultisample;
    functions.texParameteri = &TexParameteri;
    functions.generateMipmap = &GenerateMipmap;
    functions.genSamplers = &GenObjects;
    functions.deleteSamplers = &DeleteSamplers;
    functions.bindSampler = &BindSampler;
    functions.samplerParameteri = &SamplerParameteri;
    functions.samplerParameterf = &SamplerParameterf;
    functions.createShader = &CreateShader;
    functions.shaderSource = &ShaderSource;
    functions.compileShader = &CompileShader;
    functions.getShaderiv = &GetShaderiv;
    functions.getShaderInfoLog = &GetShaderInfoLog;
    functions.deleteShader = &DeleteShader;
    functions.createProgram = &CreateProgram;
    functions.attachShader = &AttachShader;
    functions.bindAttribLocation = &BindAttribLocation;
    functions.linkProgram = &LinkProgram;
    functions.getProgramiv = &GetProgramiv;
    functions.getProgramInfoLog = &GetProgramInfoLog;
    functions.detachShader = &DetachShader;
    functions.deleteProgram = &DeleteProgram;
    functions.useProgram = &UseProgram;
    functions.getUniformLocation = &GetUniformLocation;
    functions.uniform1i = &Uniform1i;
    functions.uniform1f = &Uniform1f;
    functions.uniform2f = &Uniform2f;
    functions.uniform4f = &Uniform4f;
    functions.uniformMatrix4fv = &UniformMatrix4fv;
    functions.getUniformBlockIndex = &GetUniformBlockIndex;
    functions.uniformBlockBinding = &UniformBlockBinding;
    functions.genFramebuffers = &GenObjects;
    functions.deleteFramebuffers = &DeleteObjects;
    functions.bindFramebuffer = &BindFramebuffer;
    functions.framebufferTexture2D = &FramebufferTexture2D;
    functions.checkFramebufferStatus = &CheckFramebufferStatus;
    functions.blitFramebuffer = &BlitFramebuffer;
    functions.clearColor = &ClearColor;
    functions.clearDepth = &ClearDepth;
    functions.clearStencil = &ClearStencil;
    functions.clear = &Clear;
    functions.clearBufferfv = &ClearBufferfv;
    functions.clearBufferiv = &ClearBufferiv;
    functions.clearBufferfi = &ClearBufferfi;
    functions.drawBuffers = &DrawBuffers;
    functions.readBuffer = &ReadBuffer;
    functions.drawArrays = &DrawArrays;
    functions.drawElements = &DrawElements;
    functions.drawArraysInstanced = &DrawArraysInstanced;
    functions.drawElementsInstanced = &DrawElementsInstanced;
    functions.drawElementsBaseVertex = &DrawElementsBaseVertex;
    functions.drawElementsInstancedBaseVertex =
        &DrawElementsInstancedBaseVertex;
    functions.fenceSync = &FenceSync;
    functions.deleteSync = &DeleteSync;
    functions.clientWaitSync = &ClientWaitSync;
    functions.flush = &Flush;
    functions.readPixels = &ReadPixels;
    return functions;
}

GlContextContract MakeContext(FakeBackendGl& state) noexcept {
    GlContextContract context;
    context.userData = &state;
    context.contextHandle = &state;
    context.resolve = &ResolveAll;
    context.isCurrent = &IsCurrent;
    context.currentThreadToken = &CurrentThread;
    context.owningThreadToken = state.thread;
    context.generation = 5U;
    context.embeddingMode = GlEmbeddingMode::HostReset;
    return context;
}

PipelineDescriptor MakePipeline() noexcept {
    static const std::uint8_t VertexSource[] =
        "#version 330 core\n"
        "layout(location=0) in vec2 position;\n"
        "void main(){gl_Position=vec4(position,0,1);}\n";
    static const std::uint8_t FragmentSource[] =
        "#version 330 core\n"
        "out vec4 color;\n"
        "void main(){color=vec4(1);}\n";

    PipelineDescriptor descriptor;
    descriptor.vertexShader.stage = ShaderStage::Vertex;
    descriptor.vertexShader.language = ShaderLanguage::Glsl330;
    descriptor.vertexShader.bytecode = VertexSource;
    descriptor.vertexShader.bytecodeSize =
        static_cast<std::uint32_t>(sizeof(VertexSource) - 1U);
    descriptor.vertexShader.entryPoint = StringView("main");
    descriptor.vertexShader.stableId = 1001U;
    descriptor.fragmentShader.stage = ShaderStage::Fragment;
    descriptor.fragmentShader.language = ShaderLanguage::Glsl330;
    descriptor.fragmentShader.bytecode = FragmentSource;
    descriptor.fragmentShader.bytecodeSize =
        static_cast<std::uint32_t>(sizeof(FragmentSource) - 1U);
    descriptor.fragmentShader.entryPoint = StringView("main");
    descriptor.fragmentShader.stableId = 1002U;
    descriptor.vertexLayout.bufferCount = 1U;
    descriptor.vertexLayout.attributeCount = 1U;
    descriptor.vertexLayout.buffers[0U].stride = 8U;
    descriptor.vertexLayout.attributes[0U].location = 0U;
    descriptor.vertexLayout.attributes[0U].bufferSlot = 0U;
    descriptor.vertexLayout.attributes[0U].format =
        VertexFormat::Float2;
    descriptor.blend.enabled = true;
    descriptor.blend.color.source = BlendFactor::SourceAlpha;
    descriptor.blend.color.destination =
        BlendFactor::OneMinusSourceAlpha;
    descriptor.blend.alpha.source = BlendFactor::One;
    descriptor.blend.alpha.destination =
        BlendFactor::OneMinusSourceAlpha;
    descriptor.depthStencil.depthTestEnabled = true;
    descriptor.depthStencil.depthWriteEnabled = true;
    descriptor.depthStencil.depthCompare = CompareOperation::LessEqual;
    return descriptor;
}

bool TestResourcesSubmissionReadbackAndExternalImport() {
    FakeBackendGl state;
    GlFunctionTable functions = MakeFunctions(state);
    OpenGL33BackendOptions options;
    options.embeddingMode = GlEmbeddingMode::HostReset;
    options.checkErrors = true;
    OpenGL33GraphicsBackend backend(
        functions, MakeContext(state), options);
    CHECK(backend.Initialize());
    CHECK(backend.IsInitialized());
    CHECK(backend.Kind() == GraphicsBackendKind::OpenGL33);
    CHECK(backend.Capabilities().maxTextureDimension == 8192U);
    CHECK(backend.QueryGraphicsCapabilities().shaderLanguages ==
        ShaderLanguageBit(ShaderLanguage::Glsl330));

    RhiDevice device(backend);
    CHECK(device.Initialize());

    BufferDescriptor vertexDescriptor;
    vertexDescriptor.sizeBytes = 256U;
    vertexDescriptor.usage = BufferUsage::Vertex;
    Result<ResourceHandle> vertex =
        device.CreateBuffer(vertexDescriptor);
    CHECK(vertex);

    BufferDescriptor indexDescriptor;
    indexDescriptor.sizeBytes = 128U;
    indexDescriptor.usage = BufferUsage::Index;
    Result<ResourceHandle> index =
        device.CreateBuffer(indexDescriptor);
    CHECK(index);

    BufferDescriptor uniformDescriptor;
    uniformDescriptor.sizeBytes = 256U;
    uniformDescriptor.usage = BufferUsage::Uniform;
    Result<ResourceHandle> uniform =
        device.CreateBuffer(uniformDescriptor);
    CHECK(uniform);

    TextureResourceDescriptor sampledDescriptor;
    sampledDescriptor.width = 4U;
    sampledDescriptor.height = 4U;
    sampledDescriptor.usage =
        TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination) |
        TextureUsageBit(TextureUsage::CopySource);
    Result<ResourceHandle> sampled =
        device.CreateTexture(sampledDescriptor);
    CHECK(sampled);

    TextureResourceDescriptor colorDescriptor;
    colorDescriptor.width = 8U;
    colorDescriptor.height = 8U;
    colorDescriptor.usage =
        TextureUsageBit(TextureUsage::RenderTarget) |
        TextureUsageBit(TextureUsage::CopySource);
    Result<ResourceHandle> color =
        device.CreateRenderTarget(colorDescriptor);
    CHECK(color);

    TextureResourceDescriptor depthDescriptor;
    depthDescriptor.width = 8U;
    depthDescriptor.height = 8U;
    depthDescriptor.format =
        GraphicsTextureFormat::Depth24Stencil8;
    depthDescriptor.usage =
        TextureUsageBit(TextureUsage::RenderTarget);
    Result<ResourceHandle> depth =
        device.CreateRenderTarget(depthDescriptor);
    CHECK(depth);

    SamplerDescriptor samplerDescriptor;
    Result<ResourceHandle> sampler =
        device.CreateSampler(samplerDescriptor);
    CHECK(sampler);

    Result<ResourceHandle> pipeline =
        device.CreatePipeline(MakePipeline());
    CHECK(pipeline);
    CHECK(backend.LiveResourceCount() == 8U);

    const std::uint8_t vertexBytes[24]{};
    const std::uint8_t indexBytes[6]{};
    const std::uint8_t textureBytes[64]{};
    CommandEncoder encoder;
    CHECK(encoder.UploadBuffer(
        vertex.Value(), 0U,
        Span<const std::uint8_t>(vertexBytes, sizeof(vertexBytes))));
    CHECK(encoder.UploadBuffer(
        index.Value(), 0U,
        Span<const std::uint8_t>(indexBytes, sizeof(indexBytes))));
    TextureRegion region;
    region.width = 4U;
    region.height = 4U;
    region.bytesPerRow = 16U;
    CHECK(encoder.UploadTexture(
        sampled.Value(),
        region,
        Span<const std::uint8_t>(
            textureBytes, sizeof(textureBytes))));

    RenderPassDescriptor pass;
    pass.renderArea = {0.0F, 0.0F, 8.0F, 8.0F};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0U].target = color.Value();
    pass.colorAttachments[0U].load = LoadOperation::Clear;
    pass.colorAttachments[0U].clearColor =
        {0.1F, 0.2F, 0.3F, 1.0F};
    pass.hasDepthStencil = true;
    pass.depthStencil.target = depth.Value();
    pass.depthStencil.depthLoad = LoadOperation::Clear;
    pass.depthStencil.stencilLoad = LoadOperation::Clear;
    pass.depthStencil.clearDepth = 1.0F;
    CHECK(encoder.BeginRenderPass(pass));
    CHECK(encoder.BindPipeline(pipeline.Value()));
    CHECK(encoder.BindVertexBuffer(0U, vertex.Value()));
    CHECK(encoder.BindIndexBuffer(
        index.Value(), IndexType::UInt16));
    CHECK(encoder.BindUniformBuffer(
        0U, uniform.Value(), 0U, 64U));
    CHECK(encoder.BindTextureSampler(
        0U, sampled.Value(), sampler.Value()));
    CHECK(encoder.SetScissor(
        {0.0F, 0.0F, 8.0F, 8.0F}));
    CHECK(encoder.Draw(3U));
    CHECK(encoder.DrawIndexed(3U));
    CHECK(encoder.EndRenderPass());
    Result<CommandList> commands = encoder.Finish();
    CHECK(commands);
    Result<FenceValue> submitted =
        device.Submit(commands.Value());
    CHECK(submitted);
    CHECK(submitted.Value() == 1U);
    CHECK(backend.WaitForFence(submitted.Value()));
    CHECK(backend.CompletedFence() == submitted.Value());
    CHECK(state.bufferUploads == 2U);
    CHECK(state.textureUploads == 1U);
    CHECK(state.renderPasses == 1U);
    CHECK(state.drawCalls == 1U);
    CHECK(state.indexedDrawCalls == 1U);

    PipelineDescriptor attributeFreeDescriptor = MakePipeline();
    attributeFreeDescriptor.vertexLayout =
        VertexLayoutDescriptor{};
    Result<ResourceHandle> attributeFreePipeline =
        device.CreatePipeline(attributeFreeDescriptor);
    CHECK(attributeFreePipeline);
    CommandEncoder attributeFreeEncoder;
    CHECK(attributeFreeEncoder.BeginRenderPass(pass));
    CHECK(attributeFreeEncoder.BindPipeline(
        attributeFreePipeline.Value()));
    CHECK(attributeFreeEncoder.Draw(3U));
    CHECK(attributeFreeEncoder.EndRenderPass());
    Result<CommandList> attributeFreeCommands =
        attributeFreeEncoder.Finish();
    CHECK(attributeFreeCommands);
    Result<FenceValue> attributeFreeFence =
        device.Submit(attributeFreeCommands.Value());
    CHECK(attributeFreeFence);
    CHECK(backend.WaitForFence(attributeFreeFence.Value()));
    CHECK((state.disabledAttributes & 1U) != 0U);
    CHECK((state.enabledAttributes & 1U) == 0U);

    std::uint8_t readback[8U * 8U * 4U]{};
    CHECK(backend.ReadbackTexture(
        color.Value(),
        Span<std::uint8_t>(readback, sizeof(readback)),
        8U * 4U));
    CHECK(state.readbackRows == 8U);
    CHECK(readback[0U] == 8U);
    CHECK(readback[7U * 8U * 4U] == 1U);
    Result<std::uint64_t> checksum =
        backend.ReadbackTextureChecksum(color.Value());
    CHECK(checksum);
    CHECK(checksum.Value() != 0U);

    OpenGL33ExternalRenderTargetDescriptor external;
    external.framebuffer = 900U;
    external.colorTexture = 901U;
    external.texture = colorDescriptor;
    external.contextGeneration = 5U;
    external.stableId = 77U;
    Result<ResourceHandle> imported =
        ImportOpenGL33ExternalRenderTarget(
            device, backend, external);
    CHECK(imported);
    const std::uint32_t deletesBeforeExternal =
        state.deletedTextures;
    CHECK(device.DestroyResource(imported.Value()));
    CHECK(device.CollectGarbage());
    CHECK(state.deletedTextures == deletesBeforeExternal);

    OpenGL33ExternalTextureDescriptor externalTexture;
    externalTexture.texture = 902U;
    externalTexture.descriptor = sampledDescriptor;
    externalTexture.contextGeneration = 5U;
    externalTexture.stableId = 78U;
    Result<ResourceHandle> importedTexture =
        ImportOpenGL33ExternalTexture(
            device, backend, externalTexture);
    CHECK(importedTexture);
    CHECK(device.DestroyResource(importedTexture.Value()));
    CHECK(device.CollectGarbage());
    CHECK(state.deletedTextures == deletesBeforeExternal);

    CHECK(device.DestroyResource(
        sampled.Value(), submitted.Value()));
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected);
    CHECK(collected.Value() >= 1U);
    CHECK(state.deletedTextures > deletesBeforeExternal);
    return true;
}

bool TestWrongThreadAndContextLoss() {
    FakeBackendGl state;
    OpenGL33GraphicsBackend backend(
        MakeFunctions(state), MakeContext(state));
    CHECK(backend.Initialize());
    state.thread = 20U;
    ResourceDescriptor resource;
    resource.type = ResourceType::Buffer;
    resource.buffer.sizeBytes = 16U;
    resource.buffer.usage = BufferUsage::Vertex;
    Result<void> wrongThread = backend.CreateResource(
        {0U, 1U, ResourceType::Buffer}, resource);
    CHECK(!wrongThread);
    CHECK(wrongThread.GetStatus().code == ErrorCode::WrongThread);
    state.thread = 19U;
    backend.NotifyContextLost();
    CHECK(backend.IsDeviceLost());
    Result<void> lost = backend.CreateResource(
        {0U, 1U, ResourceType::Buffer}, resource);
    CHECK(!lost);
    CHECK(lost.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

} // namespace

int main() {
    if (!TestResourcesSubmissionReadbackAndExternalImport() ||
        !TestWrongThreadAndContextLoss()) {
        return 1;
    }
    std::puts("OpenGL 3.3 backend tests passed");
    return 0;
}
