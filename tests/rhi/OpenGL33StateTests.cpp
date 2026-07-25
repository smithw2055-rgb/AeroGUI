#include <Aero/Rhi/OpenGL33.hpp>
#include <Aero/Rhi/OpenGL33State.hpp>

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

constexpr GlEnum FuncAdd = 0x8006U;
constexpr GlEnum One = 1U;
constexpr GlEnum Zero = 0U;
constexpr GlEnum Less = 0x0201U;
constexpr GlEnum Always = 0x0207U;
constexpr GlEnum Keep = 0x1E00U;
constexpr GlEnum CounterClockwise = 0x0901U;
constexpr GlEnum Fill = 0x1B02U;

struct FakeGlState final {
    GlInt major = 3;
    GlInt minor = 3;
    GlInt profile = GlConstant::ContextCoreProfileBit;
    GlInt contextFlags = GlConstant::ContextFlagDebugBit;
    GlInt maxTextureSize = 8192;
    GlInt maxArrayTextureLayers = 256;
    GlInt maxTextureUnits = 4;
    GlInt maxVertexAttributes = 16;
    GlInt maxUniformBlockSize = 65536;
    GlInt maxUniformBufferBindings = 36;
    GlInt uniformAlignment = 256;
    GlInt maxSamples = 8;
    GlInt maxColorAttachments = 4;

    GlUInt program = 1U;
    GlUInt vertexArray = 2U;
    GlUInt arrayBuffer = 3U;
    GlUInt elementArrayBuffer = 4U;
    GlUInt uniformBuffer = 5U;
    GlUInt drawFramebuffer = 6U;
    GlUInt readFramebuffer = 7U;
    GlRectangleState viewport{1, 2, 640, 480};
    bool scissorEnabled = true;
    GlRectangleState scissor{3, 4, 320, 240};
    GlBlendState blend{
        true, FuncAdd, FuncAdd, One, Zero, One, Zero,
        true, false, true, false};
    GlDepthState depth{true, Less, false};
    GlRasterState raster{true, GlConstant::Back, CounterClockwise, Fill};
    GlStencilState stencil{
        true,
        {Always, 7, 0x7FU, Keep, Keep, Keep, 0x3FU},
        {Always, 8, 0x6FU, Keep, Keep, Keep, 0x2FU}};
    std::uint32_t activeTextureUnit = 1U;
    GlUInt textures[MaxCachedGlTextureUnits]{};
    GlUInt textureArrays[MaxCachedGlTextureUnits]{};
    GlUInt samplers[MaxCachedGlTextureUnits]{};
    GlPixelUnpackState unpack{4, 12, 2, 3};

    GlThreadToken thread = 77U;
    bool current = true;
    const char* missingName = nullptr;
    std::uint64_t stateCalls = 0U;
    std::uint64_t programCalls = 0U;
};

FakeGlState* ActiveFake = nullptr;

void NoopGlProc() {}

GlProcAddress ResolveFake(void* userData, const char* name) noexcept {
    auto* state = static_cast<FakeGlState*>(userData);
    if (state->missingName != nullptr &&
        std::strcmp(state->missingName, name) == 0) {
        return nullptr;
    }
    return reinterpret_cast<GlProcAddress>(&NoopGlProc);
}

bool IsCurrent(
    void* userData,
    const void* contextHandle) noexcept {
    auto* state = static_cast<FakeGlState*>(userData);
    return state->current && contextHandle == state;
}

GlThreadToken CurrentThread(void* userData) noexcept {
    return static_cast<FakeGlState*>(userData)->thread;
}

const std::uint8_t* MockGetString(GlEnum) {
    static const std::uint8_t Version[] = "3.3 Fake";
    return Version;
}

void MockGetIntegerv(GlEnum name, GlInt* value) {
    FakeGlState& state = *ActiveFake;
    switch (name) {
    case GlConstant::MajorVersion:
        value[0] = state.major;
        break;
    case GlConstant::MinorVersion:
        value[0] = state.minor;
        break;
    case GlConstant::ContextProfileMask:
        value[0] = state.profile;
        break;
    case GlConstant::ContextFlags:
        value[0] = state.contextFlags;
        break;
    case GlConstant::MaxTextureSize:
        value[0] = state.maxTextureSize;
        break;
    case GlConstant::MaxArrayTextureLayers:
        value[0] = state.maxArrayTextureLayers;
        break;
    case GlConstant::MaxCombinedTextureImageUnits:
        value[0] = state.maxTextureUnits;
        break;
    case GlConstant::MaxVertexAttribs:
        value[0] = state.maxVertexAttributes;
        break;
    case GlConstant::MaxUniformBlockSize:
        value[0] = state.maxUniformBlockSize;
        break;
    case GlConstant::MaxUniformBufferBindings:
        value[0] = state.maxUniformBufferBindings;
        break;
    case GlConstant::UniformBufferOffsetAlignment:
        value[0] = state.uniformAlignment;
        break;
    case GlConstant::MaxSamples:
        value[0] = state.maxSamples;
        break;
    case GlConstant::MaxColorAttachments:
        value[0] = state.maxColorAttachments;
        break;
    case GlConstant::CurrentProgram:
        value[0] = static_cast<GlInt>(state.program);
        break;
    case GlConstant::VertexArrayBinding:
        value[0] = static_cast<GlInt>(state.vertexArray);
        break;
    case GlConstant::ArrayBufferBinding:
        value[0] = static_cast<GlInt>(state.arrayBuffer);
        break;
    case GlConstant::ElementArrayBufferBinding:
        value[0] = static_cast<GlInt>(state.elementArrayBuffer);
        break;
    case GlConstant::UniformBufferBinding:
        value[0] = static_cast<GlInt>(state.uniformBuffer);
        break;
    case GlConstant::DrawFramebufferBinding:
        value[0] = static_cast<GlInt>(state.drawFramebuffer);
        break;
    case GlConstant::ReadFramebufferBinding:
        value[0] = static_cast<GlInt>(state.readFramebuffer);
        break;
    case GlConstant::Viewport:
        value[0] = state.viewport.x;
        value[1] = state.viewport.y;
        value[2] = state.viewport.width;
        value[3] = state.viewport.height;
        break;
    case GlConstant::ScissorBox:
        value[0] = state.scissor.x;
        value[1] = state.scissor.y;
        value[2] = state.scissor.width;
        value[3] = state.scissor.height;
        break;
    case GlConstant::BlendEquationRgb:
        value[0] = static_cast<GlInt>(state.blend.colorEquation);
        break;
    case GlConstant::BlendEquationAlpha:
        value[0] = static_cast<GlInt>(state.blend.alphaEquation);
        break;
    case GlConstant::BlendSrcRgb:
        value[0] = static_cast<GlInt>(state.blend.sourceColor);
        break;
    case GlConstant::BlendDstRgb:
        value[0] = static_cast<GlInt>(state.blend.destinationColor);
        break;
    case GlConstant::BlendSrcAlpha:
        value[0] = static_cast<GlInt>(state.blend.sourceAlpha);
        break;
    case GlConstant::BlendDstAlpha:
        value[0] = static_cast<GlInt>(state.blend.destinationAlpha);
        break;
    case GlConstant::DepthFunc:
        value[0] = static_cast<GlInt>(state.depth.function);
        break;
    case GlConstant::CullFaceMode:
        value[0] = static_cast<GlInt>(state.raster.cullFace);
        break;
    case GlConstant::FrontFace:
        value[0] = static_cast<GlInt>(state.raster.frontFace);
        break;
    case GlConstant::PolygonMode:
        value[0] = static_cast<GlInt>(state.raster.polygonMode);
        value[1] = static_cast<GlInt>(state.raster.polygonMode);
        break;
    case GlConstant::StencilFunc:
        value[0] = static_cast<GlInt>(state.stencil.front.function);
        break;
    case GlConstant::StencilRef:
        value[0] = state.stencil.front.reference;
        break;
    case GlConstant::StencilValueMask:
        value[0] = static_cast<GlInt>(state.stencil.front.readMask);
        break;
    case GlConstant::StencilFail:
        value[0] = static_cast<GlInt>(state.stencil.front.stencilFail);
        break;
    case GlConstant::StencilPassDepthFail:
        value[0] = static_cast<GlInt>(state.stencil.front.depthFail);
        break;
    case GlConstant::StencilPassDepthPass:
        value[0] = static_cast<GlInt>(state.stencil.front.pass);
        break;
    case GlConstant::StencilWritemask:
        value[0] = static_cast<GlInt>(state.stencil.front.writeMask);
        break;
    case GlConstant::StencilBackFunc:
        value[0] = static_cast<GlInt>(state.stencil.back.function);
        break;
    case GlConstant::StencilBackRef:
        value[0] = state.stencil.back.reference;
        break;
    case GlConstant::StencilBackValueMask:
        value[0] = static_cast<GlInt>(state.stencil.back.readMask);
        break;
    case GlConstant::StencilBackFail:
        value[0] = static_cast<GlInt>(state.stencil.back.stencilFail);
        break;
    case GlConstant::StencilBackPassDepthFail:
        value[0] = static_cast<GlInt>(state.stencil.back.depthFail);
        break;
    case GlConstant::StencilBackPassDepthPass:
        value[0] = static_cast<GlInt>(state.stencil.back.pass);
        break;
    case GlConstant::StencilBackWritemask:
        value[0] = static_cast<GlInt>(state.stencil.back.writeMask);
        break;
    case GlConstant::ActiveTexture:
        value[0] = static_cast<GlInt>(
            GlConstant::Texture0 + state.activeTextureUnit);
        break;
    case GlConstant::TextureBinding2D:
        value[0] = static_cast<GlInt>(
            state.textures[state.activeTextureUnit]);
        break;
    case GlConstant::TextureBinding2DArray:
        value[0] = static_cast<GlInt>(
            state.textureArrays[state.activeTextureUnit]);
        break;
    case GlConstant::SamplerBinding:
        value[0] = static_cast<GlInt>(
            state.samplers[state.activeTextureUnit]);
        break;
    case GlConstant::UnpackAlignment:
        value[0] = state.unpack.alignment;
        break;
    case GlConstant::UnpackRowLength:
        value[0] = state.unpack.rowLength;
        break;
    case GlConstant::UnpackSkipRows:
        value[0] = state.unpack.skipRows;
        break;
    case GlConstant::UnpackSkipPixels:
        value[0] = state.unpack.skipPixels;
        break;
    default:
        value[0] = 0;
        break;
    }
}

void MockGetBooleanv(GlEnum name, GlBoolean* value) {
    FakeGlState& state = *ActiveFake;
    if (name == GlConstant::ColorWritemask) {
        value[0] = state.blend.writeRed ? 1U : 0U;
        value[1] = state.blend.writeGreen ? 1U : 0U;
        value[2] = state.blend.writeBlue ? 1U : 0U;
        value[3] = state.blend.writeAlpha ? 1U : 0U;
    } else if (name == GlConstant::DepthWritemask) {
        value[0] = state.depth.writeEnabled ? 1U : 0U;
    } else {
        value[0] = 0U;
    }
}

GlBoolean MockIsEnabled(GlEnum name) {
    const FakeGlState& state = *ActiveFake;
    if (name == GlConstant::ScissorTest) {
        return state.scissorEnabled ? 1U : 0U;
    }
    if (name == GlConstant::Blend) {
        return state.blend.enabled ? 1U : 0U;
    }
    if (name == GlConstant::DepthTest) {
        return state.depth.enabled ? 1U : 0U;
    }
    if (name == GlConstant::StencilTest) {
        return state.stencil.enabled ? 1U : 0U;
    }
    if (name == GlConstant::CullFace) {
        return state.raster.cullEnabled ? 1U : 0U;
    }
    return 0U;
}

void MockSetEnabled(GlEnum name, bool enabled) {
    FakeGlState& state = *ActiveFake;
    if (name == GlConstant::ScissorTest) {
        state.scissorEnabled = enabled;
    } else if (name == GlConstant::Blend) {
        state.blend.enabled = enabled;
    } else if (name == GlConstant::DepthTest) {
        state.depth.enabled = enabled;
    } else if (name == GlConstant::StencilTest) {
        state.stencil.enabled = enabled;
    } else if (name == GlConstant::CullFace) {
        state.raster.cullEnabled = enabled;
    }
    ++state.stateCalls;
}

void MockEnable(GlEnum name) {
    MockSetEnabled(name, true);
}

void MockDisable(GlEnum name) {
    MockSetEnabled(name, false);
}

void MockUseProgram(GlUInt program) {
    ActiveFake->program = program;
    ++ActiveFake->programCalls;
    ++ActiveFake->stateCalls;
}

void MockBindVertexArray(GlUInt vertexArray) {
    ActiveFake->vertexArray = vertexArray;
    ++ActiveFake->stateCalls;
}

void MockBindBuffer(GlEnum target, GlUInt buffer) {
    if (target == GlConstant::ArrayBuffer) {
        ActiveFake->arrayBuffer = buffer;
    } else if (target == GlConstant::ElementArrayBuffer) {
        ActiveFake->elementArrayBuffer = buffer;
    } else if (target == GlConstant::UniformBuffer) {
        ActiveFake->uniformBuffer = buffer;
    }
    ++ActiveFake->stateCalls;
}

void MockBindFramebuffer(GlEnum target, GlUInt framebuffer) {
    if (target == GlConstant::DrawFramebuffer) {
        ActiveFake->drawFramebuffer = framebuffer;
    } else if (target == GlConstant::ReadFramebuffer) {
        ActiveFake->readFramebuffer = framebuffer;
    } else {
        ActiveFake->drawFramebuffer = framebuffer;
        ActiveFake->readFramebuffer = framebuffer;
    }
    ++ActiveFake->stateCalls;
}

void MockViewport(GlInt x, GlInt y, GlSize width, GlSize height) {
    ActiveFake->viewport = {x, y, width, height};
    ++ActiveFake->stateCalls;
}

void MockScissor(GlInt x, GlInt y, GlSize width, GlSize height) {
    ActiveFake->scissor = {x, y, width, height};
    ++ActiveFake->stateCalls;
}

void MockBlendEquation(GlEnum color, GlEnum alpha) {
    ActiveFake->blend.colorEquation = color;
    ActiveFake->blend.alphaEquation = alpha;
    ++ActiveFake->stateCalls;
}

void MockBlendFunc(
    GlEnum sourceColor,
    GlEnum destinationColor,
    GlEnum sourceAlpha,
    GlEnum destinationAlpha) {
    ActiveFake->blend.sourceColor = sourceColor;
    ActiveFake->blend.destinationColor = destinationColor;
    ActiveFake->blend.sourceAlpha = sourceAlpha;
    ActiveFake->blend.destinationAlpha = destinationAlpha;
    ++ActiveFake->stateCalls;
}

void MockColorMask(
    GlBoolean red,
    GlBoolean green,
    GlBoolean blue,
    GlBoolean alpha) {
    ActiveFake->blend.writeRed = red != 0U;
    ActiveFake->blend.writeGreen = green != 0U;
    ActiveFake->blend.writeBlue = blue != 0U;
    ActiveFake->blend.writeAlpha = alpha != 0U;
    ++ActiveFake->stateCalls;
}

void MockDepthFunc(GlEnum function) {
    ActiveFake->depth.function = function;
    ++ActiveFake->stateCalls;
}

void MockDepthMask(GlBoolean enabled) {
    ActiveFake->depth.writeEnabled = enabled != 0U;
    ++ActiveFake->stateCalls;
}

void MockCullFace(GlEnum face) {
    ActiveFake->raster.cullFace = face;
    ++ActiveFake->stateCalls;
}

void MockFrontFace(GlEnum face) {
    ActiveFake->raster.frontFace = face;
    ++ActiveFake->stateCalls;
}

void MockPolygonMode(GlEnum, GlEnum mode) {
    ActiveFake->raster.polygonMode = mode;
    ++ActiveFake->stateCalls;
}

GlStencilFaceState& StencilFace(GlEnum face) {
    return face == GlConstant::Back
        ? ActiveFake->stencil.back
        : ActiveFake->stencil.front;
}

void MockStencilFunc(
    GlEnum face,
    GlEnum function,
    GlInt reference,
    GlUInt mask) {
    GlStencilFaceState& state = StencilFace(face);
    state.function = function;
    state.reference = reference;
    state.readMask = mask;
    ++ActiveFake->stateCalls;
}

void MockStencilOp(
    GlEnum face,
    GlEnum stencilFail,
    GlEnum depthFail,
    GlEnum pass) {
    GlStencilFaceState& state = StencilFace(face);
    state.stencilFail = stencilFail;
    state.depthFail = depthFail;
    state.pass = pass;
    ++ActiveFake->stateCalls;
}

void MockStencilMask(GlEnum face, GlUInt mask) {
    StencilFace(face).writeMask = mask;
    ++ActiveFake->stateCalls;
}

void MockActiveTexture(GlEnum texture) {
    ActiveFake->activeTextureUnit =
        texture - GlConstant::Texture0;
    ++ActiveFake->stateCalls;
}

void MockBindTexture(GlEnum target, GlUInt texture) {
    if (target == GlConstant::Texture2DArray) {
        ActiveFake->textureArrays[
            ActiveFake->activeTextureUnit] = texture;
    } else {
        ActiveFake->textures[
            ActiveFake->activeTextureUnit] = texture;
    }
    ++ActiveFake->stateCalls;
}

void MockBindSampler(GlUInt unit, GlUInt sampler) {
    ActiveFake->samplers[unit] = sampler;
    ++ActiveFake->stateCalls;
}

void MockPixelStore(GlEnum name, GlInt value) {
    if (name == GlConstant::UnpackAlignment) {
        ActiveFake->unpack.alignment = value;
    } else if (name == GlConstant::UnpackRowLength) {
        ActiveFake->unpack.rowLength = value;
    } else if (name == GlConstant::UnpackSkipRows) {
        ActiveFake->unpack.skipRows = value;
    } else if (name == GlConstant::UnpackSkipPixels) {
        ActiveFake->unpack.skipPixels = value;
    }
    ++ActiveFake->stateCalls;
}

GlFunctionTable MakeFunctions(FakeGlState& state) {
    ActiveFake = &state;
    Result<GlFunctionTable> loaded =
        LoadGlFunctionTable(&ResolveFake, &state);
    if (!loaded) {
        return {};
    }
    GlFunctionTable functions = loaded.Value();
    functions.getString = &MockGetString;
    functions.getIntegerv = &MockGetIntegerv;
    functions.getBooleanv = &MockGetBooleanv;
    functions.isEnabled = &MockIsEnabled;
    functions.enable = &MockEnable;
    functions.disable = &MockDisable;
    functions.useProgram = &MockUseProgram;
    functions.bindVertexArray = &MockBindVertexArray;
    functions.bindBuffer = &MockBindBuffer;
    functions.bindFramebuffer = &MockBindFramebuffer;
    functions.viewport = &MockViewport;
    functions.scissor = &MockScissor;
    functions.blendEquationSeparate = &MockBlendEquation;
    functions.blendFuncSeparate = &MockBlendFunc;
    functions.colorMask = &MockColorMask;
    functions.depthFunc = &MockDepthFunc;
    functions.depthMask = &MockDepthMask;
    functions.cullFace = &MockCullFace;
    functions.frontFace = &MockFrontFace;
    functions.polygonMode = &MockPolygonMode;
    functions.stencilFuncSeparate = &MockStencilFunc;
    functions.stencilOpSeparate = &MockStencilOp;
    functions.stencilMaskSeparate = &MockStencilMask;
    functions.activeTexture = &MockActiveTexture;
    functions.bindTexture = &MockBindTexture;
    functions.bindSampler = &MockBindSampler;
    functions.pixelStorei = &MockPixelStore;
    return functions;
}

GlContextContract MakeContract(FakeGlState& state) noexcept {
    GlContextContract contract;
    contract.userData = &state;
    contract.contextHandle = &state;
    contract.resolve = &ResolveFake;
    contract.isCurrent = &IsCurrent;
    contract.currentThreadToken = &CurrentThread;
    contract.owningThreadToken = state.thread;
    contract.generation = 5U;
    contract.embeddingMode = GlEmbeddingMode::PreserveAndRestore;
    return contract;
}

bool TestFunctionTableAndContextValidation() {
    FakeGlState state;
    ActiveFake = &state;
    Result<GlFunctionTable> loaded =
        LoadGlFunctionTable(&ResolveFake, &state);
    CHECK(loaded);
    CHECK(ValidateGlFunctionTable(loaded.Value()));

    state.missingName = "glDrawElements";
    Result<GlFunctionTable> incomplete =
        LoadGlFunctionTable(&ResolveFake, &state);
    CHECK(!incomplete);
    CHECK(incomplete.GetStatus().code == ErrorCode::InvalidArgument);
    state.missingName = nullptr;

    GlContextContract contract = MakeContract(state);
    CHECK(ValidateGlContextContract(contract));
    contract.owningThreadToken = state.thread + 1U;
    Result<void> wrongThread = ValidateGlContextContract(contract);
    CHECK(!wrongThread);
    CHECK(wrongThread.GetStatus().code == ErrorCode::WrongThread);
    contract.owningThreadToken = state.thread;
    state.current = false;
    Result<void> notCurrent = ValidateGlContextContract(contract);
    CHECK(!notCurrent);
    CHECK(notCurrent.GetStatus().code == ErrorCode::InvalidState);
    state.current = true;
    return true;
}

bool TestCapabilityQuery() {
    FakeGlState state;
    GlFunctionTable functions = MakeFunctions(state);
    GlContextContract contract = MakeContract(state);

    Result<GlCapabilities> queried =
        QueryGlCapabilities(functions, contract);
    CHECK(queried);
    CHECK(queried.Value().majorVersion == 3U);
    CHECK(queried.Value().minorVersion == 3U);
    CHECK(queried.Value().coreProfile);
    CHECK(queried.Value().debugContext);
    CHECK(queried.Value().contextGeneration == 5U);
    CHECK(queried.Value().limits.maxTextureSize == 8192U);
    CHECK(queried.Value().limits.maxArrayTextureLayers == 256U);
    CHECK(queried.Value().limits.maxCombinedTextureUnits == 4U);
    CHECK(queried.Value().limits.maxUniformBufferBindings == 36U);
    CHECK(queried.Value().limits.uniformBufferOffsetAlignment == 256U);

    state.profile = GlConstant::ContextCompatibilityProfileBit;
    Result<GlCapabilities> compatibility =
        QueryGlCapabilities(functions, contract);
    CHECK(!compatibility);
    CHECK(compatibility.GetStatus().code == ErrorCode::Unsupported);
    state.profile = GlConstant::ContextCoreProfileBit;
    state.minor = 2;
    Result<GlCapabilities> oldVersion =
        QueryGlCapabilities(functions, contract);
    CHECK(!oldVersion);
    CHECK(oldVersion.GetStatus().code == ErrorCode::Unsupported);
    return true;
}

GlCapabilities QueryCapabilities(
    FakeGlState& state,
    const GlFunctionTable& functions) {
    Result<GlCapabilities> result =
        QueryGlCapabilities(functions, MakeContract(state));
    return result ? result.Value() : GlCapabilities{};
}

bool TestHostResetStateCachingAndGeneration() {
    FakeGlState state;
    GlFunctionTable functions = MakeFunctions(state);
    GlCapabilities capabilities =
        QueryCapabilities(state, functions);
    CHECK(capabilities.coreProfile);

    GlStateCache cache;
    CHECK(cache.Initialize(functions, capabilities));
    CHECK(!cache.UseProgram(9U));
    CHECK(cache.Begin(5U, GlEmbeddingMode::HostReset));
    const std::uint64_t beforeProgram = state.programCalls;
    CHECK(cache.UseProgram(9U));
    CHECK(cache.UseProgram(9U));
    CHECK(state.programCalls == beforeProgram + 1U);
    CHECK(cache.Statistics().redundantCalls == 1U);
    CHECK(cache.BindVertexArray(10U));
    CHECK(cache.BindArrayBuffer(11U));
    CHECK(cache.BindElementArrayBuffer(12U));
    CHECK(cache.BindUniformBuffer(13U));
    CHECK(cache.BindDrawFramebuffer(14U));
    CHECK(cache.BindReadFramebuffer(15U));
    CHECK(cache.SetViewport({0, 0, 800, 600}));
    CHECK(cache.SetScissor(true, {5, 6, 100, 120}));

    GlBlendState blend{
        true, FuncAdd, FuncAdd, One, Zero, One, Zero,
        true, true, true, true};
    GlDepthState depth{true, Less, true};
    GlStencilState stencil{
        true,
        {Always, 1, 0xFFU, Keep, Keep, Keep, 0xFFU},
        {Always, 2, 0xFFU, Keep, Keep, Keep, 0xFFU}};
    CHECK(cache.SetBlendState(blend));
    CHECK(cache.SetDepthState(depth));
    CHECK(cache.SetRasterState(
        {true, GlConstant::Back, CounterClockwise, Fill}));
    CHECK(cache.SetStencilState(stencil));
    CHECK(cache.BindTextureSampler(
        2U, GlConstant::Texture2D, 21U, 22U));
    CHECK(!cache.BindTextureSampler(
        4U, GlConstant::Texture2D, 1U, 1U));
    CHECK(cache.SetPixelUnpack({1, 20, 0, 1}));
    CHECK(cache.End());

    CHECK(cache.Begin(5U, GlEmbeddingMode::HostReset));
    CHECK(cache.UseProgram(9U));
    CHECK(state.programCalls == beforeProgram + 2U);
    CHECK(cache.End());
    CHECK(!cache.Begin(6U, GlEmbeddingMode::HostReset));
    cache.Invalidate(6U);
    CHECK(cache.Begin(6U, GlEmbeddingMode::HostReset));
    CHECK(cache.End());
    return true;
}

bool TestPreserveAndRestore() {
    FakeGlState state;
    for (std::uint32_t unit = 0U; unit < 4U; ++unit) {
        state.textures[unit] = 100U + unit;
        state.textureArrays[unit] = 150U + unit;
        state.samplers[unit] = 200U + unit;
    }
    const FakeGlState original = state;
    GlFunctionTable functions = MakeFunctions(state);
    GlCapabilities capabilities =
        QueryCapabilities(state, functions);

    GlStateCache cache;
    CHECK(cache.Initialize(functions, capabilities));
    CHECK(cache.Begin(5U, GlEmbeddingMode::PreserveAndRestore));
    CHECK(cache.UseProgram(31U));
    CHECK(cache.BindVertexArray(32U));
    CHECK(cache.BindArrayBuffer(33U));
    CHECK(cache.BindElementArrayBuffer(34U));
    CHECK(cache.BindUniformBuffer(35U));
    CHECK(cache.BindDrawFramebuffer(36U));
    CHECK(cache.BindReadFramebuffer(37U));
    CHECK(cache.SetViewport({0, 0, 1280, 720}));
    CHECK(cache.SetScissor(false, {0, 0, 1280, 720}));
    CHECK(cache.SetBlendState(
        {false, FuncAdd, FuncAdd, Zero, One, Zero, One,
         false, false, false, false}));
    CHECK(cache.SetDepthState({false, Always, true}));
    CHECK(cache.SetRasterState(
        {false, GlConstant::Front, CounterClockwise, Fill}));
    CHECK(cache.SetStencilState({
        false,
        {Always, 0, 0U, Keep, Keep, Keep, 0U},
        {Always, 0, 0U, Keep, Keep, Keep, 0U}}));
    CHECK(cache.BindTextureSampler(
        3U, GlConstant::Texture2D, 333U, 444U));
    CHECK(cache.SetPixelUnpack({8, 0, 0, 0}));
    CHECK(cache.End());

    CHECK(state.program == original.program);
    CHECK(state.vertexArray == original.vertexArray);
    CHECK(state.arrayBuffer == original.arrayBuffer);
    CHECK(state.elementArrayBuffer == original.elementArrayBuffer);
    CHECK(state.uniformBuffer == original.uniformBuffer);
    CHECK(state.drawFramebuffer == original.drawFramebuffer);
    CHECK(state.readFramebuffer == original.readFramebuffer);
    CHECK(state.viewport.x == original.viewport.x);
    CHECK(state.viewport.width == original.viewport.width);
    CHECK(state.scissorEnabled == original.scissorEnabled);
    CHECK(state.blend.enabled == original.blend.enabled);
    CHECK(state.blend.writeGreen == original.blend.writeGreen);
    CHECK(state.depth.enabled == original.depth.enabled);
    CHECK(state.depth.writeEnabled == original.depth.writeEnabled);
    CHECK(state.raster.cullEnabled == original.raster.cullEnabled);
    CHECK(state.raster.cullFace == original.raster.cullFace);
    CHECK(state.stencil.front.reference ==
        original.stencil.front.reference);
    CHECK(state.stencil.back.reference ==
        original.stencil.back.reference);
    CHECK(state.activeTextureUnit == original.activeTextureUnit);
    CHECK(state.textures[3] == original.textures[3]);
    CHECK(state.textureArrays[3] == original.textureArrays[3]);
    CHECK(state.samplers[3] == original.samplers[3]);
    CHECK(state.unpack.alignment == original.unpack.alignment);
    CHECK(state.unpack.rowLength == original.unpack.rowLength);
    CHECK(cache.Statistics().captures == 1U);
    CHECK(cache.Statistics().restores == 1U);
    return true;
}

} // namespace

int main() {
    if (!TestFunctionTableAndContextValidation() ||
        !TestCapabilityQuery() ||
        !TestHostResetStateCachingAndGeneration() ||
        !TestPreserveAndRestore()) {
        return 1;
    }
    std::puts("OpenGL 3.3 function-table and state-cache tests passed");
    return 0;
}
